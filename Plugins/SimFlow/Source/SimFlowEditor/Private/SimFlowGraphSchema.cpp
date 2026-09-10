// Copyright SimFlow. All Rights Reserved.

#include "SimFlowGraphSchema.h"
#include "SimFlowGraph.h"
#include "SimFlowGraphNode.h"
#include "SimFlowEditorModule.h"
#include "SimFlowAsset.h"
#include "SimFlowNode.h"
#include "SimFlowNodes.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphUtilities.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "SimFlowGraphSchema"

const FName USimFlowGraphSchema::PC_Flow(TEXT("SimFlowExec"));

// ---------------------------------------------------------------- Actions

UEdGraphNode* FSimFlowSchemaAction_NewNode::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin,
	FSimFlowGraphLocation Location, bool bSelectNewNode)
{
	if (!ParentGraph || !NodeClass)
	{
		return nullptr;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddSimFlowNode", "Add SimFlow Node"));
	return USimFlowGraphSchema::SpawnNode(ParentGraph, NodeClass,
		SIMFLOW_GRAPH_LOCATION_TO_VECTOR2D(Location), FromPin, bSelectNewNode);
}

UEdGraphNode* FSimFlowSchemaAction_NewComment::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* /*FromPin*/,
	FSimFlowGraphLocation Location, bool bSelectNewNode)
{
	if (!ParentGraph)
	{
		return nullptr;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddSimFlowComment", "Add Comment"));
	ParentGraph->Modify();

	const FVector2D Position = SIMFLOW_GRAPH_LOCATION_TO_VECTOR2D(Location);

	UEdGraphNode_Comment* Comment = NewObject<UEdGraphNode_Comment>(ParentGraph, NAME_None, RF_Transactional);
	Comment->CreateNewGuid();
	Comment->NodePosX = static_cast<int32>(Position.X);
	Comment->NodePosY = static_cast<int32>(Position.Y);
	Comment->NodeWidth = 400;
	Comment->NodeHeight = 200;
	Comment->NodeComment = LOCTEXT("DefaultComment", "Comment").ToString();

	ParentGraph->AddNode(Comment, true, bSelectNewNode);
	ParentGraph->NotifyGraphChanged();

	return Comment;
}

// ----------------------------------------------------------------- Schema

USimFlowGraphNode* USimFlowGraphSchema::SpawnNode(UEdGraph* ParentGraph, TSubclassOf<USimFlowNode> NodeClass,
	const FVector2D& Location, UEdGraphPin* FromPin, bool bSelectNewNode)
{
	USimFlowGraph* Graph = Cast<USimFlowGraph>(ParentGraph);
	if (!Graph || !NodeClass)
	{
		return nullptr;
	}

	USimFlowAsset* Asset = Graph->GetFlowAsset();
	if (!Asset)
	{
		return nullptr;
	}

	Graph->Modify();
	Asset->Modify();

	// The runtime node lives on the asset so it survives cooking, where the
	// editor graph is stripped away.
	USimFlowNode* RuntimeNode = NewObject<USimFlowNode>(Asset, NodeClass, NAME_None, RF_Transactional);
	RuntimeNode->NodeGuid = FGuid::NewGuid();
	RuntimeNode->RebuildPins();
	Asset->Nodes.Add(RuntimeNode);

	FGraphNodeCreator<USimFlowGraphNode> Creator(*Graph);
	USimFlowGraphNode* GraphNode = Creator.CreateNode(bSelectNewNode);
	GraphNode->SetRuntimeNode(RuntimeNode);
	GraphNode->NodePosX = static_cast<int32>(Location.X);
	GraphNode->NodePosY = static_cast<int32>(Location.Y);
	Creator.Finalize();

	if (FromPin)
	{
		GraphNode->AutowireNewNode(FromPin);
	}

	Graph->CompileToAsset();
	Graph->NotifyGraphChanged();

	return GraphNode;
}

void USimFlowGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	const USimFlowGraph* Graph = Cast<USimFlowGraph>(ContextMenuBuilder.CurrentGraph);

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;

		if (!Class->IsChildOf(USimFlowNode::StaticClass()))
		{
			continue;
		}
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Hidden))
		{
			continue;
		}
		// Skip Blueprint skeleton classes.
		if (Class->GetName().StartsWith(TEXT("SKEL_")) || Class->GetName().StartsWith(TEXT("REINST_")))
		{
			continue;
		}

		const USimFlowNode* CDO = Cast<USimFlowNode>(Class->GetDefaultObject());
		if (!CDO || !CDO->IsPlaceableInGraph())
		{
			continue;
		}

		const FText Category = CDO->GetNodeCategory();
		const FText MenuDesc = CDO->GetNodeTitle();
		const FText Tooltip = CDO->GetNodeTooltip();

		TSharedPtr<FSimFlowSchemaAction_NewNode> Action = MakeShared<FSimFlowSchemaAction_NewNode>(
			Category, MenuDesc, Tooltip, 0);
		Action->NodeClass = Class;

		ContextMenuBuilder.AddAction(Action);
	}

	// Comment box.
	TSharedPtr<FSimFlowSchemaAction_NewComment> CommentAction = MakeShared<FSimFlowSchemaAction_NewComment>(
		LOCTEXT("CommentCategory", "Organisation"),
		LOCTEXT("AddComment", "Add Comment"),
		LOCTEXT("AddCommentTooltip", "Adds a comment box to the graph."),
		1);
	ContextMenuBuilder.AddAction(CommentAction);
}

