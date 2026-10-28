// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelStructureAsset.h"

#include "Engine/Texture2DArray.h"

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
    Viewport = SNew(SVSEViewport, PreviewScene, InAsset);
    Viewport->RegenerateVoxelMesh();

    // Refresh the viewport when a property changes
    DetailsView->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent&)
                                                          { Viewport->RegenerateVoxelMesh(); });

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

TConstArrayView<FAssetCategoryPath> UAssetDefinition_VSE::GetAssetCategories() const
{
    static const auto Cats = {EAssetCategoryPaths::Misc};
    return Cats;
}

EAssetCommandResult UAssetDefinition_VSE::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
    for (UVoxelStructureAsset* Asset : OpenArgs.LoadObjects<UVoxelStructureAsset>())
    {
        TSharedRef<FVSEAssetEditorToolkit> Editor = MakeShared<FVSEAssetEditorToolkit>();
        Editor->InitEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Asset);
    }
    return EAssetCommandResult::Handled;
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

FVSEViewportClient::FVSEViewportClient(FAdvancedPreviewScene* InScene, const TSharedRef<SVSEViewport>& InViewport)
    : FEditorViewportClient(nullptr, InScene, InViewport)
{
    Viewport = InViewport;

    SetRealtime(true);
    SetViewLocation(FVector(-300, 0, 150));
    SetViewRotation(FRotator(-15, 0, 0));
    SetViewportType(LVT_Perspective);
    EngineShowFlags.SetGrid(true);
}

void FVSEViewportClient::Tick(float DeltaSeconds)
{
    FEditorViewportClient::Tick(DeltaSeconds);

    if (PreviewScene)
        PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
}

void FVSEViewportClient::MouseMove(FViewport* InViewport, int32 X, int32 Y)
{
    FEditorViewportClient::MouseMove(InViewport, X, Y);

    const FViewportCursorLocation Cursor = GetCursorWorldLocationFromMousePos();
    const FVector Start = Cursor.GetOrigin();
    const FVector End = Start + Cursor.GetDirection() * 100000.0;

    bHasHoverPoint = false;

    // Option A: trace against the preview world
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HoverTrace), /*bTraceComplex*/ true);
    if (Viewport->ProcMeshComponent && Viewport->ProcMeshComponent->LineTraceComponent(Hit, Start, End, Params))
    {
        HoverPoint = Hit.ImpactPoint;
        HoverNormal = Hit.ImpactNormal;
        bHasHoverPoint = true;
    }
    // Option B: fall back to the ground plane (Z = 0)
    else
    {
        const FVector PlaneHit = FMath::LinePlaneIntersection(Start, End, FVector::ZeroVector, FVector::UpVector);
        if (!PlaneHit.ContainsNaN() && FVector::DotProduct(Cursor.GetDirection(), FVector::UpVector) != 0.0)
        {
            HoverPoint = PlaneHit;
            HoverNormal = FVector::UpVector;
            bHasHoverPoint = true;
        }
    }
}

