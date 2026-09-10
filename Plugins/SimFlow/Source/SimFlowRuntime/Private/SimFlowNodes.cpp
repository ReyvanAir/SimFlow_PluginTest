// Copyright SimFlow. All Rights Reserved.

#include "SimFlowNodes.h"
#include "SimFlowTask.h"
#include "SimFlowCondition.h"
#include "SimFlowInstance.h"
#include "SimFlowAsset.h"
#include "SimFlowBlackboard.h"
#include "SimFlowComponent.h"
#include "SimFlowRuntimeModule.h"

#define LOCTEXT_NAMESPACE "SimFlowNodes"

static const FLinearColor SimFlowColor_Entry(0.10f, 0.55f, 0.20f);
static const FLinearColor SimFlowColor_Task(0.10f, 0.32f, 0.60f);
static const FLinearColor SimFlowColor_Flow(0.45f, 0.30f, 0.60f);
static const FLinearColor SimFlowColor_Data(0.55f, 0.42f, 0.10f);
static const FLinearColor SimFlowColor_End(0.55f, 0.12f, 0.12f);

// =====================================================================
//  Entry
// =====================================================================

USimFlowNode_Entry::USimFlowNode_Entry()
{
	RebuildPins();
}

void USimFlowNode_Entry::BuildPins()
{
	AddOutputPin(SimFlowPins::Out);
}

void USimFlowNode_Entry::ExecuteInput(FName /*PinName*/)
{
	TriggerOutput(SimFlowPins::Out);
}

FText USimFlowNode_Entry::GetNodeTitle() const
{
	return LOCTEXT("EntryTitle", "Start");
}

FText USimFlowNode_Entry::GetNodeSubtitle() const
{
	return EntryName.IsNone() ? FText::GetEmpty() : FText::FromName(EntryName);
}

FLinearColor USimFlowNode_Entry::GetNodeColor() const { return SimFlowColor_Entry; }
FText USimFlowNode_Entry::GetNodeCategory() const { return LOCTEXT("CatFlow", "Flow Control"); }

// =====================================================================
//  Task
// =====================================================================

USimFlowNode_Task::USimFlowNode_Task()
{
	RebuildPins();
}

void USimFlowNode_Task::BuildPins()
{
	AddInputPin(SimFlowPins::In);
	AddOutputPin(SimFlowPins::Completed);
	AddOutputPin(SimFlowPins::Failed);
	AddOutputPin(SimFlowPins::Skipped);
	AddOutputPin(SimFlowPins::TimedOut);
}

void USimFlowNode_Task::ExecuteInput(FName /*PinName*/)
{
	ActiveTime = 0.f;
	AutoRetryCount = 0;

	if (!Task)
	{
		UE_LOG(LogSimFlow, Warning, TEXT("Task node '%s' has no task assigned - passing through."), *GetName());
		TriggerOutput(SimFlowPins::Completed);
		return;
	}

	if (FlowInstance)
	{
		FlowInstance->NotifyTaskStarted(this, Task);
	}

	Task->InitializeTask(FlowInstance, this);
	Task->StartTask();
}

void USimFlowNode_Task::TickNode(float DeltaTime)
{
	Super::TickNode(DeltaTime);

	if (!Task || !Task->bIsRunning)
	{
		return;
	}

	Task->TickTask(DeltaTime);

	if (!bActive || !Task->bIsRunning)
	{
		return;
	}

	if (AbortCondition && AbortCondition->Evaluate(FlowInstance))
	{
		Task->FinishTask(AbortResult);
		return;
	}

	if (TimeLimit > 0.f && ActiveTime >= TimeLimit)
	{
		Task->FinishTask(ESimFlowResult::TimedOut);
	}
}

void USimFlowNode_Task::OnPauseNode()
{
	if (Task)
	{
		Task->PauseTask();
	}
}

void USimFlowNode_Task::OnResumeNode()
{
	if (Task)
	{
		Task->ResumeTask();
	}
}

void USimFlowNode_Task::Cleanup()
{
	if (Task && Task->bIsRunning)
	{
		Task->AbortTask();
	}
	Super::Cleanup();
}

