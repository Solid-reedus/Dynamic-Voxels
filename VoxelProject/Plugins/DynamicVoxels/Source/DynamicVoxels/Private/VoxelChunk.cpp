#include "VoxelChunk.h"
#include "VoxelWorld.h"

#define SUPERFLAT_WORLDGEN 0

// Sets default values
AVoxelChunk::AVoxelChunk() { PrimaryActorTick.bCanEverTick = false; }

void AVoxelChunk::ApplyVoxelEdit(const TMap<FIntVector, uint8>& Edits)
{
    if (Edits.IsEmpty()) return;

    VoxelWorld->ApplyVoxelEdit(Edits);
}

FIntVector AVoxelChunk::WorldToGridPosition(const FVector& Position)
{
    FVector RelativePosition = Position / FVector(VoxelSize);
    FIntVector3 Final = FIntVector3(static_cast<int>(FMath::Floor(RelativePosition.X)),
                                    static_cast<int>(FMath::Floor(RelativePosition.Y)),
                                    static_cast<int>(FMath::Floor(RelativePosition.Z)));

    return Final;
}

void AVoxelChunk::InitializeChunk(uint32 InVoxelSize,
                                  UMaterial* InBaseMaterial,
                                  UTexture2DArray* InVoxelTextures,
                                  AVoxelWorld* InVoxelWorld,
                                  UVoxelGenerationAsset* GenerationAsset)
{
    VoxelSize = InVoxelSize;
    VoxelWorld = InVoxelWorld;

    // Create all subgrids
    SubGrids.Reserve(static_cast<int>(VOXEL_CHUNK_HEIGHT));

    for (int i = 0; i < static_cast<int>(VOXEL_CHUNK_HEIGHT); i++)
    {
        FVoxelGrid& Grid = SubGrids.Add(FIntVector(0, 0, i));
        Grid.Initialize(VOXEL_GRID_SIZE);
        Grid.CreateMaterialInstance(InVoxelSize, InBaseMaterial, InVoxelTextures);

        SubGridsArray.Add(&Grid);
    }

    GenerateVoxelData(GenerationAsset);

    RegenerateMesh();
    CreateCollision();
    MarkSubGridsAsNotDirty();
}

void AVoxelChunk::ApplyVoxelEditLocal(const TMap<FIntVector, uint8>& Edits)
{
    // Chunk position as the voxel grid
    FIntVector ChunkVoxelGridPos = ChunkPosition * VOXEL_GRID_SIZE;

    for (const auto& Edit : Edits)
    {
        FIntVector LocalPos = Edit.Key - ChunkVoxelGridPos;

        int PosMin = FMath::Min(LocalPos.X, LocalPos.Y);
        int PosMax = FMath::Max(LocalPos.X, LocalPos.Y);

        if (PosMax > static_cast<int>(VOXEL_GRID_SIZE) || PosMin < -1)
            continue;

        SetVoxelLocal(LocalPos, Edit.Value);
    }

    UpdateMaterialDirtySubGrids();
    RegenerateMesh();
    CreateCollision();
    MarkSubGridsAsNotDirty();
}

void AVoxelChunk::SetVoxelLocal(const FIntVector& VoxelPos, uint8 Value)
{
    int SubGridPosZ = AVoxelWorld::GridToChunkPosition(VoxelPos).Z;
    FIntVector LocalPosition = VoxelPos;
    LocalPosition.Z -= (SubGridPosZ * static_cast<int>(VOXEL_GRID_SIZE));

    // Handle if set voxel is at sub grid border
    if (LocalPosition.Z == 0 && SubGridPosZ > 0 && SubGridPosZ <= static_cast<int>(VOXEL_CHUNK_HEIGHT))
    {
        FVoxelGrid* SecondGrid = SubGridsArray[SubGridPosZ - 1];

        FIntVector SecondPos = LocalPosition;
        SecondPos.Z = static_cast<int>(VOXEL_GRID_SIZE);

        SecondGrid->SetVoxel(SecondPos, Value);
    }
    else if (LocalPosition.Z == static_cast<int>(VOXEL_GRID_SIZE) - 1 && SubGridPosZ >= -1 &&
             SubGridPosZ < static_cast<int>(VOXEL_CHUNK_HEIGHT) - 1)
    {
        FVoxelGrid* SecondGrid = SubGridsArray[SubGridPosZ + 1];

        FIntVector SecondPos = LocalPosition;
        SecondPos.Z = -1;

        SecondGrid->SetVoxel(SecondPos, Value);
    }

    if (SubGridPosZ >= 0 && SubGridPosZ < static_cast<int>(VOXEL_CHUNK_HEIGHT))
    {
        FVoxelGrid* CurGrid = SubGridsArray[SubGridPosZ];
        CurGrid->SetVoxel(LocalPosition, Value);
    }
}

// Called when the game starts or when spawned
void AVoxelChunk::BeginPlay() { Super::BeginPlay(); }

