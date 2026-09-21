// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelChunk.h"
#include "Engine/Texture2DArray.h"

// Sets default values
AVoxelChunk::AVoxelChunk()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = false;

    // ProceduralMesh is already created and set as RootComponent by
    // AVoxelBase's constructor, which always runs before this one - nothing
    // to do here anymore.
}

void AVoxelChunk::InitializeChunk(uint32 voxelSize, UMaterial* baseMaterial, UTexture2DArray* voxelTextures)
{
    // RegenerateMesh()/CreateCollision() read VoxelSize from the inherited
    // property rather than taking it as a parameter, so this has to be set
    // before either of them runs.
    VoxelSize = voxelSize;

    GenerateVoxelData(voxelSize);
    if (!IsChunkUniform())
    {
        // Border = 1: the outermost voxel shell is a copy of the ADJACENT
        // chunk's edge data, kept only so face-culling reads correctly at
        // the seam - it's not this chunk's own geometry and shouldn't be
        // rendered (see the comment on RegenerateMesh in VoxelBase.h).
        
        //RegenerateMesh(1);
        RegenerateMesh();
        
        CreateCollision();
        CreateMaterialInstance(voxelSize, baseMaterial, voxelTextures);
    }
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

    InitializeGrid(VOXEL_DATA_SIZE);

    bool containsSolid = false;

    // Basic terrain generation, will be optimized and improved in the future
    for (uint32 y = 0; y < VOXEL_DATA_SIZE; y++)
    {
        for (uint32 x = 0; x < VOXEL_DATA_SIZE; x++)
        {
            FIntVector2 topDownPosition = FIntVector2(x + relativePosition.X, y + relativePosition.Y);

            double perlin2DVal = FMath::PerlinNoise2D(FVector2D(topDownPosition) / 160.0) * 0.5 + 0.5;

            int terrainValue = static_cast<int>(perlin2DVal * perlin2DVal * 80.0 + 48.0);
            terrainValue -= relativePosition.Z;

            for (int z = 0; z < terrainValue; z++)
            {
                if (z >= (int)VOXEL_DATA_SIZE) break;

                uint8 blockType = 1;  // Stone
                if (terrainValue - z < 3)
                    blockType = 2;  // Grass
                else if (terrainValue - z < 6)
                    blockType = 3;  // Dirt

                FIntVector3 worldPosition = FIntVector3(x, y, z) + relativePosition;
                float perlinValue = FMath::PerlinNoise3D(FVector(worldPosition) / 16.0);

                float falloff = (128.0f - static_cast<float>(worldPosition.Z)) / 64.0f - 1.0f;
                perlinValue += falloff;

                if (perlinValue > 0.0f)
                {
                    SetVoxel(FUintVector3(x, y, z), blockType);
                    containsSolid = true;
                }
            }
        }
    }

    if (!containsSolid) m_isChunkUniform = true;
}

void AVoxelChunk::CreateMaterialInstance(uint32 voxelSize, UMaterial* baseMaterial, UTexture2DArray* voxelTextures)
{
    // Create material and texture if they don't exist yet
    if (baseMaterial && voxelTextures && m_materialDataCreated == false)
    {
        // Create texture to store voxel data
        uint32 textureHeight = Grid.Data.Num() / VOXEL_DATA_SIZE;

        VoxelDataTexture = UTexture2D::CreateTransient(VOXEL_DATA_SIZE, textureHeight, PF_R8);
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

        MaterialInstance->SetScalarParameterValue(TEXT("VoxelSize"), static_cast<float>(voxelSize));
        MaterialInstance->SetScalarParameterValue(TEXT("VoxelDataSize"), static_cast<float>(VOXEL_DATA_SIZE));

        ProceduralMesh->SetMaterial(0, MaterialInstance);

        m_materialDataCreated = true;
    }
    else
        UE_LOG(LogTemp, Warning, TEXT("Missing material data for chunk, cannot assign material!"));
}