bool FVSEViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
    // Place Voxel
    if (EventArgs.Key == EKeys::RightMouseButton && EventArgs.Event == IE_Pressed && bHoldingCtrl)
    {
        FVector RelativePosition = (HoverPoint + HoverNormal) / FVector(100.0);
        FIntVector3 Final = FIntVector3(static_cast<int>(FMath::Floor(RelativePosition.X)),
                                        static_cast<int>(FMath::Floor(RelativePosition.Y)),
                                        static_cast<int>(FMath::Floor(RelativePosition.Z)));

        Final.Z -= Viewport->Asset->EditorHeightOffset;

        // Only place voxel if its not air
        if (Viewport->Asset->EditorVoxelType != 0)
        {
            // Ensure that the currently edited voxel does not already exist
            bool AlreadyExists = false;
            for (const FStructuresVoxel& Voxel : Viewport->Asset->VoxelData)
            {
                if (Voxel.Position == Final)
                {
                    AlreadyExists = true;
                    break;
                }
            }

            if (!AlreadyExists)
            {
                FStructuresVoxel NewVoxel;
                NewVoxel.Position = Final;
                NewVoxel.VoxelType = Viewport->Asset->EditorVoxelType;
                Viewport->Asset->VoxelData.Add(NewVoxel);

                Viewport->RegenerateVoxelMesh();
            }
        }
    }
    // Remove Voxel
    if (EventArgs.Key == EKeys::LeftMouseButton && EventArgs.Event == IE_Pressed && bHoldingCtrl)
    {
        FVector RelativePosition = (HoverPoint - HoverNormal) / FVector(100.0);
        FIntVector3 Final = FIntVector3(static_cast<int>(FMath::Floor(RelativePosition.X)),
                                        static_cast<int>(FMath::Floor(RelativePosition.Y)),
                                        static_cast<int>(FMath::Floor(RelativePosition.Z)));

        Final.Z -= Viewport->Asset->EditorHeightOffset;

        // Find and remove voxel from voxel data
        int CurIndex = 0;
        for (const FStructuresVoxel& Voxel : Viewport->Asset->VoxelData)
        {
            if (Voxel.Position == Final)
            {
                Viewport->Asset->VoxelData.RemoveAt(CurIndex, EAllowShrinking::Yes);
                break;
            }
            CurIndex++;
        }

        Viewport->RegenerateVoxelMesh();
    }

    if (EventArgs.Key == EKeys::LeftControl && EventArgs.Event == IE_Pressed) bHoldingCtrl = true;
    if (EventArgs.Key == EKeys::LeftControl && EventArgs.Event == IE_Released) bHoldingCtrl = false;

    return FEditorViewportClient::InputKey(EventArgs);
}

void FVSEViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
    FEditorViewportClient::Draw(View, PDI);

    // Draw placement preview
    if (bHasHoverPoint)
    {
        FVector RelativePosition = (HoverPoint + HoverNormal.GetAbs()) / FVector(100.0);
        FVector Final =
            FVector(FMath::Floor(RelativePosition.X), FMath::Floor(RelativePosition.Y), FMath::Floor(RelativePosition.Z));
        Final *= 100.0;

        FVector Up = FVector(0.0, 0.0, 1.0);
        if (FVector::DotProduct(HoverNormal, Up) > 0.9 || FVector::DotProduct(HoverNormal, Up) < -0.9)
            Up = FVector(1.0, 0.0, 0.0);

        FVector Tangent = FVector::CrossProduct(HoverNormal, Up);
        FVector BiTangent = FVector::CrossProduct(HoverNormal, Tangent);

        Tangent = Tangent.GetAbs() * 100.0;
        BiTangent = BiTangent.GetAbs() * 100.0;

        PDI->DrawLine(Final, Final + Tangent, FLinearColor::Blue, SDPG_Foreground, 2.0f);
        PDI->DrawLine(Final, Final + BiTangent, FLinearColor::Blue, SDPG_Foreground, 2.0f);
        PDI->DrawLine(Final + Tangent, Final + Tangent + BiTangent, FLinearColor::Blue, SDPG_Foreground, 2.0f);
        PDI->DrawLine(Final + BiTangent, Final + Tangent + BiTangent, FLinearColor::Blue, SDPG_Foreground, 2.0f);
    }

    // Draw structure Origin
    {
        FVector StructOrigin = FVector(0.0, 0.0, static_cast<double>(Viewport->Asset->EditorHeightOffset) * 100.0);

        PDI->DrawLine(StructOrigin + FVector(0.0, 50.0, 0.0),
                      StructOrigin + FVector(100.0, 50.0, 0.0),
                      FLinearColor::Red,
                      SDPG_Foreground,
                      4.0f);
        PDI->DrawLine(StructOrigin + FVector(50.0, 0.0, 0.0),
                      StructOrigin + FVector(50.0, 100.0, 0.0),
                      FLinearColor::Red,
                      SDPG_Foreground,
                      4.0f);
        PDI->DrawLine(StructOrigin + FVector(50.0, 50.0, 0.0),
                      StructOrigin + FVector(50.0, 50.0, 100.0),
                      FLinearColor::Red,
                      SDPG_Foreground,
                      4.0f);
    }

    // Draw structure Bounds
    if (!Viewport->Asset->BoundsScale.IsZero())
    {
        FVector WorldMin =
            FVector(Viewport->Asset->BoundsOrigin + FIntVector(0, 0, Viewport->Asset->EditorHeightOffset)) * 100.0;
        FVector WorldScale = FVector(Viewport->Asset->BoundsScale) * 100.0;

        FVector V0 = WorldMin;
        FVector V1 = WorldMin + FVector(WorldScale.X, 0.0, 0.0);
        FVector V2 = WorldMin + FVector(WorldScale.X, WorldScale.Y, 0.0);
        FVector V3 = WorldMin + FVector(0.0, WorldScale.Y, 0.0);

        FVector V4 = WorldMin + FVector(0.0, 0.0, WorldScale.Z);
        FVector V5 = WorldMin + FVector(WorldScale.X, 0.0, WorldScale.Z);
        FVector V6 = WorldMin + FVector(WorldScale.X, WorldScale.Y, WorldScale.Z);
        FVector V7 = WorldMin + FVector(0.0, WorldScale.Y, WorldScale.Z);

        PDI->DrawLine(V0, V1, FLinearColor::White, SDPG_Foreground, 2.0f);
        PDI->DrawLine(V1, V2, FLinearColor::White, SDPG_Foreground, 2.0f);
        PDI->DrawLine(V2, V3, FLinearColor::White, SDPG_Foreground, 2.0f);
        PDI->DrawLine(V3, V0, FLinearColor::White, SDPG_Foreground, 2.0f);

        PDI->DrawLine(V4, V5, FLinearColor::White, SDPG_Foreground, 2.0f);
        PDI->DrawLine(V5, V6, FLinearColor::White, SDPG_Foreground, 2.0f);
        PDI->DrawLine(V6, V7, FLinearColor::White, SDPG_Foreground, 2.0f);
        PDI->DrawLine(V7, V4, FLinearColor::White, SDPG_Foreground, 2.0f);

        PDI->DrawLine(V0, V4, FLinearColor::White, SDPG_Foreground, 2.0f);
        PDI->DrawLine(V1, V5, FLinearColor::White, SDPG_Foreground, 2.0f);
        PDI->DrawLine(V2, V6, FLinearColor::White, SDPG_Foreground, 2.0f);
        PDI->DrawLine(V3, V7, FLinearColor::White, SDPG_Foreground, 2.0f);
    }
}

void SVSEViewport::Construct(const FArguments& InArgs, TSharedPtr<FAdvancedPreviewScene> InScene, UVoxelStructureAsset* InAsset)
{
    PreviewScene = InScene;
    Asset = InAsset;
    SEditorViewport::Construct(SEditorViewport::FArguments());

    // Load base material for structure editor
    FString MaterialPath = TEXT("/DynamicVoxels/VSEMaterial.VSEMaterial");
    BaseMaterial = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), nullptr, *MaterialPath));

    if (!BaseMaterial) UE_LOG(LogTemp, Error, TEXT("Failed to load base material for voxel structure editor!"));
}

