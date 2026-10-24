#include "VoxelBase.h"
#include "VoxelCollisionDecomposition.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/ConvexElem.h"

// Which voxel meshing approach to use
// 0 = 1D Greedy Meshing
// 1 = Regular Greedy Meshing
#define MESHING_METHOD 1

AVoxelBase::AVoxelBase()
{
    PrimaryActorTick.bCanEverTick = false;

    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
    RootComponent = ProceduralMesh;

    ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    ProceduralMesh->SetCollisionObjectType(ECollisionChannel::ECC_PhysicsBody);
    ProceduralMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
    ProceduralMesh->bUseComplexAsSimpleCollision = false;
    ProceduralMesh->SetSimulatePhysics(false);
}

void AVoxelBase::ApplyVoxelEditArray(const TArray<FIntVector>& Positions, uint8 VoxelType)
{
    TMap<FIntVector, uint8> Edits;

    for (const auto& Position : Positions)
    {
        Edits.Add(Position, VoxelType);
    }

    ApplyVoxelEdit(Edits);
}

void AVoxelBase::BeginPlay() { Super::BeginPlay(); }

void AVoxelBase::RegenerateMesh()
{
    if (!ProceduralMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("%s: ProceduralMesh component missing!"), *GetName());
        return;
    }
        
    ProceduralMesh->ClearAllMeshSections();

    int MeshSectionIndex = 0;

    for (const auto& SubGrid : SubGrids)
    {
        if (SubGrid.Value.GridSize == 0) continue;

#if MESHING_METHOD == 0
        CreateMeshPlaneGreedy1D(SubGrid, EFaceDirection::POSITIVE_X);
        CreateMeshPlaneGreedy1D(SubGrid, EFaceDirection::NEGATIVE_X);
        CreateMeshPlaneGreedy1D(SubGrid, EFaceDirection::POSITIVE_Y);
        CreateMeshPlaneGreedy1D(SubGrid, EFaceDirection::NEGATIVE_Y);
        CreateMeshPlaneGreedy1D(SubGrid, EFaceDirection::POSITIVE_Z);
        CreateMeshPlaneGreedy1D(SubGrid, EFaceDirection::NEGATIVE_Z);
#elif MESHING_METHOD == 1
        CreateMeshPlaneGreedy(SubGrid, EFaceDirection::POSITIVE_X);
        CreateMeshPlaneGreedy(SubGrid, EFaceDirection::NEGATIVE_X);
        CreateMeshPlaneGreedy(SubGrid, EFaceDirection::POSITIVE_Y);
        CreateMeshPlaneGreedy(SubGrid, EFaceDirection::NEGATIVE_Y);
        CreateMeshPlaneGreedy(SubGrid, EFaceDirection::POSITIVE_Z);
        CreateMeshPlaneGreedy(SubGrid, EFaceDirection::NEGATIVE_Z);
#endif
 
        ProceduralMesh->CreateMeshSection_LinearColor(MeshSectionIndex,
                                                      Vertices,
                                                      Indices,
                                                      Normals,
                                                      TArray<FVector2D>(),
                                                      TArray<FLinearColor>(),
                                                      TArray<FProcMeshTangent>(),
                                                      false);
        // Set Material
        if (SubGrid.Value.MaterialInstance)
        {
            ProceduralMesh->SetMaterial(MeshSectionIndex, SubGrid.Value.MaterialInstance);
        }

        MeshSectionIndex++;

        Vertices.Empty();
        Normals.Empty();
        Indices.Empty();
        IndexCount = 0;
    }
}

