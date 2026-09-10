// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphSchema.h"
#include "SimFlowEditorCompat.h"
#include "SimFlowGraphSchema.generated.h"

class USimFlowNode;
class USimFlowGraphNode;

/** Context menu action that spawns a new flow node of a given class. */
USTRUCT()
struct SIMFLOWEDITOR_API FSimFlowSchemaAction_NewNode : public FEdGraphSchemaAction
{
	GENERATED_BODY()

	UPROPERTY()
	TSubclassOf<USimFlowNode> NodeClass = nullptr;

	FSimFlowSchemaAction_NewNode() = default;

	FSimFlowSchemaAction_NewNode(FText InCategory, FText InMenuDesc, FText InToolTip, int32 InGrouping)
		: FEdGraphSchemaAction(MoveTemp(InCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping)
	{
	}

	static FName StaticGetTypeId()
	{
		static FName Type(TEXT("FSimFlowSchemaAction_NewNode"));
		return Type;
	}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); }

	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin,
		FSimFlowGraphLocation Location, bool bSelectNewNode = true) override;
};

/** Context menu action that drops a comment box on the graph. */
USTRUCT()
struct SIMFLOWEDITOR_API FSimFlowSchemaAction_NewComment : public FEdGraphSchemaAction
{
	GENERATED_BODY()

	FSimFlowSchemaAction_NewComment() = default;

	FSimFlowSchemaAction_NewComment(FText InCategory, FText InMenuDesc, FText InToolTip, int32 InGrouping)
		: FEdGraphSchemaAction(MoveTemp(InCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping)
	{
	}

	static FName StaticGetTypeId()
	{
		static FName Type(TEXT("FSimFlowSchemaAction_NewComment"));
		return Type;
	}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); }

	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin,
		FSimFlowGraphLocation Location, bool bSelectNewNode = true) override;
};

/** Rules and menus for the SimFlow node graph. */
UCLASS()
class SIMFLOWEDITOR_API USimFlowGraphSchema : public UEdGraphSchema
{
	GENERATED_BODY()

public:
	/** Pin category used for every flow execution pin. */
	static const FName PC_Flow;

	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
	virtual void CreateDefaultNodesForGraph(UEdGraph& Graph) const override;
	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override;
	virtual bool ShouldAlwaysPurgeOnModification() const override { return false; }
	virtual void BreakNodeLinks(UEdGraphNode& TargetNode) const override;
	virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const override;
	virtual void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const override;
	virtual FText GetPinDisplayName(const UEdGraphPin* Pin) const override;
	virtual int32 GetNodeSelectionCount(const UEdGraph* Graph) const override;
	virtual TSharedPtr<FEdGraphSchemaAction> GetCreateCommentAction() const override;

	/** Spawns a node of NodeClass into ParentGraph and returns the graph node. */
	static USimFlowGraphNode* SpawnNode(UEdGraph* ParentGraph, TSubclassOf<USimFlowNode> NodeClass,
		const FVector2D& Location, UEdGraphPin* FromPin, bool bSelectNewNode);
};
