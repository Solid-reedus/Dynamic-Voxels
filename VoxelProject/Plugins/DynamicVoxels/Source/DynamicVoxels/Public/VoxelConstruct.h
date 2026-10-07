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
    void InitVoxel(EVoxelCollisionType NewCollisionType, bool isStatic);

    // init voxel construct based on grid data
    void InitVoxel(EVoxelCollisionType NewCollisionType, const TMap<FIntVector, FVoxelGrid>& InSubGrids, bool isStatic);

    virtual void ApplyVoxelEdit(const TMap<FIntVector, TArray<uint8>>& Edits, uint8 Value) override;


	UFUNCTION(BlueprintCallable, Category = "Voxel Construct")
    void AttachSelfTo(AVoxelConstruct* Target, const FVector& TargetWorldHitLocation, const FVector& SelfWorldHitLocation);

    void MergeVoxelData(const TMap<FIntVector, FVoxelGrid>& OtherSubGrids, const FIntVector& OriginOffset);


protected:  
    virtual void OnConstruction(const FTransform& Transform) override;


private:


    // IMPORTANT this is testing code, it will generate a random voxel grid for the constructs
    void GenerateVoxelData();

     void OnVoxelsChanged();

    TArray<FVoxelGridInstance> BuildGridInstances() const;

    TMap<FIntVector, FVoxelGrid> SubGrids;

    bool m_static = false;
};