void AVoxelBase::CreateMeshPlaneGreedy1D(const TPair<FIntVector, FVoxelGrid>& SubGrid, EFaceDirection FaceDirection)
{
    const FVoxelGrid& Grid = SubGrid.Value;

    FIntVector2 Normal;
    bool UsingXZPlane = false;

    switch (FaceDirection)
    {
        case EFaceDirection::POSITIVE_X:
            Normal = FIntVector2(1, 0);
            break;
        case EFaceDirection::NEGATIVE_X:
            Normal = FIntVector2(-1, 0);
            break;
        case EFaceDirection::POSITIVE_Y:
            Normal = FIntVector2(0, 1);
            break;
        case EFaceDirection::NEGATIVE_Y:
            Normal = FIntVector2(0, -1);
            break;
        case EFaceDirection::POSITIVE_Z:
            Normal = FIntVector2(0, 1);
            UsingXZPlane = true;
            break;
        case EFaceDirection::NEGATIVE_Z:
            Normal = FIntVector2(0, -1);
            UsingXZPlane = true;
            break;
        default:
            break;
    }

    const TArray<uint64>* OriginPlaneBitData = &Grid.XYPlaneBitData;
    if (UsingXZPlane) OriginPlaneBitData = &Grid.XZPlaneBitData;

    for (int y = 0; y < static_cast<int>(VOXEL_GRID_SIZE); ++y)
    {
        for (int x = 0; x < static_cast<int>(VOXEL_GRID_SIZE); ++x)
        {
            int CurIndex = Grid.GetBitPlaneIndex(x, y);
            int FrontIndex = Grid.GetBitPlaneIndex(x + Normal.X, y + Normal.Y);

            uint64 CurColumn = (*OriginPlaneBitData)[CurIndex];
            uint64 FrontColumn = (*OriginPlaneBitData)[FrontIndex];

            uint64 CanHaveFaceColumn = CurColumn & ~FrontColumn;

            // No faces can be generated anywhere, so continue
            if (CanHaveFaceColumn == 0) continue;

            for (uint32 z = 0; z < VOXEL_GRID_SIZE;)
            {
                uint64 Shifted = CanHaveFaceColumn >> z;

                if ((Shifted & 1u) == 0)
                {
                    uint32 Leading0s = FMath::CountTrailingZeros64(Shifted);
                    z += Leading0s;
                }
                else
                {
                    uint32 Leading1s = FMath::CountTrailingZeros64(~Shifted);

                    FIntVector SubGridOffset = SubGrid.Key * static_cast<int>(VOXEL_GRID_SIZE);

                    FVector VoxelPos;
                    if (UsingXZPlane)
                        VoxelPos = FVector(static_cast<double>(x + SubGridOffset.X),
                                           static_cast<double>(static_cast<int>(z) + SubGridOffset.Y),
                                           static_cast<double>(y + SubGridOffset.Z));
                    else
                        VoxelPos = FVector(static_cast<double>(x + SubGridOffset.X),
                                           static_cast<double>(y + SubGridOffset.Y),
                                           static_cast<double>(static_cast<int>(z) + SubGridOffset.Z));

                    VoxelPos *= static_cast<double>(VoxelSize);
                    FVector2D FaceScale = FVector2D(static_cast<double>(VoxelSize), static_cast<double>(Leading1s * VoxelSize));

                    AddFace(VoxelPos, FaceDirection, FaceScale);

                    z += Leading1s;
                }
            }
        }
    }
}

