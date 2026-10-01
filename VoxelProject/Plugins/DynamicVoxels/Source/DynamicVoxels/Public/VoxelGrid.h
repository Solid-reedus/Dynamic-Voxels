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

    uint32 GridSize = 0;

    TArray<uint8> Data;

    TArray<uint64> XYPlaneBitData;
    TArray<uint64> XZPlaneBitData;

    uint32 BitGridSize = 0;

    void Initialize(uint32 InGridSize)
    {
        if (InGridSize > 64) UE_LOG(LogTemp, Fatal, TEXT("Voxel Grid Size cannot be bigger than 64!"));

        GridSize = InGridSize;
        BitGridSize = InGridSize + 2;
        Data.Init(0, GridSize * GridSize * GridSize);

        XYPlaneBitData.Init(0, BitGridSize * BitGridSize);
        XZPlaneBitData.Init(0, BitGridSize * BitGridSize);
    }

    inline int GetBitPlaneIndex(int PosX, int PosY)
    {
        return (PosY + 1) * BitGridSize + (PosX + 1);
    }

    inline void SetVoxelFromBitData(TArray<uint64>& BitData, const FIntVector3& Position, uint8 Value)
    {
        int Index = GetBitPlaneIndex(Position.X, Position.Y);
        if (Value != 0)
            BitData[Index] = BitData[Index] | (1ui64 << static_cast<uint64>(Position.Z));
        else
            BitData[Index] = BitData[Index] & ~(1ui64 << static_cast<uint64>(Position.Z));
    }

    inline uint8 GetVoxel(const FIntVector3& Position) const
    {
        int PosMin = Position.GetMin();
        int PosMax = Position.GetMax();

        if (PosMax >= static_cast<int>(GridSize) || PosMin < 0)
        {
            UE_LOG(LogTemp,
                   Error,
                   TEXT("FVoxelGrid: attempting to get invalid voxel position (%i,%i,%i) for GridSize %u"),
                   Position.X,
                   Position.Y,
                   Position.Z,
                   GridSize);
            return 0;
        }
        return Data[Position.Z * GridSize * GridSize + Position.Y * GridSize + Position.X];
    }
    inline void SetVoxel(const FIntVector3& Position, uint8 Value)
    {
        int PosMin = Position.GetMin();
        int PosMax = Position.GetMax();

        if (PosMax > static_cast<int>(GridSize) || PosMin < -1)
        {
            UE_LOG(LogTemp,
                   Error,
                   TEXT("FVoxelGrid: attempting to set invalid voxel position (%i,%i,%i) for GridSize %u"),
                   Position.X,
                   Position.Y,
                   Position.Z,
                   GridSize);
            return;
        }

        if (PosMax < static_cast<int>(GridSize) && PosMin >= 0)
            Data[Position.Z * GridSize * GridSize + Position.Y * GridSize + Position.X] = Value;

        if (Position.Z >= 0 && Position.Z < static_cast<int>(GridSize)) SetVoxelFromBitData(XYPlaneBitData, Position, Value);
        if (Position.Y >= 0 && Position.Y < static_cast<int>(GridSize))
            SetVoxelFromBitData(XZPlaneBitData, FIntVector3(Position.X, Position.Z, Position.Y), Value);
    }

    bool IsFilled(int32 x, int32 y, int32 z) const
    {
        if (x < 0 || y < 0 || z < 0) return false;
        if ((uint32)x >= GridSize || (uint32)y >= GridSize || (uint32)z >= GridSize) return false;
        return Data[x + y * GridSize + z * GridSize * GridSize] != 0;
    }
};