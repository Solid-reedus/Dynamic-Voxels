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
    void InitVoxel(EVoxelCollisionType NewCollisionType, const FVoxelGrid& InGrid, bool isStatic);

protected:

    virtual void OnConstruction(const FTransform& Transform) override;

    // TODO, implement a proper OnVoxelsChanged for the construct
    virtual void OnVoxelsChanged() override {}

private:

    void GenerateVoxelData();

    UPROPERTY()
    bool m_static = false;
};