
#include "VoxelBase.h"
#include "VoxelCollisionDecomposition.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/ConvexElem.h"
#include "Engine/Texture2DArray.h"

// Which voxel meshing approach to use
// 0 = Basic Face Culling (OUTDATED, DO NOT USE!)
// 1 = 1D Greedy Meshing
// 2 = Regular Greedy Meshing
#define MESHING_METHOD 2

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

void AVoxelBase::ApplyVoxelEdit(const TArray<FUintVector3>& Positions, uint8 Value)
{
    if (Positions.IsEmpty())
    {
        return;
    }

    for (const FUintVector3& Position : Positions)
    {
        Grid.SetVoxel(FIntVector3(Position), Value);
    }

    RegenerateMesh();
    CreateCollision();
    OnVoxelsChanged();
}

void AVoxelBase::BeginPlay() { Super::BeginPlay(); }

void AVoxelBase::RegenerateMesh()
{
    if (Grid.Data.IsEmpty()) return;

#if MESHING_METHOD == 0
    for (int z = 0; z < VOXEL_CHUNK_SIZE; ++z)
    {
        for (int y = 0; y < VOXEL_CHUNK_SIZE; ++y)
        {
            for (int x = 0; x < VOXEL_CHUNK_SIZE; ++x)
            {
                if (Grid.GetVoxel(FUintVector3(x, y, z)) == 0) continue;

                FVector VoxelPos = FVector(x * VoxelSize, y * VoxelSize, z * VoxelSize);
                FVector2D FaceScale = FVector2D(VoxelSize, VoxelSize);

                if (Grid.GetVoxel(FUintVector3(x + 1, y, z)) == 0) AddFace(VoxelPos, EFaceDirection::POSITIVE_X, FaceScale);
                if (Grid.GetVoxel(FUintVector3(x - 1, y, z)) == 0) AddFace(VoxelPos, EFaceDirection::NEGATIVE_X, FaceScale);
                if (Grid.GetVoxel(FUintVector3(x, y + 1, z)) == 0) AddFace(VoxelPos, EFaceDirection::POSITIVE_Y, FaceScale);
                if (Grid.GetVoxel(FUintVector3(x, y - 1, z)) == 0) AddFace(VoxelPos, EFaceDirection::NEGATIVE_Y, FaceScale);
                if (Grid.GetVoxel(FUintVector3(x, y, z + 1)) == 0) AddFace(VoxelPos, EFaceDirection::POSITIVE_Z, FaceScale);
                if (Grid.GetVoxel(FUintVector3(x, y, z - 1)) == 0) AddFace(VoxelPos, EFaceDirection::NEGATIVE_Z, FaceScale);
            }
        }
    }
#elif MESHING_METHOD == 1
    CreateMeshPlaneGreedy1D(EFaceDirection::POSITIVE_X);
    CreateMeshPlaneGreedy1D(EFaceDirection::NEGATIVE_X);
    CreateMeshPlaneGreedy1D(EFaceDirection::POSITIVE_Y);
    CreateMeshPlaneGreedy1D(EFaceDirection::NEGATIVE_Y);
    CreateMeshPlaneGreedy1D(EFaceDirection::POSITIVE_Z);
    CreateMeshPlaneGreedy1D(EFaceDirection::NEGATIVE_Z);
#elif MESHING_METHOD == 2
    CreateMeshPlaneGreedy(EFaceDirection::POSITIVE_X);
    CreateMeshPlaneGreedy(EFaceDirection::NEGATIVE_X);
    CreateMeshPlaneGreedy(EFaceDirection::POSITIVE_Y);
    CreateMeshPlaneGreedy(EFaceDirection::NEGATIVE_Y);
    CreateMeshPlaneGreedy(EFaceDirection::POSITIVE_Z);
    CreateMeshPlaneGreedy(EFaceDirection::NEGATIVE_Z);
#endif

    if (ProceduralMesh)
    {
        ProceduralMesh->ClearAllMeshSections();
        ProceduralMesh->CreateMeshSection_LinearColor(0,
                                                      Vertices,
                                                      Indices,
                                                      Normals,
                                                      TArray<FVector2D>(),
                                                      TArray<FLinearColor>(),
                                                      TArray<FProcMeshTangent>(),
                                                      false);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("%s: ProceduralMesh component missing!"), *GetName());
    }

    Vertices.Empty();
    Normals.Empty();
    Indices.Empty();
    IndexCount = 0;
}

