// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelWorld.generated.h"

class AVoxelChunk;

UCLASS()
class DYNAMICVOXELS_API AVoxelWorld : public AActor
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere)
    UTexture2DArray* VoxelTextures = nullptr;

    UPROPERTY(EditAnywhere)
    uint32 VoxelSize = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 HorizontalLoadDistance = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 VerticalLoadDistance = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool FixedLoadHeight = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ChunkUpdateRate = 2;

    UPROPERTY(VisibleAnywhere)
    UMaterial* BaseMaterial = nullptr;

    // Sets default values for this actor's properties
    AVoxelWorld();

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    // Called every frame
    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY()
    TArray<AVoxelChunk*> m_voxelChunkInstances;

    TQueue<FIntVector3> m_chunkAddQueue;

    FIntVector3 m_prevPawnChunkPosition = {};

    void AddVoxelChunkInstance(const FIntVector3& chunkPosition);

    inline FIntVector3 WorldToChunkPosition(const FVector& position) const;

    void LoadBaseMaterial();
};