bool USimFlowNode_Task::CanRetry() const
{
	return Task && Task->bIsRunning && Task->CanRetry();
}

bool USimFlowNode_Task::CanSkip() const
{
	return Task && Task->bIsRunning && Task->bAllowSkip;
}

void USimFlowNode_Task::RequestRetry()
{
	if (!Task)
	{
		return;
	}
	if (!Task->CanRetry())
	{
		UE_LOG(LogSimFlow, Log, TEXT("Retry refused for task '%s' (retry limit reached)."), *Task->GetDisplayNameText().ToString());
		Task->FinishTask(ESimFlowResult::Failed);
		return;
	}

	ActiveTime = 0.f;
	Task->RestartTask();

	if (FlowInstance)
	{
		FlowInstance->NotifyTaskRetried(this, Task);
	}
}

void USimFlowNode_Task::RequestSkip()
{
	if (Task && Task->bIsRunning && Task->bAllowSkip)
	{
		Task->FinishTask(ESimFlowResult::Skipped);
	}
}

void USimFlowNode_Task::RequestFail()
{
	if (Task && Task->bIsRunning)
	{
		Task->FinishTask(ESimFlowResult::Failed);
	}
}

void USimFlowNode_Task::HandleTaskFinished(ESimFlowResult Result)
{
	if (FlowInstance)
	{
		FlowInstance->NotifyTaskFinished(this, Task, Result);
	}

	// Auto retry before we leave the node.
	const bool bRetryableResult =
		(Result == ESimFlowResult::Failed) ||
		(Result == ESimFlowResult::TimedOut && bAutoRetryOnTimeout);

	if (bAutoRetryOnFailure && bRetryableResult && AutoRetryCount < AutoRetryLimit && Task && Task->bAllowRetry)
	{
		AutoRetryCount++;
		ActiveTime = 0.f;
		Task->RestartTask();
		return;
	}

	FName PinName;
	switch (Result)
	{
	case ESimFlowResult::Succeeded:	PinName = SimFlowPins::Completed;	break;
	case ESimFlowResult::Failed:	PinName = SimFlowPins::Failed;		break;
	case ESimFlowResult::Skipped:	PinName = SimFlowPins::Skipped;		break;
	case ESimFlowResult::TimedOut:	PinName = SimFlowPins::TimedOut;	break;
	case ESimFlowResult::Aborted:	FinishNode();						return;
	default:						PinName = SimFlowPins::Completed;	break;
	}

	if (bFallbackToCompleted && PinName != SimFlowPins::Completed)
	{
		const FSimFlowOutputPin* Pin = FindOutputPin(PinName);
		if (!Pin || Pin->Links.Num() == 0)
		{
			PinName = SimFlowPins::Completed;
		}
	}

	TriggerOutput(PinName);
}

float USimFlowNode_Task::GetRemainingTime() const
{
	return TimeLimit > 0.f ? FMath::Max(0.f, TimeLimit - ActiveTime) : -1.f;
}

FText USimFlowNode_Task::GetNodeTitle() const
{
	if (Task)
	{
		const FText TaskName = Task->GetDisplayNameText();
		if (!TaskName.IsEmpty())
		{
			return TaskName;
		}
	}
	return LOCTEXT("TaskTitle", "Task");
}

FText USimFlowNode_Task::GetNodeSubtitle() const
{
	TArray<FString> Parts;
	if (Task && !Task->Instruction.IsEmpty())
	{
		Parts.Add(Task->Instruction.ToString());
	}
	if (TimeLimit > 0.f)
	{
		Parts.Add(FString::Printf(TEXT("limit %gs"), TimeLimit));
	}
	if (bAutoRetryOnFailure)
	{
		Parts.Add(FString::Printf(TEXT("auto-retry x%d"), AutoRetryLimit));
	}
	return Parts.Num() > 0 ? FText::FromString(FString::Join(Parts, TEXT(" | "))) : FText::GetEmpty();
}

