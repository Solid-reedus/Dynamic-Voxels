// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelWorld.h"
#include "VoxelChunk.h"
#include "Kismet/GameplayStatics.h"

#include "VoxelConstruct.h"
#include "VoxelChunk.h"


AVoxelWorld::AVoxelWorld()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
}

AVoxelChunk* AVoxelWorld::GetChunkAt(const FIntVector& ChunkPosition) const 
{
    AVoxelChunk* const* Found = m_voxelChunkInstances.Find(ChunkPosition);
    return Found ? *Found : nullptr;
}


void AVoxelWorld::ForwardVoxelEdit(const FIntVector& ChunkPosition, const TArray<FUintVector3>& LocalPositions, uint8 Value)
{
    AVoxelChunk* Chunk = GetChunkAt(ChunkPosition);
    if (Chunk)
    {
        Chunk->ApplyVoxelEdit(LocalPositions, Value);
    }
    // Not currently loaded - nothing to apply to yet.
}

void AVoxelWorld::ApplyVoxelEditAtLocation(const FVector& WorldLocation, float Radius, uint8 Value)
{
    const float RadiusInVoxels = Radius / (float)VoxelSize;
    const FVector CenterInVoxels = WorldLocation / (float)VoxelSize;

    const int32 MinX = FMath::FloorToInt(CenterInVoxels.X - RadiusInVoxels);
    const int32 MaxX = FMath::CeilToInt(CenterInVoxels.X + RadiusInVoxels);
    const int32 MinY = FMath::FloorToInt(CenterInVoxels.Y - RadiusInVoxels);
    const int32 MaxY = FMath::CeilToInt(CenterInVoxels.Y + RadiusInVoxels);
    const int32 MinZ = FMath::FloorToInt(CenterInVoxels.Z - RadiusInVoxels);
    const int32 MaxZ = FMath::CeilToInt(CenterInVoxels.Z + RadiusInVoxels);

    const float RadiusSq = RadiusInVoxels * RadiusInVoxels;
    const int32 ChunkSize = (int32)AVoxelChunk::VOXEL_CHUNK_SIZE;

    TMap<FIntVector, TArray<FUintVector3>> ChunkEdits;

    for (int32 z = MinZ; z <= MaxZ; ++z)
    {
        for (int32 y = MinY; y <= MaxY; ++y)
        {
            for (int32 x = MinX; x <= MaxX; ++x)
            {
                const FVector VoxelCenter((float)x + 0.5f, (float)y + 0.5f, (float)z + 0.5f);
                if (FVector::DistSquared(VoxelCenter, CenterInVoxels) > RadiusSq)
                {
                    continue;
                }

                const int32 ChunkX = FMath::DivideAndRoundDown(x - 1, ChunkSize);
                const int32 ChunkY = FMath::DivideAndRoundDown(y - 1, ChunkSize);
                const int32 ChunkZ = FMath::DivideAndRoundDown(z - 1, ChunkSize);
                const FIntVector ChunkPos(ChunkX, ChunkY, ChunkZ);

                const FUintVector3 LocalPos((uint32)(x - ChunkX * ChunkSize),
                                            (uint32)(y - ChunkY * ChunkSize),
                                            (uint32)(z - ChunkZ * ChunkSize));

                ChunkEdits.FindOrAdd(ChunkPos).Add(LocalPos);
            }
        }
    }

    for (const TPair<FIntVector, TArray<FUintVector3>>& Entry : ChunkEdits)
    {
        ForwardVoxelEdit(Entry.Key, Entry.Value, Value);
    }
}


// Called when the game starts or when spawned
void AVoxelWorld::BeginPlay()
{
    Super::BeginPlay();

    LoadBaseMaterial();

    APawn* playerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    FIntVector2 curPawnChunkPosition;
    if (playerPawn)
        curPawnChunkPosition = FIntVector2(WorldToChunkPosition(playerPawn->GetActorLocation()));
    else
        curPawnChunkPosition = m_prevPawnChunkPosition;

    m_prevPawnChunkPosition = curPawnChunkPosition;

    // Create the initial terrain
    for (int z = 0; z < VerticalLoadDistance; z++)
    {
        for (int y = -HorizontalLoadDistance; y <= HorizontalLoadDistance; y++)
        {
            for (int x = -HorizontalLoadDistance; x <= HorizontalLoadDistance; x++)
            {
                FIntVector3 addPosition = FIntVector3(x + curPawnChunkPosition.X, y + curPawnChunkPosition.Y, z);

                if (FMath::Abs(x) < 2 && FMath::Abs(y) < 2)
                {
                    CreateNewChunk(addPosition);
                }
                else
                {
                    m_chunkAddQueue.Enqueue(addPosition);
                }
            }
        }
    }


    // Hardcoded test spawn, remove once real placement logic exists
    const FVector TestConstructLocation(100.0f, 100.0f, 12000.0f);
    AVoxelConstruct* TestConstruct = GetWorld()->SpawnActor<AVoxelConstruct>(TestConstructLocation, FRotator::ZeroRotator);
    if (TestConstruct)
    {
        TestConstruct->InitVoxel(EVoxelCollisionType::ThreeGDM, false);
    }
    m_constructInstances.Add(TestConstruct);

}

