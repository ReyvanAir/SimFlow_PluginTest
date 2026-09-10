// Copyright SimFlow. All Rights Reserved.

#include "SimFlowAsset.h"
#include "SimFlowNode.h"
#include "SimFlowNodes.h"
#include "SimFlowTask.h"
#include "SimFlowRuntimeModule.h"
#include "UObject/ObjectSaveContext.h"

#define LOCTEXT_NAMESPACE "SimFlowAsset"

USimFlowAsset::USimFlowAsset()
{
#if WITH_EDITORONLY_DATA
	AssetGuid = FGuid::NewGuid();
#endif
}

USimFlowNode* USimFlowAsset::FindNodeByGuid(const FGuid& Guid) const
{
	for (const TObjectPtr<USimFlowNode>& Node : Nodes)
	{
		if (Node && Node->NodeGuid == Guid)
		{
			return Node;
		}
	}
	return nullptr;
}

USimFlowNode_Entry* USimFlowAsset::FindEntryNode(FName EntryName) const
{
	USimFlowNode_Entry* FirstEntry = nullptr;

	for (const TObjectPtr<USimFlowNode>& Node : Nodes)
	{
		USimFlowNode_Entry* Entry = Cast<USimFlowNode_Entry>(Node);
		if (!Entry)
		{
			continue;
		}
		if (!FirstEntry)
		{
			FirstEntry = Entry;
		}
		if (!EntryName.IsNone() && Entry->EntryName == EntryName)
		{
			return Entry;
		}
	}

	return EntryName.IsNone() ? FirstEntry : nullptr;
}

TArray<FName> USimFlowAsset::GetEntryNames() const
{
	TArray<FName> Result;
	for (const TObjectPtr<USimFlowNode>& Node : Nodes)
	{
		if (const USimFlowNode_Entry* Entry = Cast<USimFlowNode_Entry>(Node))
		{
			Result.AddUnique(Entry->EntryName);
		}
	}
	return Result;
}

FText USimFlowAsset::GetDisplayNameText() const
{
	return FlowDisplayName.IsEmpty() ? FText::FromString(GetName()) : FlowDisplayName;
}

USimFlowNode* USimFlowAsset::AddNode(TSubclassOf<USimFlowNode> NodeClass)
{
	if (!NodeClass)
	{
		return nullptr;
	}

	USimFlowNode* Node = NewObject<USimFlowNode>(this, NodeClass, NAME_None, RF_Transactional);
	Node->NodeGuid = FGuid::NewGuid();
	Node->RebuildPins();
	Nodes.Add(Node);
	return Node;
}

void USimFlowAsset::RemoveNode(USimFlowNode* Node)
{
	if (!Node)
	{
		return;
	}

	const FGuid RemovedGuid = Node->NodeGuid;
	Nodes.Remove(Node);

	for (const TObjectPtr<USimFlowNode>& Other : Nodes)
	{
		if (!Other)
		{
			continue;
		}
		for (FSimFlowOutputPin& Pin : Other->OutputPins)
		{
			Pin.Links.RemoveAll([&RemovedGuid](const FSimFlowPinLink& Link)
			{
				return Link.NodeGuid == RemovedGuid;
			});
		}
	}
}

bool USimFlowAsset::ConnectNodes(USimFlowNode* FromNode, FName FromPin, USimFlowNode* ToNode, FName ToPin)
{
	if (!FromNode || !ToNode)
	{
		return false;
	}

	FSimFlowOutputPin* Pin = FromNode->FindOutputPin(FromPin);
	if (!Pin)
	{
		UE_LOG(LogSimFlow, Warning, TEXT("ConnectNodes: '%s' has no output pin '%s'."), *FromNode->GetName(), *FromPin.ToString());
		return false;
	}

	FName TargetPin = ToPin;
	if (TargetPin.IsNone())
	{
		TargetPin = ToNode->InputPins.Num() > 0 ? ToNode->InputPins[0].PinName : SimFlowPins::In;
	}

	if (!ToNode->HasInputPin(TargetPin))
	{
		UE_LOG(LogSimFlow, Warning, TEXT("ConnectNodes: '%s' has no input pin '%s'."), *ToNode->GetName(), *TargetPin.ToString());
		return false;
	}

	Pin->Links.AddUnique(FSimFlowPinLink(ToNode->NodeGuid, TargetPin));
	return true;
}

