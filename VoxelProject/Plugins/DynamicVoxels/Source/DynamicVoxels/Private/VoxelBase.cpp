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

void AVoxelBase::BeginPlay() { Super::BeginPlay(); }

void AVoxelBase::RegenerateMesh(const TArray<FVoxelGridInstance>& Grids)
{
    // IMPORTANT, NOT YET SOLVED: face-culling (every GetVoxel/bit-plane
    // "front" lookup below) only ever checks the SAME grid instance's own
    // data. A voxel sitting at the edge of one sub-grid has no way to see
    // whether the ADJACENT sub-grid has solid content right next to it, so
    // a face always gets generated at every sub-grid boundary - including
    // where two solid sub-grids actually touch with nothing between them.
    // For a construct that's still a single sub-grid (the current actual
    // state) this never shows up. The moment ApplyVoxelEdit grows a second
    // sub-grid, this becomes a visible seam/z-fight at the boundary. Same
    // category of problem as AVoxelChunk's neighbor-padding, genuinely
    // separate work, not something to solve inside this pass.

#if MESHING_METHOD == 0
    for (const FVoxelGridInstance& GridInstance : Grids)
    {
        if (!GridInstance.Grid) continue;
        const FVoxelGrid& Grid = *GridInstance.Grid;

        for (int z = 0; z < VOXEL_CHUNK_SIZE; ++z)
        {
            for (int y = 0; y < VOXEL_CHUNK_SIZE; ++y)
            {
                for (int x = 0; x < VOXEL_CHUNK_SIZE; ++x)
                {
                    if (Grid.GetVoxel(FIntVector3(x, y, z)) == 0) continue;

                    FVector VoxelPos = FVector((x + GridInstance.Offset.X) * VoxelSize,
                                               (y + GridInstance.Offset.Y) * VoxelSize,
                                               (z + GridInstance.Offset.Z) * VoxelSize);
                    FVector2D FaceScale = FVector2D(VoxelSize, VoxelSize);

                    if (Grid.GetVoxel(FIntVector3(x + 1, y, z)) == 0) AddFace(VoxelPos, EFaceDirection::POSITIVE_X, FaceScale);
                    if (Grid.GetVoxel(FIntVector3(x - 1, y, z)) == 0) AddFace(VoxelPos, EFaceDirection::NEGATIVE_X, FaceScale);
                    if (Grid.GetVoxel(FIntVector3(x, y + 1, z)) == 0) AddFace(VoxelPos, EFaceDirection::POSITIVE_Y, FaceScale);
                    if (Grid.GetVoxel(FIntVector3(x, y - 1, z)) == 0) AddFace(VoxelPos, EFaceDirection::NEGATIVE_Y, FaceScale);
                    if (Grid.GetVoxel(FIntVector3(x, y, z + 1)) == 0) AddFace(VoxelPos, EFaceDirection::POSITIVE_Z, FaceScale);
                    if (Grid.GetVoxel(FIntVector3(x, y, z - 1)) == 0) AddFace(VoxelPos, EFaceDirection::NEGATIVE_Z, FaceScale);
                }
            }
        }
    }
#elif MESHING_METHOD == 1
    for (const FVoxelGridInstance& GridInstance : Grids)
    {
        if (!GridInstance.Grid) continue;
        CreateMeshPlaneGreedy1D(GridInstance, EFaceDirection::POSITIVE_X);
        CreateMeshPlaneGreedy1D(GridInstance, EFaceDirection::NEGATIVE_X);
        CreateMeshPlaneGreedy1D(GridInstance, EFaceDirection::POSITIVE_Y);
        CreateMeshPlaneGreedy1D(GridInstance, EFaceDirection::NEGATIVE_Y);
        CreateMeshPlaneGreedy1D(GridInstance, EFaceDirection::POSITIVE_Z);
        CreateMeshPlaneGreedy1D(GridInstance, EFaceDirection::NEGATIVE_Z);
    }
#elif MESHING_METHOD == 2
    for (const FVoxelGridInstance& GridInstance : Grids)
    {
        if (!GridInstance.Grid) continue;
        CreateMeshPlaneGreedy(GridInstance, EFaceDirection::POSITIVE_X);
        CreateMeshPlaneGreedy(GridInstance, EFaceDirection::NEGATIVE_X);
        CreateMeshPlaneGreedy(GridInstance, EFaceDirection::POSITIVE_Y);
        CreateMeshPlaneGreedy(GridInstance, EFaceDirection::NEGATIVE_Y);
        CreateMeshPlaneGreedy(GridInstance, EFaceDirection::POSITIVE_Z);
        CreateMeshPlaneGreedy(GridInstance, EFaceDirection::NEGATIVE_Z);
    }
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

void AVoxelBase::CreateMaterialInstance(const FVoxelGrid& VoxelGrid, UMaterial* baseMaterial, UTexture2DArray* voxelTextures)
{
    // Create material and texture if they don't exist yet
    if (baseMaterial && voxelTextures && m_materialDataCreated == false)
    {
        // Create texture to store voxel data
        uint32 textureHeight = VoxelGrid.Data.Num() / VoxelGrid.GridSize;

        VoxelDataTexture = UTexture2D::CreateTransient(VoxelGrid.GridSize, textureHeight, PF_R8);
        VoxelDataTexture->CompressionSettings = TextureCompressionSettings::TC_Grayscale;
        VoxelDataTexture->SRGB = 0;
        VoxelDataTexture->Filter = TextureFilter::TF_Nearest;
#ifndef UE_BUILD_DEBUG
        VoxelDataTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
        VoxelDataTexture->CompressionNone = 1;
#endif  // !UE_BUILD_DEBUG

        // Upload voxel data to texture
        void* TargetTextureData = VoxelDataTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
        FMemory::Memcpy(TargetTextureData, VoxelGrid.Data.GetData(), VoxelGrid.Data.Num());

        VoxelDataTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
        VoxelDataTexture->UpdateResource();

        // Create material instance for chunk mesh
        MaterialInstance = UMaterialInstanceDynamic::Create(baseMaterial, this);
        MaterialInstance->SetTextureParameterValue(TEXT("VoxelTextures"), voxelTextures);
        MaterialInstance->SetTextureParameterValue(TEXT("VoxelDataTexture"), VoxelDataTexture);

        MaterialInstance->SetScalarParameterValue(TEXT("VoxelSize"), static_cast<float>(VoxelSize));
        MaterialInstance->SetScalarParameterValue(TEXT("VoxelDataSize"), static_cast<float>(VoxelGrid.GridSize));

        ProceduralMesh->SetMaterial(0, MaterialInstance);

        m_materialDataCreated = true;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Missing material data for chunk, cannot assign material!"));
    }
}

void AVoxelBase::CreateMeshPlaneGreedy1D(const FVoxelGridInstance& GridInstance, EFaceDirection FaceDirection)
{
    const FVoxelGrid& Grid = *GridInstance.Grid;

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

                    // XZ-plane: outer x = world X, outer y = world Z, the
                    // bit-scan z = world Y - matches how FVoxelGrid::SetVoxel
                    // itself rearranges (X,Y,Z) into (X,Z,Y) when writing
                    // XZPlaneBitData. Offset components have to follow that
                    // same rearrangement, not a straight X/Y/Z match.
                    //
                    // Every term cast to float BEFORE any addition - same
                    // signed/unsigned mixing bug as CreateMeshPlaneGreedy (z
                    // is uint32, Offset.Y/Z are signed int32; adding them
                    // directly silently converts a negative offset to a huge
                    // uint32 before the addition happens). Not currently
                    // active (MESHING_METHOD == 2), but identical fix applied
                    // so switching back to this method doesn't reintroduce it.
                    FVector VoxelPos;
                    if (UsingXZPlane)
                        VoxelPos = FVector(((float)x + (float)GridInstance.Offset.X) * (float)VoxelSize,
                                           ((float)z + (float)GridInstance.Offset.Y) * (float)VoxelSize,
                                           ((float)y + (float)GridInstance.Offset.Z) * (float)VoxelSize);
                    else
                        VoxelPos = FVector(((float)x + (float)GridInstance.Offset.X) * (float)VoxelSize,
                                           ((float)y + (float)GridInstance.Offset.Y) * (float)VoxelSize,
                                           ((float)z + (float)GridInstance.Offset.Z) * (float)VoxelSize);

                    FVector2D FaceScale = FVector2D(VoxelSize, Leading1s * VoxelSize);
                    AddFace(VoxelPos, FaceDirection, FaceScale);

                    z += Leading1s;
                }
            }
        }
    }
}

