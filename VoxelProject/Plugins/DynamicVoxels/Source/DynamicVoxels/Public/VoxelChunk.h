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

    void InitializeChunk(uint32 voxelSize,
                         UMaterial* baseMaterial,
                         UTexture2DArray* voxelTextures,
                         AVoxelWorld* world,
                         FIntVector chunkPos);
    const FIntVector& GetChunkPosition() const { return m_chunkPosition; }
    bool IsChunkUniform() const { return m_isChunkUniform; }

    virtual void ApplyVoxelEdit(const TMap<FIntVector, TArray<uint8>>& Edits, uint8 Value) override;

    void ApplyVoxelEditLocal(const TArray<uint8>& Mask, uint8 Value);

protected:
    virtual void BeginPlay() override;

private:
    FIntVector m_chunkPosition;
    bool m_isChunkUniform = false;

    void GenerateVoxelData(uint32 voxelSize);

    TArray<FVoxelGridInstance> BuildGridInstances() const;

    UPROPERTY()
    FVoxelGrid Grid;

    UPROPERTY()
    AVoxelWorld* m_world = nullptr;

    UPROPERTY()
    FIntVector m_chunkPos;
};