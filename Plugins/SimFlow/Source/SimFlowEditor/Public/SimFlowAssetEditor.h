// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Misc/NotifyHook.h"
#include "UObject/GCObject.h"
#include "GraphEditor.h"

class USimFlowAsset;
class USimFlowGraph;
class IDetailsView;
class SDockTab;
class UEdGraphNode;

/** The SimFlow graph editor window. */
class SIMFLOWEDITOR_API FSimFlowAssetEditor
	: public FAssetEditorToolkit
	, public FGCObject
	, public FNotifyHook
{
public:
	FSimFlowAssetEditor();
	virtual ~FSimFlowAssetEditor() override;

	void InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, USimFlowAsset* InAsset);

	// ---------------------------------------------------- FAssetEditorToolkit

	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void SaveAsset_Execute() override;

	// ------------------------------------------------------------- FGCObject

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FSimFlowAssetEditor"); }

	// ----------------------------------------------------------- FNotifyHook

	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;

	USimFlowAsset* GetFlowAsset() const { return FlowAsset; }

private:
	TSharedRef<SDockTab> SpawnTab_Graph(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Details(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Validation(const FSpawnTabArgs& Args);

	void CreateWidgets();
	TSharedRef<SGraphEditor> CreateGraphEditorWidget();
	void BindGraphCommands();
	void ExtendToolbar();

	void OnSelectedNodesChanged(const TSet<UObject*>& NewSelection);
	void OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged);
	void OnGraphChanged(const FEdGraphEditAction& Action);

	// Graph commands
	void DeleteSelectedNodes();
	bool CanDeleteNodes() const;
	void CopySelectedNodes();
	bool CanCopyNodes() const;
	void CutSelectedNodes();
	bool CanCutNodes() const;
	void PasteNodes();
	bool CanPasteNodes() const;
	void DuplicateNodes();
	bool CanDuplicateNodes() const;
	void SelectAllNodes();
	bool CanSelectAllNodes() const;

	void OnValidateFlow();
	void RefreshValidation();

	USimFlowGraph* GetEditorGraph() const;

	TObjectPtr<USimFlowAsset> FlowAsset = nullptr;

	TSharedPtr<SGraphEditor> GraphEditorView;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<class SListView<TSharedPtr<FString>>> ValidationListView;
	TArray<TSharedPtr<FString>> ValidationMessages;

	TSharedPtr<FUICommandList> GraphEditorCommands;

	FDelegateHandle GraphChangedHandle;

	static const FName GraphTabId;
	static const FName DetailsTabId;
	static const FName ValidationTabId;
};