void AVoxelBase::CreateMeshPlaneGreedy(const TPair<FIntVector, FVoxelGrid>& SubGrid, EFaceDirection FaceDirection)
{
    const FVoxelGrid& Grid = SubGrid.Value;

    FIntVector2 Normal;
    bool UsingXZPlane = false;

    switch (FaceDirection)
    {
        case EFaceDirection::POSITIVE_X:
            Normal = FIntVector2(1, 0);
            break;
        case EFaceDirection::NEGATIVE_X:
            Normal = FIntVector2(-1, 0);
            break;
        case EFaceDirection::POSITIVE_Y:
            Normal = FIntVector2(0, 1);
            break;
        case EFaceDirection::NEGATIVE_Y:
            Normal = FIntVector2(0, -1);
            break;
        case EFaceDirection::POSITIVE_Z:
            Normal = FIntVector2(0, 1);
            UsingXZPlane = true;
            break;
        case EFaceDirection::NEGATIVE_Z:
            Normal = FIntVector2(0, -1);
            UsingXZPlane = true;
            break;
        default:
            break;
    }

    const TArray<uint64>* OriginPlaneBitData = &Grid.XYPlaneBitData;
    if (UsingXZPlane) OriginPlaneBitData = &Grid.XZPlaneBitData;

    TempPlaneBitData.Init(0, Grid.BitGridSize * Grid.BitGridSize);
    memcpy(TempPlaneBitData.GetData(), OriginPlaneBitData->GetData(), Grid.BitGridSize * Grid.BitGridSize * sizeof(uint64));

    for (int y = 0; y < static_cast<int>(VOXEL_GRID_SIZE); ++y)
    {
        for (int x = 0; x < static_cast<int>(VOXEL_GRID_SIZE); ++x)
        {
            int CurIndex = Grid.GetBitPlaneIndex(x, y);
            int FrontIndex = Grid.GetBitPlaneIndex(x + Normal.X, y + Normal.Y);

            uint64 CurColumn = TempPlaneBitData[CurIndex];
            uint64 FrontColumn = (*OriginPlaneBitData)[FrontIndex];

            uint64 CanHaveFaceColumn = CurColumn & ~FrontColumn;

            // No faces can be generated anywhere, so continue
            if (CanHaveFaceColumn == 0) continue;

            for (uint32 z = 0; z < VOXEL_GRID_SIZE;)
            {
                uint64 Shifted = CanHaveFaceColumn >> z;

                if ((Shifted & 1u) == 0)
                {
                    // Air block (0-bits), so skip to where there are no more air
                    uint32 Leading0s = FMath::CountTrailingZeros64(Shifted);
                    z += Leading0s;
                }
                else
                {
                    // The height of the face
                    uint32 Leading1s = FMath::CountTrailingZeros64(~Shifted);
                    // The width of the face
                    uint32 XScale = 0;

                    // Creates n 1-bits at least significant bits, where n = Leading1s
                    uint64 CheckMask = ((1ui64 << Leading1s) - 1);

                    for (uint32 i = 1; i <= VOXEL_GRID_SIZE; ++i)
                    {
                        FIntVector2 LoopPos;
                        switch (FaceDirection)
                        {
                            case EFaceDirection::POSITIVE_X:
                            case EFaceDirection::NEGATIVE_X:
                                LoopPos = FIntVector2(0, i);
                                break;
                            case EFaceDirection::POSITIVE_Y:
                            case EFaceDirection::NEGATIVE_Y:
                            case EFaceDirection::POSITIVE_Z:
                            case EFaceDirection::NEGATIVE_Z:
                                LoopPos = FIntVector2(i, 0);
                                break;
                            default:
                                break;
                        }

                        int CheckCurIndex = Grid.GetBitPlaneIndex(x + LoopPos.X, y + LoopPos.Y);
                        int CheckFrontIndex = Grid.GetBitPlaneIndex(x + LoopPos.X + Normal.X, y + LoopPos.Y + Normal.Y);

                        uint64 CheckCurColumn = TempPlaneBitData[CheckCurIndex];
                        uint64 CheckFrontColumn = (*OriginPlaneBitData)[CheckFrontIndex];

                        uint64 CheckCanHaveFaceColumn = CheckCurColumn & ~CheckFrontColumn;
                        uint64 CheckShifted = CheckCanHaveFaceColumn >> z;
                        uint64 CheckShiftedMasked = CheckShifted & CheckMask;

                        // Check if face's width cannot be expanded anymore
                        if (CheckShiftedMasked != CheckMask || x + LoopPos.X >= VOXEL_GRID_SIZE ||
                            y + LoopPos.Y >= VOXEL_GRID_SIZE)
                        {
                            XScale = i;
                            break;
                        }
                        else
                        {
                            // Set bits to 0 in area where the new face will be,
                            // to prevent another face to be generated there during future iterations of these loops
                            TempPlaneBitData[CheckCurIndex] = CheckCurColumn & ~(CheckMask << z);
                        }
                    }

                    if (XScale > 0)
                    {
                        FIntVector SubGridOffset = SubGrid.Key * static_cast<int>(VOXEL_GRID_SIZE);

                        FVector VoxelPos;
                        if (UsingXZPlane)
                            VoxelPos = FVector(static_cast<double>(x + SubGridOffset.X),
                                               static_cast<double>(static_cast<int>(z) + SubGridOffset.Y),
                                               static_cast<double>(y + SubGridOffset.Z));
                        else
                            VoxelPos = FVector(static_cast<double>(x + SubGridOffset.X),
                                               static_cast<double>(y + SubGridOffset.Y),
                                               static_cast<double>(static_cast<int>(z) + SubGridOffset.Z));

                        VoxelPos *= static_cast<double>(VoxelSize);
                        FVector2D FaceScale =
                            FVector2D(static_cast<double>(XScale * VoxelSize), static_cast<double>(Leading1s * VoxelSize));

                        AddFace(VoxelPos, FaceDirection, FaceScale);
                    }

                    z += Leading1s;
                }
            }
        }
    }

    TempPlaneBitData.Empty();
}

