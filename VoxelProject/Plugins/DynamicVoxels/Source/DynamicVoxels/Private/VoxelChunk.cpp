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

    // Basic terrain generation, will be optimized and improved in the future
    /*
    for (int y = -1; y <= static_cast<int>(VOXEL_GRID_SIZE); y++)
    {
        for (int x = -1; x <= static_cast<int>(VOXEL_GRID_SIZE); x++)
        {
            FIntVector2 topDownPosition = FIntVector2(x + relativePosition.X, y + relativePosition.Y);

            double perlin2DValBiome = FMath::PerlinNoise2D(FVector2D(topDownPosition) / 200.0) * 0.5 + 0.5;

            double perlin2DVal = FMath::PerlinNoise2D(FVector2D(topDownPosition) / 160.0) * 0.5 + 0.5;
            if (perlin2DValBiome > 0.5) topDownPosition += FIntVector2(100, 100);
            double perlin2DValDetail = FMath::PerlinNoise2D(FVector2D(topDownPosition) / 64.0) * 0.5 + 0.5;

            int terrainValue = static_cast<int>(perlin2DVal * perlin2DVal * 80.0 + 48.0 + perlin2DValDetail * 20.0);
            if (perlin2DValBiome > 0.5) terrainValue -= static_cast<int>(perlin2DValBiome * 10.0);
#if SUPERFLAT_WORLDGEN
            terrainValue = 48;
#endif
            terrainValue -= relativePosition.Z;

            double BlockChoice = FMath::RandRange(0.0, 1.0);
            BlockChoice += perlin2DValDetail * 3;

            for (int z = -1; z <= static_cast<int>(VOXEL_GRID_SIZE * VOXEL_CHUNK_HEIGHT); z++)
            {
                FIntVector3 worldPosition = FIntVector3(x, y, z) + relativePosition;
                float caveValue = FMath::PerlinNoise3D(FVector(worldPosition) / 16.0);

                if (z >= terrainValue)
                {
                    float falloff = static_cast<float>(FMath::Abs(z - terrainValue)) * 0.08;
                    float applied = caveValue - falloff;

                    if (applied > 0.5f) SetVoxelLocal(FIntVector3(x, y, z), 7);  // Cobble
                }
                else if (z >= terrainValue + 10)
                {
                    break;
                }
                else
                {
                    uint8 blockType = 1;  // Stone
                    if (perlin2DValBiome > 0.5)
                    {
                        if (terrainValue - z < 9) blockType = 4;  // Sand
                    }
                    else
                    {
                        if (terrainValue - z < 3 && BlockChoice > 1.6f)
                            blockType = 2;  // Grass
                        else if (terrainValue - z < 9)
                            blockType = 3;  // Dirt
                    }

                    float falloff = (128.0f - static_cast<float>(worldPosition.Z)) / 64.0f - 1.0f;
                    float applied = caveValue + falloff;

                    if (applied > -0.2f) SetVoxelLocal(FIntVector3(x, y, z), blockType);
                }
            }
        }
    }
    */

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
                    FVector(x + RelativePosition.X, y + RelativePosition.Y, z + RelativePosition.Z) / GenAsset->CaveNoiseScale);

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

    // TOOD: Change to UpdateMaterialDirtySubGrids() function
    // Update material data for all grids
    for (FVoxelGrid* Grid : SubGridsArray)
    {
        Grid->UpdateMaterialData();
    }
}