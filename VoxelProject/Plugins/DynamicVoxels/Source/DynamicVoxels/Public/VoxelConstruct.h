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

    // Loads existing grid data (e.g. a piece split off from another
    // construct) instead of generating new content.
    //UFUNCTION(BlueprintCallable, Category = "Voxel Construct")
    void InitVoxel(EVoxelCollisionType NewCollisionType, const FVoxelGrid& InGrid, bool bIsWorld);

protected:
    //virtual void BeginPlay() override;

    // Live-preview while editing in the level - unlike AVoxelChunk (spawned
    // procedurally, never hand-edited), a construct is the kind of thing
    // you'd actually drag around and tweak in the editor, so it's worth
    // rebuilding on every edit here specifically. If CollisionType is
    // MaxIS/IS(1) and the shape is irregular, this is exactly the case
    // EstimateConcaveEdgeDensity's warning (in VoxelBase::CreateCollision)
    // is there to catch.
    virtual void OnConstruction(const FTransform& Transform) override;

    // No reaction to voxel edits yet (matches AVoxelChunk's identical
    // placeholder) - this is where flood-fill-and-split logic eventually
    // goes once damage/destruction is implemented.
    virtual void OnVoxelsChanged() override {}

private:

    void GenerateVoxelData();

    // True for a static/world-placed construct (no physics simulation);
    // false for a dynamic prop that should fall and react to forces.
    //UPROPERTY(Category = "Voxel Construct")
    UPROPERTY()
    bool bWorld = false;
};