// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelConstruct.h"

AVoxelConstruct::AVoxelConstruct() { PrimaryActorTick.bCanEverTick = false; }

void AVoxelConstruct::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    RegenerateMesh();
    CreateCollision();
}

void AVoxelConstruct::GenerateVoxelData()
{
    InitializeGrid(32);

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
    const float Center = (float)Grid.GridSize * 0.5f;

    for (uint32 y = 0; y < Grid.GridSize; y++)
    {
        for (uint32 x = 0; x < Grid.GridSize; x++)
        {
            double perlin2DVal = FMath::PerlinNoise2D(FVector2D((float)x, (float)y) / 160.0) * 0.5 + 0.5;
            float terrainHeight = (float)(perlin2DVal * perlin2DVal * 40.0 + 24.0);

            for (uint32 z = 0; z < Grid.GridSize; z++)
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
                    SetVoxel(FUintVector3(x, y, z), blockType);
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
    RegenerateMesh();
    //CreateCollision();

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

void AVoxelConstruct::InitVoxel(EVoxelCollisionType NewCollisionType, const FVoxelGrid& InGrid, bool bIsWorld)
{
    bWorld = bIsWorld;
    CollisionType = NewCollisionType;

    Grid = InGrid;
    RegenerateMesh();
    CreateCollision();

    if (ProceduralMesh)
    {
        ProceduralMesh->SetSimulatePhysics(!bWorld);
        ProceduralMesh->SetEnableGravity(!bWorld);
    }
}