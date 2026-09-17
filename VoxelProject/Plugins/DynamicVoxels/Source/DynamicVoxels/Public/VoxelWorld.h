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
    int32 ChunkLoadDistance = 5;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UMaterial* BaseMaterial = nullptr;

    // Sets default values for this actor's properties
    AVoxelWorld();

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    // Called every frame
    virtual void Tick(float DeltaTime) override;

private:
    TArray<AVoxelChunk*> m_voxelChunks;

    TArray<FIntVector3> m_chunkAddQueue;

    void LoadBaseMaterial();
};
