#pragma once

#include "CoreMinimal.h"


/*
    defines the collision type for a voxel, which determins how the collision is generated for the voxel
*/
UENUM(BlueprintType)
enum class EVoxelCollisionType : uint8
{
    Octree UMETA(DisplayName = "Octree"),
    BSP UMETA(DisplayName = "BSP"),
    ThreeGDM UMETA(DisplayName = "3GDM"),
    MaxIS UMETA(DisplayName = "MaxIS"),
    IS1 UMETA(DisplayName = "IS(1)"),
    ConvexHull UMETA(DisplayName = "convex hull"),
};