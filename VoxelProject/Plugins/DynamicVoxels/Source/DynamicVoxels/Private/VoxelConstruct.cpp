
#include "VoxelConstruct.h"

AVoxelConstruct::AVoxelConstruct() { PrimaryActorTick.bCanEverTick = false; }

void AVoxelConstruct::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    RegenerateMesh();
    CreateCollision();
}

void AVoxelConstruct::ApplyVoxelEdit(const TArray<FUintVector3>& Positions, uint8 Value)
{
    if (Positions.IsEmpty())
    {
        return;
    }

    for (const FUintVector3& Position : Positions)
    {
        Grid.SetVoxel(Position, Value);
    }

    RegenerateMesh();
    CreateCollision();
    OnVoxelsChanged();
}

void AVoxelConstruct::OnVoxelsChanged()
{
    const int32 N = static_cast<int32>(Grid.GridSize);
    if (N == 0)
    {
        return;
    }

    TArray<uint8> Visited;
    Visited.SetNumZeroed(N * N * N);

    static const int32 DX[] = {-1, 1, 0, 0, 0, 0};
    static const int32 DY[] = {0, 0, -1, 1, 0, 0};
    static const int32 DZ[] = {0, 0, 0, 0, -1, 1};

    TArray<TArray<FIntVector>> Islands;

    for (int32 z = 0; z < N; ++z)
    {
        for (int32 y = 0; y < N; ++y)
        {
            for (int32 x = 0; x < N; ++x)
            {
                const int32 StartIndex = (z * N + y) * N + x;
                if (Visited[StartIndex] || Grid.GetVoxel(FIntVector3(x, y, z)) == 0)
                {
                    continue;
                }

                TArray<FIntVector> Island;
                TArray<FIntVector> Stack;
                Stack.Push(FIntVector(x, y, z));
                Visited[StartIndex] = 1;

                while (Stack.Num() > 0)
                {
                    const FIntVector Current = Stack.Pop();
                    Island.Add(Current);

                    for (int32 Dir = 0; Dir < 6; ++Dir)
                    {
                        const int32 NX = Current.X + DX[Dir];
                        const int32 NY = Current.Y + DY[Dir];
                        const int32 NZ = Current.Z + DZ[Dir];

                        if (NX < 0 || NY < 0 || NZ < 0 || NX >= N || NY >= N || NZ >= N)
                        {
                            continue;
                        }

                        const int32 NeighborIndex = (NZ * N + NY) * N + NX;
                        if (Visited[NeighborIndex] || Grid.GetVoxel(FIntVector3(NX, NY, NZ)) == 0)
                        {
                            continue;
                        }

                        Visited[NeighborIndex] = 1;
                        Stack.Push(FIntVector(NX, NY, NZ));
                    }
                }

                if (Island.Num() >= 2)
                {
                    Islands.Add(MoveTemp(Island));
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

    for (int32 i = 0; i < Islands.Num(); ++i)
    {
        if (i == LargestIndex)
        {
            continue;
        }

        FVoxelGrid PieceGrid;
        PieceGrid.Initialize(Grid.GridSize);
        for (const FIntVector& Position : Islands[i])
        {
            //const FUintVector3 UPosition(static_cast<uint32>(Position.X), static_cast<uint32>(Position.Y), static_cast<uint32>(Position.Z));
            PieceGrid.SetVoxel(Position, Grid.GetVoxel(Position));
        }

        AVoxelConstruct* NewPiece = GetWorld()->SpawnActor<AVoxelConstruct>(GetClass(), GetActorTransform());
        if (NewPiece)
        {
            NewPiece->InitVoxel(CollisionType, PieceGrid, /*bIsWorld=*/false);
        }
    }

    FVoxelGrid LargestGrid;
    LargestGrid.Initialize(Grid.GridSize);
    for (const FIntVector& Position : Islands[LargestIndex])
    {
        //const FUintVector3 UPosition((uint32)Position.X, (uint32)Position.Y, (uint32)Position.Z);
        LargestGrid.SetVoxel(Position, Grid.GetVoxel(Position));
    }

    InitVoxel(CollisionType, LargestGrid, m_static);
}

void AVoxelConstruct::GenerateVoxelData()
{
    Grid.Initialize(VOXEL_CHUNK_SIZE);

    const FVector center = FVector(static_cast<float>(Grid.GridSize) * 0.5f);
    
    const float baseRadius = static_cast<float>(Grid.GridSize) * 0.4f;  
    const float noiseAmplitude = baseRadius * 0.4f;  
                                                     
    const float caveThreshold = 0.55f; 

    for (uint32 z = 0; z < Grid.GridSize; z++)
    {
        for (uint32 y = 0; y < Grid.GridSize; y++)
        {
            for (uint32 x = 0; x < Grid.GridSize; x++)
            {
                const FVector position(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                const float distance = FVector::Dist(position, center);

                const float broadNoise = FMath::PerlinNoise3D(position / 20.0f);
                const float fineNoise = FMath::PerlinNoise3D(position / 6.0f);
                const float surfaceNoise = broadNoise * 0.7f + fineNoise * 0.3f;
                const float noisyRadius = baseRadius + surfaceNoise * noiseAmplitude;

                if (distance > noisyRadius)
                {
                    continue;
                }

                const float caveNoise = FMath::PerlinNoise3D((position + FVector(100.0f)) / 10.0f);
                if (caveNoise > caveThreshold)
                {
                    continue;  // carve a pocket here
                }

                const float depthBelowSurface = noisyRadius - distance;
                uint8 blockType = 1;  // Stone
                if (depthBelowSurface < 3.0f)
                {
                    blockType = 2;  // Grass
                }
                else if (depthBelowSurface < 6.0f)
                {
                    blockType = 3;  // Dirt
                }

                Grid.SetVoxel(FIntVector3(x, y, z), blockType);
            }
        }
    }
}

void AVoxelConstruct::InitVoxel(EVoxelCollisionType NewCollisionType, bool isStatic)
{
    m_static = isStatic;
    CollisionType = NewCollisionType;

    GenerateVoxelData();
    RegenerateMesh();
    CreateCollision();

    if (ProceduralMesh)
    {
        ProceduralMesh->SetSimulatePhysics(!m_static);
        ProceduralMesh->SetEnableGravity(!m_static);
    }
}

void AVoxelConstruct::InitVoxel(EVoxelCollisionType NewCollisionType, const FVoxelGrid& InGrid, bool isStatic)
{
    m_static = isStatic;
    CollisionType = NewCollisionType;

    Grid = InGrid;
    RegenerateMesh();
    CreateCollision();

    if (ProceduralMesh)
    {
        ProceduralMesh->SetSimulatePhysics(!m_static);
        ProceduralMesh->SetEnableGravity(!m_static);
    }
}