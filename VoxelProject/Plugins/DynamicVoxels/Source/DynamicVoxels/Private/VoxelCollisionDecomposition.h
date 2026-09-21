#pragma once

#include "CoreMinimal.h"

struct FVoxelGrid;

namespace DecompVoxel
{
// A single axis-aligned rectangular block, given in voxel-grid units
// (min corner + size, both in voxel counts - not world/local space).
struct DYNAMICVOXELS_API FVoxelBlock
{
    int32 X = 0, Y = 0, Z = 0;              // min corner (inclusive)
    int32 SizeX = 0, SizeY = 0, SizeZ = 0;  // extent, in voxels
};

// ---- 3GDM (3D Generalized Delta Method) ----
// Fast run-merging heuristic from Hoschl & Flusser (2019), Section 2.1 -
// their comparison baseline. ~0.7s/object, ~458k total blocks on their
// 128^3 McGill benchmark (Table 1). Good default for interactive/editor use.
DYNAMICVOXELS_API TArray<FVoxelBlock> Decompose3GDM(const FVoxelGrid& Grid);

// ---- Octree ----
// Classic power-of-two recursive subdivision (Section 2.2). Simple and
// fast but produces far more blocks than any other method here - included
// for comparison, not recommended as a real collision source.
DYNAMICVOXELS_API TArray<FVoxelBlock> DecomposeOctree(const FVoxelGrid& Grid);

// ---- BSP ----
// Recursively splits the bounding box with the axis-aligned plane that
// cuts through the fewest occupied voxels (Section 2.3; this specific
// "thinnest cut" criterion is the one the paper's own experiments used).
DYNAMICVOXELS_API TArray<FVoxelBlock> DecomposeBSP(const FVoxelGrid& Grid);

// ---- MaxIS / IS(1) ----
// The paper's main proposed method (Section 3): find concave edges,
// weight candidate separators (Eq. 2, w^(2)), and repeatedly place a
// maximum independent set of same-weight, non-intersecting separators
// until no concave edges remain, then merge adjacent same-extent blocks.
// MaxIS solves the independent-set step exactly (branch-and-bound, capped
// - see MaxExactSeparatorCount in the .cpp - falling back to IS(1) above
// that to avoid combinatorial blow-up); IS(1) always uses the cheap
// "largest single axis group" approximation from Section 3.4.
//
// Both are dramatically better than Octree/BSP/3GDM in block count on
// structured shapes, but MUCH slower - seconds to tens of seconds on a
// dense 128^3 grid (matching the paper's own reported 88-107s/object for
// MaxIS on their similarly-sized benchmark). CreateCollision() is only
// ever called explicitly (never automatically on every voxel edit), but
// still be deliberate about which shapes get pointed at these two - see
// EstimateConcaveEdgeDensity() below.
DYNAMICVOXELS_API TArray<FVoxelBlock> DecomposeMaxIS(const FVoxelGrid& Grid);
DYNAMICVOXELS_API TArray<FVoxelBlock> DecomposeIS1(const FVoxelGrid& Grid);

// ---- Pre-flight complexity estimate ----
// A single cheap concave-edge count on the raw grid (no iteration, no
// separator solving) - orders of magnitude cheaper than actually running
// MaxIS/IS(1). Returns edge count normalized by GridSize^2. Empirically:
// rectilinear/structured shapes score well under 1.0, a voxelized sphere
// scores around 1.2, random/noisy density scores 3+ and keeps climbing
// with grid size. Use this to decide whether MaxIS/IS(1) are worth
// running on a given grid, e.g. falling back to 3GDM/BSP above ~2.0 -
// tune the cutoff to your own content, this isn't a hard guarantee.
DYNAMICVOXELS_API float EstimateConcaveEdgeDensity(const FVoxelGrid& Grid);
}  // namespace DecompVoxel