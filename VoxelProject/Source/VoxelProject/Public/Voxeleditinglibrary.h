#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "VoxelEditingLibrary.generated.h"

class AVoxelBase;
class AVoxelConstruct;

UCLASS()
class UVoxeleditinglibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Voxel Editing")
    static void AttachSelectedConstructTo(AVoxelConstruct* SelectedConstruct,
                                          AVoxelConstruct* Target,
                                          const FVector& TargetWorldHitLocation,
                                          const FVector& SelfWorldHitLocation);

    UFUNCTION(BlueprintCallable, Category = "Voxel Editing")
    static void RemoveVoxelsInSphere(AVoxelBase* Target, const FVector& WorldLocation, float Radius);

    UFUNCTION(BlueprintCallable, Category = "Voxel Editing")
    static void AddVoxelsInSphere(AVoxelBase* Target, const FVector& WorldLocation, float Radius, uint8 BlockType = 1);

private:
    /*
        shared by both functions above, builds the sphere as edit masks
        and applies them with the given value
    */
    static void ApplySphereEdit(AVoxelBase* Target, const FVector& WorldLocation, float Radius, uint8 Value);

    // static TWeakObjectPtr<AVoxelConstruct> SelectedConstruct;
};