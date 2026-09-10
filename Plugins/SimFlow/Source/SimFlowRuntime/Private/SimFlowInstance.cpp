// Copyright SimFlow. All Rights Reserved.

#include "SimFlowInstance.h"
#include "SimFlowIdentity.h"
#include "SimFlowGameplayTags.h"
#include "SimFlowAsset.h"
#include "SimFlowNode.h"
#include "SimFlowNodes.h"
#include "SimFlowTask.h"
#include "SimFlowTasks.h"
#include "SimFlowBlackboard.h"
#include "SimFlowComponent.h"
#include "SimFlowRuntimeModule.h"
#include "Engine/World.h"

USimFlowInstance::USimFlowInstance()
{
}

void USimFlowInstance::InitializeInstance(USimFlowAsset* InTemplate, USimFlowComponent* InComponent, USimFlowInstance* InParent)
{
	Template = InTemplate;
	OwningComponent = InComponent;
	ParentInstance = InParent;

	if (!Blackboard)
	{
		Blackboard = NewObject<USimFlowBlackboard>(this);
	}

	BuildRuntimeNodes();
}

void USimFlowInstance::BuildRuntimeNodes()
{
	RuntimeNodes.Reset();
	NodeLookup.Reset();
	ActiveNodes.Reset();

	if (!Template)
	{
		return;
	}

	for (const TObjectPtr<USimFlowNode>& SourceNode : Template->Nodes)
	{
		if (!SourceNode)
		{
			continue;
		}

		USimFlowNode* Copy = DuplicateObject<USimFlowNode>(SourceNode, this);
		Copy->NodeGuid = SourceNode->NodeGuid;
		Copy->SetFlowInstance(this);
		Copy->SetActiveInternal(false);

		RuntimeNodes.Add(Copy);
		NodeLookup.Add(Copy->NodeGuid, Copy);
	}
}

bool USimFlowInstance::StartInstance(FName EntryName)
{
	if (!Template)
	{
		UE_LOG(LogSimFlow, Warning, TEXT("StartInstance called with no flow asset."));
		return false;
	}

	USimFlowNode_Entry* Entry = Template->FindEntryNode(EntryName);
	if (!Entry)
	{
		UE_LOG(LogSimFlow, Error, TEXT("Flow '%s' has no Start node named '%s'."),
			*Template->GetName(), *EntryName.ToString());
		return false;
	}

	// Fresh state: stop anything still running, then rebuild the node copies so a
	// restart never inherits retry counts, loop counters or half-finished tasks.
	DeactivateAllNodes();
	BuildRuntimeNodes();

	PendingActivations.Reset();
	CompletedTaskNodes.Reset();
	RaisedEvents.Reset();
	Mistakes.Reset();
	ElapsedTime = 0.f;
	LastTaskResult = ESimFlowResult::Succeeded;
	LastCheckpointGuid.Invalidate();
	ActiveEntryName = Entry->EntryName;

	if (Blackboard)
	{
		Blackboard->ClearAll();
		Blackboard->FromEntries(Template->InitialBlackboard, false);
	}

	RunState = ESimFlowRunState::Running;
	OnFlowStarted.Broadcast();

	EnqueueActivation(Entry->NodeGuid, NAME_None);
	ProcessPendingActivations();

	return true;
}

void USimFlowInstance::TickInstance(float DeltaTime)
{
	if (RunState != ESimFlowRunState::Running)
	{
		return;
	}

	ElapsedTime += DeltaTime;

	// Snapshot: a node may deactivate itself or others while ticking.
	TArray<TObjectPtr<USimFlowNode>> Snapshot = ActiveNodes;
	for (const TObjectPtr<USimFlowNode>& Node : Snapshot)
	{
		if (RunState != ESimFlowRunState::Running)
		{
			break;
		}
		if (Node && Node->IsNodeActive())
		{
			Node->TickNode(DeltaTime);
		}
	}

	ProcessPendingActivations();
}

void USimFlowInstance::PauseInstance()
{
	if (RunState != ESimFlowRunState::Running)
	{
		return;
	}

	RunState = ESimFlowRunState::Paused;

	TArray<TObjectPtr<USimFlowNode>> Snapshot = ActiveNodes;
	for (const TObjectPtr<USimFlowNode>& Node : Snapshot)
	{
		if (Node)
		{
			Node->OnPauseNode();
		}
	}

	OnFlowPaused.Broadcast();
}

