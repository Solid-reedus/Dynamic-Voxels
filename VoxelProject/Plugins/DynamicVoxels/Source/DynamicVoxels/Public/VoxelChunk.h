
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

    virtual void ApplyVoxelEdit(const TArray<FUintVector3>& Positions, uint8 Value) override;   

protected:

    virtual void BeginPlay() override;


private:
    FIntVector m_chunkPosition;
    bool m_isChunkUniform = false;

    void GenerateVoxelData(uint32 voxelSize);


    UPROPERTY()
    AVoxelWorld* m_world = nullptr;


};