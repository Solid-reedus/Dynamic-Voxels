
#include "VoxelBase.h"
#include "VoxelCollisionDecomposition.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/ConvexElem.h"

AVoxelBase::AVoxelBase()
{
    PrimaryActorTick.bCanEverTick = false;

    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
    RootComponent = ProceduralMesh;

    ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    ProceduralMesh->SetCollisionObjectType(ECollisionChannel::ECC_PhysicsBody);
    ProceduralMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
    ProceduralMesh->bUseComplexAsSimpleCollision = false;
    ProceduralMesh->SetSimulatePhysics(false);
}

void AVoxelBase::BeginPlay() { Super::BeginPlay(); }

void AVoxelBase::RegenerateMesh()
{
    m_vertices.Empty();
    m_normals.Empty();
    m_uvs.Empty();
    m_indices.Empty();
    m_indexCount = 0;

    if (Grid.Data.IsEmpty())
    {
        return;
    }

    const uint32 N = Grid.GridSize;

    for (uint32 z = 0; z < N; z++)
    {
        for (uint32 y = 0; y < N; y++)
        {
            for (uint32 x = 0; x < N; x++)
            {
                if (Grid.GetVoxel(FUintVector3(x, y, z)) == 0) continue;

                FVector voxelPos = FVector(x * VoxelSize, y * VoxelSize, z * VoxelSize);

                if (Grid.GetVoxel(FUintVector3(x + 1, y, z)) == 0)
                    AddPlane(voxelPos,
                             FVector(VoxelSize, 0.0, 0.0),
                             FVector(VoxelSize, VoxelSize, 0.0),
                             FVector(VoxelSize, 0.0, VoxelSize),
                             FVector(VoxelSize, VoxelSize, VoxelSize),
                             FVector(1.0, 0.0, 0.0));
                if (Grid.GetVoxel(FUintVector3(x - 1, y, z)) == 0)
                    AddPlane(voxelPos,
                             FVector(0.0, VoxelSize, 0.0),
                             FVector(0.0, 0.0, 0.0),
                             FVector(0.0, VoxelSize, VoxelSize),
                             FVector(0.0, 0.0, VoxelSize),
                             FVector(-1.0, 0.0, 0.0));
                if (Grid.GetVoxel(FUintVector3(x, y + 1, z)) == 0)
                    AddPlane(voxelPos,
                             FVector(VoxelSize, VoxelSize, 0.0),
                             FVector(0.0, VoxelSize, 0.0),
                             FVector(VoxelSize, VoxelSize, VoxelSize),
                             FVector(0.0, VoxelSize, VoxelSize),
                             FVector(0.0, 1.0, 0.0));
                if (Grid.GetVoxel(FUintVector3(x, y - 1, z)) == 0)
                    AddPlane(voxelPos,
                             FVector(0.0, 0.0, 0.0),
                             FVector(VoxelSize, 0.0, 0.0),
                             FVector(0.0, 0.0, VoxelSize),
                             FVector(VoxelSize, 0.0, VoxelSize),
                             FVector(0.0, -1.0, 0.0));
                if (Grid.GetVoxel(FUintVector3(x, y, z + 1)) == 0)
                    AddPlane(voxelPos,
                             FVector(0.0, 0.0, VoxelSize),
                             FVector(VoxelSize, 0.0, VoxelSize),
                             FVector(0.0, VoxelSize, VoxelSize),
                             FVector(VoxelSize, VoxelSize, VoxelSize),
                             FVector(0.0, 0.0, 1.0));
                if (Grid.GetVoxel(FUintVector3(x, y, z - 1)) == 0)
                    AddPlane(voxelPos,
                             FVector(VoxelSize, 0.0, 0.0),
                             FVector(0.0, 0.0, 0.0),
                             FVector(VoxelSize, VoxelSize, 0.0),
                             FVector(0.0, VoxelSize, 0.0),
                             FVector(0.0, 0.0, -1.0));
            }
        }
    }

    if (ProceduralMesh)
    {
        ProceduralMesh->ClearAllMeshSections();
        ProceduralMesh->CreateMeshSection_LinearColor(0,
                                                      m_vertices,
                                                      m_indices,
                                                      m_normals,
                                                      m_uvs,
                                                      TArray<FLinearColor>(),
                                                      TArray<FProcMeshTangent>(),
                                                      false);  
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("%s: ProceduralMesh component missing!"), *GetName());
    }
}

void AVoxelBase::AddPlane(const FVector& Position,
                          const FVector& V0,
                          const FVector& V1,
                          const FVector& V2,
                          const FVector& V3,
                          const FVector& Normal)
{
    m_vertices.Add(V0 + Position);
    m_vertices.Add(V1 + Position);
    m_vertices.Add(V2 + Position);
    m_vertices.Add(V3 + Position);

    m_normals.Add(Normal);
    m_normals.Add(Normal);
    m_normals.Add(Normal);
    m_normals.Add(Normal);

    m_uvs.Add(FVector2D(0.0, 0.0));
    m_uvs.Add(FVector2D(1.0, 0.0));
    m_uvs.Add(FVector2D(0.0, 1.0));
    m_uvs.Add(FVector2D(1.0, 1.0));

    m_indices.Add(2 + m_indexCount);
    m_indices.Add(1 + m_indexCount);
    m_indices.Add(0 + m_indexCount);
    m_indices.Add(2 + m_indexCount);
    m_indices.Add(3 + m_indexCount);
    m_indices.Add(1 + m_indexCount);

    m_indexCount += 4;
}

