// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelChunk.h"
#include "Engine/Texture2DArray.h"

// Sets default values
AVoxelChunk::AVoxelChunk()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = false;

    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
    RootComponent = ProceduralMesh;
}

void AVoxelChunk::InitializeChunk(uint32 voxelSize, UMaterial* baseMaterial, UTexture2DArray* voxelTextures)
{
    GenerateVoxelData(voxelSize);
    if (!IsChunkUniform())
    {
        GenerateVoxelMesh(voxelSize);
        CreateMaterialInstance(voxelSize, baseMaterial, voxelTextures);
    }
}

uint8 AVoxelChunk::GetVoxel(const FUintVector3& position) const
{
    // Should only run during editor, but im unsure
#if UE_EDITOR
    if (position.X >= VOXEL_DATA_SIZE || position.Y >= VOXEL_DATA_SIZE || position.Z >= VOXEL_DATA_SIZE)
    {
        UE_LOG(LogTemp, Error, TEXT("Attempting to get invalid voxel position!"));
        return 0;
    }
#endif

    return m_voxelData[position.Z * VOXEL_DATA_SIZE * VOXEL_DATA_SIZE + position.Y * VOXEL_DATA_SIZE + position.X];
}

void AVoxelChunk::SetVoxel(const FUintVector3& position, uint8 value)
{
    // Should only run during editor, but im unsure
#if UE_EDITOR
    if (position.X >= VOXEL_DATA_SIZE || position.Y >= VOXEL_DATA_SIZE || position.Z >= VOXEL_DATA_SIZE)
    {
        UE_LOG(LogTemp, Error, TEXT("Attempting to set invalid voxel position!"));
        return;
    }
#endif

    m_voxelData[position.Z * VOXEL_DATA_SIZE * VOXEL_DATA_SIZE + position.Y * VOXEL_DATA_SIZE + position.X] = value;
}

// Called when the game starts or when spawned
void AVoxelChunk::BeginPlay() { Super::BeginPlay(); }

void AVoxelChunk::GenerateVoxelData(uint32 voxelSize)
{
    FVector actorPosition = GetActorLocation();
    FIntVector3 relativePosition = FIntVector3(static_cast<int32>(actorPosition.X),
                                               static_cast<int32>(actorPosition.Y),
                                               static_cast<int32>(actorPosition.Z)) /
                                   voxelSize;

    m_chunkPosition = relativePosition / VOXEL_CHUNK_SIZE;

    // Initialize voxel data to 0
    m_voxelData.Init(0, VOXEL_DATA_SIZE * VOXEL_DATA_SIZE * VOXEL_DATA_SIZE);

    bool containsAir = false;
    bool containsSolid = false;

    // Basic terrain generation, will be optimized and improved in the future
    for (uint32 z = 0; z < VOXEL_DATA_SIZE; z++)
    {
        for (uint32 y = 0; y < VOXEL_DATA_SIZE; y++)
        {
            for (uint32 x = 0; x < VOXEL_DATA_SIZE; x++)
            {
                FIntVector3 worldPosition = relativePosition + FIntVector3(x, y, z);
                float perlinValue = FMath::PerlinNoise3D(FVector(worldPosition) / 32.0);

                float falloff = (128.0f - static_cast<float>(worldPosition.Z)) / 64.0f - 1.0f;
                perlinValue += falloff;

                if (perlinValue > 0.0f)
                {
                    containsSolid = true;

                    SetVoxel(FUintVector3(x, y, z), 1);

                    FIntVector3 upPosition = worldPosition + FIntVector3(0, 0, 1);
                    float perlinValueUp = FMath::PerlinNoise3D(FVector(upPosition) / 32.0);

                    falloff = (128.0f - static_cast<float>(upPosition.Z)) / 64.0f - 1.0f;
                    perlinValueUp += falloff;

                    if (perlinValueUp <= 0.0f) SetVoxel(FUintVector3(x, y, z), 2);
                }
                else
                    containsAir = true;
            }
        }
    }

    if (containsAir != containsSolid) m_isChunkUniform = true;
}