void AVoxelBase::AddFace(const FVector& Position, EFaceDirection FaceDirection, const FVector2D& Scale)
{
    FVector V0;
    FVector V1;
    FVector V2;
    FVector V3;

    FVector Normal;
    switch (FaceDirection)
    {
        case EFaceDirection::POSITIVE_X:
            V0 = FVector(VoxelSize, 0.0, 0.0);
            V1 = FVector(VoxelSize, Scale.X, 0.0);
            V2 = FVector(VoxelSize, 0.0, Scale.Y);
            V3 = FVector(VoxelSize, Scale.X, Scale.Y);
            Normal = FVector(1.0, 0.0, 0.0);
            break;
        case EFaceDirection::NEGATIVE_X:
            V0 = FVector(0.0, Scale.X, 0.0);
            V1 = FVector(0.0, 0.0, 0.0);
            V2 = FVector(0.0, Scale.X, Scale.Y);
            V3 = FVector(0.0, 0.0, Scale.Y);
            Normal = FVector(-1.0, 0.0, 0.0);
            break;
        case EFaceDirection::POSITIVE_Y:
            V0 = FVector(Scale.X, VoxelSize, 0.0);
            V1 = FVector(0.0, VoxelSize, 0.0);
            V2 = FVector(Scale.X, VoxelSize, Scale.Y);
            V3 = FVector(0.0, VoxelSize, Scale.Y);
            Normal = FVector(0.0, 1.0, 0.0);
            break;
        case EFaceDirection::NEGATIVE_Y:
            V0 = FVector(0.0, 0.0, 0.0);
            V1 = FVector(Scale.X, 0.0, 0.0);
            V2 = FVector(0.0, 0.0, Scale.Y);
            V3 = FVector(Scale.X, 0.0, Scale.Y);
            Normal = FVector(0.0, -1.0, 0.0);
            break;
        case EFaceDirection::POSITIVE_Z:
            V0 = FVector(0.0, 0.0, VoxelSize);
            V1 = FVector(Scale.X, 0.0, VoxelSize);
            V2 = FVector(0.0, Scale.Y, VoxelSize);
            V3 = FVector(Scale.X, Scale.Y, VoxelSize);
            Normal = FVector(0.0, 0.0, 1.0);
            break;
        case EFaceDirection::NEGATIVE_Z:
            V0 = FVector(Scale.X, 0.0, 0.0);
            V1 = FVector(0.0, 0.0, 0.0);
            V2 = FVector(Scale.X, Scale.Y, 0.0);
            V3 = FVector(0.0, Scale.Y, 0.0);
            Normal = FVector(0.0, 0.0, -1.0);
            break;
        default:
            break;
    }

    Vertices.Add(V0 + Position);
    Vertices.Add(V1 + Position);
    Vertices.Add(V2 + Position);
    Vertices.Add(V3 + Position);

    Normals.Add(Normal);
    Normals.Add(Normal);
    Normals.Add(Normal);
    Normals.Add(Normal);

    Indices.Add(2 + IndexCount);
    Indices.Add(1 + IndexCount);
    Indices.Add(0 + IndexCount);
    Indices.Add(2 + IndexCount);
    Indices.Add(3 + IndexCount);
    Indices.Add(1 + IndexCount);

    IndexCount += 4;
}

