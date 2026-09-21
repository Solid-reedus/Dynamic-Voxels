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

    const float center = (float)Grid.GridSize * 0.5f;

    for (uint32 y = 0; y < Grid.GridSize; y++)
    {
        for (uint32 x = 0; x < Grid.GridSize; x++)
        {
            double perlin2DVal = FMath::PerlinNoise2D(FVector2D((float)x, (float)y) / 160.0) * 0.5 + 0.5;
            float terrainHeight = (float)(perlin2DVal * perlin2DVal * 40.0 + 24.0);

            for (uint32 z = 0; z < Grid.GridSize; z++)
            {
                if ((float)z >= terrainHeight)
                {
                    break;
                }
                uint8 blockType = 1;  // Stone

                if (terrainHeight - (float)z < 3.0f)
                {
                    blockType = 2;  // Grass
                }

                else if (terrainHeight - (float)z < 6.0f)
                {
                    blockType = 3;  // Dirt
                }

                float perlinValue = FMath::PerlinNoise3D(FVector((float)x, (float)y, (float)z) / 16.0f);
                float falloff = (center - (float)z) / (center * 0.5f);
                perlinValue += falloff;

                if (perlinValue > 0.0f)
                {
                    SetVoxel(FUintVector3(x, y, z), blockType);
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