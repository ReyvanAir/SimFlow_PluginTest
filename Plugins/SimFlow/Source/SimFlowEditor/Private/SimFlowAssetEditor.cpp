// Copyright SimFlow. All Rights Reserved.

#include "SimFlowAssetEditor.h"
#include "SimFlowEditorModule.h"
#include "SimFlowGraph.h"
#include "SimFlowGraphNode.h"
#include "SimFlowGraphSchema.h"
#include "SimFlowAsset.h"
#include "SimFlowNode.h"

#include "EdGraphUtilities.h"
#include "GraphEditorActions.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "EdGraph/EdGraphNode.h"

#define LOCTEXT_NAMESPACE "SimFlowAssetEditor"

const FName FSimFlowAssetEditor::GraphTabId(TEXT("SimFlowEditor_Graph"));
const FName FSimFlowAssetEditor::DetailsTabId(TEXT("SimFlowEditor_Details"));
const FName FSimFlowAssetEditor::ValidationTabId(TEXT("SimFlowEditor_Validation"));

FSimFlowAssetEditor::FSimFlowAssetEditor()
{
}

FSimFlowAssetEditor::~FSimFlowAssetEditor()
{
	if (GraphChangedHandle.IsValid())
	{
		if (USimFlowGraph* Graph = GetEditorGraph())
		{
			Graph->RemoveOnGraphChangedHandler(GraphChangedHandle);
		}
		GraphChangedHandle.Reset();
	}
}

USimFlowGraph* FSimFlowAssetEditor::GetEditorGraph() const
{
#if WITH_EDITORONLY_DATA
	return FlowAsset ? Cast<USimFlowGraph>(FlowAsset->EdGraph) : nullptr;
#else
	return nullptr;
#endif
}

void FSimFlowAssetEditor::InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, USimFlowAsset* InAsset)
{
	FlowAsset = InAsset;
	if (!FlowAsset)
	{
		return;
	}

#if WITH_EDITORONLY_DATA
	if (!FlowAsset->EdGraph)
	{
		USimFlowGraph* NewGraph = USimFlowGraph::CreateGraphForAsset(FlowAsset);
		FlowAsset->EdGraph = NewGraph;

		// Bring across nodes authored in code (or by the sample builder).
		if (FlowAsset->Nodes.Num() > 0)
		{
			NewGraph->RebuildFromAsset();
		}
		else
		{
			NewGraph->GetSchema()->CreateDefaultNodesForGraph(*NewGraph);
		}
	}
	else if (Cast<USimFlowGraph>(FlowAsset->EdGraph) &&
		Cast<USimFlowGraph>(FlowAsset->EdGraph)->Nodes.Num() == 0 &&
		FlowAsset->Nodes.Num() > 0)
	{
		// Asset built procedurally after the graph existed: resync.
		Cast<USimFlowGraph>(FlowAsset->EdGraph)->RebuildFromAsset();
	}
#endif

	CreateWidgets();

	if (USimFlowGraph* Graph = GetEditorGraph())
	{
		GraphChangedHandle = Graph->AddOnGraphChangedHandler(
			FOnGraphChanged::FDelegate::CreateSP(this, &FSimFlowAssetEditor::OnGraphChanged));
	}

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("SimFlowEditor_Layout_v1")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)->SetSizeCoefficient(0.9f)
				->Split
				(
					FTabManager::NewStack()->SetSizeCoefficient(0.72f)
					->AddTab(GraphTabId, ETabState::OpenedTab)->SetHideTabWell(true)
				)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)->SetSizeCoefficient(0.28f)
					->Split
					(
						FTabManager::NewStack()->SetSizeCoefficient(0.7f)
						->AddTab(DetailsTabId, ETabState::OpenedTab)
					)
					->Split
					(
						FTabManager::NewStack()->SetSizeCoefficient(0.3f)
						->AddTab(ValidationTabId, ETabState::OpenedTab)
					)
				)
			)
		);

	InitAssetEditor(
		Mode,
		InitToolkitHost,
		FSimFlowEditorModule::SimFlowEditorAppIdentifier,
		Layout,
		/*bCreateDefaultStandaloneMenu*/ true,
		/*bCreateDefaultToolbar*/ true,
		FlowAsset);

	ExtendToolbar();
	RegenerateMenusAndToolbars();
	RefreshValidation();

	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(FlowAsset);
	}
}