void AVoxelBase::CreateMeshPlaneGreedy(const FVoxelGridInstance& GridInstance, EFaceDirection FaceDirection)
{
    const FVoxelGrid& Grid = *GridInstance.Grid;

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

    // === DEBUG: Data-vs-bitplane integrity check =============================
    // Runs once per grid instance (gated on POSITIVE_X, always the first of the
    // six calls per instance) rather than once per direction, to avoid 6x
    // duplicate output. Every solid voxel should contribute exactly one set
    // bit to XYPlaneBitData (one bit per Z position per (x,y) column) - if
    // these two counts don't match EXACTLY, Data and the bit-planes have gone
    // out of sync for this specific grid instance. This is the direct test for
    // "collision right, mesh wrong": collision reads Data, the mesher reads
    // only the bit-planes.
    if (FaceDirection == EFaceDirection::POSITIVE_X)
    {
        int32 DataNonZeroCount = 0;
        for (uint8 Byte : Grid.Data)
        {
            if (Byte != 0) ++DataNonZeroCount;
        }

        int32 XYBitSetCount = 0;
        for (uint64 Word : Grid.XYPlaneBitData)
        {
            XYBitSetCount += FMath::CountBits(Word);
        }

        int32 XZBitSetCount = 0;
        for (uint64 Word : Grid.XZPlaneBitData)
        {
            XZBitSetCount += FMath::CountBits(Word);
        }

    }
    // === END DEBUG =============================================================

    TempPlaneBitData.Init(0, Grid.BitGridSize * Grid.BitGridSize);
    memcpy(TempPlaneBitData.GetData(), OriginPlaneBitData->GetData(), Grid.BitGridSize * Grid.BitGridSize * sizeof(uint64));

    // === DEBUG: per-call face counter ==========================================
    int32 DebugFacesAddedThisCall = 0;
    // === END DEBUG =============================================================

    for (int y = 0; y < VOXEL_CHUNK_SIZE; ++y)
    {
        for (int x = 0; x < VOXEL_CHUNK_SIZE; ++x)
        {
            int CurIndex = Grid.GetBitPlaneIndex(x, y);
            int FrontIndex = Grid.GetBitPlaneIndex(x + Normal.X, y + Normal.Y);

            uint64 CurColumn = TempPlaneBitData[CurIndex];
            uint64 FrontColumn = (*OriginPlaneBitData)[FrontIndex];

            uint64 CanHaveFaceColumn = CurColumn & ~FrontColumn;

            // === DEBUG: log any nonzero column before the early-out =============
            // Only logs when CurColumn != 0, to avoid spamming for the (many)
            // genuinely-empty columns. OriginalCurColumn lets you tell apart
            // "this is the column's real, untouched data" from "this was
            // already partially consumed by an earlier width-expansion pass
            // in this same call" - TempPlaneBitData gets bits cleared as faces
            // get consumed, so CurColumn alone can't tell you which case you're
            // looking at.

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
                        // Same XZ-plane rearrangement as CreateMeshPlaneGreedy1D -
                        // see the comment there for why the offset components
                        // don't line up straight with x/y/z here.
                        //
                        // Every term cast to float BEFORE any addition - z is
                        // uint32 (needed for the bit-scan math above), while
                        // GridInstance.Offset.Y/Z are signed int32. Writing
                        // "z + GridInstance.Offset.Z" directly lets C++'s usual
                        // arithmetic conversions silently convert the signed
                        // offset to unsigned first (same bit-width, signed
                        // loses) - a negative offset becomes a huge uint32
                        // (e.g. -32 becomes 4294967264) before the addition
                        // ever happens, and multiplying that by VoxelSize
                        // overflows uint32 again, producing an essentially
                        // arbitrary position. Casting to float first means the
                        // addition happens in floating-point space from the
                        // start, where negative values behave normally.
                        FVector VoxelPos;
                        if (UsingXZPlane)
                            VoxelPos = FVector(((float)x + (float)GridInstance.Offset.X) * (float)VoxelSize,
                                               ((float)z + (float)GridInstance.Offset.Y) * (float)VoxelSize,
                                               ((float)y + (float)GridInstance.Offset.Z) * (float)VoxelSize);
                        else
                            VoxelPos = FVector(((float)x + (float)GridInstance.Offset.X) * (float)VoxelSize,
                                               ((float)y + (float)GridInstance.Offset.Y) * (float)VoxelSize,
                                               ((float)z + (float)GridInstance.Offset.Z) * (float)VoxelSize);

                        FVector2D FaceScale = FVector2D(XScale * VoxelSize, Leading1s * VoxelSize);
                        AddFace(VoxelPos, FaceDirection, FaceScale);

                        // === DEBUG ===
                        ++DebugFacesAddedThisCall;
                        // === END DEBUG ===
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

void AVoxelBase::CreateCollision(const TArray<FVoxelGridInstance>& Grids)
{
    if (!ProceduralMesh || Grids.IsEmpty())
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
    for (const FVoxelGridInstance& GridInstance : Grids)
    {
        if (!GridInstance.Grid) continue;
        const FVoxelGrid& Grid = *GridInstance.Grid;
        const FVector WorldOffset = FVector(GridInstance.Offset) * (float)VoxelSize;

        if (CollisionType == EVoxelCollisionType::MaxIS || CollisionType == EVoxelCollisionType::IS1)
        {
            const float Density = DecompVoxel::EstimateConcaveEdgeDensity(Grid);

        }

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