// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelWorld.h"
#include "VoxelChunk.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
AVoxelWorld::AVoxelWorld()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    // Currently set to false, but will be needed later
    PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AVoxelWorld::BeginPlay()
{
    Super::BeginPlay();

    LoadBaseMaterial();

    // Create the initial terrain
    APawn* playerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    FIntVector3 curPawnChunkPosition = WorldToChunkPosition(playerPawn->GetActorLocation());
    m_prevPawnChunkPosition = curPawnChunkPosition;

    for (int z = -VerticalLoadDistance; z <= VerticalLoadDistance; z++)
    {
        for (int y = -HorizontalLoadDistance; y <= HorizontalLoadDistance; y++)
        {
            for (int x = -HorizontalLoadDistance; x <= HorizontalLoadDistance; x++)
            {
                FIntVector3 addPosition = FIntVector3(x, y, z) + curPawnChunkPosition;

                if (FMath::Abs(x) < 2 && FMath::Abs(y) < 2)
                    AddVoxelChunkInstance(addPosition);
                else
                    m_chunkAddQueue.Enqueue(addPosition);
            }
        }
    }
}

// Called every frame
void AVoxelWorld::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    APawn* playerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    FIntVector3 curPawnChunkPosition = WorldToChunkPosition(playerPawn->GetActorLocation());

    int chunksSpawned = 0;
    while (chunksSpawned < ChunkUpdateRate)
    {
        FIntVector3 chunkPosition;
        if (m_chunkAddQueue.Dequeue(chunkPosition))
        {
            bool dontSpawn = false;

            for (AVoxelChunk* voxelChunk : m_voxelChunkInstances)
            {
                if (voxelChunk)
                {
                    if (voxelChunk->GetChunkPosition() == chunkPosition)
                    {
                        dontSpawn = true;
                        break;
                    }
                }
            }

            int d1 = FMath::Abs(chunkPosition.X - curPawnChunkPosition.X);
            int d2 = FMath::Abs(chunkPosition.Y - curPawnChunkPosition.Y);
            int vert = FMath::Abs(chunkPosition.Z - curPawnChunkPosition.Z);

            int maxHori = FMath::Max(d1, d2);

            if (maxHori > HorizontalLoadDistance || vert > VerticalLoadDistance) dontSpawn = true;

            if (!dontSpawn)
            {
                chunksSpawned++;
                AddVoxelChunkInstance(chunkPosition);
            }
        }
        else
            break;
    }

    if (curPawnChunkPosition != m_prevPawnChunkPosition)
    {
        for (int i = 0; i < m_voxelChunkInstances.Num(); i++)
        {
            if (m_voxelChunkInstances[i])
            {
                FIntVector3 chunkPos = m_voxelChunkInstances[i]->GetChunkPosition();

                int d1 = FMath::Abs(chunkPos.X - curPawnChunkPosition.X);
                int d2 = FMath::Abs(chunkPos.Y - curPawnChunkPosition.Y);
                int vert = FMath::Abs(chunkPos.Z - curPawnChunkPosition.Z);

                int maxHori = FMath::Max(d1, d2);

                if (maxHori > HorizontalLoadDistance || vert > VerticalLoadDistance)
                {
                    m_voxelChunkInstances[i]->Destroy();
                    m_voxelChunkInstances[i] = nullptr;
                }
            }
        }
        for (int z = -VerticalLoadDistance; z <= VerticalLoadDistance; z++)
        {
            for (int y = -HorizontalLoadDistance; y <= HorizontalLoadDistance; y++)
            {
                for (int x = -HorizontalLoadDistance; x <= HorizontalLoadDistance; x++)
                {
                    FIntVector3 addPosition = FIntVector3(x, y, z) + curPawnChunkPosition;

                    int d1 = FMath::Abs(addPosition.X - m_prevPawnChunkPosition.X);
                    int d2 = FMath::Abs(addPosition.Y - m_prevPawnChunkPosition.Y);
                    int vert = FMath::Abs(addPosition.Z - m_prevPawnChunkPosition.Z);

                    int maxHori = FMath::Max(d1, d2);

                    if (maxHori > HorizontalLoadDistance || vert > VerticalLoadDistance) m_chunkAddQueue.Enqueue(addPosition);
                }
            }
        }
    }
    m_prevPawnChunkPosition = curPawnChunkPosition;
}

void AVoxelWorld::AddVoxelChunkInstance(const FIntVector3& chunkPosition)
{
    float chunkWorldSize = static_cast<float>(AVoxelChunk::VOXEL_CHUNK_SIZE * VoxelSize);

    FVector location(chunkWorldSize * chunkPosition.X, chunkWorldSize * chunkPosition.Y, chunkWorldSize * chunkPosition.Z);
    FRotator rotation(0.0f, 0.0f, 0.0f);
    FActorSpawnParameters spawnInfo = {};

    AVoxelChunk* voxelChunk = GetWorld()->SpawnActor<AVoxelChunk>(location, rotation, spawnInfo);
    voxelChunk->GenerateVoxelData(VoxelSize);
    voxelChunk->GenerateVoxelMesh(VoxelSize, BaseMaterial, VoxelTextures);

    int32 nullptrIndex = m_voxelChunkInstances.Find(nullptr);
    if (nullptrIndex != INDEX_NONE)
        m_voxelChunkInstances[nullptrIndex] = voxelChunk;
    else
        m_voxelChunkInstances.Add(voxelChunk);
}

inline FIntVector3 AVoxelWorld::WorldToChunkPosition(const FVector& position) const
{
    FVector relativePosition = position / FVector(VoxelSize);
    relativePosition /= FVector(AVoxelChunk::VOXEL_CHUNK_SIZE);
    FIntVector3 final = FIntVector3(static_cast<int>(FMath::Floor(relativePosition.X)),
                                    static_cast<int>(FMath::Floor(relativePosition.Y)),
                                    static_cast<int>(FMath::Floor(relativePosition.Z)));

    if (FixedLoadHeight)
        final.Z = VerticalLoadDistance;

    return final;
}

void AVoxelWorld::LoadBaseMaterial()
{
    FString materialPath = TEXT("/DynamicVoxels/VoxelMaterial.VoxelMaterial");
    BaseMaterial = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), nullptr, *materialPath));

    if (!BaseMaterial) UE_LOG(LogTemp, Error, TEXT("Failed to load base material for voxel terrain!"));
}