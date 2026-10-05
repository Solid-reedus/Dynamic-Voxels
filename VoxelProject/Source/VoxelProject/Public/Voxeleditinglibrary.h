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
    UFUNCTION(BlueprintCallable, Category = "Voxel Editing")
    static void RemoveVoxelsInSphere(AVoxelBase* Target, const FVector& WorldLocation, float Radius);

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