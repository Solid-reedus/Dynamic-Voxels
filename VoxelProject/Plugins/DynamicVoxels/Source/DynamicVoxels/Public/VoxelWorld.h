// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelWorld.generated.h"

class AVoxelChunk;
class AVoxelConstruct;

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
    int32 VerticalLoadDistance = 4;

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

    //UPROPERTY()
    TQueue<FIntVector3> m_chunkAddQueue;

    FIntVector2 m_prevPawnChunkPosition;

    UPROPERTY()
    TMap<FIntVector, AVoxelChunk*> m_voxelChunkInstances;
    UPROPERTY()
    TArray<AVoxelConstruct*> m_constructInstances; // new stuff

    void CreateNewChunk(const FIntVector3& chunkPosition);

    void LoadBaseMaterial();

    // Converts world space position to chunk space position
    FIntVector3 WorldToChunkPosition(const FVector& position) const;
    // Gets the grid distance beween two points
    static int GetGridDistance(const FIntVector2& p1, const FIntVector2& p2);
};