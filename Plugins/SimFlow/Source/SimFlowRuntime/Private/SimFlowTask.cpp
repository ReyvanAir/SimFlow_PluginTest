// Copyright SimFlow. All Rights Reserved.

#include "SimFlowTask.h"
#include "SimFlowGameplayTags.h"
#include "SimFlowInstance.h"
#include "SimFlowBlackboard.h"
#include "SimFlowNodes.h"
#include "SimFlowComponent.h"
#include "SimFlowRuntimeModule.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

USimFlowTask::USimFlowTask()
{
	DisplayName = FText::GetEmpty();
}

void USimFlowTask::InitializeTask(USimFlowInstance* InInstance, USimFlowNode_Task* InNode)
{
	FlowInstance = InInstance;
	OwningNode = InNode;
}

void USimFlowTask::StartTask()
{
	bIsRunning = true;
	bIsPaused = false;
	bFinishRequested = false;
	ElapsedTime = 0.f;

	NativeTaskStart();
	ReceiveTaskStart();
}

void USimFlowTask::TickTask(float DeltaTime)
{
	if (!bIsRunning || bFinishRequested)
	{
		return;
	}
	if (bIsPaused && !bTickWhilePaused)
	{
		return;
	}

	ElapsedTime += DeltaTime;
	NativeTaskTick(DeltaTime);
	ReceiveTaskTick(DeltaTime);
}

void USimFlowTask::PauseTask()
{
	if (!bIsRunning || bIsPaused)
	{
		return;
	}
	bIsPaused = true;
	NativeTaskPause();
	ReceiveTaskPause();
}

void USimFlowTask::ResumeTask()
{
	if (!bIsRunning || !bIsPaused)
	{
		return;
	}
	bIsPaused = false;
	NativeTaskResume();
	ReceiveTaskResume();
}

void USimFlowTask::RestartTask()
{
	// End the current activation quietly, then start again.
	if (bIsRunning)
	{
		NativeTaskEnd(ESimFlowResult::Aborted);
		ReceiveTaskEnd(ESimFlowResult::Aborted);
	}

	RetryCount++;
	ReceiveTaskRetry(RetryCount);
	StartTask();
}

void USimFlowTask::AbortTask()
{
	if (!bIsRunning)
	{
		return;
	}
	bIsRunning = false;
	bIsPaused = false;
	NativeTaskEnd(ESimFlowResult::Aborted);
	ReceiveTaskEnd(ESimFlowResult::Aborted);
}

void USimFlowTask::FinishTask(ESimFlowResult Result)
{
	if (!bIsRunning || bFinishRequested)
	{
		return;
	}

	bFinishRequested = true;
	bIsRunning = false;
	bIsPaused = false;

	if (USimFlowBlackboard* Blackboard = GetBlackboard())
	{
		if (Result == ESimFlowResult::Succeeded && !FMath::IsNearlyZero(ScoreOnSuccess))
		{
			Blackboard->AddScore(ScoreOnSuccess);
		}
		else if ((Result == ESimFlowResult::Failed || Result == ESimFlowResult::TimedOut) && !FMath::IsNearlyZero(ScoreOnFailure))
		{
			Blackboard->AddScore(ScoreOnFailure);
		}
	}

	NativeTaskEnd(Result);
	ReceiveTaskEnd(Result);
	OnTaskFinished.Broadcast(Result);

	if (OwningNode)
	{
		OwningNode->HandleTaskFinished(Result);
	}
}

void USimFlowTask::RecordMistake(FGameplayTag Kind, UObject* Involved, const FText& Description,
	ESimFlowMatchQuality Severity)
{
	if (FlowInstance)
	{
		FlowInstance->RecordMistake(Kind, Involved, Description, Severity, TaskId);
	}
}

bool USimFlowTask::ApplyMismatchPolicy(ESimFlowMismatchPolicy Policy, FGameplayTag MistakeKind, UObject* Involved,
	const FText& Description, ESimFlowMatchQuality Severity)
{
	if (Policy == ESimFlowMismatchPolicy::Ignore)
	{
		return false;
	}

	RecordMistake(MistakeKind, Involved, Description, Severity);

	if (USimFlowBlackboard* Blackboard = GetBlackboard())
	{
		Blackboard->AddToValue(SimFlowKeys::WrongAttempts, FSimFlowValue::MakeInt(1));
	}

	if (Policy == ESimFlowMismatchPolicy::FailTask)
	{
		FinishTask(ESimFlowResult::Failed);
		return true;
	}

	return false;
}

USimFlowBlackboard* USimFlowTask::GetBlackboard() const
{
	return FlowInstance ? FlowInstance->GetBlackboard() : nullptr;
}

AActor* USimFlowTask::GetFlowOwner() const
{
	if (FlowInstance)
	{
		if (USimFlowComponent* Component = FlowInstance->GetOwningComponent())
		{
			return Component->GetOwner();
		}
	}
	return nullptr;
}

APawn* USimFlowTask::GetPlayerPawn() const
{
	const UWorld* World = GetWorld();
	return World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
}

bool USimFlowTask::CanRetry() const
{
	if (!bAllowRetry)
	{
		return false;
	}
	return MaxRetries <= 0 || RetryCount < MaxRetries;
}

FText USimFlowTask::GetDisplayNameText() const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}
	return FText::FromString(GetClass()->GetName().Replace(TEXT("SimFlowTask_"), TEXT("")));
}

void USimFlowTask::SetSavedValue(FName Key, const FSimFlowValue& Value)
{
	if (!Key.IsNone())
	{
		SavedState.Add(Key, Value);
	}
}

FSimFlowValue USimFlowTask::GetSavedValue(FName Key) const
{
	if (const FSimFlowValue* Found = SavedState.Find(Key))
	{
		return *Found;
	}
	return FSimFlowValue();
}

void USimFlowTask::SaveTaskState(TArray<FSimFlowBlackboardEntry>& OutData)
{
	SavedState.Reset();
	ReceiveSaveTaskState();

	OutData.Reset();
	for (TPair<FName, FSimFlowValue>& Pair : SavedState)
	{
		Pair.Value.StripObjectReferences();
		OutData.Emplace(Pair.Key, Pair.Value);
	}
}

void USimFlowTask::LoadTaskState(const TArray<FSimFlowBlackboardEntry>& InData)
{
	SavedState.Reset();
	for (const FSimFlowBlackboardEntry& Entry : InData)
	{
		SavedState.Add(Entry.Key, Entry.Value);
	}
	ReceiveLoadTaskState();
}

UWorld* USimFlowTask::GetWorld() const
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