void FSimFlowAssetEditor::CreateWidgets()
{
	BindGraphCommands();

	GraphEditorView = CreateGraphEditorWidget();

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs Args;
	Args.bUpdatesFromSelection = false;
	Args.bLockable = false;
	Args.bAllowSearch = true;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	Args.bHideSelectionTip = true;
	Args.NotifyHook = this;

	DetailsView = PropertyModule.CreateDetailView(Args);

	ValidationListView = SNew(SListView<TSharedPtr<FString>>)
		.ListItemsSource(&ValidationMessages)
		.OnGenerateRow_Lambda([](TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& Owner)
		{
			return SNew(STableRow<TSharedPtr<FString>>, Owner)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item.IsValid() ? *Item : FString()))
				.AutoWrapText(true)
			];
		});
}

TSharedRef<SGraphEditor> FSimFlowAssetEditor::CreateGraphEditorWidget()
{
	FGraphAppearanceInfo Appearance;
	Appearance.CornerText = LOCTEXT("GraphCornerText", "SIMFLOW");

	SGraphEditor::FGraphEditorEvents Events;
	Events.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FSimFlowAssetEditor::OnSelectedNodesChanged);
	Events.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &FSimFlowAssetEditor::OnNodeTitleCommitted);

	return SNew(SGraphEditor)
		.AdditionalCommands(GraphEditorCommands)
		.IsEditable(true)
		.Appearance(Appearance)
		.GraphToEdit(GetEditorGraph())
		.GraphEvents(Events)
		.ShowGraphStateOverlay(false);
}

void FSimFlowAssetEditor::BindGraphCommands()
{
	if (GraphEditorCommands.IsValid())
	{
		return;
	}

	GraphEditorCommands = MakeShared<FUICommandList>();

	GraphEditorCommands->MapAction(FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &FSimFlowAssetEditor::DeleteSelectedNodes),
		FCanExecuteAction::CreateSP(this, &FSimFlowAssetEditor::CanDeleteNodes));

	GraphEditorCommands->MapAction(FGenericCommands::Get().Copy,
		FExecuteAction::CreateSP(this, &FSimFlowAssetEditor::CopySelectedNodes),
		FCanExecuteAction::CreateSP(this, &FSimFlowAssetEditor::CanCopyNodes));

	GraphEditorCommands->MapAction(FGenericCommands::Get().Cut,
		FExecuteAction::CreateSP(this, &FSimFlowAssetEditor::CutSelectedNodes),
		FCanExecuteAction::CreateSP(this, &FSimFlowAssetEditor::CanCutNodes));

	GraphEditorCommands->MapAction(FGenericCommands::Get().Paste,
		FExecuteAction::CreateSP(this, &FSimFlowAssetEditor::PasteNodes),
		FCanExecuteAction::CreateSP(this, &FSimFlowAssetEditor::CanPasteNodes));

	GraphEditorCommands->MapAction(FGenericCommands::Get().Duplicate,
		FExecuteAction::CreateSP(this, &FSimFlowAssetEditor::DuplicateNodes),
		FCanExecuteAction::CreateSP(this, &FSimFlowAssetEditor::CanDuplicateNodes));

	GraphEditorCommands->MapAction(FGenericCommands::Get().SelectAll,
		FExecuteAction::CreateSP(this, &FSimFlowAssetEditor::SelectAllNodes),
		FCanExecuteAction::CreateSP(this, &FSimFlowAssetEditor::CanSelectAllNodes));
}

