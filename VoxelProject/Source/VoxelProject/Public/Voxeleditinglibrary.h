// This file belongs in your GAME project's Source folder (e.g.
// Source/YourGameName/), NOT in Plugins/DynamicVoxels/Source/. It's
// gameplay-specific (a spherical removal shape is a design choice, not
// generic voxel infrastructure) and depends on AVoxelBase's public API
// (VoxelSize, ApplyVoxelEdit) rather than being part of it.
//
// No _API export macro - a class only used within its own module doesn't
// need one. Add your game module's own _API macro only if some other
// module also needs to reference this class directly.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VoxelEditingLibrary.generated.h"

class AVoxelBase;

UCLASS()
class UVoxeleditinglibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Given a voxel actor you've already identified as hit (by your own
    // line trace + cast in Blueprint, or however else), works out which
    // voxels fall within Radius world units of WorldLocation and clears
    // them via Target->ApplyVoxelEdit(). Does nothing if Target is null.
    //
    // This is deliberately the ONLY thing this function does - finding
    // what got hit is left entirely to the caller (native Line Trace By
    // Channel + Cast To VoxelBase nodes in Blueprint cover that fine on
    // their own), so this stays a small, single-purpose piece of gameplay
    // logic rather than a black box that also does tracing internally.
    UFUNCTION(BlueprintCallable, Category = "Voxel Editing")
    static void RemoveVoxelsInSphere(AVoxelBase* Target, const FVector& WorldLocation, float Radius);

    // The inverse of RemoveVoxelsInSphere: fills every voxel within Radius
    // world units of WorldLocation with BlockType instead of clearing it.
    // Overwrites unconditionally, including voxels that already held a
    // different material.
    UFUNCTION(BlueprintCallable, Category = "Voxel Editing")
    static void AddVoxelsInSphere(AVoxelBase* Target, const FVector& WorldLocation, float Radius, uint8 BlockType = 1);

private:
    // Shared by both functions above - builds the sphere's affected
    // positions as a hashmap of 32^3 edit masks (AVoxelBase::ApplyVoxelEdit's
    // format) and applies it with the given Value. The only difference
    // between Remove and Add is this Value (0 vs BlockType), so this avoids
    // duplicating the mask-building logic twice.
    static void ApplySphereEdit(AVoxelBase* Target, const FVector& WorldLocation, float Radius, uint8 Value);
};