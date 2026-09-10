// Copyright SimFlow. All Rights Reserved.

#include "SimFlowGraphNode.h"
#include "SimFlowGraph.h"
#include "SimFlowGraphSchema.h"
#include "SimFlowEditorModule.h"
#include "SimFlowNode.h"
#include "SimFlowAsset.h"
#include "EdGraph/EdGraphPin.h"

#define LOCTEXT_NAMESPACE "SimFlowGraphNode"

void USimFlowGraphNode::SetRuntimeNode(USimFlowNode* InNode)
{
	RuntimeNode = InNode;

	if (RuntimeNode)
	{
		NodeGuid = RuntimeNode->NodeGuid;
		NodeComment = RuntimeNode->NodeComment;

#if WITH_EDITORONLY_DATA
		if (PinsChangedHandle.IsValid())
		{
			RuntimeNode->OnPinsChanged.Remove(PinsChangedHandle);
		}
		PinsChangedHandle = RuntimeNode->OnPinsChanged.AddUObject(this, &USimFlowGraphNode::HandleRuntimePinsChanged);
#endif
	}
}

USimFlowAsset* USimFlowGraphNode::GetFlowAsset() const
{
	if (const USimFlowGraph* Graph = Cast<USimFlowGraph>(GetGraph()))
	{
		return Graph->GetFlowAsset();
	}
	return RuntimeNode ? RuntimeNode->GetOwningAsset() : nullptr;
}

void USimFlowGraphNode::AllocateDefaultPins()
{
	if (!RuntimeNode)
	{
		return;
	}

	for (const FSimFlowInputPin& In : RuntimeNode->InputPins)
	{
		UEdGraphPin* Pin = CreatePin(EGPD_Input, USimFlowGraphSchema::PC_Flow, In.PinName);
		Pin->PinFriendlyName = In.DisplayName.IsEmpty()
			? FText::FromName(In.PinName)
			: FText::FromString(In.DisplayName);
	}

	for (const FSimFlowOutputPin& Out : RuntimeNode->OutputPins)
	{
		UEdGraphPin* Pin = CreatePin(EGPD_Output, USimFlowGraphSchema::PC_Flow, Out.PinName);
		Pin->PinFriendlyName = Out.DisplayName.IsEmpty()
			? FText::FromName(Out.PinName)
			: FText::FromString(Out.DisplayName);
	}
}

UEdGraphPin* USimFlowGraphNode::FindPinByRuntimeName(FName PinName, EEdGraphPinDirection Direction) const
{
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin && Pin->PinName == PinName && Pin->Direction == Direction)
		{
			return Pin;
		}
	}
	return nullptr;
}

void USimFlowGraphNode::SyncPinsWithRuntimeNode()
{
	if (!RuntimeNode)
	{
		return;
	}

	// Remember what everything was wired to.
	struct FSavedLink
	{
		FName PinName;
		EEdGraphPinDirection Direction;
		TArray<UEdGraphPin*> Links;
	};

	TArray<FSavedLink> Saved;
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin && Pin->LinkedTo.Num() > 0)
		{
			Saved.Add(FSavedLink{ Pin->PinName, Pin->Direction.GetValue(), Pin->LinkedTo });
		}
	}

	// Rebuild.
	Modify();
	TArray<UEdGraphPin*> OldPins = Pins;
	Pins.Reset();
	AllocateDefaultPins();

	for (const FSavedLink& Link : Saved)
	{
		if (UEdGraphPin* NewPin = FindPinByRuntimeName(Link.PinName, Link.Direction))
		{
			for (UEdGraphPin* Other : Link.Links)
			{
				if (Other)
				{
					NewPin->MakeLinkTo(Other);
				}
			}
		}
	}

	for (UEdGraphPin* OldPin : OldPins)
	{
		if (OldPin)
		{
			OldPin->BreakAllPinLinks();
			OldPin->MarkAsGarbage();
		}
	}

	if (UEdGraph* Graph = GetGraph())
	{
		Graph->NotifyGraphChanged();
	}
}

FText USimFlowGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (!RuntimeNode)
	{
		return LOCTEXT("InvalidNode", "Invalid Node");
	}

	const FText Title = RuntimeNode->GetNodeTitle();

	if (TitleType == ENodeTitleType::FullTitle)
	{
		const FText Subtitle = RuntimeNode->GetNodeSubtitle();
		if (!Subtitle.IsEmpty())
		{
			return FText::Format(LOCTEXT("TitleWithSubtitle", "{0}\n{1}"), Title, Subtitle);
		}
	}

	return Title;
}