void USimFlowInstance::ResumeInstance()
{
	if (RunState != ESimFlowRunState::Paused)
	{
		return;
	}

	RunState = ESimFlowRunState::Running;

	TArray<TObjectPtr<USimFlowNode>> Snapshot = ActiveNodes;
	for (const TObjectPtr<USimFlowNode>& Node : Snapshot)
	{
		if (Node)
		{
			Node->OnResumeNode();
		}
	}

	OnFlowResumed.Broadcast();
	ProcessPendingActivations();
}

void USimFlowInstance::StopInstance()
{
	if (RunState == ESimFlowRunState::Running || RunState == ESimFlowRunState::Paused)
	{
		FinishInstance(ESimFlowRunState::Aborted, true);
	}
	else
	{
		DeactivateAllNodes();
	}
}

void USimFlowInstance::FinishInstance(ESimFlowRunState FinalState, bool bStopOtherBranches)
{
	if (RunState == ESimFlowRunState::Completed ||
		RunState == ESimFlowRunState::Failed ||
		RunState == ESimFlowRunState::Aborted)
	{
		return;
	}

	RunState = FinalState;
	PendingActivations.Reset();

	if (bStopOtherBranches)
	{
		DeactivateAllNodes();
	}

	UE_LOG(LogSimFlow, Log, TEXT("Flow '%s' finished: %s"),
		Template ? *Template->GetName() : TEXT("<none>"),
		*StaticEnum<ESimFlowRunState>()->GetNameStringByValue(static_cast<int64>(FinalState)));

	OnFlowFinished.Broadcast(FinalState);
}

// --------------------------------------------------------------- Execution

void USimFlowInstance::EnqueueActivation(const FGuid& NodeGuid, FName InputPin)
{
	PendingActivations.Add(FPendingActivation{ NodeGuid, InputPin });
}

void USimFlowInstance::ProcessPendingActivations()
{
	if (bProcessingActivations)
	{
		return;
	}

	bProcessingActivations = true;

	int32 Processed = 0;
	bool bOverflowed = false;

	while (PendingActivations.Num() > 0 && RunState == ESimFlowRunState::Running)
	{
		if (++Processed > MaxActivationsPerProcess)
		{
			bOverflowed = true;
			break;
		}

		const FPendingActivation Next = PendingActivations[0];
		PendingActivations.RemoveAt(0, 1, EAllowShrinking::No);

		if (USimFlowNode* Node = FindRuntimeNode(Next.NodeGuid))
		{
			ActivateNode(Node, Next.InputPin);
		}
	}

	bProcessingActivations = false;

	if (bOverflowed)
	{
		UE_LOG(LogSimFlow, Error,
			TEXT("Flow '%s' exceeded %d activations in one step - suspected infinite loop. Failing the flow."),
			Template ? *Template->GetName() : TEXT("<none>"), MaxActivationsPerProcess);

		PendingActivations.Reset();
		FinishInstance(ESimFlowRunState::Failed, true);
	}
}

void USimFlowInstance::ActivateNode(USimFlowNode* Node, FName InputPin)
{
	if (!Node || RunState != ESimFlowRunState::Running)
	{
		return;
	}

	if (Node->IsNodeActive() && !Node->AllowsReentrantActivation())
	{
		UE_LOG(LogSimFlow, Verbose, TEXT("Node '%s' is already active - ignoring re-entrant activation."), *Node->GetName());
		return;
	}

	if (!Node->IsNodeActive())
	{
		Node->SetActiveInternal(true);
		Node->ActiveTime = 0.f;
		ActiveNodes.AddUnique(Node);
	}

	Node->ActiveInputPin = InputPin;
	Node->ExecuteInput(InputPin);
}

void USimFlowInstance::DeactivateNode(USimFlowNode* Node)
{
	if (!Node || !Node->IsNodeActive())
	{
		return;
	}

	Node->SetActiveInternal(false);
	Node->Cleanup();
	ActiveNodes.Remove(Node);
}

