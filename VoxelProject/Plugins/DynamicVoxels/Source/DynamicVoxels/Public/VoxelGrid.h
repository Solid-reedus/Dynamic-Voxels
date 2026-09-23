#pragma once

#include "CoreMinimal.h"
#include "VoxelGrid.generated.h"

/*
    a flat array of voxel data, with a grid size and a data array of uint8 values,
    meant to encapsulate the voxel data and provide some basic functionality to access and modify the data
*/
USTRUCT()
struct DYNAMICVOXELS_API FVoxelGrid
{
    GENERATED_BODY()

    UPROPERTY()
    uint32 GridSize = 0;

    UPROPERTY()
    TArray<uint8> Data;

    void Initialize(uint32 InGridSize)
    {
        GridSize = InGridSize;
        Data.Init(0, GridSize * GridSize * GridSize);
    }

    bool IsInsideGrid(const FUintVector3& Position) const
    {
        if (Position.X >= GridSize || Position.Y >= GridSize || Position.Z >= GridSize)
            return false;
        else
            return true;
    }

    uint8 GetVoxel(const FUintVector3& Position) const
    {
        if (Position.X >= GridSize || Position.Y >= GridSize || Position.Z >= GridSize)
        {
            UE_LOG(LogTemp,
                   Error,
                   TEXT("FVoxelGrid: attempting to get invalid voxel position (%u,%u,%u) for GridSize %u"),
                   Position.X,
                   Position.Y,
                   Position.Z,
                   GridSize);
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

    static uint8 GetVoxelFromData(const TArray<uint8>& Data, uint32_t GridSize, const FUintVector3& Position)
    {
        if (Position.X >= GridSize || Position.Y >= GridSize || Position.Z >= GridSize)
        {
            UE_LOG(LogTemp,
                   Error,
                   TEXT("FVoxelGrid: attempting to get invalid voxel position (%u,%u,%u) for GridSize %u"),
                   Position.X,
                   Position.Y,
                   Position.Z,
                   GridSize);
            return 0;
        }
        return Data[Position.Z * GridSize * GridSize + Position.Y * GridSize + Position.X];
    }
    static void SetVoxelFromData(TArray<uint8>& Data, uint32_t GridSize, const FUintVector3& Position, uint8 Value)
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

    bool IsFilled(int32 x, int32 y, int32 z) const
    {
        if (x < 0 || y < 0 || z < 0) return false;
        if ((uint32)x >= GridSize || (uint32)y >= GridSize || (uint32)z >= GridSize) return false;
        return Data[x + y * GridSize + z * GridSize * GridSize] != 0;
    }
};