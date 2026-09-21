// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelBase.h"
#include "VoxelChunk.generated.h"

UCLASS()
class DYNAMICVOXELS_API AVoxelChunk : public AVoxelBase
{
    GENERATED_BODY()

public:
    static constexpr uint32 VOXEL_CHUNK_SIZE = 32;
    static constexpr uint32 VOXEL_DATA_SIZE = VOXEL_CHUNK_SIZE + 2;

    AVoxelChunk();

    void InitializeChunk(uint32 voxelSize, UMaterial* baseMaterial, UTexture2DArray* voxelTextures);
    const FIntVector& GetChunkPosition() const { return m_chunkPosition; }
    bool IsChunkUniform() const { return m_isChunkUniform; }

protected:

    virtual void BeginPlay() override;
    virtual void OnVoxelsChanged() override {}

private:
    FIntVector m_chunkPosition;
    bool m_isChunkUniform = false;
    bool m_materialDataCreated = false;

    void GenerateVoxelData(uint32 voxelSize);
    void CreateMaterialInstance(uint32 voxelSize, UMaterial* baseMaterial, UTexture2DArray* voxelTextures);
};