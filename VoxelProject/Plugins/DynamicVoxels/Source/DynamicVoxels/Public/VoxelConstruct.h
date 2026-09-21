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
	// Sets default values for this actor's properties
	AVoxelConstruct();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	TArray<uint8> m_voxelData;


};
