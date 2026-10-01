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

    // Fills in a test grid and rebuilds mesh/collision. bIsWorld controls
    // whether this construct simulates physics (false - a dynamic prop that
    // falls and reacts to forces) or behaves as static world geometry (true
    // - no physics simulation, matching how a chunk behaves).
    //UFUNCTION(BlueprintCallable, Category = "Voxel Construct")
    void InitVoxel(EVoxelCollisionType NewCollisionType, bool bIsWorld);

    // Loads existing sub-grid data (e.g. a piece split off from another
    // construct) instead of generating new content. Takes the whole hashmap
    // now, not a single FVoxelGrid, since a split-off piece can itself span
    // more than one sub-grid.
    //UFUNCTION(BlueprintCallable, Category = "Voxel Construct")
    void InitVoxel(EVoxelCollisionType NewCollisionType, const TMap<FIntVector, FVoxelGrid>& InSubGrids, bool bIsWorld);

    // Edits is a hashmap of masks keyed by sub-grid coordinate (see
    // AVoxelBase::ApplyVoxelEdit's comment for the mask format). For each
    // entry, finds or creates that sub-grid in SubGrids - creating one for a
    // coordinate that doesn't exist yet IS how a construct grows: no resize,
    // no recenter, no copying old data into a bigger array, just a new
    // hashmap entry. Then rebuilds mesh/collision from every sub-grid
    // currently in SubGrids via AVoxelBase's shared multi-grid path.
    virtual void ApplyVoxelEdit(const TMap<FIntVector, TArray<uint8>>& Edits, uint8 Value) override;

protected:  
    // Live-preview while editing in the level - unlike AVoxelChunk (spawned
    // procedurally, never hand-edited), a construct is the kind of thing
    // you'd actually drag around and tweak in the editor, so it's worth
    // rebuilding on every edit here specifically. If CollisionType is
    // MaxIS/IS(1) and the shape is irregular, this is exactly the case
    // EstimateConcaveEdgeDensity's warning (in VoxelBase::CreateCollision)
    // is there to catch.
    virtual void OnConstruction(const FTransform& Transform) override;


private:
    void GenerateVoxelData();

     void OnVoxelsChanged();

    // Builds the AVoxelBase::FVoxelGridInstance array RegenerateMesh/
    // CreateCollision expect, from whatever's currently in SubGrids.
    TArray<FVoxelGridInstance> BuildGridInstances() const;

    // The construct's actual voxel storage - a sparse collection of
    // 32-voxel cubes, keyed by sub-grid coordinate (NOT world position -
    // sub-grid (1,0,0) sits at local voxel offset (32,0,0), etc). Replaces
    // a single FVoxelGrid member (removed from AVoxelBase itself)
    // specifically so a construct isn't forced into one fixed-size grid.
    TMap<FIntVector, FVoxelGrid> SubGrids;

    // True for a static/world-placed construct (no physics simulation);
    // false for a dynamic prop that should fall and react to forces.
    //UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel Construct")
    bool bWorld = false;
};