void AVoxelBase::CreateCollision()
{
    if (!ProceduralMesh || SubGrids.IsEmpty())
    {
        return;
    }

    UBodySetup* BodySetup = ProceduralMesh->GetBodySetup();
    if (!BodySetup)
    {
        return;
    }

    BodySetup->AggGeom.BoxElems.Reset();
    BodySetup->AggGeom.ConvexElems.Reset();

    // Jumps from grid instance to grid instance, decomposing each
    // independently (DecompVoxel operates on one FVoxelGrid at a time
    // regardless of how many exist) and accumulating every result into the
    // same shared BodySetup, offset by that instance's own position. The
    // per-sub-grid hashmap lookup cost (a cache miss at worst, per the
    // earlier discussion) is the accepted tradeoff for not needing one
    // contiguous array at all.
    for (const auto& SubGrid : SubGrids)
    {
        if (SubGrid.Value.GridSize == 0) continue;
        const FVoxelGrid& Grid = SubGrid.Value;
        const FVector WorldOffset = FVector(SubGrid.Key * static_cast<int>(VOXEL_GRID_SIZE)) * static_cast<double>(VoxelSize);

        if (CollisionType == EVoxelCollisionType::MaxIS || CollisionType == EVoxelCollisionType::IS1)
        {
            const float Density = DecompVoxel::EstimateConcaveEdgeDensity(Grid);
        }

        if (CollisionType == EVoxelCollisionType::ConvexHull)
        {
            TArray<FVector> HullPoints;
            for (uint32 z = 0; z < VOXEL_GRID_SIZE; ++z)
            {
                for (uint32 y = 0; y < VOXEL_GRID_SIZE; ++y)
                {
                    for (uint32 x = 0; x < VOXEL_GRID_SIZE; ++x)
                    {
                        if (Grid.GetVoxel(FIntVector3(x, y, z)) == 0) continue;
                        const bool bSurface =
                            Grid.GetVoxel(FIntVector3(x - 1, y, z)) == 0 || Grid.GetVoxel(FIntVector3(x + 1, y, z)) == 0 ||
                            Grid.GetVoxel(FIntVector3(x, y - 1, z)) == 0 || Grid.GetVoxel(FIntVector3(x, y + 1, z)) == 0 ||
                            Grid.GetVoxel(FIntVector3(x, y, z - 1)) == 0 || Grid.GetVoxel(FIntVector3(x, y, z + 1)) == 0;
                        if (!bSurface) continue;

                        const FVector Min = WorldOffset + FVector((float)x, (float)y, (float)z) * (float)VoxelSize;
                        const FVector Max =
                            WorldOffset + FVector((float)(x + 1), (float)(y + 1), (float)(z + 1)) * (float)VoxelSize;
                        HullPoints.Add(FVector(Min.X, Min.Y, Min.Z));
                        HullPoints.Add(FVector(Max.X, Min.Y, Min.Z));
                        HullPoints.Add(FVector(Min.X, Max.Y, Min.Z));
                        HullPoints.Add(FVector(Max.X, Max.Y, Min.Z));
                        HullPoints.Add(FVector(Min.X, Min.Y, Max.Z));
                        HullPoints.Add(FVector(Max.X, Min.Y, Max.Z));
                        HullPoints.Add(FVector(Min.X, Max.Y, Max.Z));
                        HullPoints.Add(FVector(Max.X, Max.Y, Max.Z));
                    }
                }
            }

            FKConvexElem ConvexElem;
            ConvexElem.VertexData = HullPoints;
            ConvexElem.UpdateElemBox();
            BodySetup->AggGeom.ConvexElems.Add(ConvexElem);
        }
        else
        {
            TArray<DecompVoxel::FVoxelBlock> Blocks;
            switch (CollisionType)
            {
                case EVoxelCollisionType::Octree:
                    Blocks = DecompVoxel::DecomposeOctree(Grid);
                    break;
                case EVoxelCollisionType::BSP:
                    Blocks = DecompVoxel::DecomposeBSP(Grid);
                    break;
                case EVoxelCollisionType::MaxIS:
                    Blocks = DecompVoxel::DecomposeMaxIS(Grid);
                    break;
                case EVoxelCollisionType::IS1:
                    Blocks = DecompVoxel::DecomposeIS1(Grid);
                    break;
                case EVoxelCollisionType::ThreeGDM:
                default:
                    Blocks = DecompVoxel::Decompose3GDM(Grid);
                    break;
            }

            BodySetup->AggGeom.BoxElems.Reserve(BodySetup->AggGeom.BoxElems.Num() + Blocks.Num());
            for (const DecompVoxel::FVoxelBlock& Block : Blocks)
            {
                const FVector BlockMin = WorldOffset + FVector(Block.X, Block.Y, Block.Z) * (float)VoxelSize;
                const FVector BlockSize = FVector(Block.SizeX, Block.SizeY, Block.SizeZ) * (float)VoxelSize;
                const FVector BlockCenter = BlockMin + BlockSize * 0.5f;

                FKBoxElem BoxElem;
                BoxElem.Center = BlockCenter;
                BoxElem.Rotation = FRotator::ZeroRotator;
                BoxElem.X = BlockSize.X;
                BoxElem.Y = BlockSize.Y;
                BoxElem.Z = BlockSize.Z;

                BodySetup->AggGeom.BoxElems.Add(BoxElem);
            }
        }
    }

    BodySetup->InvalidatePhysicsData();
    BodySetup->CreatePhysicsMeshes();
    ProceduralMesh->RecreatePhysicsState();
}