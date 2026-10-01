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

    // No GetGrid()/GetGridSize() anymore - there's no single Grid on this
    // class to return. AVoxelChunk has its own Grid member (always exactly
    // one, fixed 32-size); AVoxelConstruct has its own hashmap of sub-grids.
    // Anything that needs a size/position (VoxelEditingLibrary's sphere
    // math, for instance) needs reworking against whichever of those two
    // shapes it's actually targeting - deferred, not done here yet.

    // Pure virtual - AVoxelBase has no voxel storage of its own anymore, so
    // it has nothing to apply an edit to. Edits is a hashmap of edit masks:
    // key is a sub-grid coordinate (which 32-voxel cube this mask targets -
    // for a chunk, a world chunk coordinate; for a construct, one of its own
    // sub-grid keys), value is a 32*32*32-entry mask where a nonzero byte
    // means "set this local voxel to Value". Value is one shared value for
    // the whole call, same as before - the mask says WHERE, Value says WHAT.
    virtual void ApplyVoxelEdit(const TMap<FIntVector, TArray<uint8>>& Edits, uint8 Value) {}
    //virtual void ApplyVoxelEdit(const TMap<FIntVector, TArray<uint8>>& Edits, uint8 Value) = 0;

protected:
    virtual void BeginPlay() override;

    // One voxel grid, positioned within an actor's own local voxel-space.
    // Offset is in VOXEL units (not world units) - a sub-grid at hashmap key
    // (1,0,0) has Offset (32,0,0), so its own local (0,0,0) voxel sits 32
    // voxels over, not at the actor's origin. AVoxelChunk builds a
    // single-entry array (its one Grid, Offset (0,0,0)); AVoxelConstruct
    // builds one entry per sub-grid in its hashmap, at SubGridKey*32. Lets
    // RegenerateMesh/CreateCollision stay agnostic to "is this actor backed
    // by one grid or a hashmap of them" - that distinction lives entirely in
    // how each subclass builds this array, not in these shared functions.
    struct FVoxelGridInstance
    {
        FIntVector Offset;
        const FVoxelGrid* Grid = nullptr;
    };

    // Rebuilds the render mesh from one or more grid instances. The actual
    // per-voxel meshing algorithm (CreateMeshPlaneGreedy /
    // CreateMeshPlaneGreedy1D / AddFace) is whatever it already is - this
    // just means it now runs once per grid instance, each offset into the
    // right place, accumulating into the same shared Vertices/Normals/
    // Indices before one combined mesh section is created at the end.
    void RegenerateMesh(const TArray<FVoxelGridInstance>& Grids);

    // Only AVoxelChunk calls this (one grid, one texture) - takes the grid
    // directly rather than an FVoxelGridInstance, since Offset has no
    // meaning for a texture encoding raw voxel bytes.
    void CreateMaterialInstance(const FVoxelGrid& VoxelGrid, UMaterial* baseMaterial, UTexture2DArray* voxelTextures);

    // Iterates every grid instance, decomposing each independently via
    // DecompVoxel and accumulating all resulting boxes into one shared
    // BodySetup - "jumping from hashmap to hashmap" rather than operating on
    // one contiguous array, since DecompVoxel works on one FVoxelGrid at a
    // time regardless of how many there are.
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