void AVoxelChunk::GenerateVoxelMesh(uint32 voxelSize)
{
    // Generate faces & polygons for voxels
    for (uint32 z = 1; z <= VOXEL_CHUNK_SIZE; z++)
    {
        for (uint32 y = 1; y <= VOXEL_CHUNK_SIZE; y++)
        {
            for (uint32 x = 1; x <= VOXEL_CHUNK_SIZE; x++)
            {
                if (GetVoxel(FUintVector3(x, y, z)) > 0)
                {
                    FVector voxelPos = FVector(x * voxelSize, y * voxelSize, z * voxelSize);

                    if (GetVoxel(FUintVector3(x + 1, y, z)) == 0)
                        AddPlane(voxelPos,
                                 FVector(voxelSize, 0.0, 0.0),
                                 FVector(voxelSize, voxelSize, 0.0),
                                 FVector(voxelSize, 0.0, voxelSize),
                                 FVector(voxelSize, voxelSize, voxelSize),
                                 FVector(1.0, 0.0, 0.0));
                    if (GetVoxel(FUintVector3(x - 1, y, z)) == 0)
                        AddPlane(voxelPos,
                                 FVector(0.0, voxelSize, 0.0),
                                 FVector(0.0, 0.0, 0.0),
                                 FVector(0.0, voxelSize, voxelSize),
                                 FVector(0.0, 0.0, voxelSize),
                                 FVector(-1.0, 0.0, 0.0));
                    if (GetVoxel(FUintVector3(x, y + 1, z)) == 0)
                        AddPlane(voxelPos,
                                 FVector(voxelSize, voxelSize, 0.0),
                                 FVector(0.0, voxelSize, 0.0),
                                 FVector(voxelSize, voxelSize, voxelSize),
                                 FVector(0.0, voxelSize, voxelSize),
                                 FVector(0.0, 1.0, 0.0));
                    if (GetVoxel(FUintVector3(x, y - 1, z)) == 0)
                        AddPlane(voxelPos,
                                 FVector(0.0, 0.0, 0.0),
                                 FVector(voxelSize, 0.0, 0.0),
                                 FVector(0.0, 0.0, voxelSize),
                                 FVector(voxelSize, 0.0, voxelSize),
                                 FVector(0.0, -1.0, 0.0));
                    if (GetVoxel(FUintVector3(x, y, z + 1)) == 0)
                        AddPlane(voxelPos,
                                 FVector(0.0, 0.0, voxelSize),
                                 FVector(voxelSize, 0.0, voxelSize),
                                 FVector(0.0, voxelSize, voxelSize),
                                 FVector(voxelSize, voxelSize, voxelSize),
                                 FVector(0.0, 0.0, 1.0));
                    if (GetVoxel(FUintVector3(x, y, z - 1)) == 0)
                        AddPlane(voxelPos,
                                 FVector(voxelSize, 0.0, 0.0),
                                 FVector(0.0, 0.0, 0.0),
                                 FVector(voxelSize, voxelSize, 0.0),
                                 FVector(0.0, voxelSize, 0.0),
                                 FVector(0.0, 0.0, -1.0));
                }
            }
        }
    }

    if (ProceduralMesh)
    {
        ProceduralMesh->ClearAllMeshSections();
        ProceduralMesh->CreateMeshSection_LinearColor(
            0,
            m_vertices,
            m_indices,
            m_normals,
            m_uvs,
            TArray<FLinearColor>(),
            TArray<FProcMeshTangent>(),
            true);  // TODO: Disable collision once more efficient collision algorithm is in place!
    }
    else
        UE_LOG(LogTemp, Error, TEXT("ProceduralMesh component missing!"));

    m_vertices.Empty();
    m_normals.Empty();
    m_uvs.Empty();
    m_indices.Empty();
    m_indexCount = 0;
}

void AVoxelChunk::CreateMaterialInstance(uint32 voxelSize, UMaterial* baseMaterial, UTexture2DArray* voxelTextures)
{
    // Create material and texture if they don't exist yet
    if (baseMaterial && voxelTextures && m_materialDataCreated == false)
    {
        // Create texture to store voxel data
        uint32 textureHeight = m_voxelData.Num() / VOXEL_DATA_SIZE;

        VoxelDataTexture = UTexture2D::CreateTransient(VOXEL_DATA_SIZE, textureHeight, PF_R8);
        VoxelDataTexture->CompressionSettings = TextureCompressionSettings::TC_Grayscale;
        VoxelDataTexture->SRGB = 0;
        VoxelDataTexture->Filter = TextureFilter::TF_Nearest;
        VoxelDataTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
        VoxelDataTexture->CompressionNone = 1;

        // Upload voxel data to texture
        void* TargetTextureData = VoxelDataTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
        FMemory::Memcpy(TargetTextureData, m_voxelData.GetData(), m_voxelData.Num());

        VoxelDataTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
        VoxelDataTexture->UpdateResource();

        // Create material instance for chunk mesh
        MaterialInstance = UMaterialInstanceDynamic::Create(baseMaterial, this);
        MaterialInstance->SetTextureParameterValue(TEXT("VoxelTextures"), voxelTextures);
        MaterialInstance->SetTextureParameterValue(TEXT("VoxelDataTexture"), VoxelDataTexture);

        MaterialInstance->SetScalarParameterValue(TEXT("VoxelSize"), static_cast<float>(voxelSize));
        MaterialInstance->SetScalarParameterValue(TEXT("VoxelDataSize"), static_cast<float>(VOXEL_DATA_SIZE));

        ProceduralMesh->SetMaterial(0, MaterialInstance);

        m_materialDataCreated = true;
    }
    else UE_LOG(LogTemp, Warning, TEXT("Missing material data for chunk, cannot assign material!"));
}

inline void AVoxelChunk::AddPlane(const FVector& position,
                                  const FVector& v0,
                                  const FVector& v1,
                                  const FVector& v2,
                                  const FVector& v3,
                                  const FVector& normal)
{
    m_vertices.Add(v0 + position);
    m_vertices.Add(v1 + position);
    m_vertices.Add(v2 + position);
    m_vertices.Add(v3 + position);

    m_normals.Add(normal);
    m_normals.Add(normal);
    m_normals.Add(normal);
    m_normals.Add(normal);

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