FText USimFlowGraphNode::GetTooltipText() const
{
	if (!RuntimeNode)
	{
		return FText::GetEmpty();
	}

	FText Tooltip = RuntimeNode->GetNodeTooltip();
	if (!RuntimeNode->NodeComment.IsEmpty())
	{
		Tooltip = FText::Format(LOCTEXT("TooltipWithComment", "{0}\n\n{1}"),
			Tooltip, FText::FromString(RuntimeNode->NodeComment));
	}
	return Tooltip;
}

FLinearColor USimFlowGraphNode::GetNodeTitleColor() const
{
	return RuntimeNode ? RuntimeNode->GetNodeColor() : FLinearColor::Gray;
}

void USimFlowGraphNode::PrepareForCopying()
{
	if (RuntimeNode)
	{
		// Re-outer so the runtime node travels with the copied graph node.
		RuntimeNode->Rename(nullptr, this, REN_DontCreateRedirectors | REN_DoNotDirty);
	}
}

void USimFlowGraphNode::PostCopyNode()
{
	if (!RuntimeNode)
	{
		return;
	}

	USimFlowAsset* Asset = GetFlowAsset();
	if (Asset && RuntimeNode->GetOuter() != Asset)
	{
		RuntimeNode->Rename(nullptr, Asset, REN_DontCreateRedirectors | REN_DoNotDirty);
		RuntimeNode->ClearFlags(RF_Transient);
	}
}

void USimFlowGraphNode::PostPasteNode()
{
	Super::PostPasteNode();

	if (RuntimeNode)
	{
		if (USimFlowAsset* Asset = GetFlowAsset())
		{
			RuntimeNode->Rename(nullptr, Asset, REN_DontCreateRedirectors | REN_DoNotDirty);
			RuntimeNode->NodeGuid = FGuid::NewGuid();
			NodeGuid = RuntimeNode->NodeGuid;

			// A pasted node must not keep the original's outgoing links.
			for (FSimFlowOutputPin& Pin : RuntimeNode->OutputPins)
			{
				Pin.Links.Reset();
			}

			Asset->Nodes.AddUnique(RuntimeNode);
		}
	}
}

void USimFlowGraphNode::DestroyNode()
{
	if (RuntimeNode)
	{
		if (USimFlowAsset* Asset = GetFlowAsset())
		{
			Asset->Modify();
			Asset->RemoveNode(RuntimeNode);
		}

#if WITH_EDITORONLY_DATA
		if (PinsChangedHandle.IsValid())
		{
			RuntimeNode->OnPinsChanged.Remove(PinsChangedHandle);
			PinsChangedHandle.Reset();
		}
#endif
		RuntimeNode = nullptr;
	}

	Super::DestroyNode();
}

void USimFlowGraphNode::ReconstructNode()
{
	SyncPinsWithRuntimeNode();
}

void USimFlowGraphNode::AutowireNewNode(UEdGraphPin* FromPin)
{
	if (!FromPin)
	{
		return;
	}

	const UEdGraphSchema* Schema = GetSchema();
	const EEdGraphPinDirection WantedDirection = (FromPin->Direction == EGPD_Output) ? EGPD_Input : EGPD_Output;

	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin && Pin->Direction == WantedDirection)
		{
			if (Schema->TryCreateConnection(FromPin, Pin))
			{
				break;
			}
		}
	}

	if (UEdGraph* Graph = GetGraph())
	{
		Graph->NotifyGraphChanged();
	}
}

bool USimFlowGraphNode::CanUserDeleteNode() const
{
	return true;
}

bool USimFlowGraphNode::CanDuplicateNode() const
{
	return RuntimeNode && !RuntimeNode->IsEntryNode();
}

void USimFlowGraphNode::OnUpdateCommentText(const FString& NewComment)
{
	Super::OnUpdateCommentText(NewComment);

	if (RuntimeNode && RuntimeNode->NodeComment != NewComment)
	{
		RuntimeNode->Modify();
		RuntimeNode->NodeComment = NewComment;
	}
}

void USimFlowGraphNode::NodeConnectionListChanged()
{
	Super::NodeConnectionListChanged();

	if (USimFlowGraph* Graph = Cast<USimFlowGraph>(GetGraph()))
	{
		Graph->CompileToAsset();
	}
}

void USimFlowGraphNode::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);

	if (USimFlowGraph* Graph = Cast<USimFlowGraph>(GetGraph()))
	{
		Graph->CompileToAsset();
	}
}

#if WITH_EDITOR
void USimFlowGraphNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (RuntimeNode)
	{
		SyncPinsWithRuntimeNode();
	}
}
#endif

void USimFlowGraphNode::HandleRuntimePinsChanged()
{
	SyncPinsWithRuntimeNode();
}

#undef LOCTEXT_NAMESPACE