void FSimFlowAssetEditor::ExtendToolbar()
{
	TSharedPtr<FExtender> Extender = MakeShared<FExtender>();

	Extender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateLambda([this](FToolBarBuilder& Builder)
		{
			Builder.BeginSection("SimFlow");
			Builder.AddToolBarButton(
				FUIAction(FExecuteAction::CreateSP(this, &FSimFlowAssetEditor::OnValidateFlow)),
				NAME_None,
				LOCTEXT("ValidateFlow", "Validate"),
				LOCTEXT("ValidateFlowTooltip", "Checks the flow for missing tasks, dead ends and unreachable nodes."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh"));
			Builder.EndSection();
		}));

	AddToolbarExtender(Extender);
}

// ---------------------------------------------------------------- Tabs

void FSimFlowAssetEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu", "SimFlow Editor"));
	const TSharedRef<FWorkspaceItem> Category = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(GraphTabId, FOnSpawnTab::CreateSP(this, &FSimFlowAssetEditor::SpawnTab_Graph))
		.SetDisplayName(LOCTEXT("GraphTab", "Graph"))
		.SetGroup(Category)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"));

	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FSimFlowAssetEditor::SpawnTab_Details))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(Category)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(ValidationTabId, FOnSpawnTab::CreateSP(this, &FSimFlowAssetEditor::SpawnTab_Validation))
		.SetDisplayName(LOCTEXT("ValidationTab", "Validation"))
		.SetGroup(Category)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "MessageLog.TabIcon"));
}

void FSimFlowAssetEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(GraphTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	InTabManager->UnregisterTabSpawner(ValidationTabId);
}

TSharedRef<SDockTab> FSimFlowAssetEditor::SpawnTab_Graph(const FSpawnTabArgs& /*Args*/)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("GraphTabTitle", "Graph"))
		[
			GraphEditorView.IsValid() ? GraphEditorView.ToSharedRef() : SNullWidget::NullWidget
		];
}

TSharedRef<SDockTab> FSimFlowAssetEditor::SpawnTab_Details(const FSpawnTabArgs& /*Args*/)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTabTitle", "Details"))
		[
			DetailsView.IsValid() ? DetailsView.ToSharedRef() : SNullWidget::NullWidget
		];
}

TSharedRef<SDockTab> FSimFlowAssetEditor::SpawnTab_Validation(const FSpawnTabArgs& /*Args*/)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("ValidationTabTitle", "Validation"))
		[
			SNew(SBorder)
			.Padding(4.f)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				ValidationListView.IsValid() ? ValidationListView.ToSharedRef() : SNullWidget::NullWidget
			]
		];
}

// -------------------------------------------------------------- Toolkit ids

FName FSimFlowAssetEditor::GetToolkitFName() const					{ return FName("SimFlowAssetEditor"); }
FText FSimFlowAssetEditor::GetBaseToolkitName() const				{ return LOCTEXT("AppLabel", "SimFlow Editor"); }
FString FSimFlowAssetEditor::GetWorldCentricTabPrefix() const		{ return LOCTEXT("WorldCentricTabPrefix", "SimFlow ").ToString(); }
FLinearColor FSimFlowAssetEditor::GetWorldCentricTabColorScale() const { return FLinearColor(0.2f, 0.4f, 0.7f, 0.5f); }

void FSimFlowAssetEditor::SaveAsset_Execute()
{
	if (USimFlowGraph* Graph = GetEditorGraph())
	{
		Graph->CompileToAsset();
	}
	RefreshValidation();

	FAssetEditorToolkit::SaveAsset_Execute();
}

void FSimFlowAssetEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(FlowAsset);
}

void FSimFlowAssetEditor::NotifyPostChange(const FPropertyChangedEvent& /*PropertyChangedEvent*/, FProperty* /*PropertyThatChanged*/)
{
	if (USimFlowGraph* Graph = GetEditorGraph())
	{
		Graph->CompileToAsset();
		Graph->NotifyGraphChanged();
	}
	RefreshValidation();
}