const FPinConnectionResponse USimFlowGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
	if (!A || !B)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("InvalidPin", "Invalid pin."));
	}

	if (A->GetOwningNode() == B->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("SameNode", "A node cannot connect to itself."));
	}

	if (A->Direction == B->Direction)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("SameDirection", "Connect an output pin to an input pin."));
	}

	for (const UEdGraphPin* Linked : A->LinkedTo)
	{
		if (Linked == B)
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("AlreadyConnected", "These pins are already connected."));
		}
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, LOCTEXT("Connect", "Connect"));
}

void USimFlowGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	USimFlowGraph* FlowGraph = Cast<USimFlowGraph>(&Graph);
	if (!FlowGraph)
	{
		return;
	}

	USimFlowAsset* Asset = FlowGraph->GetFlowAsset();
	if (!Asset || Asset->FindEntryNode() != nullptr)
	{
		return;
	}

	SpawnNode(FlowGraph, USimFlowNode_Entry::StaticClass(), FVector2D(-300.f, 0.f), nullptr, false);
}

FLinearColor USimFlowGraphSchema::GetPinTypeColor(const FEdGraphPinType& /*PinType*/) const
{
	return FLinearColor(0.85f, 0.85f, 0.85f);
}

void USimFlowGraphSchema::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
	const FScopedTransaction Transaction(LOCTEXT("BreakNodeLinks", "Break Node Links"));
	Super::BreakNodeLinks(TargetNode);

	if (USimFlowGraph* Graph = Cast<USimFlowGraph>(TargetNode.GetGraph()))
	{
		Graph->CompileToAsset();
	}
}

void USimFlowGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const
{
	const FScopedTransaction Transaction(LOCTEXT("BreakPinLinks", "Break Pin Links"));
	Super::BreakPinLinks(TargetPin, bSendsNodeNotification);

	if (const UEdGraphNode* Node = TargetPin.GetOwningNodeUnchecked())
	{
		if (USimFlowGraph* Graph = Cast<USimFlowGraph>(Node->GetGraph()))
		{
			Graph->CompileToAsset();
		}
	}
}

void USimFlowGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
	const FScopedTransaction Transaction(LOCTEXT("BreakSinglePinLink", "Break Pin Link"));
	Super::BreakSinglePinLink(SourcePin, TargetPin);

	if (SourcePin)
	{
		if (const UEdGraphNode* Node = SourcePin->GetOwningNodeUnchecked())
		{
			if (USimFlowGraph* Graph = Cast<USimFlowGraph>(Node->GetGraph()))
			{
				Graph->CompileToAsset();
			}
		}
	}
}

FText USimFlowGraphSchema::GetPinDisplayName(const UEdGraphPin* Pin) const
{
	if (!Pin)
	{
		return FText::GetEmpty();
	}
	return Pin->PinFriendlyName.IsEmpty() ? FText::FromName(Pin->PinName) : Pin->PinFriendlyName;
}

int32 USimFlowGraphSchema::GetNodeSelectionCount(const UEdGraph* /*Graph*/) const
{
	// The asset editor toolkit tracks selection; the schema does not need to.
	return 0;
}

TSharedPtr<FEdGraphSchemaAction> USimFlowGraphSchema::GetCreateCommentAction() const
{
	return MakeShared<FSimFlowSchemaAction_NewComment>(
		FText::GetEmpty(),
		LOCTEXT("AddComment", "Add Comment"),
		LOCTEXT("AddCommentTooltip", "Adds a comment box to the graph."),
		0);
}

#undef LOCTEXT_NAMESPACE
