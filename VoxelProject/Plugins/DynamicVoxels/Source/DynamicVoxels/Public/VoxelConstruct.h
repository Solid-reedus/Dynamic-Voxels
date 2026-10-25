// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelBase.h"
#include "VoxelConstruct.generated.h"

UCLASS()
class DYNAMICVOXELS_API AVoxelConstruct : public AVoxelBase
{
    GENERATED_BODY()

public:
    AVoxelConstruct();

    // init voxel construct randomly
    void InitVoxel(EVoxelCollisionType NewCollisionType,
                   bool isStatic,
                   uint32 InVoxelSize,
                   UMaterial* InBaseMaterial,
                   UTexture2DArray* InVoxelTextures);

    // init voxel construct based on grid data
    void InitVoxel(EVoxelCollisionType NewCollisionType,
                   const TMap<FIntVector, FVoxelGrid>& InSubGrids,
                   bool isStatic,
                   int32 InVoxelSize,
                   UMaterial* InBaseMaterial,
                   UTexture2DArray* InVoxelTextures);

    virtual void ApplyVoxelEdit(const TMap<FIntVector, uint8>& Edits) override;
    virtual FIntVector WorldToGridPosition(const FVector& Position) override;

    UFUNCTION(BlueprintCallable, Category = "Voxel Construct")
    void AttachSelfTo(AVoxelConstruct* Target, const FVector& TargetWorldHitLocation, const FVector& SelfWorldHitLocation);

    void MergeVoxelData(const TMap<FIntVector, FVoxelGrid>& OtherSubGrids, const FIntVector& OriginOffset);

protected:
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    // IMPORTANT this is testing code, it will generate a random voxel grid for the constructs
    void GenerateVoxelData();

    void OnVoxelsChanged();

    bool m_static = false;

    // Keeping track of these objects for when new sub grids are created
    UPROPERTY()
    UMaterial* BaseMaterial;

    UPROPERTY()
    UTexture2DArray* VoxelTextures;
};