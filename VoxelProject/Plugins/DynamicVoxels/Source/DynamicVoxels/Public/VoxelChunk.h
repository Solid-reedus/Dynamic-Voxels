#pragma once

#include "CoreMinimal.h"
#include "VoxelBase.h"
#include "VoxelChunk.generated.h"

class AVoxelWorld;

UCLASS()
class DYNAMICVOXELS_API AVoxelChunk : public AVoxelBase
{
    GENERATED_BODY()

public:
    AVoxelChunk();

    void InitializeChunk(uint32 voxelSize, UMaterial* baseMaterial, UTexture2DArray* voxelTextures, AVoxelWorld* world);
    const FIntVector& GetChunkPosition() const { return m_chunkPosition; }
    bool IsChunkUniform() const { return m_isChunkUniform; }

    // Stubbed empty for now, per explicit request - signature matches
    // AVoxelBase's pure virtual (hashmap-of-masks format) so this compiles,
    // but the actual neighbor-forwarding logic isn't locked down yet and
    // isn't implemented here.
    virtual void ApplyVoxelEdit(const TMap<FIntVector, TArray<uint8>>& Edits, uint8 Value) override;

protected:
    virtual void BeginPlay() override;

private:
    FIntVector m_chunkPosition;
    bool m_isChunkUniform = false;

    void GenerateVoxelData(uint32 voxelSize);

    // Builds the single-entry FVoxelGridInstance array RegenerateMesh/
    // CreateCollision expect - a chunk always has exactly one grid, at
    // offset (0,0,0) within its own local voxel-space.
    TArray<FVoxelGridInstance> BuildGridInstances() const;

    // This class's own grid - AVoxelBase no longer provides one, since
    // AVoxelConstruct needs a hashmap of several instead of exactly one.
    UPROPERTY()
    FVoxelGrid Grid;

    UPROPERTY()
    AVoxelWorld* m_world = nullptr;
};