void USimFlowInstance::TriggerNodeOutput(USimFlowNode* Node, FName PinName, bool bStayActive)
{
	if (!Node)
	{
		return;
	}

	const FSimFlowOutputPin* Pin = Node->FindOutputPin(PinName);

	TArray<FSimFlowPinLink> Links;
	if (Pin)
	{
		Links = Pin->Links;
	}
	else
	{
		UE_LOG(LogSimFlow, Warning, TEXT("Node '%s' has no output pin '%s'."), *Node->GetName(), *PinName.ToString());
	}

	if (!bStayActive)
	{
		DeactivateNode(Node);
	}

	for (const FSimFlowPinLink& Link : Links)
	{
		EnqueueActivation(Link.NodeGuid, Link.PinName);
	}

	if (Links.Num() == 0 && !bStayActive)
	{
		// Dead end. If nothing else is running the flow is over.
		if (ActiveNodes.Num() == 0 && PendingActivations.Num() == 0 && RunState == ESimFlowRunState::Running)
		{
			UE_LOG(LogSimFlow, Log, TEXT("Flow '%s' reached a dead end with nothing else running - completing."),
				Template ? *Template->GetName() : TEXT("<none>"));
			FinishInstance(ESimFlowRunState::Completed, true);
			return;
		}
	}

	ProcessPendingActivations();
}

void USimFlowInstance::DeactivateAllNodes()
{
	TArray<TObjectPtr<USimFlowNode>> Snapshot = ActiveNodes;
	ActiveNodes.Reset();

	for (const TObjectPtr<USimFlowNode>& Node : Snapshot)
	{
		if (Node)
		{
			Node->SetActiveInternal(false);
			Node->Cleanup();
		}
	}
}

// ---------------------------------------------------------- Player controls

int32 USimFlowInstance::RetryActiveTasks()
{
	int32 Count = 0;
	TArray<TObjectPtr<USimFlowNode>> Snapshot = ActiveNodes;
	for (const TObjectPtr<USimFlowNode>& Node : Snapshot)
	{
		if (Node && Node->CanRetry())
		{
			Node->RequestRetry();
			Count++;
		}
	}
	ProcessPendingActivations();
	return Count;
}

int32 USimFlowInstance::SkipActiveTasks()
{
	int32 Count = 0;
	TArray<TObjectPtr<USimFlowNode>> Snapshot = ActiveNodes;
	for (const TObjectPtr<USimFlowNode>& Node : Snapshot)
	{
		if (Node && Node->CanSkip())
		{
			Node->RequestSkip();
			Count++;
		}
	}
	ProcessPendingActivations();
	return Count;
}

int32 USimFlowInstance::FailActiveTasks()
{
	int32 Count = 0;
	TArray<TObjectPtr<USimFlowNode>> Snapshot = ActiveNodes;
	for (const TObjectPtr<USimFlowNode>& Node : Snapshot)
	{
		if (Node)
		{
			Node->RequestFail();
			Count++;
		}
	}
	ProcessPendingActivations();
	return Count;
}

void USimFlowInstance::RaiseEvent(FGameplayTag EventTag, UObject* Payload)
{
	if (!EventTag.IsValid())
	{
		return;
	}

	RaisedEvents.AddTag(EventTag);
	OnEventRaised.Broadcast(EventTag, Payload);
	ProcessPendingActivations();
}

bool USimFlowInstance::WasEventRaised(FGameplayTag EventTag) const
{
	return EventTag.IsValid() && RaisedEvents.HasTag(EventTag);
}

void USimFlowInstance::ClearRaisedEvents()
{
	RaisedEvents.Reset();
}

// ---------------------------------------------------------------- Save/load


// ------------------------------------------------------------------- Mistakes

