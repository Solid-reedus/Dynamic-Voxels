// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelChunk.h"

// Sets default values
AVoxelChunk::AVoxelChunk() { PrimaryActorTick.bCanEverTick = false; }

void AVoxelChunk::InitializeChunk(uint32 voxelSize, UMaterial* baseMaterial, UTexture2DArray* voxelTextures)
{
    VoxelSize = voxelSize;

    GenerateVoxelData(voxelSize);
    if (!IsChunkUniform())
    {
        RegenerateMesh();
        CreateCollision();
        CreateMaterialInstance(baseMaterial, voxelTextures);
    }
}

// Called when the game starts or when spawned
void AVoxelChunk::BeginPlay() { Super::BeginPlay(); }

void AVoxelChunk::GenerateVoxelData(uint32 voxelSize)
{
    FVector actorPosition = GetActorLocation();
    FIntVector3 relativePosition = FIntVector3(static_cast<int32>(actorPosition.X),
                                               static_cast<int32>(actorPosition.Y),
                                               static_cast<int32>(actorPosition.Z)) / voxelSize;

    m_chunkPosition = relativePosition / VOXEL_CHUNK_SIZE;

    Grid.Initialize(VOXEL_GRID_SIZE);

    bool containsSolid = false;

    // Basic terrain generation, will be optimized and improved in the future
    for (uint32 y = 0; y < VOXEL_GRID_SIZE; y++)
    {
        for (uint32 x = 0; x < VOXEL_GRID_SIZE; x++)
        {
            FIntVector2 topDownPosition = FIntVector2(x + relativePosition.X, y + relativePosition.Y);

            double perlin2DVal = FMath::PerlinNoise2D(FVector2D(topDownPosition) / 160.0) * 0.5 + 0.5;

            int terrainValue = static_cast<int>(perlin2DVal * perlin2DVal * 80.0 + 48.0);
            terrainValue -= relativePosition.Z;

            for (int z = 0; z < terrainValue; z++)
            {
                if (z >= static_cast<int>(VOXEL_GRID_SIZE)) break;

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