void SVSEViewport::RegenerateVoxelMesh()
{
    if (!ProcMeshComponent)
    {
        ProcMeshComponent = NewObject<UProceduralMeshComponent>(GetTransientPackage());
        PreviewScene->AddComponent(ProcMeshComponent, FTransform::Identity);
    }

    if (!bCreatedMatInstance && BaseMaterial)
    {
        MaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, ProcMeshComponent);
        bCreatedMatInstance = true;
    }

    BoundsMin = FIntVector(INT32_MAX);
    BoundsMax = FIntVector(INT32_MIN);
    bool bValidVoxels = false;

    for (const FStructuresVoxel& Voxel : Asset->VoxelData)
    {
        if (Voxel.VoxelType != 0)
        {
            FVector VoxelPos = FVector(Voxel.Position + FIntVector(0, 0, Asset->EditorHeightOffset)) * 100.0f;

// Face culling will currently not work
#if 0
            uint8* XPos = Asset->VoxelData.Find(Voxel.Key + FIntVector(1, 0, 0));
            if (XPos == nullptr || (*XPos) == 0) AddFace(VoxelPos, EFaceDirection::POSITIVE_X, Voxel.Value);

            uint8* XNeg = Asset->VoxelData.Find(Voxel.Key + FIntVector(-1, 0, 0));
            if (XNeg == nullptr || (*XNeg) == 0) AddFace(VoxelPos, EFaceDirection::NEGATIVE_X, Voxel.Value);

            uint8* YPos = Asset->VoxelData.Find(Voxel.Key + FIntVector(0, 1, 0));
            if (YPos == nullptr || (*YPos) == 0) AddFace(VoxelPos, EFaceDirection::POSITIVE_Y, Voxel.Value);

            uint8* YNeg = Asset->VoxelData.Find(Voxel.Key + FIntVector(0, -1, 0));
            if (YNeg == nullptr || (*YNeg) == 0) AddFace(VoxelPos, EFaceDirection::NEGATIVE_Y, Voxel.Value);

            uint8* ZPos = Asset->VoxelData.Find(Voxel.Key + FIntVector(0, 0, 1));
            if (ZPos == nullptr || (*ZPos) == 0) AddFace(VoxelPos, EFaceDirection::POSITIVE_Z, Voxel.Value);

            uint8* ZNeg = Asset->VoxelData.Find(Voxel.Key + FIntVector(0, 0, -1));
            if (ZNeg == nullptr || (*ZNeg) == 0) AddFace(VoxelPos, EFaceDirection::NEGATIVE_Z, Voxel.Value);
#else
            AddFace(VoxelPos, EFaceDirection::POSITIVE_X, Voxel.VoxelType);
            AddFace(VoxelPos, EFaceDirection::NEGATIVE_X, Voxel.VoxelType);
            AddFace(VoxelPos, EFaceDirection::POSITIVE_Y, Voxel.VoxelType);
            AddFace(VoxelPos, EFaceDirection::NEGATIVE_Y, Voxel.VoxelType);
            AddFace(VoxelPos, EFaceDirection::POSITIVE_Z, Voxel.VoxelType);
            AddFace(VoxelPos, EFaceDirection::NEGATIVE_Z, Voxel.VoxelType);
#endif

            BoundsMin = Voxel.Position.ComponentMin(BoundsMin);
            BoundsMax = Voxel.Position.ComponentMax(BoundsMax);
            bValidVoxels = true;
        }
    }

    // Update Structure's bounds
    if (!bValidVoxels)
    {
        // Set everything to zero
        BoundsMin = FIntVector::ZeroValue;
        BoundsMax = FIntVector::ZeroValue;
        Asset->BoundsOrigin = FIntVector::ZeroValue;
        Asset->BoundsScale = FIntVector::ZeroValue;
    }
    else
    {
        Asset->BoundsOrigin = BoundsMin;
        Asset->BoundsScale = (BoundsMax - BoundsMin) + FIntVector(1);
    }

    ProcMeshComponent->ClearAllMeshSections();
    if (!Vertices.IsEmpty())
    {
        ProcMeshComponent->CreateMeshSection_LinearColor(0,
                                                         Vertices,
                                                         Indices,
                                                         Normals,
                                                         Uvs,
                                                         TArray<FLinearColor>(),
                                                         TArray<FProcMeshTangent>(),
                                                         true);

        // Set Material and update paremeters
        MaterialInstance->SetTextureParameterValue(TEXT("VoxelTextures"), Asset->VoxelTextures);
        MaterialInstance->SetScalarParameterValue(TEXT("TextureScale"), Asset->TextureScale);
        ProcMeshComponent->SetMaterial(0, MaterialInstance);
    }

    Vertices.Empty();
    Normals.Empty();
    Uvs.Empty();
    Indices.Empty();
    IndexCount = 0;
}

