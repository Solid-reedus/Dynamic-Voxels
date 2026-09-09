// Fill out your copyright notice in the Description page of Project Settings.

#include "TestMeshActor.h"

// Sets default values
ATestMeshActor::ATestMeshActor()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = false;

    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
}

// Called when the game starts or when spawned
void ATestMeshActor::BeginPlay()
{
    Super::BeginPlay();

    GenerateVoxelData();
    GenerateVoxelMesh();
}

void ATestMeshActor::GenerateVoxelData()
{
    // Initialize voxel data to 0
    m_voxelData.Init(0, M_VOXEL_MESH_SIZE * M_VOXEL_MESH_SIZE * M_VOXEL_MESH_SIZE);

    float center = static_cast<float>(M_VOXEL_MESH_SIZE >> 1);

    for (uint32 z = 0; z < M_VOXEL_MESH_SIZE; z++)
    {
        for (uint32 y = 0; y < M_VOXEL_MESH_SIZE; y++)
        {
            for (uint32 x = 0; x < M_VOXEL_MESH_SIZE; x++)
            {
                // Set the border voxels to air
                if (x == 0 || x == M_VOXEL_MESH_SIZE - 1 || y == 0 || y == M_VOXEL_MESH_SIZE - 1 || z == 0 ||
                    z == M_VOXEL_MESH_SIZE - 1)
                    continue;

                float perlinValue = FMath::PerlinNoise3D(FVector(x, y, z) / NoiseScale);

                float distx = abs(static_cast<float>(x) - center);
                float disty = abs(static_cast<float>(y) - center);
                float distz = abs(static_cast<float>(z) - center);

                float distance = FMath::Max3(distx, disty, distz);
                distance = FMath::Clamp(distance - (center * 0.5f), 0.0f, 1e30f) * 2.0f;

                perlinValue -= (distance / center);

                if (perlinValue > NoiseCutoff) SetVoxel(x, y, z, 1);
            }
        }
    }
}

void ATestMeshActor::GenerateVoxelMesh()
{
    for (uint32 z = 1; z < M_VOXEL_MESH_SIZE - 1; z++)
    {
        for (uint32 y = 1; y < M_VOXEL_MESH_SIZE - 1; y++)
        {
            for (uint32 x = 1; x < M_VOXEL_MESH_SIZE - 1; x++)
            {
                if (GetVoxel(x, y, z) > 0)
                {
                    FVector voxelPos = FVector(x * 100, y * 100, z * 100);

                    if (GetVoxel(x + 1, y, z) == 0)
                        AddPlane(voxelPos,
                                 FVector(100.0, 0.0, 0.0),
                                 FVector(100.0, 100.0, 0.0),
                                 FVector(100.0, 0.0, 100.0),
                                 FVector(100.0, 100.0, 100.0),
                                 FVector(1.0, 0.0, 0.0));
                    if (GetVoxel(x - 1, y, z) == 0)
                        AddPlane(voxelPos,
                                 FVector(0.0, 100.0, 0.0),
                                 FVector(0.0, 0.0, 0.0),
                                 FVector(0.0, 100.0, 100.0),
                                 FVector(0.0, 0.0, 100.0),
                                 FVector(-1.0, 0.0, 0.0));
                    if (GetVoxel(x, y + 1, z) == 0)
                        AddPlane(voxelPos,
                                 FVector(100.0, 100.0, 0.0),
                                 FVector(0.0, 100.0, 0.0),
                                 FVector(100.0, 100.0, 100.0),
                                 FVector(0.0, 100.0, 100.0),
                                 FVector(0.0, 1.0, 0.0));
                    if (GetVoxel(x, y - 1, z) == 0)
                        AddPlane(voxelPos,
                                 FVector(0.0, 0.0, 0.0),
                                 FVector(100.0, 0.0, 0.0),
                                 FVector(0.0, 0.0, 100.0),
                                 FVector(100.0, 0.0, 100.0),
                                 FVector(0.0, -1.0, 0.0));
                    if (GetVoxel(x, y, z + 1) == 0)
                        AddPlane(voxelPos,
                                 FVector(0.0, 0.0, 100.0),
                                 FVector(100.0, 0.0, 100.0),
                                 FVector(0.0, 100.0, 100.0),
                                 FVector(100.0, 100.0, 100.0),
                                 FVector(0.0, 0.0, 1.0));
                    if (GetVoxel(x, y, z - 1) == 0)
                        AddPlane(voxelPos,
                                 FVector(100.0, 0.0, 0.0),
                                 FVector(0.0, 0.0, 0.0),
                                 FVector(100.0, 100.0, 0.0),
                                 FVector(0.0, 100.0, 0.0),
                                 FVector(0.0, 0.0, -1.0));
                }
            }
        }
    }

    if (ProceduralMesh)
    {
        ProceduralMesh->ClearAllMeshSections();
        ProceduralMesh->CreateMeshSection_LinearColor(0,
                                                      m_vertices,
                                                      m_indices,
                                                      m_normals,
                                                      m_uvs,
                                                      TArray<FLinearColor>(),
                                                      TArray<FProcMeshTangent>(),
                                                      true);

        ProceduralMesh->SetMaterial(0, MeshMaterial);
    }

    m_vertices.Empty();
    m_normals.Empty();
    m_uvs.Empty();
    m_indices.Empty();
    m_indexCount = 0;
}

inline void ATestMeshActor::AddPlane(const FVector& position,
                                     const FVector& v0,
                                     const FVector& v1,
                                     const FVector& v2,
                                     const FVector& v3,
                                     const FVector& normal)
{
    m_vertices.Add(v0 + position);
    m_vertices.Add(v1 + position);
    m_vertices.Add(v2 + position);
    m_vertices.Add(v3 + position);

    m_normals.Add(normal);
    m_normals.Add(normal);
    m_normals.Add(normal);
    m_normals.Add(normal);

    m_uvs.Add(FVector2D(0.0, 0.0));
    m_uvs.Add(FVector2D(1.0, 0.0));
    m_uvs.Add(FVector2D(0.0, 1.0));
    m_uvs.Add(FVector2D(1.0, 1.0));

    m_indices.Add(2 + m_indexCount);
    m_indices.Add(1 + m_indexCount);
    m_indices.Add(0 + m_indexCount);
    m_indices.Add(2 + m_indexCount);
    m_indices.Add(3 + m_indexCount);
    m_indices.Add(1 + m_indexCount);

    m_indexCount += 4;
}

inline uint8 ATestMeshActor::GetVoxel(uint32 x, uint32 y, uint32 z) const
{
    return m_voxelData[z * M_VOXEL_MESH_SIZE * M_VOXEL_MESH_SIZE + y * M_VOXEL_MESH_SIZE + x];
}

inline void ATestMeshActor::SetVoxel(uint32 x, uint32 y, uint32 z, uint8 val)
{
    m_voxelData[z * M_VOXEL_MESH_SIZE * M_VOXEL_MESH_SIZE + y * M_VOXEL_MESH_SIZE + x] = val;
}