void USimFlowInstance::RecordMistake(FGameplayTag Kind, UObject* Involved, const FText& Description,
	ESimFlowMatchQuality Severity, FName TaskId)
{
	FSimFlowMistake Mistake;
	Mistake.Kind = Kind.IsValid() ? Kind : SimFlowTags::Mistake;
	Mistake.Involved = Involved;
	Mistake.Description = Description;
	Mistake.Severity = Severity;
	Mistake.TimeSeconds = ElapsedTime;
	Mistake.TaskId = TaskId;

	// Capture the name now: the pointer is dropped when the flow is saved.
	if (const AActor* AsActor = Cast<AActor>(Involved))
	{
		Mistake.InvolvedName = USimFlowIdentityStatics::GetIdentityDisplayName(AsActor).ToString();
	}
	else if (Involved)
	{
		Mistake.InvolvedName = Involved->GetName();
	}

	Mistakes.Add(Mistake);

	// Keep the legacy counter in step so existing conditions still work.
	if (Blackboard)
	{
		Blackboard->AddToValue(SimFlowKeys::Mistakes, FSimFlowValue::MakeInt(1));
	}

	UE_LOG(LogSimFlow, Verbose, TEXT("Mistake recorded: %s (%s)"),
		*Mistake.Description.ToString(), *Mistake.Kind.ToString());

	OnMistakeRecorded.Broadcast(Mistake);
}

TArray<FSimFlowMistake> USimFlowInstance::GetMistakesOfKind(FGameplayTag Kind, bool bMatchChildTags) const
{
	TArray<FSimFlowMistake> Out;
	if (!Kind.IsValid())
	{
		return Out;
	}

	for (const FSimFlowMistake& Mistake : Mistakes)
	{
		const bool bMatches = bMatchChildTags ? Mistake.Kind.MatchesTag(Kind) : (Mistake.Kind == Kind);
		if (bMatches)
		{
			Out.Add(Mistake);
		}
	}
	return Out;
}

void USimFlowInstance::ClearMistakes()
{
	Mistakes.Reset();
	if (Blackboard)
	{
		Blackboard->SetInt(SimFlowKeys::Mistakes, 0);
	}
}

FSimFlowSaveState USimFlowInstance::SaveInstanceState() const
{
	FSimFlowSaveState State;

	if (Template)
	{
		State.FlowAssetPath = Template->GetPathName();
	}
	if (const USimFlowComponent* Component = GetOwningComponent())
	{
		State.FlowSaveId = Component->FlowSaveId;
	}

	State.RunState = RunState;
	State.EntryName = ActiveEntryName;
	State.ElapsedTime = ElapsedTime;
	State.LastCheckpointGuid = LastCheckpointGuid;
	State.SavedAt = FDateTime::UtcNow();

	if (Blackboard)
	{
		State.Blackboard = Blackboard->ToEntries(true);
	}

	for (const FGameplayTag& Tag : RaisedEvents)
	{
		State.RaisedEvents.Add(Tag.GetTagName());
	}

	State.Mistakes = Mistakes;
	for (FSimFlowMistake& Mistake : State.Mistakes)
	{
		Mistake.StripObjectReferences();
	}

	for (const TObjectPtr<USimFlowNode>& Node : RuntimeNodes)
	{
		if (!Node)
		{
			continue;
		}
		FSimFlowNodeSaveState NodeState;
		Node->SaveNodeState(NodeState);
		NodeState.ActiveInputPin = Node->ActiveInputPin;
		State.NodeStates.Add(MoveTemp(NodeState));
	}

	if (const USimFlowTask* Current = GetCurrentTask())
	{
		State.Label = Current->GetDisplayNameText().ToString();
	}
	else if (Template)
	{
		State.Label = Template->GetDisplayNameText().ToString();
	}

	return State;
}

