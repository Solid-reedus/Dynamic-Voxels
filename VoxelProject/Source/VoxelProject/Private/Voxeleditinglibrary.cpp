#include "VoxelEditingLibrary.h"
#include "VoxelBase.h"

void UVoxeleditinglibrary::RemoveVoxelsInSphere(AVoxelBase* Target, const FVector& WorldLocation, float Radius)
{
    ApplySphereEdit(Target, WorldLocation, Radius, 0);
}

void UVoxeleditinglibrary::AddVoxelsInSphere(AVoxelBase* Target, const FVector& WorldLocation, float Radius, uint8 BlockType)
{
    ApplySphereEdit(Target, WorldLocation, Radius, BlockType);
}

void UVoxeleditinglibrary::ApplySphereEdit(AVoxelBase* Target, const FVector& WorldLocation, float Radius, uint8 Value)
{
    if (!Target || Target->VoxelSize == 0)
    {
        return;
    }

    const FVector LocalLocation = Target->GetActorTransform().InverseTransformPosition(WorldLocation);
    const FVector GridSpaceLocation = LocalLocation / static_cast<float>(Target->VoxelSize);
    const float RadiusInVoxels = Radius / static_cast<float>(Target->VoxelSize);

    const int32 MinX = FMath::FloorToInt(GridSpaceLocation.X - RadiusInVoxels);
    const int32 MaxX = FMath::CeilToInt(GridSpaceLocation.X + RadiusInVoxels);
    const int32 MinY = FMath::FloorToInt(GridSpaceLocation.Y - RadiusInVoxels);
    const int32 MaxY = FMath::CeilToInt(GridSpaceLocation.Y + RadiusInVoxels);
    const int32 MinZ = FMath::FloorToInt(GridSpaceLocation.Z - RadiusInVoxels);
    const int32 MaxZ = FMath::CeilToInt(GridSpaceLocation.Z + RadiusInVoxels);

    const float RadiusSq = RadiusInVoxels * RadiusInVoxels;

    constexpr int32 SubRegionShift = 5;  // log2(32)
    constexpr int32 SubRegionMask = 31;  // 32 - 1
    constexpr int32 VoxelsPerSubRegion = 32 * 32 * 32;

    TMap<FIntVector, TArray<uint8>> Edits;
    int32 TouchedCount = 0;

    for (int32 z = MinZ; z <= MaxZ; ++z)
    {
        for (int32 y = MinY; y <= MaxY; ++y)
        {
            for (int32 x = MinX; x <= MaxX; ++x)
            {
                const FVector VoxelCenter = FVector((float)x, (float)y, (float)z) + 0.5f;
                if (FVector::DistSquared(VoxelCenter, GridSpaceLocation) > RadiusSq)
                {
                    continue;
                }

                const FIntVector SubRegion(x >> SubRegionShift, y >> SubRegionShift, z >> SubRegionShift);
                const int32 LocalX = x & SubRegionMask;
                const int32 LocalY = y & SubRegionMask;
                const int32 LocalZ = z & SubRegionMask;
                const int32 BitIndex = (LocalZ * 32 + LocalY) * 32 + LocalX;

                TArray<uint8>& Mask = Edits.FindOrAdd(SubRegion);
                if (Mask.Num() == 0)
                {
                    Mask.Init(0, VoxelsPerSubRegion);
                }
                Mask[BitIndex] = 1;
                ++TouchedCount;
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("amount of voxels changed %d across %d sub-region(s)"), TouchedCount, Edits.Num());

    Target->ApplyVoxelEdit(Edits, Value);
}