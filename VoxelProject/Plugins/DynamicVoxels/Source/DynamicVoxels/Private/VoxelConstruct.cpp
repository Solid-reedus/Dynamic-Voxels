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

void AVoxelConstruct::InitVoxel(EVoxelCollisionType NewCollisionType, bool isStatic)
{
    m_static = isStatic;
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
        ProceduralMesh->SetSimulatePhysics(!m_static);
        ProceduralMesh->SetEnableGravity(!m_static);
    }
}

void AVoxelConstruct::InitVoxel(EVoxelCollisionType NewCollisionType, const TMap<FIntVector, FVoxelGrid>& InSubGrids, bool isStatic)
{
    m_static = isStatic;
    CollisionType = NewCollisionType;

    SubGrids = InSubGrids;

    const TArray<FVoxelGridInstance> GridInstances = BuildGridInstances();
    RegenerateMesh(GridInstances);
    CreateCollision(GridInstances);

    if (ProceduralMesh)
    {
        ProceduralMesh->SetSimulatePhysics(!m_static);
        ProceduralMesh->SetEnableGravity(!m_static);
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
    if (SubGrids.IsEmpty())
    {
        return;
    }

    constexpr int32 SubGridSize = 32;
    constexpr int32 SubGridShift = 5;  // log2(32)
    constexpr int32 SubGridMask = 31;  // 32 - 1

    // Looks up a voxel by its GLOBAL position (not local to any one
    // sub-grid) - same >>5/&31 split used everywhere else in this class.
    // A sub-grid that doesn't exist is treated as empty space rather than
    // an error - there's no reason every coordinate should have one
    // allocated.
    auto GetVoxelAt = [this](const FIntVector& GlobalPos) -> uint8
    {
        const FIntVector SubGridCoord(GlobalPos.X >> SubGridShift, GlobalPos.Y >> SubGridShift, GlobalPos.Z >> SubGridShift);
        const FVoxelGrid* Found = SubGrids.Find(SubGridCoord);
        if (!Found)
        {
            return 0;
        }

        const int32 LocalX = GlobalPos.X & SubGridMask;
        const int32 LocalY = GlobalPos.Y & SubGridMask;
        const int32 LocalZ = GlobalPos.Z & SubGridMask;
        return Found->GetVoxel(FIntVector3(LocalX, LocalY, LocalZ));
    };

    static const int32 DX[] = {-1, 1, 0, 0, 0, 0};
    static const int32 DY[] = {0, 0, -1, 1, 0, 0};
    static const int32 DZ[] = {0, 0, 0, 0, -1, 1};

    // Visited has to be a sparse set, not a flat pre-sized array - unlike
    // the single-Grid version, there's no bounded [0,N) range to size it
    // against, since a construct can span an arbitrary, non-contiguous set
    // of sub-grids.
    TSet<FIntVector> Visited;
    TArray<TArray<FIntVector>> Islands;

    // Flood-fill starting from every solid voxel in every sub-grid.
    // Iterates each sub-grid's own local [0,32) range and converts to
    // global coordinates for the actual flood, since the flood itself
    // needs to be able to cross into neighboring sub-grids.
    for (const TPair<FIntVector, FVoxelGrid>& SubGridEntry : SubGrids)
    {
        const FIntVector& SubGridCoord = SubGridEntry.Key;

        for (int32 LocalZ = 0; LocalZ < SubGridSize; ++LocalZ)
        {
            for (int32 LocalY = 0; LocalY < SubGridSize; ++LocalY)
            {
                for (int32 LocalX = 0; LocalX < SubGridSize; ++LocalX)
                {
                    const FIntVector StartGlobal(SubGridCoord.X * SubGridSize + LocalX,
                                                 SubGridCoord.Y * SubGridSize + LocalY,
                                                 SubGridCoord.Z * SubGridSize + LocalZ);

                    if (Visited.Contains(StartGlobal) || GetVoxelAt(StartGlobal) == 0)
                    {
                        continue;
                    }

                    TArray<FIntVector> Island;
                    TArray<FIntVector> Stack;
                    Stack.Push(StartGlobal);
                    Visited.Add(StartGlobal);

                    while (Stack.Num() > 0)
                    {
                        const FIntVector Current = Stack.Pop();
                        Island.Add(Current);

                        for (int32 Dir = 0; Dir < 6; ++Dir)
                        {
                            const FIntVector Neighbor(Current.X + DX[Dir], Current.Y + DY[Dir], Current.Z + DZ[Dir]);

                            if (Visited.Contains(Neighbor) || GetVoxelAt(Neighbor) == 0)
                            {
                                continue;
                            }

                            Visited.Add(Neighbor);
                            Stack.Push(Neighbor);
                        }
                    }

                    if (Island.Num() >= 2)
                    {
                        Islands.Add(MoveTemp(Island));
                    }
                }
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("%s: island count %d"), *GetName(), Islands.Num());

    if (Islands.Num() <= 1)
    {
        return;
    }

    int32 LargestIndex = 0;
    for (int32 i = 1; i < Islands.Num(); ++i)
    {
        if (Islands[i].Num() > Islands[LargestIndex].Num())
        {
            LargestIndex = i;
        }
    }

    // Builds a fresh SubGrids map containing only the given island's
    // voxels. An island can span multiple sub-grids, so this re-derives
    // which sub-grid each global position belongs to - exactly like
    // ApplyVoxelEdit does - rather than assuming it all fits in one.
    auto BuildSubGridsForIsland = [this, &GetVoxelAt](const TArray<FIntVector>& Island) -> TMap<FIntVector, FVoxelGrid>
    {
        TMap<FIntVector, FVoxelGrid> Result;
        for (const FIntVector& GlobalPos : Island)
        {
            const FIntVector SubGridCoord(GlobalPos.X >> SubGridShift,
                                          GlobalPos.Y >> SubGridShift,
                                          GlobalPos.Z >> SubGridShift);
            const int32 LocalX = GlobalPos.X & SubGridMask;
            const int32 LocalY = GlobalPos.Y & SubGridMask;
            const int32 LocalZ = GlobalPos.Z & SubGridMask;

            FVoxelGrid& TargetGrid = Result.FindOrAdd(SubGridCoord);
            if (TargetGrid.GridSize == 0)
            {
                TargetGrid.Initialize(SubGridSize);
            }

            // Preserves the actual block type (stone/grass/dirt, ...)
            // rather than hardcoding a generic "filled" value.
            TargetGrid.SetVoxel(FIntVector3(LocalX, LocalY, LocalZ), GetVoxelAt(GlobalPos));
        }
        return Result;
    };

    for (int32 i = 0; i < Islands.Num(); ++i)
    {
        if (i == LargestIndex)
        {
            continue;
        }

        TMap<FIntVector, FVoxelGrid> PieceSubGrids = BuildSubGridsForIsland(Islands[i]);

        AVoxelConstruct* NewPiece = GetWorld()->SpawnActor<AVoxelConstruct>(GetClass(), GetActorTransform());
        if (NewPiece)
        {
            NewPiece->InitVoxel(CollisionType, PieceSubGrids, /*bIsWorld=*/false);
        }
    }

    TMap<FIntVector, FVoxelGrid> LargestSubGrids = BuildSubGridsForIsland(Islands[LargestIndex]);
    InitVoxel(CollisionType, LargestSubGrids, m_static);
}