// ------------------------------------------------------------------ Events

void FSimFlowAssetEditor::OnSelectedNodesChanged(const TSet<UObject*>& NewSelection)
{
	if (!DetailsView.IsValid())
	{
		return;
	}

	TArray<UObject*> ToShow;
	for (UObject* Selected : NewSelection)
	{
		if (const USimFlowGraphNode* GraphNode = Cast<USimFlowGraphNode>(Selected))
		{
			if (GraphNode->RuntimeNode)
			{
				ToShow.Add(GraphNode->RuntimeNode);
			}
		}
		else if (Selected)
		{
			ToShow.Add(Selected);
		}
	}

	if (ToShow.Num() > 0)
	{
		DetailsView->SetObjects(ToShow, /*bForceRefresh*/ true);
	}
	else
	{
		DetailsView->SetObject(FlowAsset, true);
	}
}

void FSimFlowAssetEditor::OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type /*CommitInfo*/, UEdGraphNode* NodeBeingChanged)
{
	if (!NodeBeingChanged)
	{
		return;
	}
	const FScopedTransaction Transaction(LOCTEXT("RenameNode", "Rename Node"));
	NodeBeingChanged->Modify();
	NodeBeingChanged->OnRenameNode(NewText.ToString());
}

void FSimFlowAssetEditor::OnGraphChanged(const FEdGraphEditAction& /*Action*/)
{
	if (USimFlowGraph* Graph = GetEditorGraph())
	{
		Graph->CompileToAsset();
	}
	RefreshValidation();
}

// ---------------------------------------------------------- Graph commands

void FSimFlowAssetEditor::DeleteSelectedNodes()
{
	if (!GraphEditorView.IsValid())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("DeleteNodes", "Delete Nodes"));

	const FGraphPanelSelectionSet Selection = GraphEditorView->GetSelectedNodes();
	GraphEditorView->ClearSelectionSet();

	for (UObject* Selected : Selection)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(Selected);
		if (!Node || !Node->CanUserDeleteNode())
		{
			continue;
		}
		Node->Modify();
		Node->DestroyNode();
	}

	if (USimFlowGraph* Graph = GetEditorGraph())
	{
		Graph->CompileToAsset();
		Graph->NotifyGraphChanged();
	}
	RefreshValidation();
}

bool FSimFlowAssetEditor::CanDeleteNodes() const
{
	if (!GraphEditorView.IsValid())
	{
		return false;
	}
	for (UObject* Selected : GraphEditorView->GetSelectedNodes())
	{
		const UEdGraphNode* Node = Cast<UEdGraphNode>(Selected);
		if (Node && Node->CanUserDeleteNode())
		{
			return true;
		}
	}
	return false;
}

void FSimFlowAssetEditor::CopySelectedNodes()
{
	if (!GraphEditorView.IsValid())
	{
		return;
	}

	FGraphPanelSelectionSet Selection = GraphEditorView->GetSelectedNodes();

	for (UObject* Selected : Selection)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(Selected))
		{
			Node->PrepareForCopying();
		}
	}

	FString ExportedText;
	FEdGraphUtilities::ExportNodesToText(Selection, ExportedText);
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);

	// PrepareForCopying re-outered the runtime nodes onto the graph nodes; put them back.
	for (UObject* Selected : Selection)
	{
		if (USimFlowGraphNode* Node = Cast<USimFlowGraphNode>(Selected))
		{
			Node->PostCopyNode();
		}
	}
}

bool FSimFlowAssetEditor::CanCopyNodes() const
{
	if (!GraphEditorView.IsValid())
	{
		return false;
	}
	for (UObject* Selected : GraphEditorView->GetSelectedNodes())
	{
		const UEdGraphNode* Node = Cast<UEdGraphNode>(Selected);
		if (Node && Node->CanDuplicateNode())
		{
			return true;
		}
	}
	return false;
}

