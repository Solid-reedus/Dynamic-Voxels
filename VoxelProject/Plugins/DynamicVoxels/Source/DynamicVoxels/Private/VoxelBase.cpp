
#include "VoxelBase.h"
#include "VoxelCollisionDecomposition.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/ConvexElem.h"
#include "Engine/Texture2DArray.h"

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
        Grid.SetVoxel(Position, Value);
    }

    RegenerateMesh();
    CreateCollision();
    OnVoxelsChanged();
}

void AVoxelBase::BeginPlay() { Super::BeginPlay(); }

void AVoxelBase::RegenerateMesh()
{
    if (Grid.Data.IsEmpty()) return;

    // Wether to use old or new meshing approach
#if 0
    for (uint32 z = 1; z <= VOXEL_CHUNK_SIZE; ++z)
    {
        for (uint32 y = 1; y <= VOXEL_CHUNK_SIZE; ++y)
        {
            for (uint32 x = 1; x <= VOXEL_CHUNK_SIZE; ++x)
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
#else
    CreateMeshPlanesGreedy(EFaceDirection::POSITIVE_X);
    CreateMeshPlanesGreedy(EFaceDirection::NEGATIVE_X);
    CreateMeshPlanesGreedy(EFaceDirection::POSITIVE_Y);
    CreateMeshPlanesGreedy(EFaceDirection::NEGATIVE_Y);
    CreateMeshPlanesGreedy(EFaceDirection::POSITIVE_Z);
    CreateMeshPlanesGreedy(EFaceDirection::NEGATIVE_Z);
#endif


    if (ProceduralMesh)
    {
        ProceduralMesh->ClearAllMeshSections();
        ProceduralMesh->CreateMeshSection_LinearColor(0,
                                                      m_vertices,
                                                      m_indices,
                                                      m_normals,
                                                      m_uvs,
                                                      TArray<FLinearColor>(),
                                                      TArray<FProcMeshTangent>(),
                                                      false);  
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("%s: ProceduralMesh component missing!"), *GetName());
    }

    m_vertices.Empty();
    m_normals.Empty();
    m_uvs.Empty();
    m_indices.Empty();
    m_indexCount = 0;
}

void AVoxelBase::CreateMaterialInstance(UMaterial* baseMaterial, UTexture2DArray* voxelTextures)
{
    // Create material and texture if they don't exist yet
    if (baseMaterial && voxelTextures && m_materialDataCreated == false)
    {
        // Create texture to store voxel data
        uint32 textureHeight = Grid.Data.Num() / VOXEL_GRID_SIZE;

        VoxelDataTexture = UTexture2D::CreateTransient(VOXEL_GRID_SIZE, textureHeight, PF_R8);
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
        MaterialInstance->SetScalarParameterValue(TEXT("VoxelDataSize"), static_cast<float>(VOXEL_GRID_SIZE));

        ProceduralMesh->SetMaterial(0, MaterialInstance);

        m_materialDataCreated = true;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Missing material data for chunk, cannot assign material!"));
    }
}

void AVoxelBase::CreateMeshPlanesGreedy(EFaceDirection FaceDirection)
{
    // Copy data into temp data
    TempGridData.Init(0, Grid.GridSize * Grid.GridSize * Grid.GridSize);
    memcpy(TempGridData.GetData(), Grid.Data.GetData(), Grid.GridSize * Grid.GridSize * Grid.GridSize);

    FIntVector3 Normal;
    switch (FaceDirection)
    {
        case EFaceDirection::POSITIVE_X:
            Normal = FIntVector3(1, 0, 0);
            break;
        case EFaceDirection::NEGATIVE_X:
            Normal = FIntVector3(-1, 0, 0);
            break;
        case EFaceDirection::POSITIVE_Y:
            Normal = FIntVector3(0, 1, 0);
            break;
        case EFaceDirection::NEGATIVE_Y:
            Normal = FIntVector3(0, -1, 0);
            break;
        case EFaceDirection::POSITIVE_Z:
            Normal = FIntVector3(0, 0, 1);
            break;
        case EFaceDirection::NEGATIVE_Z:
            Normal = FIntVector3(0, 0, -1);
            break;
        default:
            break;
    }

    for (uint32 z = 1; z <= VOXEL_CHUNK_SIZE; ++z)
    {
        for (uint32 y = 1; y <= VOXEL_CHUNK_SIZE; ++y)
        {
            for (uint32 x = 1; x <= VOXEL_CHUNK_SIZE; ++x)
            {
                if (Grid.GetVoxelFromData(TempGridData, Grid.GridSize, FUintVector3(x, y, z)) == 0) continue;
                if (Grid.GetVoxel(FUintVector3(FIntVector3(x, y, z) + Normal)) != 0) continue;

                uint32 XScale = VOXEL_CHUNK_SIZE;
                uint32 YScale = VOXEL_CHUNK_SIZE;
                bool Finished = false;

                for (uint32 i = 0; i < VOXEL_CHUNK_SIZE; ++i)
                {
                    for (uint32 j = 0; j < VOXEL_CHUNK_SIZE; ++j)
                    {
                        if (j >= YScale) break;

                        FIntVector3 LoopPos;
                        switch (FaceDirection)
                        {
                            case EFaceDirection::POSITIVE_X:
                                LoopPos = FIntVector3(0, i, j);
                                break;
                            case EFaceDirection::NEGATIVE_X:
                                LoopPos = FIntVector3(0, i, j);
                                break;
                            case EFaceDirection::POSITIVE_Y:
                                LoopPos = FIntVector3(i, 0, j);
                                break;
                            case EFaceDirection::NEGATIVE_Y:
                                LoopPos = FIntVector3(i, 0, j);
                                break;
                            case EFaceDirection::POSITIVE_Z:
                                LoopPos = FIntVector3(i, j, 0);
                                break;
                            case EFaceDirection::NEGATIVE_Z:
                                LoopPos = FIntVector3(i, j, 0);
                                break;
                            default:
                                break;
                        }
                        FUintVector3 CheckPosInfront = FUintVector3(FIntVector3(x, y, z) + LoopPos + Normal);
                        FUintVector3 CheckPos = FUintVector3(FIntVector3(x, y, z) + LoopPos);
                        if (CheckPos.X > VOXEL_CHUNK_SIZE || CheckPos.Y > VOXEL_CHUNK_SIZE ||
                            CheckPos.Z > VOXEL_CHUNK_SIZE ||
                            Grid.GetVoxel(CheckPosInfront) != 0 ||
                            Grid.GetVoxelFromData(TempGridData, Grid.GridSize, CheckPos) == 0)
                        {
                            if (i != 0)
                            {
                                Finished = true;
                                break;
                            }
                            YScale = j;
                            break;
                        }
                    }
                    if (Finished)
                    {
                        XScale = i;
                        break;
                    }
                }

                for (uint32 i = 0; i < XScale; ++i)
                {
                    for (uint32 j = 0; j < YScale; ++j)
                    {
                        FIntVector3 LoopPos;
                        switch (FaceDirection)
                        {
                            case EFaceDirection::POSITIVE_X:
                                LoopPos = FIntVector3(0, i, j);
                                break;
                            case EFaceDirection::NEGATIVE_X:
                                LoopPos = FIntVector3(0, i, j);
                                break;
                            case EFaceDirection::POSITIVE_Y:
                                LoopPos = FIntVector3(i, 0, j);
                                break;
                            case EFaceDirection::NEGATIVE_Y:
                                LoopPos = FIntVector3(i, 0, j);
                                break;
                            case EFaceDirection::POSITIVE_Z:
                                LoopPos = FIntVector3(i, j, 0);
                                break;
                            case EFaceDirection::NEGATIVE_Z:
                                LoopPos = FIntVector3(i, j, 0);
                                break;
                            default:
                                break;
                        }
                        FUintVector3 CheckPos = FUintVector3(FIntVector3(x, y, z) + LoopPos);
                        // Set this voxel temporarily to air, so that we dont try and create a face here again.
                        Grid.SetVoxelFromData(TempGridData, Grid.GridSize, CheckPos, 0);
                    }
                }

                if (XScale > 0 && YScale > 0)
                {
                    FVector VoxelPos = FVector(x * VoxelSize, y * VoxelSize, z * VoxelSize);
                    FVector2D FaceScale = FVector2D(XScale * VoxelSize, YScale * VoxelSize);
                    AddFace(VoxelPos, FaceDirection, FaceScale);
                }
            }
        }
    }

    TempGridData.Empty();
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

    m_vertices.Add(V0 + Position);
    m_vertices.Add(V1 + Position);
    m_vertices.Add(V2 + Position);
    m_vertices.Add(V3 + Position);

    m_normals.Add(Normal);
    m_normals.Add(Normal);
    m_normals.Add(Normal);
    m_normals.Add(Normal);

    m_uvs.Add(FVector2D(0.0, 0.0));
    m_uvs.Add(FVector2D(1.0, 0.0));
    m_uvs.Add(FVector2D(0.0, 1.0));
    m_uvs.Add(FVector2D(1.0, 1.0));

    m_indices.Add(2 + m_indexCount);
    m_indices.Add(1 + m_indexCount);
    m_indices.Add(0 + m_indexCount);
    m_indices.Add(2 + m_indexCount);
    m_indices.Add(3 + m_indexCount);
    m_indices.Add(1 + m_indexCount);

    m_indexCount += 4;
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
        for (uint32 z = 1; z <= VOXEL_CHUNK_SIZE; ++z)
        {
            for (uint32 y = 1; y <= VOXEL_CHUNK_SIZE; ++y)
            {
                for (uint32 x = 1; x <= VOXEL_CHUNK_SIZE; ++x)
                {
                    if (Grid.GetVoxel(FUintVector3(x, y, z)) == 0) continue;
                    const bool bSurface =
                        Grid.GetVoxel(FUintVector3(x - 1, y, z)) == 0 || Grid.GetVoxel(FUintVector3(x + 1, y, z)) == 0 ||
                        Grid.GetVoxel(FUintVector3(x, y - 1, z)) == 0 || Grid.GetVoxel(FUintVector3(x, y + 1, z)) == 0 ||
                        Grid.GetVoxel(FUintVector3(x, y, z - 1)) == 0 || Grid.GetVoxel(FUintVector3(x, y, z + 1)) == 0;
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