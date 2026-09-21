#pragma once

#include "CoreMinimal.h"

// Split into its own tiny header rather than living in AVoxelBase.h or being
// nested inside it (UHT rejects a UENUM declared inside a UCLASS body -
// "'Enum' is not allowed here" - this has to sit at file scope). Both
// AVoxelBase.h and voxelDecompose3gdm.h include this instead of one
// depending on the other, so AVoxelBase.h's public surface never has to pull
// in DecompVoxel's namespace/functions just to expose this dropdown.
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