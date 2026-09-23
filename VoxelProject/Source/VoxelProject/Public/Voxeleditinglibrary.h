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

	
};
