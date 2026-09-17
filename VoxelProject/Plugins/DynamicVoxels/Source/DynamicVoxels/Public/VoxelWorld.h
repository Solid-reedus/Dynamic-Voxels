// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelWorld.generated.h"

UCLASS()
class DYNAMICVOXELS_API AVoxelWorld : public AActor
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTexture2DArray* VoxelTextures = nullptr;

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
    void LoadBaseMaterial();
};
