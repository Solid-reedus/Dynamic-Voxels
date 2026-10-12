// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelStructureAsset.h"

const FName FVSEAssetEditorToolkit::ViewportTabId("VSEDataAssetEditor_Viewport");
const FName FVSEAssetEditorToolkit::DetailsTabId("VSEDataAssetEditor_Details");

void FVSEAssetEditorToolkit::InitEditor(const EToolkitMode::Type Mode,
                                       const TSharedPtr<IToolkitHost>& Host,
                                       UVoxelStructureAsset* InAsset)
{
    Asset = InAsset;
    PreviewScene = MakeShared<FAdvancedPreviewScene>(FPreviewScene::ConstructionValues());

    // Details view
    FPropertyEditorModule& PropModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    FDetailsViewArgs Args;
    Args.NotifyHook = nullptr;
    DetailsView = PropModule.CreateDetailView(Args);
    DetailsView->SetObject(Asset);

    // Viewport
    Viewport = SNew(SVSEViewport, PreviewScene);
    Viewport->SetMesh(Asset->PreviewMesh);

    // Refresh the viewport when a property changes
    DetailsView->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent&)
                                                          { Viewport->SetMesh(Asset->PreviewMesh); });

    const TSharedRef<FTabManager::FLayout> Layout =
        FTabManager::NewLayout("VSEDataAssetEditorLayout_v1")
            ->AddArea(
                FTabManager::NewPrimaryArea()
                    ->SetOrientation(Orient_Horizontal)
                    ->Split(FTabManager::NewStack()->SetSizeCoefficient(0.7f)->AddTab(ViewportTabId, ETabState::OpenedTab))
                    ->Split(FTabManager::NewStack()->SetSizeCoefficient(0.3f)->AddTab(DetailsTabId, ETabState::OpenedTab)));

    InitAssetEditor(Mode,
                    Host,
                    "VSEDataAssetEditorApp",
                    Layout,
                    /*bCreateDefaultStandaloneMenu*/ true,
                    /*bCreateDefaultToolbar*/ true,
                    Asset);
}

void FVSEAssetEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
    FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
    WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(INVTEXT("VSE Data Asset Editor"));

    InTabManager->RegisterTabSpawner(ViewportTabId, FOnSpawnTab::CreateSP(this, &FVSEAssetEditorToolkit::SpawnViewportTab))
        .SetDisplayName(INVTEXT("Viewport"))
        .SetGroup(WorkspaceMenuCategory.ToSharedRef());

    InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FVSEAssetEditorToolkit::SpawnDetailsTab))
        .SetDisplayName(INVTEXT("Details"))
        .SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FVSEAssetEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
    FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
    InTabManager->UnregisterTabSpawner(ViewportTabId);
    InTabManager->UnregisterTabSpawner(DetailsTabId);
}

TSharedRef<SDockTab> FVSEAssetEditorToolkit::SpawnViewportTab(const FSpawnTabArgs&)
{
    return SNew(SDockTab)[Viewport.ToSharedRef()];
}

TSharedRef<SDockTab> FVSEAssetEditorToolkit::SpawnDetailsTab(const FSpawnTabArgs&)
{
    return SNew(SDockTab)[DetailsView.ToSharedRef()];
}

UVSEFactory::UVSEFactory()
{
    SupportedClass = UVoxelStructureAsset::StaticClass();
    bCreateNew = true;     // makes it appear in the Add / Create menu
    bEditAfterNew = false;  // opens after creation
}

UObject* UVSEFactory::FactoryCreateNew(UClass* InClass,
                                       UObject* InParent,
                                       FName InName,
                                       EObjectFlags Flags,
                                       UObject* Context,
                                       FFeedbackContext* Warn)
{
    return NewObject<UVoxelStructureAsset>(InParent, InClass, InName, Flags | RF_Transactional);
}