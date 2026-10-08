#include "VoxelWorld.h"
#include "VoxelChunk.h"
#include "Kismet/GameplayStatics.h"

#include "VoxelConstruct.h"

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

void AVoxelWorld::ApplyVoxelEdit(const TMap<FIntVector, uint8>& Edits)
{
    FIntVector EditsMinPos = FIntVector(INT32_MAX);
    FIntVector EditsMaxPos = FIntVector(INT32_MIN);

    for (const auto& Edit : Edits)
    {
        EditsMinPos.X = FMath::Min(Edit.Key.X, EditsMinPos.X);
        EditsMinPos.Y = FMath::Min(Edit.Key.Y, EditsMinPos.Y);
        EditsMinPos.Z = FMath::Min(Edit.Key.Z, EditsMinPos.Z);

        EditsMaxPos.X = FMath::Max(Edit.Key.X, EditsMaxPos.X);
        EditsMaxPos.Y = FMath::Max(Edit.Key.Y, EditsMaxPos.Y);
        EditsMaxPos.Z = FMath::Max(Edit.Key.Z, EditsMaxPos.Z);
    }

    // TODO(David): Current issue where chunks right next to edit dont get updated, cause those chunks are outside of
    // ChunkMin / ChunkMax range
    // Temporary fix in the line below
    EditsMinPos -= FIntVector(1);
    EditsMaxPos += FIntVector(1);

    FIntVector ChunkMin = GridToChunkPosition(EditsMinPos);
    FIntVector ChunkMax = GridToChunkPosition(EditsMaxPos);

    // Only call ApplyVoxelEditLocal() for chunks that overlap with the voxel edit
    for (int z = ChunkMin.Z; z <= ChunkMax.Z; ++z)
    {
        for (int y = ChunkMin.Y; y <= ChunkMax.Y; ++y)
        {
            for (int x = ChunkMin.X; x <= ChunkMax.X; ++x)
            {
                AVoxelChunk* Target = GetChunkAt(FIntVector(x, y, z));
                if (Target) Target->ApplyVoxelEditLocal(Edits);
            }
        }
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
    voxelChunk->InitializeChunk(VoxelSize, BaseMaterial, VoxelTextures, this, chunkPosition);

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

FIntVector3 AVoxelWorld::GridToChunkPosition(const FIntVector& position)
{
    FVector relativePosition = FVector(position);
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