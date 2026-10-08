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
    static constexpr uint32 VOXEL_CHUNK_SIZE = 32;

    AVoxelBase();

    UPROPERTY(VisibleAnywhere, Category = "Voxel")
    EVoxelCollisionType CollisionType = EVoxelCollisionType::ThreeGDM;

    // migh need to make this a normal property, as all voxels actors need to be same size
    UPROPERTY(VisibleAnywhere, Category = "Voxel")
    uint32 VoxelSize = 100;

    virtual void ApplyVoxelEdit(const TMap<FIntVector, TArray<uint8>>& Edits, uint8 Value) {}

protected:
    virtual void BeginPlay() override;

    /*
        a single voxel grid and its position, so one actor can be made up
        of more than one grid instead of just one
    */
    struct FVoxelGridInstance
    {
        FIntVector Offset;
        const FVoxelGrid* Grid = nullptr;
    };

    void RegenerateMesh(const TArray<FVoxelGridInstance>& Grids);
    void CreateMaterialInstance(const FVoxelGrid& VoxelGrid, UMaterial* baseMaterial, UTexture2DArray* voxelTextures);
    void CreateCollision(const TArray<FVoxelGridInstance>& Grids);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel")
    UProceduralMeshComponent* ProceduralMesh = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel")
    UMaterialInstanceDynamic* MaterialInstance = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel")
    UTexture2D* VoxelDataTexture = nullptr;

private:
    void CreateMeshPlaneGreedy1D(const FVoxelGridInstance& GridInstance, EFaceDirection FaceDirection);
    void CreateMeshPlaneGreedy(const FVoxelGridInstance& GridInstance, EFaceDirection FaceDirection);
    inline void AddFace(const FVector& Position, EFaceDirection FaceDirection, const FVector2D& Scale);

    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<int32> Indices;
    int32 IndexCount = 0;

    TArray<uint64> TempPlaneBitData;

    bool m_materialDataCreated = false;
};