FLinearColor USimFlowNode_Task::GetNodeColor() const { return SimFlowColor_Task; }
FText USimFlowNode_Task::GetNodeCategory() const { return LOCTEXT("CatTask", "Tasks"); }

FString USimFlowNode_Task::GetDebugStatus() const
{
	FString Status = GetNodeTitle().ToString();
	if (TimeLimit > 0.f)
	{
		Status += FString::Printf(TEXT("  [%.1fs left]"), GetRemainingTime());
	}
	if (Task && Task->RetryCount > 0)
	{
		Status += FString::Printf(TEXT("  [retry %d]"), Task->RetryCount);
	}
	return Status;
}

void USimFlowNode_Task::SaveNodeState(FSimFlowNodeSaveState& OutState) const
{
	Super::SaveNodeState(OutState);
	OutState.IntState = AutoRetryCount;

	if (Task)
	{
		Task->SaveTaskState(OutState.CustomData);
		OutState.CustomData.Emplace(TEXT("__RetryCount"), FSimFlowValue::MakeInt(Task->RetryCount));
		OutState.CustomData.Emplace(TEXT("__TaskElapsed"), FSimFlowValue::MakeFloat(Task->ElapsedTime));
	}
}

void USimFlowNode_Task::LoadNodeState(const FSimFlowNodeSaveState& InState)
{
	Super::LoadNodeState(InState);
	AutoRetryCount = InState.IntState;

	if (Task)
	{
		Task->LoadTaskState(InState.CustomData);
		for (const FSimFlowBlackboardEntry& Entry : InState.CustomData)
		{
			if (Entry.Key == TEXT("__RetryCount"))
			{
				Task->RetryCount = FMath::RoundToInt(Entry.Value.AsNumber());
			}
			else if (Entry.Key == TEXT("__TaskElapsed"))
			{
				Task->ElapsedTime = static_cast<float>(Entry.Value.AsNumber());
			}
		}
	}
}

// =====================================================================
//  Delay
// =====================================================================

USimFlowNode_Delay::USimFlowNode_Delay()
{
	RebuildPins();
}

void USimFlowNode_Delay::BuildPins()
{
	AddInputPin(SimFlowPins::In);
	AddOutputPin(SimFlowPins::Out);
}

void USimFlowNode_Delay::ExecuteInput(FName /*PinName*/)
{
	ActiveTime = 0.f;
	if (Duration <= 0.f)
	{
		TriggerOutput(SimFlowPins::Out);
	}
}

void USimFlowNode_Delay::TickNode(float DeltaTime)
{
	Super::TickNode(DeltaTime);
	if (ActiveTime >= Duration)
	{
		TriggerOutput(SimFlowPins::Out);
	}
}

FText USimFlowNode_Delay::GetNodeSubtitle() const
{
	return FText::FromString(FString::Printf(TEXT("%g s"), Duration));
}

FLinearColor USimFlowNode_Delay::GetNodeColor() const { return SimFlowColor_Flow; }
FText USimFlowNode_Delay::GetNodeCategory() const { return LOCTEXT("CatFlow", "Flow Control"); }

FString USimFlowNode_Delay::GetDebugStatus() const
{
	return FString::Printf(TEXT("Delay  [%.1f / %.1f s]"), ActiveTime, Duration);
}

// =====================================================================
//  Branch
// =====================================================================

USimFlowNode_Branch::USimFlowNode_Branch()
{
	Cases.AddDefaulted(2);
	RebuildPins();
}

FName USimFlowNode_Branch::MakeCasePinName(int32 Index)
{
	return FName(*FString::Printf(TEXT("Case_%d"), Index));
}

void USimFlowNode_Branch::BuildPins()
{
	AddInputPin(SimFlowPins::In);

	for (int32 Index = 0; Index < Cases.Num(); ++Index)
	{
		FString Display = Cases[Index].Label;
		if (Display.IsEmpty())
		{
			Display = (Cases[Index].Condition)
				? Cases[Index].Condition->GetConditionDescription().ToString()
				: FString::Printf(TEXT("Case %d"), Index);
		}
		AddOutputPin(MakeCasePinName(Index), Display);
	}

	if (bHasDefaultPin)
	{
		AddOutputPin(SimFlowPins::Default, TEXT("Default"));
	}
}