void FSimFlowAssetEditor::CutSelectedNodes()
{
	CopySelectedNodes();
	DeleteSelectedNodes();
}

bool FSimFlowAssetEditor::CanCutNodes() const
{
	return CanCopyNodes() && CanDeleteNodes();
}

void FSimFlowAssetEditor::PasteNodes()
{
	USimFlowGraph* Graph = GetEditorGraph();
	if (!Graph || !GraphEditorView.IsValid())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("PasteNodes", "Paste Nodes"));
	Graph->Modify();
	if (FlowAsset)
	{
		FlowAsset->Modify();
	}

	GraphEditorView->ClearSelectionSet();

	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	TSet<UEdGraphNode*> PastedNodes;
	FEdGraphUtilities::ImportNodesFromText(Graph, TextToImport, PastedNodes);

	FVector2D AvgPosition(0.f, 0.f);
	for (UEdGraphNode* Node : PastedNodes)
	{
		AvgPosition.X += Node->NodePosX;
		AvgPosition.Y += Node->NodePosY;
	}
	if (PastedNodes.Num() > 0)
	{
		AvgPosition /= static_cast<float>(PastedNodes.Num());
	}

	for (UEdGraphNode* Node : PastedNodes)
	{
		Node->NodePosX = static_cast<int32>(Node->NodePosX - AvgPosition.X + 40.f);
		Node->NodePosY = static_cast<int32>(Node->NodePosY - AvgPosition.Y + 40.f);
		Node->SnapToGrid(16);
		Node->CreateNewGuid();
		Node->PostPasteNode();

		GraphEditorView->SetNodeSelection(Node, true);
	}

	Graph->CompileToAsset();
	Graph->NotifyGraphChanged();
	RefreshValidation();
}

bool FSimFlowAssetEditor::CanPasteNodes() const
{
	USimFlowGraph* Graph = GetEditorGraph();
	if (!Graph)
	{
		return false;
	}
	FString Clipboard;
	FPlatformApplicationMisc::ClipboardPaste(Clipboard);
	return FEdGraphUtilities::CanImportNodesFromText(Graph, Clipboard);
}

void FSimFlowAssetEditor::DuplicateNodes()
{
	CopySelectedNodes();
	PasteNodes();
}

bool FSimFlowAssetEditor::CanDuplicateNodes() const
{
	return CanCopyNodes();
}

void FSimFlowAssetEditor::SelectAllNodes()
{
	if (GraphEditorView.IsValid())
	{
		GraphEditorView->SelectAllNodes();
	}
}

bool FSimFlowAssetEditor::CanSelectAllNodes() const
{
	return GraphEditorView.IsValid();
}

// -------------------------------------------------------------- Validation

void FSimFlowAssetEditor::OnValidateFlow()
{
	if (USimFlowGraph* Graph = GetEditorGraph())
	{
		Graph->CompileToAsset();
	}
	RefreshValidation();
}

void FSimFlowAssetEditor::RefreshValidation()
{
	ValidationMessages.Reset();

	if (FlowAsset)
	{
		TArray<FString> Errors;
		TArray<FString> Warnings;
		FlowAsset->ValidateFlow(Errors, Warnings);

		for (const FString& Error : Errors)
		{
			ValidationMessages.Add(MakeShared<FString>(FString::Printf(TEXT("ERROR   %s"), *Error)));
		}
		for (const FString& Warning : Warnings)
		{
			ValidationMessages.Add(MakeShared<FString>(FString::Printf(TEXT("warning %s"), *Warning)));
		}
		if (ValidationMessages.Num() == 0)
		{
			ValidationMessages.Add(MakeShared<FString>(TEXT("No problems found.")));
		}
	}

	if (ValidationListView.IsValid())
	{
		ValidationListView->RequestListRefresh();
	}
}

#undef LOCTEXT_NAMESPACE