bool USimFlowInstance::LoadInstanceState(const FSimFlowSaveState& State, ESimFlowLoadMode LoadMode)
{
	if (!Template)
	{
		UE_LOG(LogSimFlow, Warning, TEXT("LoadInstanceState: no flow asset assigned."));
		return false;
	}

	if (!State.FlowAssetPath.IsEmpty() && State.FlowAssetPath != Template->GetPathName())
	{
		UE_LOG(LogSimFlow, Warning,
			TEXT("LoadInstanceState: save was made with '%s' but this component runs '%s'. Loading anyway."),
			*State.FlowAssetPath, *Template->GetPathName());
	}

	BuildRuntimeNodes();
	PendingActivations.Reset();
	CompletedTaskNodes.Reset();

	ElapsedTime = State.ElapsedTime;
	ActiveEntryName = State.EntryName;
	LastCheckpointGuid = State.LastCheckpointGuid;
	LastTaskResult = ESimFlowResult::Succeeded;

	RaisedEvents.Reset();
	for (const FName& TagName : State.RaisedEvents)
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TagName, false);
		if (Tag.IsValid())
		{
			RaisedEvents.AddTag(Tag);
		}
	}

	if (Blackboard)
	{
		Blackboard->FromEntries(State.Blackboard, true);
	}

	Mistakes = State.Mistakes;

	RunState = ESimFlowRunState::Running;
	OnFlowStarted.Broadcast();

	if (LoadMode == ESimFlowLoadMode::FromLastCheckpoint)
	{
		if (State.LastCheckpointGuid.IsValid() && FindRuntimeNode(State.LastCheckpointGuid))
		{
			EnqueueActivation(State.LastCheckpointGuid, SimFlowPins::In);
			ProcessPendingActivations();
			return true;
		}

		UE_LOG(LogSimFlow, Log, TEXT("No checkpoint in save - restarting the flow from its entry."));
		const bool bStarted = StartInstance(State.EntryName);
		if (bStarted && Blackboard)
		{
			// StartInstance resets the blackboard; put the saved values back.
			Blackboard->FromEntries(State.Blackboard, false);
		}
		return bStarted;
	}

	// Exact restore: bring back every node that was active, then hand it its saved state.
	bool bAnyActivated = false;
	for (const FSimFlowNodeSaveState& NodeState : State.NodeStates)
	{
		USimFlowNode* Node = FindRuntimeNode(NodeState.NodeGuid);
		if (!Node)
		{
			continue;
		}

		if (!NodeState.bActive)
		{
			// Still restore counters on stateful nodes such as Loop and Join.
			Node->LoadNodeState(NodeState);
			Node->SetActiveInternal(false);
			continue;
		}

		ActivateNode(Node, NodeState.ActiveInputPin);
		bAnyActivated = true;

		if (Node->IsNodeActive())
		{
			Node->LoadNodeState(NodeState);
			Node->SetActiveInternal(true);
			Node->ActiveTime = NodeState.ActiveTime;
		}
	}

	ProcessPendingActivations();

	if (!bAnyActivated && ActiveNodes.Num() == 0)
	{
		UE_LOG(LogSimFlow, Warning, TEXT("Loaded flow state had no active nodes - starting from the entry instead."));
		const bool bStarted = StartInstance(State.EntryName);
		if (bStarted && Blackboard)
		{
			Blackboard->FromEntries(State.Blackboard, false);
		}
		return bStarted;
	}

	return true;
}

// -------------------------------------------------------------- Accessors

USimFlowComponent* USimFlowInstance::GetOwningComponent() const
{
	if (OwningComponent.IsValid())
	{
		return OwningComponent.Get();
	}
	return ParentInstance ? ParentInstance->GetOwningComponent() : nullptr;
}

TArray<USimFlowNode*> USimFlowInstance::GetActiveNodes() const
{
	TArray<USimFlowNode*> Result;
	Result.Reserve(ActiveNodes.Num());
	for (const TObjectPtr<USimFlowNode>& Node : ActiveNodes)
	{
		if (Node)
		{
			Result.Add(Node);
		}
	}
	return Result;
}

TArray<USimFlowNode_Task*> USimFlowInstance::GetActiveTaskNodes() const
{
	TArray<USimFlowNode_Task*> Result;
	for (const TObjectPtr<USimFlowNode>& Node : ActiveNodes)
	{
		if (USimFlowNode_Task* TaskNode = Cast<USimFlowNode_Task>(Node))
		{
			Result.Add(TaskNode);
		}
		else if (const USimFlowNode_SubFlow* SubFlow = Cast<USimFlowNode_SubFlow>(Node))
		{
			if (USimFlowInstance* Child = SubFlow->GetChildInstance())
			{
				Result.Append(Child->GetActiveTaskNodes());
			}
		}
	}
	return Result;
}

USimFlowTask* USimFlowInstance::GetCurrentTask() const
{
	const TArray<USimFlowNode_Task*> TaskNodes = GetActiveTaskNodes();
	return TaskNodes.Num() > 0 ? TaskNodes[0]->GetTask() : nullptr;
}