void AVoxelBase::CreateMaterialInstance(UMaterial* baseMaterial, UTexture2DArray* voxelTextures)
{
    // Create material and texture if they don't exist yet
    if (baseMaterial && voxelTextures && m_materialDataCreated == false)
    {
        // Create texture to store voxel data
        uint32 textureHeight = Grid.Data.Num() / Grid.GridSize;

        VoxelDataTexture = UTexture2D::CreateTransient(Grid.GridSize, textureHeight, PF_R8);
        VoxelDataTexture->CompressionSettings = TextureCompressionSettings::TC_Grayscale;
        VoxelDataTexture->SRGB = 0;
        VoxelDataTexture->Filter = TextureFilter::TF_Nearest;
        VoxelDataTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
        VoxelDataTexture->CompressionNone = 1;

        // Upload voxel data to texture
        void* TargetTextureData = VoxelDataTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
        FMemory::Memcpy(TargetTextureData, Grid.Data.GetData(), Grid.Data.Num());

        VoxelDataTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
        VoxelDataTexture->UpdateResource();

        // Create material instance for chunk mesh
        MaterialInstance = UMaterialInstanceDynamic::Create(baseMaterial, this);
        MaterialInstance->SetTextureParameterValue(TEXT("VoxelTextures"), voxelTextures);
        MaterialInstance->SetTextureParameterValue(TEXT("VoxelDataTexture"), VoxelDataTexture);

        MaterialInstance->SetScalarParameterValue(TEXT("VoxelSize"), static_cast<float>(VoxelSize));
        MaterialInstance->SetScalarParameterValue(TEXT("VoxelDataSize"), static_cast<float>(Grid.GridSize));

        ProceduralMesh->SetMaterial(0, MaterialInstance);

        m_materialDataCreated = true;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Missing material data for chunk, cannot assign material!"));
    }
}

void AVoxelBase::CreateMeshPlaneGreedy1D(EFaceDirection FaceDirection)
{
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

    for (int y = 0; y < VOXEL_CHUNK_SIZE; ++y)
    {
        for (int x = 0; x < VOXEL_CHUNK_SIZE; ++x)
        {
            int CurIndex = Grid.GetBitPlaneIndex(x, y);
            int FrontIndex = Grid.GetBitPlaneIndex(x + Normal.X, y + Normal.Y);

            uint64 CurColumn = (*OriginPlaneBitData)[CurIndex];
            uint64 FrontColumn = (*OriginPlaneBitData)[FrontIndex];

            uint64 CanHaveFaceColumn = CurColumn & ~FrontColumn;

            if (CanHaveFaceColumn == 0) continue;

            for (uint32 z = 0; z < VOXEL_CHUNK_SIZE;)
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

                    FVector VoxelPos;
                    if (UsingXZPlane)
                        VoxelPos = FVector(x * VoxelSize, z * VoxelSize, y * VoxelSize);
                    else
                        VoxelPos = FVector(x * VoxelSize, y * VoxelSize, z * VoxelSize);

                    FVector2D FaceScale = FVector2D(VoxelSize, Leading1s * VoxelSize);
                    AddFace(VoxelPos, FaceDirection, FaceScale);

                    z += Leading1s;
                }
            }
        }
    }
}