void AVoxelBase::CreateCollision()
{
    if (!ProceduralMesh || Grid.Data.IsEmpty())
    {
        return;
    }

    if (CollisionType == EVoxelCollisionType::MaxIS || CollisionType == EVoxelCollisionType::IS1)
    {
        const float Density = DecompVoxel::EstimateConcaveEdgeDensity(Grid);
        if (Density > 2.0f)
        {
            UE_LOG(LogTemp,
                   Warning,
                   TEXT("%s: %s collision decomposition has a high concave-edge density (%.2f, structured shapes are usually "
                        "<1.0) - this will likely take several seconds or more. Consider 3GDM/BSP for this shape instead."),
                   *GetName(),
                   CollisionType == EVoxelCollisionType::MaxIS ? TEXT("MaxIS") : TEXT("IS(1)"),
                   Density);
        }
    }

    UBodySetup* BodySetup = ProceduralMesh->GetBodySetup();
    if (!BodySetup)
    {
        return;
    }

    BodySetup->AggGeom.BoxElems.Reset();
    BodySetup->AggGeom.ConvexElems.Reset();

    const uint32 N = Grid.GridSize;

    if (CollisionType == EVoxelCollisionType::ConvexHull)
    {
        TArray<FVector> HullPoints;
        for (uint32 z = 0; z < N; ++z)
        {
            for (uint32 y = 0; y < N; ++y)
            {
                for (uint32 x = 0; x < N; ++x)
                {
                    if (Grid.GetVoxel(FUintVector3(x, y, z)) == 0) continue;
                    const bool bSurface =
                        Grid.GetVoxel(FUintVector3(x - 1, y, z)) == 0 || Grid.GetVoxel(FUintVector3(x + 1, y, z)) == 0 ||
                        Grid.GetVoxel(FUintVector3(x, y - 1, z)) == 0 || Grid.GetVoxel(FUintVector3(x, y + 1, z)) == 0 ||
                        Grid.GetVoxel(FUintVector3(x, y, z - 1)) == 0 || Grid.GetVoxel(FUintVector3(x, y, z + 1)) == 0;
                    if (!bSurface) continue;

                    const FVector Min = FVector((float)x, (float)y, (float)z) * VoxelSize;
                    const FVector Max = FVector((float)(x + 1), (float)(y + 1), (float)(z + 1)) * VoxelSize;
                    HullPoints.Add(FVector(Min.X, Min.Y, Min.Z));
                    HullPoints.Add(FVector(Max.X, Min.Y, Min.Z));
                    HullPoints.Add(FVector(Min.X, Max.Y, Min.Z));
                    HullPoints.Add(FVector(Max.X, Max.Y, Min.Z));
                    HullPoints.Add(FVector(Min.X, Min.Y, Max.Z));
                    HullPoints.Add(FVector(Max.X, Min.Y, Max.Z));
                    HullPoints.Add(FVector(Min.X, Max.Y, Max.Z));
                    HullPoints.Add(FVector(Max.X, Max.Y, Max.Z));
                }
            }
        }

        FKConvexElem ConvexElem;
        ConvexElem.VertexData = HullPoints;
        ConvexElem.UpdateElemBox();
        BodySetup->AggGeom.ConvexElems.Add(ConvexElem);
    }
    else
    {
        TArray<DecompVoxel::FVoxelBlock> Blocks;
        switch (CollisionType)
        {
            case EVoxelCollisionType::Octree:
                Blocks = DecompVoxel::DecomposeOctree(Grid);
                break;
            case EVoxelCollisionType::BSP:
                Blocks = DecompVoxel::DecomposeBSP(Grid);
                break;
            case EVoxelCollisionType::MaxIS:
                Blocks = DecompVoxel::DecomposeMaxIS(Grid);
                break;
            case EVoxelCollisionType::IS1:
                Blocks = DecompVoxel::DecomposeIS1(Grid);
                break;
            case EVoxelCollisionType::ThreeGDM:
            default:
                Blocks = DecompVoxel::Decompose3GDM(Grid);
                break;
        }

        BodySetup->AggGeom.BoxElems.Reserve(Blocks.Num());
        for (const DecompVoxel::FVoxelBlock& Block : Blocks)
        {
            const FVector BlockMin = FVector(Block.X, Block.Y, Block.Z) * VoxelSize;
            const FVector BlockSize = FVector(Block.SizeX, Block.SizeY, Block.SizeZ) * VoxelSize;
            const FVector BlockCenter = BlockMin + BlockSize * 0.5f;

            FKBoxElem BoxElem;
            BoxElem.Center = BlockCenter;
            BoxElem.Rotation = FRotator::ZeroRotator;
            BoxElem.X = BlockSize.X;
            BoxElem.Y = BlockSize.Y;
            BoxElem.Z = BlockSize.Z;

            BodySetup->AggGeom.BoxElems.Add(BoxElem);
        }
    }

    BodySetup->InvalidatePhysicsData();
    BodySetup->CreatePhysicsMeshes();
    ProceduralMesh->RecreatePhysicsState();
}