#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "VoxelCollisionType.h"
#include "VoxelGrid.h"
#include "VoxelBase.generated.h"


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
    static constexpr uint32 VOXEL_GRID_SIZE = VOXEL_CHUNK_SIZE + 2;

    AVoxelBase();

    UPROPERTY(VisibleAnywhere, Category = "Voxel")
    EVoxelCollisionType CollisionType = EVoxelCollisionType::ThreeGDM;

    UPROPERTY(VisibleAnywhere, Category = "Voxel")
    uint32 VoxelSize = 100;

    const FVoxelGrid& GetGrid() const { return Grid; }

    uint8 GetVoxel(const FUintVector3& Position) const { return Grid.GetVoxel(Position); }
    void SetVoxel(const FUintVector3& Position, uint8 Value) { Grid.SetVoxel(Position, Value); }
    int32 GetGridSize() const { return static_cast<int32>(Grid.GridSize); }

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

    inline void AddPlane(const FVector& Position,
                         const FVector& V0,
                         const FVector& V1,
                         const FVector& V2,
                         const FVector& V3,
                         const FVector& Normal);

    TArray<FVector> m_vertices;
    TArray<FVector> m_normals;
    TArray<FVector2D> m_uvs;
    TArray<int32> m_indices;
    int32 m_indexCount = 0;

    bool m_materialDataCreated = false;
};