bool USimFlowAsset::DisconnectNodes(USimFlowNode* FromNode, FName FromPin, USimFlowNode* ToNode, FName ToPin)
{
	if (!FromNode || !ToNode)
	{
		return false;
	}

	FSimFlowOutputPin* Pin = FromNode->FindOutputPin(FromPin);
	if (!Pin)
	{
		return false;
	}

	const int32 Removed = Pin->Links.RemoveAll([ToNode, ToPin](const FSimFlowPinLink& Link)
	{
		return Link.NodeGuid == ToNode->NodeGuid && (ToPin.IsNone() || Link.PinName == ToPin);
	});

	return Removed > 0;
}

void USimFlowAsset::SanitizeLinks()
{
	TSet<FGuid> ValidGuids;
	for (const TObjectPtr<USimFlowNode>& Node : Nodes)
	{
		if (Node)
		{
			ValidGuids.Add(Node->NodeGuid);
		}
	}

	Nodes.RemoveAll([](const TObjectPtr<USimFlowNode>& Node) { return Node == nullptr; });

	for (const TObjectPtr<USimFlowNode>& Node : Nodes)
	{
		for (FSimFlowOutputPin& Pin : Node->OutputPins)
		{
			Pin.Links.RemoveAll([&ValidGuids](const FSimFlowPinLink& Link)
			{
				return !ValidGuids.Contains(Link.NodeGuid);
			});
		}
	}
}

void USimFlowAsset::ValidateFlow(TArray<FString>& OutErrors, TArray<FString>& OutWarnings) const
{
	OutErrors.Reset();
	OutWarnings.Reset();

	const USimFlowNode_Entry* Entry = FindEntryNode();
	if (!Entry)
	{
		OutErrors.Add(TEXT("The flow has no Start node, so it can never run."));
	}

	TSet<FGuid> ValidGuids;
	for (const TObjectPtr<USimFlowNode>& Node : Nodes)
	{
		if (Node)
		{
			ValidGuids.Add(Node->NodeGuid);
		}
	}

	// Reachability from every entry node.
	TSet<FGuid> Reachable;
	TArray<const USimFlowNode*> Frontier;
	for (const TObjectPtr<USimFlowNode>& Node : Nodes)
	{
		if (Node && Node->IsEntryNode())
		{
			Reachable.Add(Node->NodeGuid);
			Frontier.Add(Node);
		}
	}

	while (Frontier.Num() > 0)
	{
		const USimFlowNode* Current = Frontier.Pop(EAllowShrinking::No);
		for (const FGuid& NextGuid : Current->GetConnectedNodeGuids())
		{
			if (!Reachable.Contains(NextGuid))
			{
				if (const USimFlowNode* Next = FindNodeByGuid(NextGuid))
				{
					Reachable.Add(NextGuid);
					Frontier.Add(Next);
				}
			}
		}
	}

	for (const TObjectPtr<USimFlowNode>& Node : Nodes)
	{
		if (!Node)
		{
			OutWarnings.Add(TEXT("The flow contains a null node entry."));
			continue;
		}

		const FString NodeLabel = Node->GetNodeTitle().ToString();

		for (const FSimFlowOutputPin& Pin : Node->OutputPins)
		{
			for (const FSimFlowPinLink& Link : Pin.Links)
			{
				if (!ValidGuids.Contains(Link.NodeGuid))
				{
					OutErrors.Add(FString::Printf(TEXT("'%s' pin '%s' links to a node that no longer exists."),
						*NodeLabel, *Pin.PinName.ToString()));
				}
			}
		}

		if (const USimFlowNode_Task* TaskNode = Cast<USimFlowNode_Task>(Node))
		{
			if (!TaskNode->Task)
			{
				OutWarnings.Add(FString::Printf(TEXT("Task node '%s' has no task assigned."), *NodeLabel));
			}
		}

		if (!Node->IsEntryNode() && !Reachable.Contains(Node->NodeGuid))
		{
			OutWarnings.Add(FString::Printf(TEXT("'%s' can never be reached from a Start node."), *NodeLabel));
		}

		const bool bIsTerminal = Node->OutputPins.Num() == 0;
		if (!bIsTerminal)
		{
			bool bAnyLink = false;
			for (const FSimFlowOutputPin& Pin : Node->OutputPins)
			{
				if (Pin.Links.Num() > 0)
				{
					bAnyLink = true;
					break;
				}
			}
			if (!bAnyLink && Reachable.Contains(Node->NodeGuid))
			{
				OutWarnings.Add(FString::Printf(TEXT("'%s' has no outgoing connections - the flow will stall here."), *NodeLabel));
			}
		}
	}
}

void USimFlowAsset::PostLoad()
{
	Super::PostLoad();
	SanitizeLinks();
}

#if WITH_EDITOR
void USimFlowAsset::PreSave(FObjectPreSaveContext SaveContext)
{
	SanitizeLinks();
	Super::PreSave(SaveContext);
}
#endif

#undef LOCTEXT_NAMESPACE
