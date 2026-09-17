// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelWorld.h"
#include "VoxelChunk.h"

// Sets default values
AVoxelWorld::AVoxelWorld()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    // Currently set to false, but will be needed later
    PrimaryActorTick.bCanEverTick = false;
}

// Called when the game starts or when spawned
void AVoxelWorld::BeginPlay()
{
    Super::BeginPlay();

    LoadBaseMaterial();

    float chunkWorldSize = static_cast<float>(AVoxelChunk::VOXEL_CHUNK_SIZE * VoxelSize);

    // For now just spawn a 5x5x5 area of chunks
    for (int x = -ChunkLoadDistance; x <= ChunkLoadDistance; x++)
    {
        for (int y = -ChunkLoadDistance; y <= ChunkLoadDistance; y++)
        {
            for (int z = -ChunkLoadDistance; z <= ChunkLoadDistance; z++)
            {
                FVector location(chunkWorldSize * x, chunkWorldSize * y, chunkWorldSize * z);
                FRotator rotation(0.0f, 0.0f, 0.0f);
                FActorSpawnParameters spawnInfo = {};

                AVoxelChunk* voxelChunk = GetWorld()->SpawnActor<AVoxelChunk>(location, rotation, spawnInfo);
                voxelChunk->GenerateVoxelData(VoxelSize);
                voxelChunk->GenerateVoxelMesh(VoxelSize, BaseMaterial, VoxelTextures);
            }
        }
    }
}

// Called every frame
void AVoxelWorld::Tick(float DeltaTime) { Super::Tick(DeltaTime); }

void AVoxelWorld::LoadBaseMaterial()
{
    FString materialPath = TEXT("/DynamicVoxels/VoxelMaterial.VoxelMaterial");
    BaseMaterial = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), nullptr, *materialPath));

    if (!BaseMaterial) UE_LOG(LogTemp, Error, TEXT("Failed to load base material for voxel terrain!"));
}