// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "TestMeshActor.generated.h"

UCLASS()
class DYNAMICVOXELS_API ATestMeshActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ATestMeshActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UProceduralMeshComponent* ProceduralMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NoiseScale = 16.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NoiseCutoff = -0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UMaterial* MeshMaterial = nullptr;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

private:
        static constexpr uint32 M_VOXEL_MESH_SIZE = 128;

        TArray<uint8> m_voxelData;

        // Mesh Data
        TArray<FVector> m_vertices;
        TArray<FVector> m_normals;
        TArray<FVector2D> m_uvs;

        TArray<int32> m_indices;
        int32 m_indexCount = 0;

        void GenerateVoxelData();
        void GenerateVoxelMesh();

        inline void AddPlane(const FVector& position,
                             const FVector& v0,
                             const FVector& v1,
                             const FVector& v2,
                             const FVector& v3,
                             const FVector& normal);

        inline uint8 GetVoxel(uint32 x, uint32 y, uint32 z) const;
        inline void SetVoxel(uint32 x, uint32 y, uint32 z, uint8 val);
};