void USimFlowNode_Branch::ExecuteInput(FName /*PinName*/)
{
	TArray<FName> Matched;

	for (int32 Index = 0; Index < Cases.Num(); ++Index)
	{
		const FSimFlowBranchCase& Case = Cases[Index];
		const bool bPassed = Case.Condition ? Case.Condition->Evaluate(FlowInstance) : false;
		if (bPassed)
		{
			Matched.Add(MakeCasePinName(Index));
			if (!bFireAllMatchingCases)
			{
				break;
			}
		}
	}

	if (Matched.Num() == 0)
	{
		if (bHasDefaultPin)
		{
			TriggerOutput(SimFlowPins::Default);
		}
		else
		{
			FinishNode();
		}
		return;
	}

	for (int32 Index = 0; Index < Matched.Num(); ++Index)
	{
		const bool bLast = (Index == Matched.Num() - 1);
		TriggerOutput(Matched[Index], !bLast);
	}
}

FText USimFlowNode_Branch::GetNodeSubtitle() const
{
	return FText::FromString(FString::Printf(TEXT("%d case%s%s"),
		Cases.Num(), Cases.Num() == 1 ? TEXT("") : TEXT("s"),
		bFireAllMatchingCases ? TEXT(", fire all") : TEXT("")));
}

FLinearColor USimFlowNode_Branch::GetNodeColor() const { return SimFlowColor_Flow; }
FText USimFlowNode_Branch::GetNodeCategory() const { return LOCTEXT("CatFlow", "Flow Control"); }

// =====================================================================
//  Random Branch
// =====================================================================

USimFlowNode_RandomBranch::USimFlowNode_RandomBranch()
{
	RebuildPins();
}

void USimFlowNode_RandomBranch::BuildPins()
{
	AddInputPin(SimFlowPins::In);

	const int32 Count = FMath::Clamp(NumOutputs, 2, 16);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		AddOutputPin(FName(*FString::Printf(TEXT("Out_%d"), Index)), FString::Printf(TEXT("Out %d"), Index));
	}
}

void USimFlowNode_RandomBranch::ExecuteInput(FName /*PinName*/)
{
	const int32 Count = FMath::Clamp(NumOutputs, 2, 16);

	float TotalWeight = 0.f;
	TArray<float> Resolved;
	Resolved.SetNumUninitialized(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		float Weight = Weights.IsValidIndex(Index) ? FMath::Max(0.f, Weights[Index]) : 1.f;
		if (bAvoidRepeats && Index == LastPicked && Count > 1)
		{
			Weight = 0.f;
		}
		Resolved[Index] = Weight;
		TotalWeight += Weight;
	}

	int32 Picked = 0;
	if (TotalWeight <= 0.f)
	{
		Picked = FMath::RandRange(0, Count - 1);
	}
	else
	{
		float Roll = FMath::FRandRange(0.f, TotalWeight);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Roll -= Resolved[Index];
			if (Roll <= 0.f)
			{
				Picked = Index;
				break;
			}
		}
	}

	LastPicked = Picked;
	TriggerOutput(FName(*FString::Printf(TEXT("Out_%d"), Picked)));
}

FLinearColor USimFlowNode_RandomBranch::GetNodeColor() const { return SimFlowColor_Flow; }
FText USimFlowNode_RandomBranch::GetNodeCategory() const { return LOCTEXT("CatFlow", "Flow Control"); }

// =====================================================================
//  Parallel
// =====================================================================

USimFlowNode_Parallel::USimFlowNode_Parallel()
{
	RebuildPins();
}

void USimFlowNode_Parallel::BuildPins()
{
	AddInputPin(SimFlowPins::In);

	const int32 Count = FMath::Clamp(NumOutputs, 2, 16);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		AddOutputPin(FName(*FString::Printf(TEXT("Out_%d"), Index)), FString::Printf(TEXT("Out %d"), Index));
	}
}

