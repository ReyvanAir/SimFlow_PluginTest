// Copyright SimFlow. All Rights Reserved.

#include "SimFlowNode.h"
#include "SimFlowInstance.h"
#include "SimFlowAsset.h"
#include "SimFlowBlackboard.h"
#include "SimFlowRuntimeModule.h"

#define LOCTEXT_NAMESPACE "SimFlowNode"

USimFlowNode::USimFlowNode()
{
	NodeGuid = FGuid::NewGuid();
}

FText USimFlowNode::GetNodeTitle() const
{
	FString Name = GetClass()->GetName();
	Name.RemoveFromStart(TEXT("SimFlowNode_"));
	return FText::FromString(FName::NameToDisplayString(Name, false));
}

FText USimFlowNode::GetNodeSubtitle() const
{
	return FText::GetEmpty();
}

FText USimFlowNode::GetNodeTooltip() const
{
	return GetNodeTitle();
}

FText USimFlowNode::GetNodeCategory() const
{
	return LOCTEXT("DefaultCategory", "Flow");
}

FLinearColor USimFlowNode::GetNodeColor() const
{
	return FLinearColor(0.12f, 0.35f, 0.55f);
}

FString USimFlowNode::GetDebugStatus() const
{
	return GetNodeTitle().ToString();
}

void USimFlowNode::RebuildPins()
{
	// Preserve links across a rebuild wherever the pin name still exists.
	TMap<FName, TArray<FSimFlowPinLink>> PreservedLinks;
	for (const FSimFlowOutputPin& Pin : OutputPins)
	{
		if (Pin.Links.Num() > 0)
		{
			PreservedLinks.Add(Pin.PinName, Pin.Links);
		}
	}

	InputPins.Reset();
	OutputPins.Reset();

	BuildPins();

	for (FSimFlowOutputPin& Pin : OutputPins)
	{
		if (const TArray<FSimFlowPinLink>* Found = PreservedLinks.Find(Pin.PinName))
		{
			Pin.Links = *Found;
		}
	}
}

void USimFlowNode::BuildPins()
{
	// Sensible default: a single pass-through node.
	AddInputPin(SimFlowPins::In);
	AddOutputPin(SimFlowPins::Out);
}

const FSimFlowOutputPin* USimFlowNode::FindOutputPin(FName PinName) const
{
	return OutputPins.FindByPredicate([PinName](const FSimFlowOutputPin& Pin) { return Pin.PinName == PinName; });
}

FSimFlowOutputPin* USimFlowNode::FindOutputPin(FName PinName)
{
	return OutputPins.FindByPredicate([PinName](const FSimFlowOutputPin& Pin) { return Pin.PinName == PinName; });
}

bool USimFlowNode::HasInputPin(FName PinName) const
{
	return InputPins.ContainsByPredicate([PinName](const FSimFlowInputPin& Pin) { return Pin.PinName == PinName; });
}

void USimFlowNode::AddInputPin(FName PinName, const FString& DisplayName)
{
	if (!HasInputPin(PinName))
	{
		InputPins.Emplace(PinName, DisplayName);
	}
}

void USimFlowNode::AddOutputPin(FName PinName, const FString& DisplayName)
{
	if (!FindOutputPin(PinName))
	{
		OutputPins.Emplace(PinName, DisplayName);
	}
}

TArray<FGuid> USimFlowNode::GetConnectedNodeGuids() const
{
	TArray<FGuid> Result;
	for (const FSimFlowOutputPin& Pin : OutputPins)
	{
		for (const FSimFlowPinLink& Link : Pin.Links)
		{
			Result.AddUnique(Link.NodeGuid);
		}
	}
	return Result;
}

void USimFlowNode::ExecuteInput(FName /*PinName*/)
{
	// Default behaviour: pass straight through the first output.
	if (OutputPins.Num() > 0)
	{
		TriggerOutput(OutputPins[0].PinName);
	}
	else
	{
		FinishNode();
	}
}

void USimFlowNode::TickNode(float DeltaTime)
{
	ActiveTime += DeltaTime;
}

void USimFlowNode::Cleanup()
{
	ActiveTime = 0.f;
}

void USimFlowNode::TriggerOutput(FName PinName, bool bStayActive)
{
	if (!FlowInstance)
	{
		UE_LOG(LogSimFlow, Warning, TEXT("TriggerOutput on '%s' with no flow instance."), *GetName());
		return;
	}
	FlowInstance->TriggerNodeOutput(this, PinName, bStayActive);
}

void USimFlowNode::FinishNode()
{
	if (FlowInstance)
	{
		FlowInstance->DeactivateNode(this);
	}
}

void USimFlowNode::SaveNodeState(FSimFlowNodeSaveState& OutState) const
{
	OutState.NodeGuid = NodeGuid;
	OutState.bActive = bActive;
	OutState.ActiveTime = ActiveTime;
}

void USimFlowNode::LoadNodeState(const FSimFlowNodeSaveState& InState)
{
	bActive = InState.bActive;
	ActiveTime = InState.ActiveTime;
}

USimFlowBlackboard* USimFlowNode::GetBlackboard() const
{
	return FlowInstance ? FlowInstance->GetBlackboard() : nullptr;
}

USimFlowAsset* USimFlowNode::GetOwningAsset() const
{
	return GetTypedOuter<USimFlowAsset>();
}

UWorld* USimFlowNode::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}
	if (FlowInstance)
	{
		return FlowInstance->GetWorld();
	}
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

#if WITH_EDITOR
void USimFlowNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	RebuildPins();
	OnPinsChanged.Broadcast();
}

void USimFlowNode::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	RebuildPins();
	OnPinsChanged.Broadcast();
}
#endif

#undef LOCTEXT_NAMESPACE
