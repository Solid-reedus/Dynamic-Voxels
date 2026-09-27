
#include "VoxelEditingLibrary.h"
#include "VoxelBase.h"
#include "VoxelGrid.h"

void UVoxeleditinglibrary::RemoveVoxelsInSphere(AVoxelBase* Target, const FVector& WorldLocation, float Radius)
{
    if (!Target)
    {
        return;
    }

    const FVoxelGrid& Grid = Target->GetGrid();
    if (Grid.GridSize == 0 || Target->VoxelSize == 0)
    {
        return;
    }

    const FVector LocalLocation = Target->GetActorTransform().InverseTransformPosition(WorldLocation);
    const FVector GridSpaceLocation = LocalLocation / static_cast<float>(Target->VoxelSize);
    const float RadiusInVoxels = Radius / static_cast<float>(Target->VoxelSize);

    const int32 N = static_cast<int32>(Grid.GridSize);
    const int32 MinX = FMath::Clamp(FMath::FloorToInt(GridSpaceLocation.X - RadiusInVoxels), 0, N - 1);
    const int32 MaxX = FMath::Clamp(FMath::CeilToInt(GridSpaceLocation.X + RadiusInVoxels), 0, N - 1);
    const int32 MinY = FMath::Clamp(FMath::FloorToInt(GridSpaceLocation.Y - RadiusInVoxels), 0, N - 1);
    const int32 MaxY = FMath::Clamp(FMath::CeilToInt(GridSpaceLocation.Y + RadiusInVoxels), 0, N - 1);
    const int32 MinZ = FMath::Clamp(FMath::FloorToInt(GridSpaceLocation.Z - RadiusInVoxels), 0, N - 1);
    const int32 MaxZ = FMath::Clamp(FMath::CeilToInt(GridSpaceLocation.Z + RadiusInVoxels), 0, N - 1);

    const float RadiusSq = RadiusInVoxels * RadiusInVoxels;

    TArray<FUintVector3> Positions;
    for (int32 z = MinZ; z <= MaxZ; ++z)
    {
        for (int32 y = MinY; y <= MaxY; ++y)
        {
            for (int32 x = MinX; x <= MaxX; ++x)
            {
                const FVector VoxelCenter = FVector(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)) + 0.5f;
                if (FVector::DistSquared(VoxelCenter, GridSpaceLocation) <= RadiusSq)
                {
                    Positions.Add(FUintVector3((uint32)x, (uint32)y, (uint32)z));
                }
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("amount of voxels changed %d"), Positions.Num());

    Target->ApplyVoxelEdit(Positions, 0);
}

void UVoxeleditinglibrary::AddVoxelsInSphere(AVoxelBase* Target, const FVector& WorldLocation, float Radius, uint8 BlockType)
{
    if (!Target)
    {
        return;
    }

    const FVoxelGrid& Grid = Target->GetGrid();
    if (Grid.GridSize == 0 || Target->VoxelSize == 0)
    {
        return;
    }

    const FVector LocalLocation = Target->GetActorTransform().InverseTransformPosition(WorldLocation);
    const FVector GridSpaceLocation = LocalLocation / static_cast<float>(Target->VoxelSize);
    const float RadiusInVoxels = Radius / static_cast<float>(Target->VoxelSize);

    const int32 N = static_cast<int32>(Grid.GridSize);
    const int32 MinX = FMath::Clamp(FMath::FloorToInt(GridSpaceLocation.X - RadiusInVoxels), 0, N - 1);
    const int32 MaxX = FMath::Clamp(FMath::CeilToInt(GridSpaceLocation.X + RadiusInVoxels), 0, N - 1);
    const int32 MinY = FMath::Clamp(FMath::FloorToInt(GridSpaceLocation.Y - RadiusInVoxels), 0, N - 1);
    const int32 MaxY = FMath::Clamp(FMath::CeilToInt(GridSpaceLocation.Y + RadiusInVoxels), 0, N - 1);
    const int32 MinZ = FMath::Clamp(FMath::FloorToInt(GridSpaceLocation.Z - RadiusInVoxels), 0, N - 1);
    const int32 MaxZ = FMath::Clamp(FMath::CeilToInt(GridSpaceLocation.Z + RadiusInVoxels), 0, N - 1);

    const float RadiusSq = RadiusInVoxels * RadiusInVoxels;

    TArray<FUintVector3> Positions;
    for (int32 z = MinZ; z <= MaxZ; ++z)
    {
        for (int32 y = MinY; y <= MaxY; ++y)
        {
            for (int32 x = MinX; x <= MaxX; ++x)
            {
                const FVector VoxelCenter = FVector(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)) + 0.5f;
                if (FVector::DistSquared(VoxelCenter, GridSpaceLocation) <= RadiusSq)
                {
                    Positions.Add(FUintVector3((uint32)x, (uint32)y, (uint32)z));
                }
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("amount of voxels added %d"), Positions.Num());

    Target->ApplyVoxelEdit(Positions, BlockType);
}