#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "VoxelCollisionType.h"
#include "VoxelGrid.h"
#include "VoxelBase.generated.h"

UENUM()
enum class EFaceDirection
{
    POSITIVE_X,
    NEGATIVE_X,
    POSITIVE_Y,
    NEGATIVE_Y,
    POSITIVE_Z,
    NEGATIVE_Z
};

/*
    base class for voxel-based actors, it has all basic functionality to manage
    a voxel, mesh of the voxel and collision
*/
UCLASS(Abstract)
class DYNAMICVOXELS_API AVoxelBase : public AActor
{
    GENERATED_BODY()

public:
    static constexpr uint32 VOXEL_GRID_SIZE = 32;

    UPROPERTY()
    EVoxelCollisionType CollisionType = EVoxelCollisionType::ThreeGDM;

    UPROPERTY()
    uint32 VoxelSize = 100;

    AVoxelBase();

    UFUNCTION(BlueprintCallable, Category = "Voxel Editing")
    virtual FIntVector WorldToGridPosition(const FVector& WorldPos) { return FIntVector(); }

    UFUNCTION(BlueprintCallable, Category = "Voxel Editing")
    virtual void ApplyVoxelEdit(const TMap<FIntVector, uint8>& Edits) {}

    UFUNCTION(BlueprintCallable, Category = "Voxel Editing")
    void ApplyVoxelEditArray(const TArray<FIntVector>& Positions, uint8 VoxelType);

protected:
    UPROPERTY()
    UProceduralMeshComponent* ProceduralMesh = nullptr;

    UPROPERTY()
    TMap<FIntVector, FVoxelGrid> SubGrids;

    virtual void BeginPlay() override;

    void RegenerateMesh();
    //void CreateMaterialInstance(const FVoxelGrid& VoxelGrid, UMaterial* baseMaterial, UTexture2DArray* voxelTextures);
    void CreateCollision();

    /*UPROPERTY()
    UMaterialInstanceDynamic* MaterialInstance = nullptr;

    UPROPERTY()
    UTexture2D* VoxelDataTexture = nullptr;*/

private:
    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<int32> Indices;
    int32 IndexCount = 0;

    TArray<uint64> TempPlaneBitData;

    void CreateMeshPlaneGreedy1D(const TPair<FIntVector, FVoxelGrid>& SubGrid, EFaceDirection FaceDirection);
    void CreateMeshPlaneGreedy(const TPair<FIntVector, FVoxelGrid>& SubGrid, EFaceDirection FaceDirection);
    inline void AddFace(const FVector& Position, EFaceDirection FaceDirection, const FVector2D& Scale);
};