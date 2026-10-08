#include "VoxelEditingLibrary.h"
#include "VoxelBase.h"
#include "VoxelConstruct.h"

void UVoxeleditinglibrary::AttachSelectedConstructTo(AVoxelConstruct* SelectedConstruct,
                                                     AVoxelConstruct* Target,
                                                     const FVector& TargetWorldHitLocation,
                                                     const FVector& SelfWorldHitLocation)
{
    SelectedConstruct->AttachSelfTo(Target, TargetWorldHitLocation, SelfWorldHitLocation);
}

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
    if (!Target || Target->VoxelSize == 0 || Radius < 0.0f)
        return;

    const FIntVector GridSpaceLocation = Target->WorldToGridPosition(WorldLocation);
    const float RadiusInVoxels = Radius / static_cast<float>(Target->VoxelSize);

    const int32 MinX = FMath::FloorToInt(GridSpaceLocation.X - RadiusInVoxels);
    const int32 MaxX = FMath::CeilToInt(GridSpaceLocation.X + RadiusInVoxels);
    const int32 MinY = FMath::FloorToInt(GridSpaceLocation.Y - RadiusInVoxels);
    const int32 MaxY = FMath::CeilToInt(GridSpaceLocation.Y + RadiusInVoxels);
    const int32 MinZ = FMath::FloorToInt(GridSpaceLocation.Z - RadiusInVoxels);
    const int32 MaxZ = FMath::CeilToInt(GridSpaceLocation.Z + RadiusInVoxels);

    const float RadiusSq = RadiusInVoxels * RadiusInVoxels;

    TMap<FIntVector, uint8> Edits;

    for (int32 z = MinZ; z <= MaxZ; ++z)
    {
        for (int32 y = MinY; y <= MaxY; ++y)
        {
            for (int32 x = MinX; x <= MaxX; ++x)
            {
                const FVector VoxelCenter = FVector((float)x, (float)y, (float)z);
                if (FVector::DistSquared(VoxelCenter, FVector(GridSpaceLocation)) > RadiusSq)
                    continue;

                Edits.Add(FIntVector(x, y, z), Value);
            }
        }
    }

    Target->ApplyVoxelEdit(Edits);
}