void USimFlowNode_Parallel::ExecuteInput(FName /*PinName*/)
{
	// Snapshot the pin names: triggering can cause re-entrancy.
	TArray<FName> PinNames;
	PinNames.Reserve(OutputPins.Num());
	for (const FSimFlowOutputPin& Pin : OutputPins)
	{
		if (Pin.Links.Num() > 0)
		{
			PinNames.Add(Pin.PinName);
		}
	}

	if (PinNames.Num() == 0)
	{
		FinishNode();
		return;
	}

	for (int32 Index = 0; Index < PinNames.Num(); ++Index)
	{
		const bool bLast = (Index == PinNames.Num() - 1);
		TriggerOutput(PinNames[Index], !bLast);
	}
}

FText USimFlowNode_Parallel::GetNodeSubtitle() const
{
	return FText::FromString(FString::Printf(TEXT("%d branches"), FMath::Clamp(NumOutputs, 2, 16)));
}

FLinearColor USimFlowNode_Parallel::GetNodeColor() const { return SimFlowColor_Flow; }
FText USimFlowNode_Parallel::GetNodeCategory() const { return LOCTEXT("CatFlow", "Flow Control"); }

// =====================================================================
//  Join
// =====================================================================

USimFlowNode_Join::USimFlowNode_Join()
{
	RebuildPins();
}

void USimFlowNode_Join::BuildPins()
{
	const int32 Count = FMath::Clamp(NumInputs, 2, 16);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		AddInputPin(FName(*FString::Printf(TEXT("In_%d"), Index)), FString::Printf(TEXT("In %d"), Index));
	}
	AddOutputPin(SimFlowPins::Out);
}

void USimFlowNode_Join::ExecuteInput(FName PinName)
{
	if (Mode == ESimFlowJoinMode::WaitForAny)
	{
		if (bHasFired)
		{
			// The race is already decided - swallow whatever arrives late so the
			// downstream section cannot be triggered twice.
			return;
		}

		bHasFired = true;
		ReceivedInputs.AddUnique(PinName);

		// Stay active on purpose: the losing branch will still arrive here, and
		// this node has to be alive to absorb it.
		TriggerOutput(SimFlowPins::Out, /*bStayActive*/ true);
		return;
	}

	// Wait for all.
	ReceivedInputs.AddUnique(PinName);

	const int32 Required = FMath::Clamp(NumInputs, 2, 16);
	if (ReceivedInputs.Num() < Required)
	{
		return;
	}

	ReceivedInputs.Reset();
	bHasFired = true;

	// Leaving the node deactivates it, which resets the counters ready for the
	// next pass when the join sits inside a loop.
	TriggerOutput(SimFlowPins::Out);
}

void USimFlowNode_Join::Cleanup()
{
	ReceivedInputs.Reset();
	bHasFired = false;
	Super::Cleanup();
}

FText USimFlowNode_Join::GetNodeSubtitle() const
{
	return (Mode == ESimFlowJoinMode::WaitForAll)
		? FText::FromString(FString::Printf(TEXT("wait for all %d"), FMath::Clamp(NumInputs, 2, 16)))
		: LOCTEXT("JoinAny", "wait for any (race)");
}

FLinearColor USimFlowNode_Join::GetNodeColor() const { return SimFlowColor_Flow; }
FText USimFlowNode_Join::GetNodeCategory() const { return LOCTEXT("CatFlow", "Flow Control"); }

FString USimFlowNode_Join::GetDebugStatus() const
{
	return FString::Printf(TEXT("Join  [%d / %d]"), ReceivedInputs.Num(), FMath::Clamp(NumInputs, 2, 16));
}

void USimFlowNode_Join::SaveNodeState(FSimFlowNodeSaveState& OutState) const
{
	Super::SaveNodeState(OutState);
	OutState.ReceivedInputs = ReceivedInputs;
	OutState.IntState = bHasFired ? 1 : 0;
}

void USimFlowNode_Join::LoadNodeState(const FSimFlowNodeSaveState& InState)
{
	Super::LoadNodeState(InState);
	ReceivedInputs = InState.ReceivedInputs;
	bHasFired = InState.IntState != 0;
}

