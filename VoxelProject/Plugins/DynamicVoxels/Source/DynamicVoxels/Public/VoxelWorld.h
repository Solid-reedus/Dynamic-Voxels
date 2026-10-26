// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelGenerationAsset.h"
#include "VoxelWorld.generated.h"

class AVoxelChunk;
class AVoxelConstruct;

UCLASS()
class DYNAMICVOXELS_API AVoxelWorld : public AActor
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere)
    UVoxelGenerationAsset* GenerationAsset = nullptr;

    UPROPERTY(EditAnywhere)
    UTexture2DArray* VoxelTextures = nullptr;

    UPROPERTY(EditAnywhere)
    uint32 VoxelSize = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 HorizontalLoadDistance = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ChunkUpdateRate = 1;

    UPROPERTY(VisibleAnywhere)
    UMaterial* BaseMaterial = nullptr;

    // Sets default values for this actor's properties
    AVoxelWorld();

    UFUNCTION(BlueprintCallable, Category = "Voxel World")
    AVoxelChunk* GetChunkAt(const FIntVector& ChunkPosition) const;

    void ApplyVoxelEdit(const TMap<FIntVector, uint8>& Edits);

    // Converts world space position to chunk space position
    FIntVector3 WorldToChunkPosition(const FVector& position) const;
    // Converts voxel grid space position to chunk space position
    static FIntVector3 GridToChunkPosition(const FIntVector& position);

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    // Called every frame
    virtual void Tick(float DeltaTime) override;

private:
    TQueue<FIntVector3> m_chunkAddQueue;
    FIntVector2 m_prevPawnChunkPosition;

    UPROPERTY()
    TMap<FIntVector, AVoxelChunk*> m_voxelChunkInstances;
    UPROPERTY()
    TArray<AVoxelConstruct*> m_constructInstances;

    void CreateNewChunk(const FIntVector3& chunkPosition);

    void LoadBaseMaterial();

    // Gets the grid distance beween two points
    static int GetGridDistance(const FIntVector2& p1, const FIntVector2& p2);
};