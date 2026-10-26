// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelGenerationAsset.h"

const FName FVGEAssetEditorToolkit::DetailsTabId("VGEDataAsset_Details");

void FVGEAssetEditorToolkit::InitEditor(const EToolkitMode::Type Mode,
                                        const TSharedPtr<IToolkitHost>& Host,
                                        UVoxelGenerationAsset* InAsset)
{
    Asset = InAsset;

    // Details view
    FPropertyEditorModule& PropModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    FDetailsViewArgs Args;
    Args.NotifyHook = nullptr;
    DetailsView = PropModule.CreateDetailView(Args);
    DetailsView->SetObject(Asset);

    const TSharedRef<FTabManager::FLayout> Layout =
        FTabManager::NewLayout("VGEDataAssetEditorLayout_v1")
            ->AddArea(
                FTabManager::NewPrimaryArea()
                    ->SetOrientation(Orient_Horizontal)
                    ->Split(FTabManager::NewStack()->SetSizeCoefficient(1.0f)->AddTab(DetailsTabId, ETabState::OpenedTab)));

    InitAssetEditor(Mode,
                    Host,
                    "VGEDataAssetEditorApp",
                    Layout,
                    /*bCreateDefaultStandaloneMenu*/ true,
                    /*bCreateDefaultToolbar*/ true,
                    Asset);
}

void FVGEAssetEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
    FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
    WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(INVTEXT("VGE Data Asset Editor"));

    InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FVGEAssetEditorToolkit::SpawnDetailsTab))
        .SetDisplayName(INVTEXT("Details"))
        .SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FVGEAssetEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
    FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
    InTabManager->UnregisterTabSpawner(DetailsTabId);
}

TSharedRef<SDockTab> FVGEAssetEditorToolkit::SpawnDetailsTab(const FSpawnTabArgs&)
{
    return SNew(SDockTab)[DetailsView.ToSharedRef()];
}

TConstArrayView<FAssetCategoryPath> UVGEAssetDefinition::GetAssetCategories() const
{
    static const auto Cats = {EAssetCategoryPaths::Misc};
    return Cats;
}

EAssetCommandResult UVGEAssetDefinition::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
    for (UVoxelGenerationAsset* Asset : OpenArgs.LoadObjects<UVoxelGenerationAsset>())
    {
        TSharedRef<FVGEAssetEditorToolkit> Editor = MakeShared<FVGEAssetEditorToolkit>();
        Editor->InitEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Asset);
    }
    return EAssetCommandResult::Handled;
}

UVGEFactory::UVGEFactory()
{
    SupportedClass = UVoxelGenerationAsset::StaticClass();
    bCreateNew = true;      // makes it appear in the Add / Create menu
    bEditAfterNew = false;  // opens after creation
}

UObject* UVGEFactory::FactoryCreateNew(UClass* InClass,
                                       UObject* InParent,
                                       FName InName,
                                       EObjectFlags Flags,
                                       UObject* Context,
                                       FFeedbackContext* Warn)
{
    return NewObject<UVoxelGenerationAsset>(InParent, InClass, InName, Flags | RF_Transactional);
}