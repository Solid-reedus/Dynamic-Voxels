#pragma once

#include "CoreMinimal.h"
#include "Voxeleditinglibrary.generated.h"

UCLASS()
class VOXELPROJECT_API UVoxeleditinglibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()


public:
    UFUNCTION(BlueprintCallable, Category = "Voxel Editing")
    static void RemoveVoxelsInSphere(AVoxelBase* Target, const FVector& WorldLocation, float Radius);

	UFUNCTION(BlueprintCallable, Category = "Voxel Editing")
    static void AddVoxelsInSphere(AVoxelBase* Target, const FVector& WorldLocation, float Radius, uint8 BlockType = 1);
	
};