void AVoxelBase::CreateMeshPlaneGreedy(EFaceDirection FaceDirection)
{
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

    for (int y = 0; y < VOXEL_CHUNK_SIZE; ++y)
    {
        for (int x = 0; x < VOXEL_CHUNK_SIZE; ++x)
        {
            int CurIndex = Grid.GetBitPlaneIndex(x, y);
            int FrontIndex = Grid.GetBitPlaneIndex(x + Normal.X, y + Normal.Y);

            uint64 CurColumn = TempPlaneBitData[CurIndex];
            uint64 FrontColumn = (*OriginPlaneBitData)[FrontIndex];

            uint64 CanHaveFaceColumn = CurColumn & ~FrontColumn;

            // No faces can be generated anywhere, so continue
            if (CanHaveFaceColumn == 0) continue;

            for (uint32 z = 0; z < VOXEL_CHUNK_SIZE;)
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

                    for (uint32 i = 1; i <= VOXEL_CHUNK_SIZE; ++i)
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
                        if (CheckShiftedMasked != CheckMask || x + LoopPos.X >= VOXEL_CHUNK_SIZE ||
                            y + LoopPos.Y >= VOXEL_CHUNK_SIZE)
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
                        FVector VoxelPos;
                        if (UsingXZPlane)
                            VoxelPos = FVector(x * VoxelSize, z * VoxelSize, y * VoxelSize);
                        else
                            VoxelPos = FVector(x * VoxelSize, y * VoxelSize, z * VoxelSize);

                        FVector2D FaceScale = FVector2D(XScale * VoxelSize, Leading1s * VoxelSize);
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
    if (!ProceduralMesh || Grid.Data.IsEmpty())
    {
        return;
    }

    if (CollisionType == EVoxelCollisionType::MaxIS || CollisionType == EVoxelCollisionType::IS1)
    {
        const float Density = DecompVoxel::EstimateConcaveEdgeDensity(Grid);
        if (Density > 2.0f)
        {
            UE_LOG(LogTemp,
                   Warning,
                   TEXT("%s: %s collision decomposition has a high concave-edge density (%.2f, structured shapes are usually "
                        "<1.0) - this will likely take several seconds or more. Consider 3GDM/BSP for this shape instead."),
                   *GetName(),
                   CollisionType == EVoxelCollisionType::MaxIS ? TEXT("MaxIS") : TEXT("IS(1)"),
                   Density);
        }
    }

    UBodySetup* BodySetup = ProceduralMesh->GetBodySetup();
    if (!BodySetup)
    {
        return;
    }

    BodySetup->AggGeom.BoxElems.Reset();
    BodySetup->AggGeom.ConvexElems.Reset();

    if (CollisionType == EVoxelCollisionType::ConvexHull)
    {
        TArray<FVector> HullPoints;
        for (uint32 z = 0; z < VOXEL_CHUNK_SIZE; ++z)
        {
            for (uint32 y = 0; y < VOXEL_CHUNK_SIZE; ++y)
            {
                for (uint32 x = 0; x < VOXEL_CHUNK_SIZE; ++x)
                {
                    if (Grid.GetVoxel(FIntVector3(x, y, z)) == 0) continue;
                    const bool bSurface =
                        Grid.GetVoxel(FIntVector3(x - 1, y, z)) == 0 || Grid.GetVoxel(FIntVector3(x + 1, y, z)) == 0 ||
                        Grid.GetVoxel(FIntVector3(x, y - 1, z)) == 0 || Grid.GetVoxel(FIntVector3(x, y + 1, z)) == 0 ||
                        Grid.GetVoxel(FIntVector3(x, y, z - 1)) == 0 || Grid.GetVoxel(FIntVector3(x, y, z + 1)) == 0;
                    if (!bSurface) continue;

                    const FVector Min = FVector((float)x, (float)y, (float)z) * VoxelSize;
                    const FVector Max = FVector((float)(x + 1), (float)(y + 1), (float)(z + 1)) * VoxelSize;
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

        BodySetup->AggGeom.BoxElems.Reserve(Blocks.Num());
        for (const DecompVoxel::FVoxelBlock& Block : Blocks)
        {
            const FVector BlockMin = FVector(Block.X, Block.Y, Block.Z) * VoxelSize;
            const FVector BlockSize = FVector(Block.SizeX, Block.SizeY, Block.SizeZ) * VoxelSize;
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

    BodySetup->InvalidatePhysicsData();
    BodySetup->CreatePhysicsMeshes();
    ProceduralMesh->RecreatePhysicsState();
}