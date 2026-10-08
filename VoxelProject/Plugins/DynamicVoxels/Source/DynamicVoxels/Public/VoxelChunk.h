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

    const FIntVector& GetChunkPosition() const { return m_chunkPosition; }
    bool IsChunkUniform() const { return m_isChunkUniform; }

    virtual void ApplyVoxelEdit(const TMap<FIntVector, uint8>& Edits) override;
    virtual FIntVector WorldToGridPosition(const FVector& Position) override;

protected:
    friend class AVoxelWorld;

    void InitializeChunk(uint32 voxelSize,
                         UMaterial* baseMaterial,
                         UTexture2DArray* voxelTextures,
                         AVoxelWorld* world,
                         FIntVector chunkPos);


    void ApplyVoxelEditLocal(const TMap<FIntVector, uint8>& Edits);

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

    // Chunk position in chunk space
    FIntVector m_chunkPos;
};