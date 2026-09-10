// Copyright SimFlow. All Rights Reserved.

#include "SimFlowGraph.h"
#include "SimFlowGraphNode.h"
#include "SimFlowGraphSchema.h"
#include "SimFlowEditorModule.h"
#include "SimFlowAsset.h"
#include "SimFlowNode.h"
#include "SimFlowNodes.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"

USimFlowAsset* USimFlowGraph::GetFlowAsset() const
{
	return GetTypedOuter<USimFlowAsset>();
}

USimFlowGraphNode* USimFlowGraph::FindGraphNodeForRuntimeNode(const USimFlowNode* Node) const
{
	if (!Node)
	{
		return nullptr;
	}

	for (UEdGraphNode* GraphNode : Nodes)
	{
		USimFlowGraphNode* FlowGraphNode = Cast<USimFlowGraphNode>(GraphNode);
		if (FlowGraphNode && FlowGraphNode->RuntimeNode == Node)
		{
			return FlowGraphNode;
		}
	}
	return nullptr;
}

void USimFlowGraph::CompileToAsset()
{
	USimFlowAsset* Asset = GetFlowAsset();
	if (!Asset)
	{
		return;
	}

	Asset->Modify();

	// 1. Collect the runtime nodes that are actually present in the graph.
	TArray<TObjectPtr<USimFlowNode>> CollectedNodes;
	TMap<const UEdGraphNode*, USimFlowNode*> NodeMap;

	for (UEdGraphNode* GraphNode : Nodes)
	{
		USimFlowGraphNode* FlowGraphNode = Cast<USimFlowGraphNode>(GraphNode);
		if (!FlowGraphNode || !FlowGraphNode->RuntimeNode)
		{
			continue;
		}

		USimFlowNode* Runtime = FlowGraphNode->RuntimeNode;
		Runtime->Modify();

#if WITH_EDITORONLY_DATA
		Runtime->GraphPosition = FVector2D(FlowGraphNode->NodePosX, FlowGraphNode->NodePosY);
#endif
		Runtime->NodeComment = FlowGraphNode->NodeComment;

		CollectedNodes.Add(Runtime);
		NodeMap.Add(GraphNode, Runtime);
	}

	// 2. Clear every existing link, then rebuild from the visual connections.
	for (const TObjectPtr<USimFlowNode>& Node : CollectedNodes)
	{
		for (FSimFlowOutputPin& Pin : Node->OutputPins)
		{
			Pin.Links.Reset();
		}
	}

	for (UEdGraphNode* GraphNode : Nodes)
	{
		USimFlowGraphNode* FlowGraphNode = Cast<USimFlowGraphNode>(GraphNode);
		if (!FlowGraphNode || !FlowGraphNode->RuntimeNode)
		{
			continue;
		}

		USimFlowNode* SourceRuntime = FlowGraphNode->RuntimeNode;

		for (UEdGraphPin* Pin : FlowGraphNode->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output)
			{
				continue;
			}

			FSimFlowOutputPin* OutPin = SourceRuntime->FindOutputPin(Pin->PinName);
			if (!OutPin)
			{
				continue;
			}

			for (UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (!Linked || !Linked->GetOwningNodeUnchecked())
				{
					continue;
				}

				USimFlowNode** TargetRuntime = NodeMap.Find(Linked->GetOwningNode());
				if (!TargetRuntime || !*TargetRuntime)
				{
					continue;
				}

				OutPin->Links.AddUnique(FSimFlowPinLink((*TargetRuntime)->NodeGuid, Linked->PinName));
			}
		}
	}

	Asset->Nodes = MoveTemp(CollectedNodes);
	Asset->SanitizeLinks();
	Asset->MarkPackageDirty();
}

void USimFlowGraph::RebuildFromAsset()
{
	USimFlowAsset* Asset = GetFlowAsset();
	if (!Asset)
	{
		return;
	}

	// Start from a clean graph.
	Nodes.Reset();

	TMap<FGuid, USimFlowGraphNode*> Created;

	for (const TObjectPtr<USimFlowNode>& RuntimeNode : Asset->Nodes)
	{
		if (!RuntimeNode)
		{
			continue;
		}

		USimFlowGraphNode* GraphNode = NewObject<USimFlowGraphNode>(this, USimFlowGraphNode::StaticClass(), NAME_None, RF_Transactional);
		GraphNode->SetRuntimeNode(RuntimeNode);
		GraphNode->CreateNewGuid();
		GraphNode->NodeGuid = RuntimeNode->NodeGuid;

#if WITH_EDITORONLY_DATA
		GraphNode->NodePosX = static_cast<int32>(RuntimeNode->GraphPosition.X);
		GraphNode->NodePosY = static_cast<int32>(RuntimeNode->GraphPosition.Y);
#endif
		GraphNode->NodeComment = RuntimeNode->NodeComment;
		GraphNode->bCommentBubbleVisible = !RuntimeNode->NodeComment.IsEmpty();

		GraphNode->AllocateDefaultPins();

		AddNode(GraphNode, /*bFromUI*/ false, /*bSelectNewNode*/ false);
		Created.Add(RuntimeNode->NodeGuid, GraphNode);
	}

	// Recreate the visual wires from the stored links.
	for (const TObjectPtr<USimFlowNode>& RuntimeNode : Asset->Nodes)
	{
		if (!RuntimeNode)
		{
			continue;
		}

		USimFlowGraphNode** SourceNode = Created.Find(RuntimeNode->NodeGuid);
		if (!SourceNode || !*SourceNode)
		{
			continue;
		}

		for (const FSimFlowOutputPin& OutPin : RuntimeNode->OutputPins)
		{
			UEdGraphPin* SourcePin = (*SourceNode)->FindPinByRuntimeName(OutPin.PinName, EGPD_Output);
			if (!SourcePin)
			{
				continue;
			}

			for (const FSimFlowPinLink& Link : OutPin.Links)
			{
				USimFlowGraphNode** TargetNode = Created.Find(Link.NodeGuid);
				if (!TargetNode || !*TargetNode)
				{
					continue;
				}

				if (UEdGraphPin* TargetPin = (*TargetNode)->FindPinByRuntimeName(Link.PinName, EGPD_Input))
				{
					SourcePin->MakeLinkTo(TargetPin);
				}
			}
		}
	}

	NotifyGraphChanged();
}

USimFlowGraph* USimFlowGraph::CreateGraphForAsset(USimFlowAsset* Asset)
{
	if (!Asset)
	{
		return nullptr;
	}

	USimFlowGraph* Graph = CastChecked<USimFlowGraph>(FBlueprintEditorUtils::CreateNewGraph(
		Asset,
		NAME_None,
		USimFlowGraph::StaticClass(),
		USimFlowGraphSchema::StaticClass()));

	Graph->bAllowDeletion = false;
	return Graph;
}