// Called every frame
void AVoxelWorld::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    APawn* playerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    FIntVector2 curPawnChunkPosition;
    if (playerPawn)
        curPawnChunkPosition = FIntVector2(WorldToChunkPosition(playerPawn->GetActorLocation()));
    else
        curPawnChunkPosition = m_prevPawnChunkPosition;

    // Spawn new chunks that are in the chunk queue
    int chunksSpawned = 0;
    while (chunksSpawned < ChunkUpdateRate)
    {
        FIntVector3 chunkPosition;
        if (m_chunkAddQueue.Dequeue(chunkPosition))
        {
            int chunkDistance = GetGridDistance(FIntVector2(chunkPosition), curPawnChunkPosition);

            if (chunkDistance <= HorizontalLoadDistance && !m_voxelChunkInstances.Contains(chunkPosition))
            {
                CreateNewChunk(chunkPosition);
                chunksSpawned++;
            }
        }
        else
        {
            break;
        }
    }

    if (curPawnChunkPosition != m_prevPawnChunkPosition)
    {
        // Remove chunks that are too far away from the player
        for (auto it = m_voxelChunkInstances.CreateIterator(); it; ++it)
        {
            if (it.Value())
            {
                FIntVector3 chunkPos = it.Value()->GetChunkPosition();

                int chunkDistance = GetGridDistance(FIntVector2(chunkPos), FIntVector2(curPawnChunkPosition));

                if (chunkDistance > HorizontalLoadDistance)
                {
                    it.Value()->Destroy();
                    it.Value() = nullptr;
                    it.RemoveCurrent();
                }
            }
        }
        // Spawn chunks that were previously too far away from the player
        for (int z = 0; z < VerticalLoadDistance; z++)
        {
            for (int y = -HorizontalLoadDistance; y <= HorizontalLoadDistance; y++)
            {
                for (int x = -HorizontalLoadDistance; x <= HorizontalLoadDistance; x++)
                {
                    FIntVector3 addPosition = FIntVector3(x + curPawnChunkPosition.X, y + curPawnChunkPosition.Y, z);

                    // Ensure that whe are only queuing up chunks to be added that where outside of the render distance
                    int chunkDistance = GetGridDistance(FIntVector2(addPosition), FIntVector2(m_prevPawnChunkPosition));
                    if (chunkDistance > HorizontalLoadDistance) m_chunkAddQueue.Enqueue(addPosition);
                }
            }
        }
    }
    m_prevPawnChunkPosition = curPawnChunkPosition;
}

void AVoxelWorld::CreateNewChunk(const FIntVector3& chunkPosition)
{
    float chunkWorldSize = static_cast<float>(AVoxelBase::VOXEL_CHUNK_SIZE * VoxelSize);

    FVector location(chunkWorldSize * chunkPosition.X, chunkWorldSize * chunkPosition.Y, chunkWorldSize * chunkPosition.Z);
    FRotator rotation(0.0f, 0.0f, 0.0f);
    FActorSpawnParameters spawnInfo = {};

    AVoxelChunk* voxelChunk = GetWorld()->SpawnActor<AVoxelChunk>(location, rotation, spawnInfo);
    voxelChunk->InitializeChunk(VoxelSize, BaseMaterial, VoxelTextures, this);

    m_voxelChunkInstances.Add(chunkPosition, voxelChunk);
}

void AVoxelWorld::LoadBaseMaterial()
{
    FString materialPath = TEXT("/DynamicVoxels/VoxelMaterial.VoxelMaterial");
    BaseMaterial = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), nullptr, *materialPath));

    if (!BaseMaterial) UE_LOG(LogTemp, Error, TEXT("Failed to load base material for voxel terrain!"));
}

FIntVector3 AVoxelWorld::WorldToChunkPosition(const FVector& position) const
{
    FVector relativePosition = position / FVector(VoxelSize);
    relativePosition /= FVector(AVoxelBase::VOXEL_CHUNK_SIZE);
    FIntVector3 final = FIntVector3(static_cast<int>(FMath::Floor(relativePosition.X)),
                                    static_cast<int>(FMath::Floor(relativePosition.Y)),
                                    static_cast<int>(FMath::Floor(relativePosition.Z)));

    return final;
}

int AVoxelWorld::GetGridDistance(const FIntVector2& p1, const FIntVector2& p2)
{
    int d1 = FMath::Abs(p1.X - p2.X);
    int d2 = FMath::Abs(p1.Y - p2.Y);

    return FMath::Max(d1, d2);
}