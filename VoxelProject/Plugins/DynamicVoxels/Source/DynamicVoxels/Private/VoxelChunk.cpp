// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelChunk.h"
#include "VoxelWorld.h"

// Sets default values
AVoxelChunk::AVoxelChunk() { PrimaryActorTick.bCanEverTick = false; }

void AVoxelChunk::InitializeChunk(uint32 voxelSize, UMaterial* baseMaterial, UTexture2DArray* voxelTextures, AVoxelWorld* world)
{
    VoxelSize = voxelSize;
    m_world = world;

    GenerateVoxelData(voxelSize);
    if (!IsChunkUniform())
    {
        CreateMaterialInstance(baseMaterial, voxelTextures);
        RegenerateMesh();
        CreateCollision();
    }
}

// Called when the game starts or when spawned
void AVoxelChunk::BeginPlay() { Super::BeginPlay(); }


void AVoxelChunk::ApplyVoxelEdit(const TArray<FUintVector3>& Positions, uint8 Value)
{
    if (Positions.IsEmpty())
    {
        return;
    }

    // A chunk's REAL content (the 32 voxels it actually owns and renders)
    // lives at local indices [1, VOXEL_CHUNK_SIZE], not the full grid range.
    // Indices 0 and VOXEL_CHUNK_SIZE+1 are the padding border - a peek at
    // the neighbor's edge data, kept only so face-culling reads correctly
    // at the seam. An edit landing exactly on that border is NOT this
    // chunk's own content either, even though it's technically within
    // Grid.GridSize - it belongs to whichever neighbor actually owns that
    // world position, same as anything further out.
    const int32 ChunkSize = (int32)VOXEL_CHUNK_SIZE;
    const FIntVector& ThisChunkPos = GetChunkPosition();

    TArray<FUintVector3> LocalPositions;
    TMap<FIntVector, TArray<FUintVector3>> NeighborEdits;

    for (const FUintVector3& Position : Positions)
    {
        // Positions is unsigned, so a conceptually-negative position has to
        // arrive here already wrapped around (built as e.g.
        // FUintVector3((uint32)someNegativeInt32, ...) by whoever computed
        // it) - reinterpreting the bits back via (int32) recovers the
        // original signed value correctly on every platform Unreal actually
        // runs on. This only works if the caller did that cast consistently
        // when building Positions in the first place.
        const int32 X = (int32)Position.X;
        const int32 Y = (int32)Position.Y;
        const int32 Z = (int32)Position.Z;

        const bool bOwnedByThisChunk = X >= 1 && X <= ChunkSize && Y >= 1 && Y <= ChunkSize && Z >= 1 && Z <= ChunkSize;

        if (bOwnedByThisChunk)
        {
            LocalPositions.Add(Position);
            continue;
        }

        // If this position is still within THIS chunk's own array (i.e.
        // it's the padding border, not genuinely beyond the grid), mirror
        // the edit into this chunk's own copy too. This chunk's meshing
        // currently has no Border-skipping wired in (CreateMeshPlanesGreedy
        // doesn't take one), so it renders its padding border as real
        // geometry - without this mirror, an edit at the border only ever
        // updates the neighbor's data, and whatever THIS chunk is actually
        // showing at that spot never changes.
        const int32 N = (int32)Grid.GridSize;
        if (X >= 0 && X < N && Y >= 0 && Y < N && Z >= 0 && Z < N)
        {
            LocalPositions.Add(Position);
        }

        // The canonical owner still needs the real update regardless of
        // whether a local mirror copy was just added above.
        // Convert to a world-voxel coordinate (a pure linear map, valid
        // regardless of whether this position is in this chunk's own array
        // bounds), then work out which chunk's [1, ChunkSize] real-content
        // range that coordinate actually falls into, and what local index
        // it is within that chunk. DivideAndRoundDown (not plain integer
        // division) matters here - plain '/' truncates toward zero, which
        // gives the wrong chunk for negative world-voxel coordinates.
        const int32 WorldVoxelX = X + ChunkSize * ThisChunkPos.X;
        const int32 WorldVoxelY = Y + ChunkSize * ThisChunkPos.Y;
        const int32 WorldVoxelZ = Z + ChunkSize * ThisChunkPos.Z;

        const int32 NeighborChunkX = FMath::DivideAndRoundDown(WorldVoxelX - 1, ChunkSize);
        const int32 NeighborChunkY = FMath::DivideAndRoundDown(WorldVoxelY - 1, ChunkSize);
        const int32 NeighborChunkZ = FMath::DivideAndRoundDown(WorldVoxelZ - 1, ChunkSize);

        const FIntVector NeighborPos(NeighborChunkX, NeighborChunkY, NeighborChunkZ);
        const FUintVector3 NeighborLocal((uint32)(WorldVoxelX - NeighborChunkX * ChunkSize),
                                         (uint32)(WorldVoxelY - NeighborChunkY * ChunkSize),
                                         (uint32)(WorldVoxelZ - NeighborChunkZ * ChunkSize));

        NeighborEdits.FindOrAdd(NeighborPos).Add(NeighborLocal);
    }

    if (!LocalPositions.IsEmpty())
    {
        for (const FUintVector3& Position : LocalPositions)
        {
            Grid.SetVoxel(Position, Value);
        }

        RegenerateMesh();
        CreateCollision();
    }

    if (NeighborEdits.IsEmpty())
    {
        return;
    }

    if (!m_world)
    {
        UE_LOG(LogTemp,
               Warning,
               TEXT("%s: edit reaches beyond this chunk's bounds but m_world isn't set - was this chunk's InitializeChunk "
                    "given a valid AVoxelWorld?"),
               *GetName());
        return;
    }

    // Hand each affected neighbor its own share, already translated into
    // that neighbor's local coordinates. If that neighbor's own call to
    // ApplyVoxelEdit finds part of what it just received ALSO overflows its
    // bounds, it comes back through this exact same function from there -
    // that's the "recursive" cascading, achieved through the normal call
    // chain rather than any explicit recursion-tracking code.
    for (const TPair<FIntVector, TArray<FUintVector3>>& Entry : NeighborEdits)
    {
        m_world->ForwardVoxelEdit(Entry.Key, Entry.Value, Value);
    }
}


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