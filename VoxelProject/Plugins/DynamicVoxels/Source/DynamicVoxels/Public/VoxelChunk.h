#pragma once

#include "CoreMinimal.h"
#include "VoxelBase.h"
#include "VoxelChunk.generated.h"

class AVoxelWorld;
class UVoxelGenerationAsset;

UCLASS()
class DYNAMICVOXELS_API AVoxelChunk : public AVoxelBase
{
    GENERATED_BODY()

public:
    // Chunk height in grids
    static constexpr uint32 VOXEL_CHUNK_HEIGHT = 4;

    AVoxelChunk();

    const FIntVector& GetChunkPosition() const { return ChunkPosition; }

    virtual void ApplyVoxelEdit(const TMap<FIntVector, uint8>& Edits) override;
    virtual FIntVector WorldToGridPosition(const FVector& Position) override;

protected:
    friend class AVoxelWorld;

    void InitializeChunk(uint32 InVoxelSize,
                         UMaterial* InBaseMaterial,
                         UTexture2DArray* InVoxelTextures,
                         AVoxelWorld* InVoxelWorld,
                         UVoxelGenerationAsset* GenerationAsset);

    void ApplyVoxelEditLocal(const TMap<FIntVector, uint8>& Edits);

    // Correctly sets a voxel across multiple grids
    void SetVoxelLocal(const FIntVector& VoxelPos, uint8 Value);

    virtual void BeginPlay() override;

private:
    FIntVector ChunkPosition;

    UPROPERTY()
    AVoxelWorld* VoxelWorld = nullptr;

    // TODO: Might be unsafe, need to investigate
    // Store pointers to the subgrids as flat array for faster access
    TArray<FVoxelGrid*> SubGridsArray;

    void GenerateVoxelData(UVoxelGenerationAsset* GenerationAsset);
};