// =====================================================================
//  Loop
// =====================================================================

const FName USimFlowNode_Loop::ContinuePin(TEXT("Continue"));

USimFlowNode_Loop::USimFlowNode_Loop()
{
	RebuildPins();
}

void USimFlowNode_Loop::BuildPins()
{
	AddInputPin(SimFlowPins::In);
	AddInputPin(ContinuePin, TEXT("Continue"));
	AddOutputPin(SimFlowPins::LoopBody, TEXT("Loop Body"));
	AddOutputPin(SimFlowPins::Completed);
}

void USimFlowNode_Loop::ExecuteInput(FName PinName)
{
	if (PinName == SimFlowPins::In)
	{
		CurrentIteration = 0;
	}
	else if (PinName == ContinuePin)
	{
		CurrentIteration++;
	}

	if (CurrentIteration >= MaxIterations)
	{
		UE_LOG(LogSimFlow, Warning, TEXT("Loop node '%s' hit MaxIterations (%d) - exiting."), *GetName(), MaxIterations);
		TriggerOutput(SimFlowPins::Completed);
		return;
	}

	if (Iterations > 0 && CurrentIteration >= Iterations)
	{
		TriggerOutput(SimFlowPins::Completed);
		return;
	}

	if (BreakCondition && BreakCondition->Evaluate(FlowInstance))
	{
		TriggerOutput(SimFlowPins::Completed);
		return;
	}

	if (!IterationBlackboardKey.IsNone())
	{
		if (USimFlowBlackboard* Blackboard = GetBlackboard())
		{
			Blackboard->SetInt(IterationBlackboardKey, CurrentIteration);
		}
	}

	TriggerOutput(SimFlowPins::LoopBody, /*bStayActive*/ true);
}

void USimFlowNode_Loop::Cleanup()
{
	CurrentIteration = 0;
	Super::Cleanup();
}

FText USimFlowNode_Loop::GetNodeSubtitle() const
{
	return (Iterations > 0)
		? FText::FromString(FString::Printf(TEXT("%d iterations"), Iterations))
		: LOCTEXT("LoopUntil", "until break condition");
}

FLinearColor USimFlowNode_Loop::GetNodeColor() const { return SimFlowColor_Flow; }
FText USimFlowNode_Loop::GetNodeCategory() const { return LOCTEXT("CatFlow", "Flow Control"); }

FString USimFlowNode_Loop::GetDebugStatus() const
{
	return FString::Printf(TEXT("Loop  [%d / %d]"), CurrentIteration, Iterations);
}

void USimFlowNode_Loop::SaveNodeState(FSimFlowNodeSaveState& OutState) const
{
	Super::SaveNodeState(OutState);
	OutState.IntState = CurrentIteration;
}

void USimFlowNode_Loop::LoadNodeState(const FSimFlowNodeSaveState& InState)
{
	Super::LoadNodeState(InState);
	CurrentIteration = InState.IntState;
}

// =====================================================================
//  Sub Flow
// =====================================================================

USimFlowNode_SubFlow::USimFlowNode_SubFlow()
{
	RebuildPins();
}

void USimFlowNode_SubFlow::BuildPins()
{
	AddInputPin(SimFlowPins::In);
	AddOutputPin(SimFlowPins::Completed);
	AddOutputPin(SimFlowPins::Failed);
}

void USimFlowNode_SubFlow::ExecuteInput(FName /*PinName*/)
{
	if (!SubFlow)
	{
		UE_LOG(LogSimFlow, Warning, TEXT("Sub Flow node '%s' has no asset assigned."), *GetName());
		TriggerOutput(SimFlowPins::Completed);
		return;
	}

	if (!FlowInstance)
	{
		FinishNode();
		return;
	}

	ChildInstance = NewObject<USimFlowInstance>(this);
	ChildInstance->InitializeInstance(SubFlow, FlowInstance->GetOwningComponent(), FlowInstance);

	if (bInheritBlackboard)
	{
		ChildInstance->GetBlackboard()->MergeFrom(FlowInstance->GetBlackboard(), true);
	}

	ChildInstance->OnFlowFinished.AddDynamic(this, &USimFlowNode_SubFlow::HandleChildFinished);
	ChildInstance->StartInstance(EntryName);
}

