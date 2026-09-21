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
    // Kept as static constexpr, not UPROPERTY - AVoxelWorld::CreateNewChunk
    // and WorldToChunkPosition both read AVoxelChunk::VOXEL_CHUNK_SIZE
    // statically, with no chunk instance in hand, so this can't become a
    // per-instance property without also changing those call sites.
    static constexpr uint32 VOXEL_CHUNK_SIZE = 32;
    static constexpr uint32 VOXEL_DATA_SIZE = VOXEL_CHUNK_SIZE + 2;

    // Sets default values for this actor's properties
    AVoxelChunk();

    void InitializeChunk(uint32 voxelSize, UMaterial* baseMaterial, UTexture2DArray* voxelTextures);

    // Get chunk position in chunk space
    const FIntVector& GetChunkPosition() const { return m_chunkPosition; }
    // Returns true if the chunk is fully solid or fully empty
    bool IsChunkUniform() const { return m_isChunkUniform; }

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    // No reaction to voxel edits yet (unlike AVoxelConstruct's flood-fill
    // split) - empty for now, add real behavior here once digging/placing
    // needs to do something.
    virtual void OnVoxelsChanged() override {}

private:
    FIntVector m_chunkPosition;
    bool m_isChunkUniform = false;
    bool m_materialDataCreated = false;

    void GenerateVoxelData(uint32 voxelSize);
    void CreateMaterialInstance(uint32 voxelSize, UMaterial* baseMaterial, UTexture2DArray* voxelTextures);
};