// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "VoxelChunk.generated.h"

UCLASS()
class DYNAMICVOXELS_API AVoxelChunk : public AActor
{
    GENERATED_BODY()

public:
    // TODO(David): Make these UPROPERTY instead
    static constexpr uint32 VOXEL_CHUNK_SIZE = 32;
    static constexpr uint32 VOXEL_DATA_SIZE = VOXEL_CHUNK_SIZE + 2;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UProceduralMeshComponent* ProceduralMesh = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UMaterialInstanceDynamic* MaterialInstance = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UTexture2D* VoxelDataTexture = nullptr;

    // Sets default values for this actor's properties
    AVoxelChunk();

    uint8 GetVoxel(const FUint32Vector3& position) const;
    void SetVoxel(const FUint32Vector3& position, uint8 value);

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

private:
    friend class AVoxelWorld;

    TArray<uint8> m_voxelData;

    // Mesh Data
    TArray<FVector> m_vertices;
    TArray<FVector> m_normals;
    TArray<FVector2D> m_uvs;

    TArray<int32> m_indices;
    int32 m_indexCount = 0;

    void GenerateVoxelData(uint32 voxelSize);
    void GenerateVoxelMesh(uint32 voxelSize, UMaterial* baseMaterial, UTexture2DArray* voxelTextures);

    inline void AddPlane(const FVector& position,
                         const FVector& v0,
                         const FVector& v1,
                         const FVector& v2,
                         const FVector& v3,
                         const FVector& normal);
};