USimFlowNode* USimFlowInstance::FindRuntimeNode(const FGuid& Guid) const
{
	if (const TObjectPtr<USimFlowNode>* Found = NodeLookup.Find(Guid))
	{
		return *Found;
	}
	return nullptr;
}

float USimFlowInstance::GetProgress() const
{
	int32 TotalTaskNodes = 0;
	for (const TObjectPtr<USimFlowNode>& Node : RuntimeNodes)
	{
		if (Cast<USimFlowNode_Task>(Node))
		{
			TotalTaskNodes++;
		}
	}

	if (TotalTaskNodes == 0)
	{
		return (RunState == ESimFlowRunState::Completed) ? 1.f : 0.f;
	}

	return FMath::Clamp(static_cast<float>(CompletedTaskNodes.Num()) / static_cast<float>(TotalTaskNodes), 0.f, 1.f);
}

FString USimFlowInstance::BuildDebugString() const
{
	FString Result;
	Result += FString::Printf(TEXT("Flow: %s  [%s]  t=%.1fs  progress=%.0f%%\n"),
		Template ? *Template->GetDisplayNameText().ToString() : TEXT("<none>"),
		*StaticEnum<ESimFlowRunState>()->GetNameStringByValue(static_cast<int64>(RunState)),
		ElapsedTime,
		GetProgress() * 100.f);

	for (const TObjectPtr<USimFlowNode>& Node : ActiveNodes)
	{
		if (Node)
		{
			Result += FString::Printf(TEXT("  > %s\n"), *Node->GetDebugStatus());
		}
	}

	if (Blackboard)
	{
		const FString BlackboardText = Blackboard->ToDebugString();
		if (!BlackboardText.IsEmpty())
		{
			Result += TEXT("  Blackboard:\n");
			Result += BlackboardText;
		}
	}

	for (const TObjectPtr<USimFlowNode>& Node : ActiveNodes)
	{
		if (const USimFlowNode_SubFlow* SubFlow = Cast<USimFlowNode_SubFlow>(Node))
		{
			if (const USimFlowInstance* Child = SubFlow->GetChildInstance())
			{
				Result += TEXT("  -- sub flow --\n");
				Result += Child->BuildDebugString();
			}
		}
	}

	return Result;
}

UWorld* USimFlowInstance::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}
	if (const USimFlowComponent* Component = GetOwningComponent())
	{
		return Component->GetWorld();
	}
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

// -------------------------------------------------------------- Node hooks

void USimFlowInstance::NotifyTaskStarted(USimFlowNode_Task* Node, USimFlowTask* Task)
{
	UE_LOG(LogSimFlow, Verbose, TEXT("Task started: %s"), Task ? *Task->GetDisplayNameText().ToString() : TEXT("<none>"));
	OnTaskStarted.Broadcast(Node, Task);
}

void USimFlowInstance::NotifyTaskFinished(USimFlowNode_Task* Node, USimFlowTask* Task, ESimFlowResult Result)
{
	LastTaskResult = Result;

	if (Blackboard)
	{
		Blackboard->SetName(SimFlowKeys::LastResult,
			FName(*StaticEnum<ESimFlowResult>()->GetNameStringByValue(static_cast<int64>(Result))));
	}

	if (Node && Result != ESimFlowResult::Aborted)
	{
		CompletedTaskNodes.Add(Node->NodeGuid);
	}

	OnTaskFinished.Broadcast(Node, Task, Result);
}

void USimFlowInstance::NotifyTaskRetried(USimFlowNode_Task* Node, USimFlowTask* Task)
{
	OnTaskRetried.Broadcast(Node, Task);
}

void USimFlowInstance::NotifyCheckpointReached(USimFlowNode_Checkpoint* Checkpoint)
{
	if (Checkpoint)
	{
		LastCheckpointGuid = Checkpoint->NodeGuid;
	}
	OnCheckpointReached.Broadcast(Checkpoint);
}

void USimFlowInstance::NotifyQuizPresented(USimFlowTask_Quiz* Quiz)
{
	OnQuizPresented.Broadcast(Quiz);
}
