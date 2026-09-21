#pragma once

#include "CoreMinimal.h"

struct FVoxelGrid;

/*
    Turns a solid voxel grid into a small set of axis-aligned boxes for
    compound collision, instead of one expensive convex hull or a collider
    per voxel.
    
    Most of this - the concave-edge/MaxIS/IS(1) methods especially - is a
    C++/Unreal port of the algorithm from Hoschl & Flusser, "Close-to-optimal
    algorithm for rectangular decomposition of 3D shapes", Kybernetika 55(5),
    2019. Written with AI help (Claude) to translate the paper into working
    code, so if some part doesn't match the paper, that mismatch is more
    likely a translation slip than an intentional change.
*/
namespace DecompVoxel
{
    struct DYNAMICVOXELS_API FVoxelBlock
    {
        int32 X = 0, Y = 0, Z = 0;              // min corner (inclusive)
        int32 SizeX = 0, SizeY = 0, SizeZ = 0;  // extent, in voxels
    };

    // Fast, solid default. Merges runs of voxels into boxes; cheap enough
    // to run live while editing in the Unreal Editor.
    DYNAMICVOXELS_API TArray<FVoxelBlock> Decompose3GDM(const FVoxelGrid& Grid);

    // Classic octree subdivision. Simple, but produces way more boxes than
    // anything else here - mostly kept around as a comparison baseline.
    DYNAMICVOXELS_API TArray<FVoxelBlock> DecomposeOctree(const FVoxelGrid& Grid);

    // Recursively splits the grid along whichever plane cuts through the
    // fewest voxels. Beats Octree, still loses to MaxIS/IS(1).
    DYNAMICVOXELS_API TArray<FVoxelBlock> DecomposeBSP(const FVoxelGrid& Grid);

    // The paper's actual method: finds concave edges and solves for the
    // best cuts to remove them. Gives by far the fewest boxes on
    // structured shapes, but can take seconds to well over a minute on a
    // large or messy grid - check EstimateConcaveEdgeDensity below before
    // pointing this at something, and don't run it on every voxel edit.
    DYNAMICVOXELS_API TArray<FVoxelBlock> DecomposeMaxIS(const FVoxelGrid& Grid);

    // Same idea as MaxIS, but with a much cheaper (and somewhat worse)
    // approximation for choosing cuts.
    DYNAMICVOXELS_API TArray<FVoxelBlock> DecomposeIS1(const FVoxelGrid& Grid);

    // Quick check for whether MaxIS/IS(1) are even worth running on a given
    // grid. Clean/structured shapes score under ~1, noisy or random ones
    // score 3+ and keep climbing with grid size - if it's high, reach for
    // 3GDM or BSP instead.
    DYNAMICVOXELS_API float EstimateConcaveEdgeDensity(const FVoxelGrid& Grid);
}  // namespace DecompVoxel