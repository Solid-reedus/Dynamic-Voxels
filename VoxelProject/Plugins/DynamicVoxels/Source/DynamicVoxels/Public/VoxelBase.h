#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "VoxelCollisionType.h"
#include "VoxelGrid.h"
#include "VoxelBase.generated.h"

// Shared base for anything backed by a voxel grid + procedural mesh +
// compound collision. Holds only what's genuinely identical across every
// voxel-based actor (storage, mesh building, collision generation); says
// nothing about what a "voxel changed" reaction means - that's
// OnVoxelsChanged(), pure virtual, implemented completely differently by
// each subclass (a construct: flood-fill and split into pieces; a chunk:
// whatever terrain-editing reaction makes sense, probably nothing like
// splitting).
//
// Deliberately does NOT own anything chunk-specific: fixed chunk size, the
// +1-per-side neighbor-sampling padding, and "is this chunk uniform" all
// exist because streaming needs a grid of fixed-size tiles - only a chunk
// class cares about that. Grid.GridSize is a plain per-instance runtime
// value so a construct can be whatever size its actual content needs.
UCLASS(Abstract)
class DYNAMICVOXELS_API AVoxelBase : public AActor
{
    GENERATED_BODY()

public:
    AVoxelBase();

    // EditAnywhere/BlueprintReadWrite so this is tunable per instance or per
    // Blueprint subclass without either subclass needing to redeclare it.
    UPROPERTY(EditAnywhere, Category = "Voxel")
    EVoxelCollisionType CollisionType = EVoxelCollisionType::ThreeGDM;

    // World-space size of one voxel. uint32 to match InitializeChunk's
    // existing parameter type elsewhere in the project.
    UPROPERTY(EditAnywhere, Category = "Voxel")
    uint32 VoxelSize = 100;

    // Thin pass-throughs to Grid's own methods - kept here because a plain
    // USTRUCT (Grid's type) can't expose BlueprintCallable methods of its
    // own; Blueprint has no way to call Grid.GetVoxel(...) directly, so this
    // is the actual callable surface. The index math and bounds-checking
    // itself lives once, in FVoxelGrid (voxel_grid.h) - not duplicated here.
    //UFUNCTION(BlueprintCallable, Category = "Voxel")
    uint8 GetVoxel(const FUintVector3& Position) const { return Grid.GetVoxel(Position); }

    // Does NOT regenerate mesh/collision - for bulk edits (generating a
    // whole chunk, carving out a damage sphere), call this in a loop and
    // then call RegenerateMesh()/CreateCollision() once when done, exactly
    // like InitializeChunk's existing GenerateVoxelData -> GenerateVoxelMesh
    // sequencing already does.
    //UFUNCTION(BlueprintCallable, Category = "Voxel")
    void SetVoxel(const FUintVector3& Position, uint8 Value) { Grid.SetVoxel(Position, Value); }

    //UFUNCTION(BlueprintCallable, Category = "Voxel")
    int32 GetGridSize() const { return (int32)Grid.GridSize; }

    // Resizes Grid to GridSize^3 and zeroes it. Call before any SetVoxel
    // calls when (re)initializing an actor's grid from scratch.
    //UFUNCTION(BlueprintCallable, Category = "Voxel")
    void InitializeGrid(uint32 InGridSize) { Grid.Initialize(InGridSize); }

protected:
    virtual void BeginPlay() override;

    // Rebuilds the render mesh from the current voxel data via face-culling
    // (a face is only emitted where the neighboring voxel is empty).
    // Identical for every subclass - geometry doesn't care what a voxel's
    // block-type value means, only whether it's zero or not.
    void RegenerateMesh();

    // Rebuilds the compound collision using CollisionType. Identical for
    // every subclass - the Octree/BSP/3GDM/MaxIS/IS(1)/ConvexHull switch.
    void CreateCollision();

    // Called after voxel data has changed and you've decided to regenerate
    // (SetVoxel does NOT call this automatically - see its comment above).
    // No default behavior - every concrete subclass MUST decide what "voxels
    // changed" means for it.
    virtual void OnVoxelsChanged() PURE_VIRTUAL(AVoxelBase::OnVoxelsChanged, );

    // Direct access for subclasses doing bulk work (terrain generation, a
    // flood-fill split) where going through the SetVoxel/GetVoxel wrappers
    // above one call at a time isn't the point - those wrappers exist for
    // external/Blueprint callers, not as the only way in for derived C++.
    UPROPERTY()
    FVoxelGrid Grid;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel")
    UProceduralMeshComponent* ProceduralMesh = nullptr;

    // Not every subclass will need these (a plain-colored construct might
    // not), but they're broadly useful enough - matching the texture-array
    // material approach already used for chunk terrain - to live here rather
    // than being duplicated per subclass. Fine to leave unused otherwise.
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
};