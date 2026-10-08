#include "VoxelChunk.h"
#include "VoxelWorld.h"

#define SUPERFLAT_WORLDGEN 0

// Sets default values
AVoxelChunk::AVoxelChunk() { PrimaryActorTick.bCanEverTick = false; }

TArray<AVoxelBase::FVoxelGridInstance> AVoxelChunk::BuildGridInstances() const
{
    return {FVoxelGridInstance{FIntVector::ZeroValue, &Grid}};
}

void AVoxelChunk::ApplyVoxelEdit(const TMap<FIntVector, uint8>& Edits)
{
    if (Edits.IsEmpty()) return;

    m_world->ApplyVoxelEdit(Edits);
}

FIntVector AVoxelChunk::WorldToGridPosition(const FVector& Position)
{
    FVector RelativePosition = Position / FVector(VoxelSize);
    FIntVector3 Final = FIntVector3(static_cast<int>(FMath::Floor(RelativePosition.X)),
                                    static_cast<int>(FMath::Floor(RelativePosition.Y)),
                                    static_cast<int>(FMath::Floor(RelativePosition.Z)));

    return Final;
}

void AVoxelChunk::InitializeChunk(uint32 voxelSize,
                                  UMaterial* baseMaterial,
                                  UTexture2DArray* voxelTextures,
                                  AVoxelWorld* world,
                                  FIntVector chunkPos)
{
    VoxelSize = voxelSize;
    m_world = world;
    m_chunkPos = chunkPos;

    GenerateVoxelData(voxelSize);
    if (!IsChunkUniform())
    {
        CreateMaterialInstance(Grid, baseMaterial, voxelTextures);

        const TArray<FVoxelGridInstance> GridInstances = BuildGridInstances();
        RegenerateMesh(GridInstances);
        CreateCollision(GridInstances);
    }
}

void AVoxelChunk::ApplyVoxelEditLocal(const TMap<FIntVector, uint8>& Edits)
{
    // Chunk position as the voxel grid
    FIntVector ChunkVoxelGridPos = m_chunkPos * VOXEL_CHUNK_SIZE;

    for (const auto& Edit : Edits)
    {
        FIntVector LocalPos = Edit.Key - ChunkVoxelGridPos;

        int PosMin = LocalPos.GetMin();
        int PosMax = LocalPos.GetMax();

        if (PosMax > static_cast<int>(Grid.GridSize) || PosMin < -1)
            continue;

        Grid.SetVoxel(LocalPos, Edit.Value);
    }

    const TArray<FVoxelGridInstance> GridInstances = BuildGridInstances();
    RegenerateMesh(GridInstances);
    CreateCollision(GridInstances);
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

    Grid.Initialize(VOXEL_CHUNK_SIZE);

    bool containsSolid = false;

    // Basic terrain generation, will be optimized and improved in the future
    for (int y = -1; y <= static_cast<int>(VOXEL_CHUNK_SIZE); y++)
    {
        for (int x = -1; x <= static_cast<int>(VOXEL_CHUNK_SIZE); x++)
        {
            FIntVector2 topDownPosition = FIntVector2(x + relativePosition.X, y + relativePosition.Y);

            double perlin2DValBiome = FMath::PerlinNoise2D(FVector2D(topDownPosition) / 200.0) * 0.5 + 0.5;

            double perlin2DVal = FMath::PerlinNoise2D(FVector2D(topDownPosition) / 160.0) * 0.5 + 0.5;
            if (perlin2DValBiome > 0.5) topDownPosition += FIntVector2(100, 100);
            double perlin2DValDetail = FMath::PerlinNoise2D(FVector2D(topDownPosition) / 64.0) * 0.5 + 0.5;

            int terrainValue = static_cast<int>(perlin2DVal * perlin2DVal * 80.0 + 48.0 + perlin2DValDetail * 20.0);
            if (perlin2DValBiome > 0.5) terrainValue -= static_cast<int>(perlin2DValBiome * 10.0);
#if SUPERFLAT_WORLDGEN
            terrainValue = 48;
#endif
            terrainValue -= relativePosition.Z;

            double BlockChoice = FMath::RandRange(0.0, 1.0);
            BlockChoice += perlin2DValDetail * 3;

            for (int z = -1; z <= static_cast<int>(VOXEL_CHUNK_SIZE); z++)
            {
                FIntVector3 worldPosition = FIntVector3(x, y, z) + relativePosition;
                float caveValue = FMath::PerlinNoise3D(FVector(worldPosition) / 16.0);

                if (z >= terrainValue)
                {
                    float falloff = static_cast<float>(FMath::Abs(z - terrainValue)) * 0.08;
                    float applied = caveValue - falloff;

                    if (applied > 0.5f)
                    {
                        Grid.SetVoxel(FIntVector3(x, y, z), 7);  // Cobble
                        containsSolid = true;
                    }
                }
                else
                {
                    uint8 blockType = 1;  // Stone
                    if (perlin2DValBiome > 0.5)
                    {
                        if (terrainValue - z < 9) blockType = 4;  // Sand
                    }
                    else
                    {
                        if (terrainValue - z < 3 && BlockChoice > 1.6f)
                            blockType = 2;  // Grass
                        else if (terrainValue - z < 9)
                            blockType = 3;  // Dirt
                    }

                    float falloff = (128.0f - static_cast<float>(worldPosition.Z)) / 64.0f - 1.0f;
                    float applied = caveValue + falloff;

                    if (applied > -0.2f)
                    {
                        Grid.SetVoxel(FIntVector3(x, y, z), blockType);
                        containsSolid = true;
                    }
                }
            }
        }
    }

    if (!containsSolid)
    {
        m_isChunkUniform = true;
    }
}