void USimFlowNode_SubFlow::TickNode(float DeltaTime)
{
	Super::TickNode(DeltaTime);
	if (ChildInstance)
	{
		ChildInstance->TickInstance(DeltaTime);
	}
}

void USimFlowNode_SubFlow::OnPauseNode()
{
	if (ChildInstance)
	{
		ChildInstance->PauseInstance();
	}
}

void USimFlowNode_SubFlow::OnResumeNode()
{
	if (ChildInstance)
	{
		ChildInstance->ResumeInstance();
	}
}

void USimFlowNode_SubFlow::Cleanup()
{
	if (ChildInstance)
	{
		ChildInstance->OnFlowFinished.RemoveDynamic(this, &USimFlowNode_SubFlow::HandleChildFinished);
		ChildInstance->StopInstance();
		ChildInstance = nullptr;
	}
	Super::Cleanup();
}

void USimFlowNode_SubFlow::RequestSkip()
{
	if (ChildInstance)
	{
		ChildInstance->OnFlowFinished.RemoveDynamic(this, &USimFlowNode_SubFlow::HandleChildFinished);
		ChildInstance->StopInstance();
		ChildInstance = nullptr;
	}
	TriggerOutput(SimFlowPins::Completed);
}

void USimFlowNode_SubFlow::RequestFail()
{
	if (ChildInstance)
	{
		ChildInstance->OnFlowFinished.RemoveDynamic(this, &USimFlowNode_SubFlow::HandleChildFinished);
		ChildInstance->StopInstance();
		ChildInstance = nullptr;
	}
	TriggerOutput(SimFlowPins::Failed);
}

void USimFlowNode_SubFlow::HandleChildFinished(ESimFlowRunState FinalState)
{
	if (bWriteBackBlackboard && ChildInstance && FlowInstance)
	{
		FlowInstance->GetBlackboard()->MergeFrom(ChildInstance->GetBlackboard(), true);
	}

	if (ChildInstance)
	{
		ChildInstance->OnFlowFinished.RemoveDynamic(this, &USimFlowNode_SubFlow::HandleChildFinished);
		ChildInstance = nullptr;
	}

	const bool bSucceeded = (FinalState == ESimFlowRunState::Completed);
	FName PinName = bSucceeded ? SimFlowPins::Completed : SimFlowPins::Failed;

	const FSimFlowOutputPin* Pin = FindOutputPin(PinName);
	if (!bSucceeded && (!Pin || Pin->Links.Num() == 0))
	{
		PinName = SimFlowPins::Completed;
	}

	TriggerOutput(PinName);
}

FText USimFlowNode_SubFlow::GetNodeSubtitle() const
{
	return SubFlow ? FText::FromString(SubFlow->GetName()) : LOCTEXT("NoSubFlow", "<none>");
}

FLinearColor USimFlowNode_SubFlow::GetNodeColor() const { return FLinearColor(0.20f, 0.45f, 0.45f); }
FText USimFlowNode_SubFlow::GetNodeCategory() const { return LOCTEXT("CatComposition", "Composition"); }

FString USimFlowNode_SubFlow::GetDebugStatus() const
{
	return FString::Printf(TEXT("Sub Flow: %s"), SubFlow ? *SubFlow->GetName() : TEXT("<none>"));
}

// =====================================================================
//  Checkpoint
// =====================================================================

USimFlowNode_Checkpoint::USimFlowNode_Checkpoint()
{
	RebuildPins();
}

void USimFlowNode_Checkpoint::BuildPins()
{
	AddInputPin(SimFlowPins::In);
	AddOutputPin(SimFlowPins::Out);
}

