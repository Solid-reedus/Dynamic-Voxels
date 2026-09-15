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

    static float chunkWorldSize = static_cast<float>(AVoxelChunk::VOXEL_CHUNK_SIZE * AVoxelChunk::VOXEL_SIZE);

    // For now just spawn a 5x5x5 area of chunks
    for (int x = 0; x < 5; x++)
    {
        for (int y = 0; y < 5; y++)
        {
            for (int z = 0; z < 5; z++)
            {
                FVector location(chunkWorldSize * x, chunkWorldSize * y, chunkWorldSize * z);
                FRotator rotation(0.0f, 0.0f, 0.0f);
                FActorSpawnParameters spawnInfo = {};
                GetWorld()->SpawnActor<AVoxelChunk>(location, rotation, spawnInfo);
            }
        }
    }
}

// Called every frame
void AVoxelWorld::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

