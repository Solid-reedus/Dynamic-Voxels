// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AssetDefinitionDefault.h"
#include "EditorViewportClient.h"
#include "AdvancedPreviewScene.h"
#include "SEditorViewport.h"
#include "VoxelStructureAsset.generated.h"

/**
 *
 */
UCLASS()
class DYNAMICVOXELS_API UVoxelStructureAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere) TObjectPtr<UStaticMesh> PreviewMesh;
};

/**
 * ===========================================================================================================
 * The Following code was made using help from Claude AI.
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
    virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
    {
        static const auto Cats = {EAssetCategoryPaths::Misc};
        return Cats;
    }

    virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override
    {
        for (UVoxelStructureAsset* Asset : OpenArgs.LoadObjects<UVoxelStructureAsset>())
        {
            TSharedRef<FVSEAssetEditorToolkit> Editor = MakeShared<FVSEAssetEditorToolkit>();
            Editor->InitEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Asset);
        }
        return EAssetCommandResult::Handled;
    }
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

class FVSEViewportClient : public FEditorViewportClient
{
public:
    FVSEViewportClient(FAdvancedPreviewScene* InScene, const TSharedRef<SEditorViewport>& InViewport)
        : FEditorViewportClient(nullptr, InScene, InViewport)
    {
        SetRealtime(true);
        SetViewLocation(FVector(-300, 0, 150));
        SetViewRotation(FRotator(-15, 0, 0));
        SetViewportType(LVT_Perspective);
        EngineShowFlags.SetGrid(true);
    }

    virtual void Tick(float DeltaSeconds) override
    {
        FEditorViewportClient::Tick(DeltaSeconds);
        if (PreviewScene)
        {
            PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
        }
    }
};

class SVSEViewport : public SEditorViewport
{
public:
    SLATE_BEGIN_ARGS(SVSEViewport) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, TSharedPtr<FAdvancedPreviewScene> InScene)
    {
        PreviewScene = InScene;
        SEditorViewport::Construct(SEditorViewport::FArguments());
    }

    UStaticMeshComponent* GetMeshComponent() const { return MeshComp; }

    void SetMesh(UStaticMesh* Mesh)
    {
        if (!MeshComp)
        {
            MeshComp = NewObject<UStaticMeshComponent>(GetTransientPackage());
            PreviewScene->AddComponent(MeshComp, FTransform::Identity);
        }
        MeshComp->SetStaticMesh(Mesh);
    }

protected:
    virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override
    {
        ViewportClient = MakeShared<FVSEViewportClient>(PreviewScene.Get(), SharedThis(this));
        return ViewportClient.ToSharedRef();
    }

private:
    TSharedPtr<FAdvancedPreviewScene> PreviewScene;
    TSharedPtr<FVSEViewportClient> ViewportClient;
    TObjectPtr<UStaticMeshComponent> MeshComp;
};