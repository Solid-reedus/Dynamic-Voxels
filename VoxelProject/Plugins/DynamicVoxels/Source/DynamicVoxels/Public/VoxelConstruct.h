
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


    virtual void ApplyVoxelEdit(const TArray<FUintVector3>& Positions, uint8 Value) override;   

private:

    //
    void OnVoxelsChanged();
    
    // IMPORTANT this is testing code, it will generate a random voxel grid for the constructs
    void GenerateVoxelData();

    UPROPERTY()
    bool m_static = false;
};