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

    UPROPERTY(VisibleAnywhere, Category = "Voxel")
    uint32 VoxelSize = 100;

    const FVoxelGrid& GetGrid() const { return Grid; }

    //uint8 GetVoxel(const FUintVector3& Position) const { return Grid.GetVoxel(Position); }
    //void SetVoxel(const FUintVector3& Position, uint8 Value) { Grid.SetVoxel(Position, Value); }
    int32 GetGridSize() const { return static_cast<int32>(Grid.GridSize); }

	//virtual void ApplyVoxelEdit(const TArray<FIntVector>& Positions, uint8 Value, uint32 Border = 0);
    void ApplyVoxelEdit(const TArray<FUintVector3>& Positions, uint8 Value);

protected:
    virtual void BeginPlay() override;

    // make a mesh out of a voxel grid
    void RegenerateMesh();
    void CreateMaterialInstance(UMaterial* baseMaterial, UTexture2DArray* voxelTextures);

    // SHOULD be called after the mesh is generated, to create collision for the voxel
    void CreateCollision();

    // custom override function that is called when the voxel data is changed, each class that inherits from this class should
    // implement this function to handle the voxel data change
    virtual void OnVoxelsChanged() PURE_VIRTUAL(AVoxelBase::OnVoxelsChanged, );

    UPROPERTY()
    FVoxelGrid Grid;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel")
    UProceduralMeshComponent* ProceduralMesh = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel")
    UMaterialInstanceDynamic* MaterialInstance = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel")
    UTexture2D* VoxelDataTexture = nullptr;

private:

    void CreateMeshPlaneGreedy1D(EFaceDirection FaceDirection);
    void CreateMeshPlaneGreedy(EFaceDirection FaceDirection);
    inline void AddFace(const FVector& Position, EFaceDirection FaceDirection, const FVector2D& Scale);

    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<int32> Indices;
    int32 IndexCount = 0;

    TArray<uint64> TempPlaneBitData;

    bool m_materialDataCreated = false;
};