void USimFlowNode_Checkpoint::ExecuteInput(FName /*PinName*/)
{
	if (FlowInstance)
	{
		FlowInstance->NotifyCheckpointReached(this);

		if (bAutoSave)
		{
			if (USimFlowComponent* Component = FlowInstance->GetOwningComponent())
			{
				const FString Slot = SaveSlotName.IsEmpty() ? Component->DefaultSaveSlotName : SaveSlotName;
				Component->SaveFlowToSlot(Slot, SaveUserIndex);
			}
		}
	}

	TriggerOutput(SimFlowPins::Out);
}

FText USimFlowNode_Checkpoint::GetNodeSubtitle() const
{
	FString Text = CheckpointId.IsNone() ? FString() : CheckpointId.ToString();
	if (bAutoSave)
	{
		Text += Text.IsEmpty() ? TEXT("auto-save") : TEXT(" (auto-save)");
	}
	return FText::FromString(Text);
}

FLinearColor USimFlowNode_Checkpoint::GetNodeColor() const { return FLinearColor(0.15f, 0.45f, 0.35f); }
FText USimFlowNode_Checkpoint::GetNodeCategory() const { return LOCTEXT("CatPersistence", "Persistence"); }

// =====================================================================
//  Set Blackboard
// =====================================================================

USimFlowNode_SetBlackboard::USimFlowNode_SetBlackboard()
{
	RebuildPins();
}

void USimFlowNode_SetBlackboard::BuildPins()
{
	AddInputPin(SimFlowPins::In);
	AddOutputPin(SimFlowPins::Out);
}

void USimFlowNode_SetBlackboard::ExecuteInput(FName /*PinName*/)
{
	if (USimFlowBlackboard* Blackboard = GetBlackboard())
	{
		if (bAdd)
		{
			Blackboard->AddToValue(Key, Value);
		}
		else
		{
			Blackboard->SetValue(Key, Value);
		}
	}
	TriggerOutput(SimFlowPins::Out);
}

FText USimFlowNode_SetBlackboard::GetNodeSubtitle() const
{
	return FText::FromString(FString::Printf(TEXT("%s %s %s"),
		*Key.ToString(), bAdd ? TEXT("+=") : TEXT("="), *Value.ToDisplayString()));
}

FLinearColor USimFlowNode_SetBlackboard::GetNodeColor() const { return SimFlowColor_Data; }
FText USimFlowNode_SetBlackboard::GetNodeCategory() const { return LOCTEXT("CatData", "Data"); }

// =====================================================================
//  Finish
// =====================================================================

USimFlowNode_Finish::USimFlowNode_Finish()
{
	RebuildPins();
}

void USimFlowNode_Finish::BuildPins()
{
	AddInputPin(SimFlowPins::In);
}

void USimFlowNode_Finish::ExecuteInput(FName /*PinName*/)
{
	if (!FlowInstance)
	{
		return;
	}

	ESimFlowRunState FinalState = ESimFlowRunState::Completed;
	switch (FinishMode)
	{
	case ESimFlowFinishMode::Complete:	FinalState = ESimFlowRunState::Completed;	break;
	case ESimFlowFinishMode::Fail:		FinalState = ESimFlowRunState::Failed;		break;
	case ESimFlowFinishMode::Abort:		FinalState = ESimFlowRunState::Aborted;		break;
	}

	FlowInstance->FinishInstance(FinalState, bStopOtherBranches);
}

FText USimFlowNode_Finish::GetNodeTitle() const
{
	switch (FinishMode)
	{
	case ESimFlowFinishMode::Fail:	return LOCTEXT("FinishFail", "Fail Flow");
	case ESimFlowFinishMode::Abort:	return LOCTEXT("FinishAbort", "Abort Flow");
	default:						return LOCTEXT("FinishComplete", "Complete Flow");
	}
}

FLinearColor USimFlowNode_Finish::GetNodeColor() const
{
	return (FinishMode == ESimFlowFinishMode::Complete) ? FLinearColor(0.10f, 0.45f, 0.18f) : SimFlowColor_End;
}

FText USimFlowNode_Finish::GetNodeCategory() const { return LOCTEXT("CatFlow", "Flow Control"); }

#undef LOCTEXT_NAMESPACE
