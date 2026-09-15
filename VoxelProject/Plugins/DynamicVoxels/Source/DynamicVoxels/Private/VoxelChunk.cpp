// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelChunk.h"

// Sets default values
AVoxelChunk::AVoxelChunk()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = false;

    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
}

uint8 AVoxelChunk::GetVoxel(const FUint32Vector3& position) const
{
    // Should only run during editor, but im unsure
#if UE_EDITOR
    if (position.X >= VOXEL_DATA_SIZE || position.Y >= VOXEL_DATA_SIZE || position.Z >= VOXEL_DATA_SIZE)
    {
        UE_LOG(LogTemp, Error, TEXT("Attempting to get invalid voxel position!"));
        return 0;
    }
#endif

    return m_voxelData[position.Z * VOXEL_DATA_SIZE * VOXEL_DATA_SIZE + position.Y * VOXEL_DATA_SIZE + position.X];
}

void AVoxelChunk::SetVoxel(const FUint32Vector3& position, uint8 value)
{
    // Should only run during editor, but im unsure
#if UE_EDITOR
    if (position.X >= VOXEL_DATA_SIZE || position.Y >= VOXEL_DATA_SIZE || position.Z >= VOXEL_DATA_SIZE)
    {
        UE_LOG(LogTemp, Error, TEXT("Attempting to set invalid voxel position!"));
        return;
    }
#endif

    m_voxelData[position.Z * VOXEL_DATA_SIZE * VOXEL_DATA_SIZE + position.Y * VOXEL_DATA_SIZE + position.X] = value;
}

// Called when the game starts or when spawned
void AVoxelChunk::BeginPlay()
{
    Super::BeginPlay();

    GenerateVoxelData();
    GenerateVoxelMesh();
}

void AVoxelChunk::GenerateVoxelData()
{
    FVector worldPosition = GetActorLocation();
    FUint32Vector3 relativePosition = FUint32Vector3(static_cast<uint32>(worldPosition.X),
                                                     static_cast<uint32>(worldPosition.Y),
                                                     static_cast<uint32>(worldPosition.Z)) /
                                      VOXEL_SIZE;

    // Initialize voxel data to 0
    m_voxelData.Init(0, VOXEL_DATA_SIZE * VOXEL_DATA_SIZE * VOXEL_DATA_SIZE);

    for (uint32 z = 0; z < VOXEL_DATA_SIZE; z++)
    {
        for (uint32 y = 0; y < VOXEL_DATA_SIZE; y++)
        {
            for (uint32 x = 0; x < VOXEL_DATA_SIZE; x++)
            {
                FUint32Vector3 localPosition = relativePosition + FUint32Vector3(x, y, z);

                float perlinValue = FMath::PerlinNoise3D(FVector(localPosition) / 16.0);

                if (perlinValue > 0.2f) SetVoxel(FUint32Vector3(x, y, z), 1);
            }
        }
    }
}

void AVoxelChunk::GenerateVoxelMesh()
{
    for (uint32 z = 1; z <= VOXEL_CHUNK_SIZE; z++)
    {
        for (uint32 y = 1; y <= VOXEL_CHUNK_SIZE; y++)
        {
            for (uint32 x = 1; x <= VOXEL_CHUNK_SIZE; x++)
            {
                if (GetVoxel(FUint32Vector3(x, y, z)) > 0)
                {
                    FVector voxelPos = FVector(x * VOXEL_SIZE, y * VOXEL_SIZE, z * VOXEL_SIZE);

                    if (GetVoxel(FUint32Vector3(x + 1, y, z)) == 0)
                        AddPlane(voxelPos,
                                 FVector(VOXEL_SIZE, 0.0, 0.0),
                                 FVector(VOXEL_SIZE, VOXEL_SIZE, 0.0),
                                 FVector(VOXEL_SIZE, 0.0, VOXEL_SIZE),
                                 FVector(VOXEL_SIZE, VOXEL_SIZE, VOXEL_SIZE),
                                 FVector(1.0, 0.0, 0.0));
                    if (GetVoxel(FUint32Vector3(x - 1, y, z)) == 0)
                        AddPlane(voxelPos,
                                 FVector(0.0, VOXEL_SIZE, 0.0),
                                 FVector(0.0, 0.0, 0.0),
                                 FVector(0.0, VOXEL_SIZE, VOXEL_SIZE),
                                 FVector(0.0, 0.0, VOXEL_SIZE),
                                 FVector(-1.0, 0.0, 0.0));
                    if (GetVoxel(FUint32Vector3(x, y + 1, z)) == 0)
                        AddPlane(voxelPos,
                                 FVector(VOXEL_SIZE, VOXEL_SIZE, 0.0),
                                 FVector(0.0, VOXEL_SIZE, 0.0),
                                 FVector(VOXEL_SIZE, VOXEL_SIZE, VOXEL_SIZE),
                                 FVector(0.0, VOXEL_SIZE, VOXEL_SIZE),
                                 FVector(0.0, 1.0, 0.0));
                    if (GetVoxel(FUint32Vector3(x, y - 1, z)) == 0)
                        AddPlane(voxelPos,
                                 FVector(0.0, 0.0, 0.0),
                                 FVector(VOXEL_SIZE, 0.0, 0.0),
                                 FVector(0.0, 0.0, VOXEL_SIZE),
                                 FVector(VOXEL_SIZE, 0.0, VOXEL_SIZE),
                                 FVector(0.0, -1.0, 0.0));
                    if (GetVoxel(FUint32Vector3(x, y, z + 1)) == 0)
                        AddPlane(voxelPos,
                                 FVector(0.0, 0.0, VOXEL_SIZE),
                                 FVector(VOXEL_SIZE, 0.0, VOXEL_SIZE),
                                 FVector(0.0, VOXEL_SIZE, VOXEL_SIZE),
                                 FVector(VOXEL_SIZE, VOXEL_SIZE, VOXEL_SIZE),
                                 FVector(0.0, 0.0, 1.0));
                    if (GetVoxel(FUint32Vector3(x, y, z - 1)) == 0)
                        AddPlane(voxelPos,
                                 FVector(VOXEL_SIZE, 0.0, 0.0),
                                 FVector(0.0, 0.0, 0.0),
                                 FVector(VOXEL_SIZE, VOXEL_SIZE, 0.0),
                                 FVector(0.0, VOXEL_SIZE, 0.0),
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
                                                      false);
    }
    else
        UE_LOG(LogTemp, Error, TEXT("ProceduralMesh component missing!"));

    m_vertices.Empty();
    m_normals.Empty();
    m_uvs.Empty();
    m_indices.Empty();
    m_indexCount = 0;
}

inline void AVoxelChunk::AddPlane(const FVector& position,
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