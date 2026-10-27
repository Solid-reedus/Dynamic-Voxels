// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AssetDefinitionDefault.h"
#include "VoxelStructureAsset.h"
#include "VoxelGenerationAsset.generated.h"

USTRUCT()
struct FTerrainLayer
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere)
    uint8 VoxelType = 1;

    UPROPERTY(EditAnywhere)
    int MaxDepth = 2;

    UPROPERTY(EditAnywhere)
    float LayerChance = 1.0f;

    UPROPERTY(EditAnywhere)
    float LayerChanceVariation = 0.0f;
};

USTRUCT()
struct FStructureSpawnInfo
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere)
    UVoxelStructureAsset* StructureAsset = nullptr;

    UPROPERTY(EditAnywhere)
    float SpawnChance = 0.01f;
};

/**
 * 
 */
UCLASS()
class DYNAMICVOXELS_API UVoxelGenerationAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Generation")
    float BiomeScale = 200.0f;

    UPROPERTY(EditAnywhere, Category = "Generation")
    FVector2D TerrainNoiseScale = FVector2D(160.0);

    UPROPERTY(EditAnywhere, Category = "Generation")
    FVector2D DetailNoiseScale = FVector2D(40.0);

    UPROPERTY(EditAnywhere, Category = "Generation")
    FVector CaveNoiseScale = FVector(20.0);

    UPROPERTY(EditAnywhere, Category = "Generation")
    FVector2D CaveNoiseFalloff = FVector2D(-1.0, 1.0);

    UPROPERTY(EditAnywhere, Category = "Generation")
    float CaveCutoff = -0.2f;

    UPROPERTY(EditAnywhere, Category = "Generation")
    float BiomeBlending = 5.0f;

    /* ========================================================== */

    UPROPERTY(EditAnywhere, Category = "Biome 1")
    float B1BaseTerrainHeight = 50.0f;

    UPROPERTY(EditAnywhere, Category = "Biome 1")
    float B1TerrainNoiseAmplitude = 80.0f;

    UPROPERTY(EditAnywhere, Category = "Biome 1")
    float B1DetailNoiseAmplitude = 20.0f;

    UPROPERTY(EditAnywhere, Category = "Biome 1")
    TArray<FTerrainLayer> B1TerrainLayers;

    UPROPERTY(EditAnywhere, Category = "Biome 1")
    TArray<FStructureSpawnInfo> B1Structures;

    /* ========================================================== */

    UPROPERTY(EditAnywhere, Category = "Biome 2")
    float B2BaseTerrainHeight = 30.0f;

    UPROPERTY(EditAnywhere, Category = "Biome 2")
    float B2TerrainNoiseAmplitude = 80.0f;

    UPROPERTY(EditAnywhere, Category = "Biome 2")
    float B2DetailNoiseAmplitude = 20.0f;

    UPROPERTY(EditAnywhere, Category = "Biome 2")
    TArray<FTerrainLayer> B2TerrainLayers;

    UPROPERTY(EditAnywhere, Category = "Biome 2")
    TArray<FStructureSpawnInfo> B2Structures;

    /* ========================================================== */

    FIntVector StructureBoundsMin = FIntVector(INT32_MAX);
    FIntVector StructureBoundsMax = FIntVector(INT32_MIN);
};

/**
 * ===========================================================================================================
 * The code for setting up a custom viewport for the data assset was made using help from Claude AI.
 * ===========================================================================================================
 */

class FVGEAssetEditorToolkit : public FAssetEditorToolkit
{
public:
    void InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& Host, UVoxelGenerationAsset* InAsset);

    virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
    virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

    virtual FName GetToolkitFName() const override { return "VGEDataAssetEditor"; }
    virtual FText GetBaseToolkitName() const override { return INVTEXT("VGE Data Asset Editor"); }
    virtual FString GetWorldCentricTabPrefix() const override { return TEXT("VGEDataAsset"); }
    virtual FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor::White; }

private:
    TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& Args);

    static const FName ViewportTabId;
    static const FName DetailsTabId;

    TObjectPtr<UVoxelGenerationAsset> Asset;
    TSharedPtr<IDetailsView> DetailsView;
};

UCLASS()
class UVGEAssetDefinition : public UAssetDefinitionDefault
{
    GENERATED_BODY()

public:
    virtual FText GetAssetDisplayName() const override { return INVTEXT("Voxel Generation Asset"); }
    virtual FLinearColor GetAssetColor() const override { return FLinearColor::Red; }
    virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UVoxelGenerationAsset::StaticClass(); }

    virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
    virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};

UCLASS()
class UVGEFactory : public UFactory
{
    GENERATED_BODY()

public:
    UVGEFactory();

    virtual UObject* FactoryCreateNew(UClass* InClass,
                                      UObject* InParent,
                                      FName InName,
                                      EObjectFlags Flags,
                                      UObject* Context,
                                      FFeedbackContext* Warn) override;
};