// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AssetDefinitionDefault.h"
#include "EditorViewportClient.h"
#include "AdvancedPreviewScene.h"
#include "SEditorViewport.h"
#include "ProceduralMeshComponent.h"
#include "VoxelStructureAsset.generated.h"

USTRUCT()
struct FStructuresVoxel
{
    GENERATED_BODY()

public:
    UPROPERTY()
    FIntVector Position;

    UPROPERTY()
    uint8 VoxelType;
};

/**
 *
 */
UCLASS()
class DYNAMICVOXELS_API UVoxelStructureAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    // Properties only needed for the structure editor
    UPROPERTY(EditAnywhere, Category = "Editor")
    UTexture2DArray* VoxelTextures = nullptr;

    UPROPERTY(EditAnywhere, Category = "Editor")
    uint8 EditorVoxelType = 1;

    UPROPERTY(EditAnywhere, Category = "Editor")
    int32 EditorHeightOffset = 0;

    UPROPERTY(EditAnywhere, Category = "Editor")
    float TextureScale = 100.0f;

    // Properties needed to spawn in structures
    UPROPERTY(VisibleAnywhere, Category = "Data")
    FIntVector BoundsOrigin = FIntVector::ZeroValue;

    UPROPERTY(VisibleAnywhere, Category = "Data")
    FIntVector BoundsScale = FIntVector::ZeroValue;

    UPROPERTY()  // Removed EditAnywhere due to performance issues
    TArray<FStructuresVoxel> VoxelData;
};

/**
 * ===========================================================================================================
 * The code for setting up a custom viewport for the data assset was made using help from Claude AI.
 * ===========================================================================================================
 */

class SVSEViewport;

class FVSEAssetEditorToolkit : public FAssetEditorToolkit
{
public:
    void InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& Host, UVoxelStructureAsset* InAsset);

    virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
    virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

    virtual FName GetToolkitFName() const override { return "VSEDataAssetEditor"; }
    virtual FText GetBaseToolkitName() const override { return INVTEXT("VSE Data Asset Editor"); }
    virtual FString GetWorldCentricTabPrefix() const override { return TEXT("VSEDataAsset"); }
    virtual FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor::White; }

private:
    TSharedRef<SDockTab> SpawnViewportTab(const FSpawnTabArgs& Args);
    TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& Args);

    static const FName ViewportTabId;
    static const FName DetailsTabId;

    TObjectPtr<UVoxelStructureAsset> Asset;
    TSharedPtr<FAdvancedPreviewScene> PreviewScene;
    TSharedPtr<SVSEViewport> Viewport;
    TSharedPtr<IDetailsView> DetailsView;
};

UCLASS()
class UAssetDefinition_VSE : public UAssetDefinitionDefault
{
    GENERATED_BODY()
public:
    virtual FText GetAssetDisplayName() const override { return INVTEXT("Voxel Structure Asset"); }
    virtual FLinearColor GetAssetColor() const override { return FLinearColor::Green; }
    virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UVoxelStructureAsset::StaticClass(); }

    virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
    virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};

UCLASS()
class UVSEFactory : public UFactory
{
    GENERATED_BODY()

public:
    UVSEFactory();

    virtual UObject* FactoryCreateNew(UClass* InClass,
                                      UObject* InParent,
                                      FName InName,
                                      EObjectFlags Flags,
                                      UObject* Context,
                                      FFeedbackContext* Warn) override;
};

class SVSEViewport;

class FVSEViewportClient : public FEditorViewportClient
{
public:
    FVSEViewportClient(FAdvancedPreviewScene* InScene, const TSharedRef<SVSEViewport>& InViewport);

protected:
    virtual void Tick(float DeltaSeconds) override;

    virtual void MouseMove(FViewport* InViewport, int32 X, int32 Y) override;
    virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;

    virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;

private:
    bool bHasHoverPoint = false;
    FVector HoverPoint = FVector::ZeroVector;
    FVector HoverNormal = FVector::ZeroVector;

    bool bHoldingCtrl = false;

    TSharedPtr<SVSEViewport> Viewport;
};

class SVSEViewport : public SEditorViewport
{
public:
    SLATE_BEGIN_ARGS(SVSEViewport) {}
    SLATE_END_ARGS()

    enum class EFaceDirection
    {
        POSITIVE_X,
        NEGATIVE_X,
        POSITIVE_Y,
        NEGATIVE_Y,
        POSITIVE_Z,
        NEGATIVE_Z
    };

    void Construct(const FArguments& InArgs, TSharedPtr<FAdvancedPreviewScene> InScene, UVoxelStructureAsset* InAsset);

    void RegenerateVoxelMesh();

protected:
    virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;

private:
    friend class FVSEViewportClient;

    void AddFace(const FVector& Position, EFaceDirection FaceDirection, uint8 VoxelType);

    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<FVector2D> Uvs;
    TArray<int32> Indices;
    int32 IndexCount = 0;

    TSharedPtr<FAdvancedPreviewScene> PreviewScene;
    TSharedPtr<FVSEViewportClient> ViewportClient;
    
    TObjectPtr<UVoxelStructureAsset> Asset;
    TObjectPtr<UProceduralMeshComponent> ProcMeshComponent;
    TObjectPtr<UMaterial> BaseMaterial;
    TObjectPtr<UMaterialInstanceDynamic> MaterialInstance;

    FIntVector BoundsMin = FIntVector::ZeroValue;
    FIntVector BoundsMax = FIntVector::ZeroValue;

    bool bCreatedMatInstance = false;
};