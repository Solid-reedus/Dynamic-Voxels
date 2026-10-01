// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelConstruct.h"

AVoxelConstruct::AVoxelConstruct() { PrimaryActorTick.bCanEverTick = false; }

void AVoxelConstruct::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    const TArray<FVoxelGridInstance> GridInstances = BuildGridInstances();
    RegenerateMesh(GridInstances);
    CreateCollision(GridInstances);
}

TArray<AVoxelConstruct::FVoxelGridInstance> AVoxelConstruct::BuildGridInstances() const
{
    TArray<FVoxelGridInstance> GridInstances;
    GridInstances.Reserve(SubGrids.Num());

    constexpr int32 SubGridSize = 32;
    for (const TPair<FIntVector, FVoxelGrid>& Entry : SubGrids)
    {
        GridInstances.Add({ Entry.Key * SubGridSize, &Entry.Value });
    }

    return GridInstances;
}

void AVoxelConstruct::GenerateVoxelData()
{
    constexpr uint32 SubGridSize = 32;

    // For now, everything lands in the one sub-grid at (0,0,0) - the
    // original generator was already sized at exactly 32, matching one
    // sub-grid precisely, so this is a direct port rather than a new design.
    FVoxelGrid& Grid = SubGrids.FindOrAdd(FIntVector(0, 0, 0));
    Grid.Initialize(SubGridSize);

    // Adapted from AVoxelChunk::GenerateVoxelData - same two Perlin passes
    // (2D height + 3D density for overhangs/caves), but sampled in the
    // construct's own LOCAL grid coordinates rather than world position.
    // The chunk version ties terrainValue and the falloff directly to
    // absolute world height, which makes sense for one slice of continuous
    // terrain; a standalone construct has no such context, and porting it
    // verbatim means the shape (or lack of one) depends entirely on where
    // you happen to place the actor. Centering everything on the grid's own
    // middle instead makes this produce a similar island/outcrop shape
    // regardless of world position. The exact numbers below are tuned to
    // taste, not load-bearing - adjust freely.
    const float Center = (float)SubGridSize * 0.5f;

    for (uint32 y = 0; y < SubGridSize; y++)
    {
        for (uint32 x = 0; x < SubGridSize; x++)
        {
            double perlin2DVal = FMath::PerlinNoise2D(FVector2D((float)x, (float)y) / 160.0) * 0.5 + 0.5;
            float terrainHeight = (float)(perlin2DVal * perlin2DVal * 40.0 + 24.0);

            for (uint32 z = 0; z < SubGridSize; z++)
            {
                if ((float)z >= terrainHeight) break;

                uint8 blockType = 1;  // Stone
                if (terrainHeight - (float)z < 3.0f)
                    blockType = 2;  // Grass
                else if (terrainHeight - (float)z < 6.0f)
                    blockType = 3;  // Dirt

                float perlinValue = FMath::PerlinNoise3D(FVector((float)x, (float)y, (float)z) / 16.0f);
                float falloff = (Center - (float)z) / (Center * 0.5f);
                perlinValue += falloff;

                if (perlinValue > 0.0f)
                {
                    Grid.SetVoxel(FIntVector3(x, y, z), blockType);
                }
            }
        }
    }
}

void AVoxelConstruct::InitVoxel(EVoxelCollisionType NewCollisionType, bool bIsWorld)
{
    bWorld = bIsWorld;
    CollisionType = NewCollisionType;

    GenerateVoxelData();

    const TArray<FVoxelGridInstance> GridInstances = BuildGridInstances();
    RegenerateMesh(GridInstances);
    CreateCollision(GridInstances);

    // Enabling physics/gravity happens last, after mesh and collision
    // actually exist - doing it earlier (as the old BeginPlay-based version
    // did) meant this could run before there was any valid geometry to
    // simulate against, and could race against BeginPlay applying a stale
    // default bWorld if BeginPlay happened to run before this was called
    // (the normal SpawnActor -> BeginPlay -> InitVoxel order).
    if (ProceduralMesh)
    {
        ProceduralMesh->SetSimulatePhysics(!bWorld);
        ProceduralMesh->SetEnableGravity(!bWorld);
    }
}

void AVoxelConstruct::InitVoxel(EVoxelCollisionType NewCollisionType, const TMap<FIntVector, FVoxelGrid>& InSubGrids, bool bIsWorld)
{
    bWorld = bIsWorld;
    CollisionType = NewCollisionType;

    SubGrids = InSubGrids;

    const TArray<FVoxelGridInstance> GridInstances = BuildGridInstances();
    RegenerateMesh(GridInstances);
    CreateCollision(GridInstances);

    if (ProceduralMesh)
    {
        ProceduralMesh->SetSimulatePhysics(!bWorld);
        ProceduralMesh->SetEnableGravity(!bWorld);
    }
}

void AVoxelConstruct::ApplyVoxelEdit(const TMap<FIntVector, TArray<uint8>>& Edits, uint8 Value)
{
    if (Edits.IsEmpty())
    {
        return;
    }

    constexpr uint32 SubGridSize = 32;
    constexpr int32 VoxelsPerSubGrid = SubGridSize * SubGridSize * SubGridSize;

    for (const TPair<FIntVector, TArray<uint8>>& Entry : Edits)
    {
        const FIntVector& SubGridCoord = Entry.Key;
        const TArray<uint8>& Mask = Entry.Value;

        if (Mask.Num() != VoxelsPerSubGrid)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("%s: edit mask for sub-grid (%d,%d,%d) has %d entries, expected %d - skipping"),
                *GetName(), SubGridCoord.X, SubGridCoord.Y, SubGridCoord.Z, Mask.Num(), VoxelsPerSubGrid);
            continue;
        }

        // FindOrAdd creating a new entry here, for a sub-grid coordinate
        // that didn't exist before, IS the construct growing - no resize,
        // no recenter, no copying old data into a bigger array like the
        // single-Grid version needed.
        FVoxelGrid& TargetGrid = SubGrids.FindOrAdd(SubGridCoord);
        if (TargetGrid.GridSize == 0)
        {
            TargetGrid.Initialize(SubGridSize);
        }

        for (int32 BitIndex = 0; BitIndex < VoxelsPerSubGrid; ++BitIndex)
        {
            if (Mask[BitIndex] == 0)
            {
                continue;
            }

            const int32 LocalX = BitIndex % SubGridSize;
            const int32 LocalY = (BitIndex / SubGridSize) % SubGridSize;
            const int32 LocalZ = BitIndex / (SubGridSize * SubGridSize);

            TargetGrid.SetVoxel(FIntVector3(LocalX, LocalY, LocalZ), Value);
        }
    }

    const TArray<FVoxelGridInstance> GridInstances = BuildGridInstances();
    RegenerateMesh(GridInstances);
    CreateCollision(GridInstances);
    OnVoxelsChanged();
}

void AVoxelConstruct::OnVoxelsChanged()
{
    // NOT YET REIMPLEMENTED - see the header comment above this
    // declaration. The old flood-fill operated on one contiguous [0,N)
    // Grid via Grid.GetVoxel, neither of which exists anymore. A correct
    // version needs to flood across sub-grid boundaries (checking
    // SubGrids.Find(NeighborSubGridCoord) whenever the fill reaches a
    // sub-grid's edge), which is real, separate work.
    UE_LOG(LogTemp, Warning,
        TEXT("%s: OnVoxelsChanged (split detection) is not yet reimplemented for the hashmap-of-sub-grids storage model - no-op for now."),
        *GetName());
}