TSharedRef<FEditorViewportClient> SVSEViewport::MakeEditorViewportClient()
{
    ViewportClient = MakeShared<FVSEViewportClient>(PreviewScene.Get(), SharedThis(this));
    return ViewportClient.ToSharedRef();
}

void SVSEViewport::AddFace(const FVector& Position, EFaceDirection FaceDirection, uint8 VoxelType)
{
    const double VoxelSize = 100.0;
    const FVector2D Scale = FVector2D(VoxelSize, VoxelSize);

    FVector V0;
    FVector V1;
    FVector V2;
    FVector V3;

    FVector Normal;
    switch (FaceDirection)
    {
        case EFaceDirection::POSITIVE_X:
            V0 = FVector(VoxelSize, 0.0, 0.0);
            V1 = FVector(VoxelSize, Scale.X, 0.0);
            V2 = FVector(VoxelSize, 0.0, Scale.Y);
            V3 = FVector(VoxelSize, Scale.X, Scale.Y);
            Normal = FVector(1.0, 0.0, 0.0);
            break;
        case EFaceDirection::NEGATIVE_X:
            V0 = FVector(0.0, Scale.X, 0.0);
            V1 = FVector(0.0, 0.0, 0.0);
            V2 = FVector(0.0, Scale.X, Scale.Y);
            V3 = FVector(0.0, 0.0, Scale.Y);
            Normal = FVector(-1.0, 0.0, 0.0);
            break;
        case EFaceDirection::POSITIVE_Y:
            V0 = FVector(Scale.X, VoxelSize, 0.0);
            V1 = FVector(0.0, VoxelSize, 0.0);
            V2 = FVector(Scale.X, VoxelSize, Scale.Y);
            V3 = FVector(0.0, VoxelSize, Scale.Y);
            Normal = FVector(0.0, 1.0, 0.0);
            break;
        case EFaceDirection::NEGATIVE_Y:
            V0 = FVector(0.0, 0.0, 0.0);
            V1 = FVector(Scale.X, 0.0, 0.0);
            V2 = FVector(0.0, 0.0, Scale.Y);
            V3 = FVector(Scale.X, 0.0, Scale.Y);
            Normal = FVector(0.0, -1.0, 0.0);
            break;
        case EFaceDirection::POSITIVE_Z:
            V0 = FVector(0.0, 0.0, VoxelSize);
            V1 = FVector(Scale.X, 0.0, VoxelSize);
            V2 = FVector(0.0, Scale.Y, VoxelSize);
            V3 = FVector(Scale.X, Scale.Y, VoxelSize);
            Normal = FVector(0.0, 0.0, 1.0);
            break;
        case EFaceDirection::NEGATIVE_Z:
            V0 = FVector(Scale.X, 0.0, 0.0);
            V1 = FVector(0.0, 0.0, 0.0);
            V2 = FVector(Scale.X, Scale.Y, 0.0);
            V3 = FVector(0.0, Scale.Y, 0.0);
            Normal = FVector(0.0, 0.0, -1.0);
            break;
        default:
            break;
    }

    Vertices.Add(V0 + Position);
    Vertices.Add(V1 + Position);
    Vertices.Add(V2 + Position);
    Vertices.Add(V3 + Position);

    Normals.Add(Normal);
    Normals.Add(Normal);
    Normals.Add(Normal);
    Normals.Add(Normal);

    Uvs.Add(FVector2D(static_cast<double>(VoxelType), 0.0));
    Uvs.Add(FVector2D(static_cast<double>(VoxelType), 0.0));
    Uvs.Add(FVector2D(static_cast<double>(VoxelType), 0.0));
    Uvs.Add(FVector2D(static_cast<double>(VoxelType), 0.0));

    Indices.Add(2 + IndexCount);
    Indices.Add(1 + IndexCount);
    Indices.Add(0 + IndexCount);
    Indices.Add(2 + IndexCount);
    Indices.Add(3 + IndexCount);
    Indices.Add(1 + IndexCount);

    IndexCount += 4;
}