void AVoxelChunk::GenerateVoxelData(UVoxelGenerationAsset* GenAsset)
{
    if (!GenAsset) return;

    FVector ActorPosition = GetActorLocation();
    FIntVector3 RelativePosition = FIntVector3(static_cast<int32>(ActorPosition.X),
                                               static_cast<int32>(ActorPosition.Y),
                                               static_cast<int32>(ActorPosition.Z)) /
                                   VoxelSize;

    ChunkPosition = RelativePosition / VOXEL_GRID_SIZE;

    // Terrain Generation
    for (int y = -1; y <= static_cast<int>(VOXEL_GRID_SIZE); ++y)
    {
        for (int x = -1; x <= static_cast<int>(VOXEL_GRID_SIZE); ++x)
        {
            FVector2D VoxelPosition2D = FVector2D(x + RelativePosition.X, y + RelativePosition.Y);

            float BiomeValue = FMath::PerlinNoise2D(VoxelPosition2D / GenAsset->BiomeScale) * 0.5f + 0.5f;
            float BiomeValueDetail = FMath::PerlinNoise2D(VoxelPosition2D / (GenAsset->BiomeScale * 0.5f)) * 0.5f + 0.5f;
            BiomeValue = BiomeValue * 0.7f + BiomeValueDetail * 0.3f;

            float TerrainValue = FMath::PerlinNoise2D(VoxelPosition2D / GenAsset->TerrainNoiseScale) * 0.5f + 0.5f;
            float DetailValue = FMath::PerlinNoise2D(VoxelPosition2D / GenAsset->DetailNoiseScale) * 0.5f + 0.5f;

            float TerrainHeight = 0.0f;
            TArray<FTerrainLayer>* TerrainLayers = nullptr;

            if (BiomeValue < 0.5f)
            {
                TerrainHeight = GenAsset->B1BaseTerrainHeight + TerrainValue * GenAsset->B1TerrainNoiseAmplitude +
                                      DetailValue * GenAsset->B1DetailNoiseAmplitude;

                TerrainLayers = &GenAsset->B1TerrainLayers;
            }
            else
            {
                TerrainHeight = GenAsset->B2BaseTerrainHeight + TerrainValue * GenAsset->B2TerrainNoiseAmplitude +
                                      DetailValue * GenAsset->B2DetailNoiseAmplitude;

                TerrainLayers = &GenAsset->B2TerrainLayers;

                if (GenAsset->BiomeBlending > 0.0f)
                {
                    float TerrainHeight2 = GenAsset->B1BaseTerrainHeight + TerrainValue * GenAsset->B1TerrainNoiseAmplitude +
                                           DetailValue * GenAsset->B1DetailNoiseAmplitude;

                    float BiomeBorderLerp = FMath::Clamp((BiomeValue - 0.5f) / GenAsset->BiomeBlending, 0.0f, 1.0f);
                    BiomeBorderLerp = FMath::SmoothStep(0.0f, 1.0f, BiomeBorderLerp);
                    TerrainHeight = FMath::Lerp(TerrainHeight2, TerrainHeight, BiomeBorderLerp);
                }
            }

            float BlockChoice = FMath::RandRange(0.0f, 1.0f);

            int TerrainHeightInt = static_cast<int>(TerrainHeight);

            for (int z = -1; z <= static_cast<int>(VOXEL_GRID_SIZE * VOXEL_CHUNK_HEIGHT); z++)
            {
                if (z >= TerrainHeightInt) break;

                // Perform caves
                float CaveValue = FMath::PerlinNoise3D(
                    FVector(x + RelativePosition.X, y + RelativePosition.Y, z) / GenAsset->CaveNoiseScale);

                float HeightLimitLerp =
                    FMath::Clamp(static_cast<float>(z) / static_cast<float>(VOXEL_GRID_SIZE * VOXEL_CHUNK_HEIGHT), 0.0f, 1.0f);
                CaveValue -= FMath::Lerp(GenAsset->CaveNoiseFalloff.X, GenAsset->CaveNoiseFalloff.Y, HeightLimitLerp);

                if (CaveValue < GenAsset->CaveCutoff) continue;

                // Calculate voxel layers
                int TerrainHeightDiff = TerrainHeightInt - z;

                uint8 VoxelType = 1;

                for (const FTerrainLayer& TerrainLayer : (*TerrainLayers))
                {
                    if (TerrainHeightDiff <= TerrainLayer.MaxDepth)
                    {
                        float CurBlockChoice = BlockChoice - (TerrainLayer.LayerChanceVariation * DetailValue);
                        if (CurBlockChoice < TerrainLayer.LayerChance)
                        {
                            VoxelType = TerrainLayer.VoxelType;
                            break;
                        }
                    }
                }

                SetVoxelLocal(FIntVector(x, y, z), VoxelType);
            }
        }
    }

    // Spawn Structures
    FIntVector StructureChunkMin = GenAsset->StructureBoundsMax * -1;
    FIntVector StructureChunkMax = (GenAsset->StructureBoundsMin * -1) + FIntVector(static_cast<int>(VOXEL_GRID_SIZE));

    for (int y = StructureChunkMin.Y; y <= StructureChunkMax.Y; ++y)
    {
        for (int x = StructureChunkMin.X; x <= StructureChunkMax.X; ++x)
        {
            // Get the terrain Height Again
            FVector2D VoxelPosition2D = FVector2D(x + RelativePosition.X, y + RelativePosition.Y);
            FIntVector2 VoxelPositionInt = FIntVector2(x + RelativePosition.X, y + RelativePosition.Y);

            float BiomeValue = FMath::PerlinNoise2D(VoxelPosition2D / GenAsset->BiomeScale) * 0.5f + 0.5f;
            float BiomeValueDetail = FMath::PerlinNoise2D(VoxelPosition2D / (GenAsset->BiomeScale * 0.5f)) * 0.5f + 0.5f;
            BiomeValue = BiomeValue * 0.7f + BiomeValueDetail * 0.3f;

            float TerrainValue = FMath::PerlinNoise2D(VoxelPosition2D / GenAsset->TerrainNoiseScale) * 0.5f + 0.5f;
            float DetailValue = FMath::PerlinNoise2D(VoxelPosition2D / GenAsset->DetailNoiseScale) * 0.5f + 0.5f;

            float TerrainHeight = 0.0f;
            TArray<FStructureSpawnInfo>* StructuresSpawn = nullptr;

            if (BiomeValue < 0.5f)
            {
                TerrainHeight = GenAsset->B1BaseTerrainHeight + TerrainValue * GenAsset->B1TerrainNoiseAmplitude +
                                DetailValue * GenAsset->B1DetailNoiseAmplitude;

                StructuresSpawn = &GenAsset->B1Structures;
            }
            else
            {
                TerrainHeight = GenAsset->B2BaseTerrainHeight + TerrainValue * GenAsset->B2TerrainNoiseAmplitude +
                                DetailValue * GenAsset->B2DetailNoiseAmplitude;

                StructuresSpawn = &GenAsset->B2Structures;

                if (GenAsset->BiomeBlending > 0.0f)
                {
                    float TerrainHeight2 = GenAsset->B1BaseTerrainHeight + TerrainValue * GenAsset->B1TerrainNoiseAmplitude +
                                           DetailValue * GenAsset->B1DetailNoiseAmplitude;

                    float BiomeBorderLerp = FMath::Clamp((BiomeValue - 0.5f) / GenAsset->BiomeBlending, 0.0f, 1.0f);
                    BiomeBorderLerp = FMath::SmoothStep(0.0f, 1.0f, BiomeBorderLerp);
                    TerrainHeight = FMath::Lerp(TerrainHeight2, TerrainHeight, BiomeBorderLerp);
                }
            }

            int TerrainHeightInt = static_cast<int>(TerrainHeight);

            // Perform caves
            float CaveValue = FMath::PerlinNoise3D(
                FVector(x + RelativePosition.X, y + RelativePosition.Y, TerrainHeightInt - 1) / GenAsset->CaveNoiseScale);

            float HeightLimitLerp = FMath::Clamp(
                static_cast<float>(TerrainHeightInt - 1) / static_cast<float>(VOXEL_GRID_SIZE * VOXEL_CHUNK_HEIGHT),
                0.0f,
                1.0f);
            CaveValue -= FMath::Lerp(GenAsset->CaveNoiseFalloff.X, GenAsset->CaveNoiseFalloff.Y, HeightLimitLerp);
            
            // Dont't spawn structure
            if (CaveValue < GenAsset->CaveCutoff) continue;

            //float StructureChoice = (VoxelPositionInt.X % 9 == 0 && VoxelPositionInt.Y % 9 == 0) ? 0.0f : 1.0f;

            int SeedAdvance = 19;
            for (const FStructureSpawnInfo& Structure : (*StructuresSpawn))
            {
                float StructureChoice = RandomFloat01(VoxelPositionInt, SeedAdvance);
                SeedAdvance += 21;

                if (StructureChoice < Structure.SpawnChance)
                {
                    for (const FStructuresVoxel& Voxel : Structure.StructureAsset->VoxelData)
                    {
                        FIntVector Final = Voxel.Position + FIntVector(x, y, TerrainHeightInt);

                        if (Final.X >= -1 && Final.X <= static_cast<int>(VOXEL_GRID_SIZE) && Final.Y >= -1 &&
                            Final.Y <= static_cast<int>(VOXEL_GRID_SIZE))
                        {
                            SetVoxelLocal(Final, Voxel.VoxelType);
                        }
                    }
                    break; // Dont spawn multiple structures on the same block
                }
            }
        }
    }

    // Update material data for all dirty grids
    UpdateMaterialDirtySubGrids();
}