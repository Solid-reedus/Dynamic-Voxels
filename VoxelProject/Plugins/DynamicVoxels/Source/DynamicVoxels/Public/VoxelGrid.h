#pragma once

#include "CoreMinimal.h"
#include "VoxelGrid.generated.h"

// The project's actual voxel storage type: a flat, indexed array of
// block-type values (0 = empty/air, nonzero = solid material), plus the
// index math and bounds-checking needed to read/write it safely. AVoxelBase
// holds one of these directly (not a raw TArray<uint8>) so this logic lives
// in exactly one place instead of being duplicated between the actor and
// whatever needs to read the data (DecompVoxel's decomposition functions
// take this exact type by const-reference, no copying or adapting needed).
USTRUCT()
struct DYNAMICVOXELS_API FVoxelGrid
{
    GENERATED_BODY()

    UPROPERTY()
    uint32 GridSize = 0;

    UPROPERTY()
    TArray<uint8> Data;

    // Resizes to GridSize^3 and zeroes every voxel. Call before any
    // SetVoxel calls when (re)initializing from scratch.
    void Initialize(uint32 InGridSize)
    {
        GridSize = InGridSize;
        Data.Init(0, GridSize * GridSize * GridSize);
    }

    uint8 GetVoxel(const FUintVector3& Position) const
    {
        if (Position.X >= GridSize || Position.Y >= GridSize || Position.Z >= GridSize)
        {
            return 0;
        }
        return Data[Position.Z * GridSize * GridSize + Position.Y * GridSize + Position.X];
    }

    void SetVoxel(const FUintVector3& Position, uint8 Value)
    {
        if (Position.X >= GridSize || Position.Y >= GridSize || Position.Z >= GridSize)
        {
            UE_LOG(LogTemp,
                   Error,
                   TEXT("FVoxelGrid: attempting to set invalid voxel position (%u,%u,%u) for GridSize %u"),
                   Position.X,
                   Position.Y,
                   Position.Z,
                   GridSize);
            return;
        }
        Data[Position.Z * GridSize * GridSize + Position.Y * GridSize + Position.X] = Value;
    }

    // Consumed by DecompVoxel's decomposition functions. Collision only
    // cares about solid-vs-empty, not which material a voxel is, so any
    // nonzero block-type value counts as filled.
    bool IsFilled(int32 x, int32 y, int32 z) const
    {
        if (x < 0 || y < 0 || z < 0) return false;
        if ((uint32)x >= GridSize || (uint32)y >= GridSize || (uint32)z >= GridSize) return false;
        return Data[x + y * GridSize + z * GridSize * GridSize] != 0;
    }
};