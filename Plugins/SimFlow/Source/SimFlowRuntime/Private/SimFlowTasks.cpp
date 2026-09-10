// Copyright SimFlow. All Rights Reserved.

#include "SimFlowTasks.h"
#include "SimFlowZone.h"
#include "SimFlowGameplayTags.h"
#include "SimFlowInstance.h"
#include "SimFlowBlackboard.h"
#include "SimFlowCondition.h"
#include "SimFlowRuntimeModule.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "SimFlowTasks"

// ---------------------------------------------------------------------- Delay

USimFlowTask_Delay::USimFlowTask_Delay()
{
	DisplayName = LOCTEXT("DelayName", "Delay");
	bAllowRetry = false;
}

void USimFlowTask_Delay::NativeTaskStart()
{
	TargetTime = Duration + (RandomExtra > 0.f ? FMath::FRandRange(0.f, RandomExtra) : 0.f);
	if (TargetTime <= 0.f)
	{
		FinishTask(ESimFlowResult::Succeeded);
	}
}

void USimFlowTask_Delay::NativeTaskTick(float /*DeltaTime*/)
{
	if (ElapsedTime >= TargetTime)
	{
		FinishTask(ESimFlowResult::Succeeded);
	}
}

// ------------------------------------------------------------------------ Log

USimFlowTask_Log::USimFlowTask_Log()
{
	DisplayName = LOCTEXT("LogName", "Log Message");
	bAllowRetry = false;
}

void USimFlowTask_Log::NativeTaskStart()
{
	UE_LOG(LogSimFlow, Log, TEXT("[SimFlow] %s"), *Message);

	if (bPrintToScreen && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, ScreenDuration, FColor::Cyan, FString::Printf(TEXT("[SimFlow] %s"), *Message));
	}

	FinishTask(ESimFlowResult::Succeeded);
}

// -------------------------------------------------------------- SetBlackboard

USimFlowTask_SetBlackboard::USimFlowTask_SetBlackboard()
{
	DisplayName = LOCTEXT("SetBlackboardName", "Set Blackboard Value");
	bAllowRetry = false;
}

void USimFlowTask_SetBlackboard::NativeTaskStart()
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
	FinishTask(ESimFlowResult::Succeeded);
}

// -------------------------------------------------------------- WaitForEvent

USimFlowTask_WaitForEvent::USimFlowTask_WaitForEvent()
{
	DisplayName = LOCTEXT("WaitForEventName", "Wait For Event");
}

void USimFlowTask_WaitForEvent::NativeTaskStart()
{
	if (!EventTag.IsValid())
	{
		UE_LOG(LogSimFlow, Warning, TEXT("WaitForEvent task has no tag set - finishing immediately."));
		FinishTask(ESimFlowResult::Succeeded);
		return;
	}

	// A past event kept only its tag, so there is no payload left to check against.
	// Honouring bAcceptAlreadyRaised here would let the wrong object satisfy the task.
	if (bAcceptAlreadyRaised && !ExpectedPayload.IsSet() && FlowInstance && FlowInstance->WasEventRaised(EventTag))
	{
		FinishTask(ESimFlowResult::Succeeded);
		return;
	}

	if (bAcceptAlreadyRaised && ExpectedPayload.IsSet())
	{
		UE_LOG(LogSimFlow, Verbose,
			TEXT("WaitForEvent '%s': bAcceptAlreadyRaised ignored because an expected payload is set."),
			*GetDisplayNameText().ToString());
	}

	if (FlowInstance)
	{
		FlowInstance->OnEventRaised.AddDynamic(this, &USimFlowTask_WaitForEvent::HandleEvent);
	}
}

void USimFlowTask_WaitForEvent::NativeTaskEnd(ESimFlowResult /*Result*/)
{
	if (FlowInstance)
	{
		FlowInstance->OnEventRaised.RemoveDynamic(this, &USimFlowTask_WaitForEvent::HandleEvent);
	}
}

void USimFlowTask_WaitForEvent::HandleEvent(FGameplayTag Tag, UObject* Payload)
{
	if (!bIsRunning || bIsPaused)
	{
		return;
	}

	const bool bMatches = bMatchChildTags ? Tag.MatchesTag(EventTag) : (Tag == EventTag);
	if (!bMatches)
	{
		return;
	}

	if (ExpectedPayload.IsSet())
	{
		const ESimFlowMatchQuality Quality = ExpectedPayload.MatchObject(Payload, FlowInstance);
		if (Quality != ESimFlowMatchQuality::Exact)
		{
			OnPayloadRejected.Broadcast(Payload, Quality);

			const FText Description = FText::Format(
				LOCTEXT("WrongTargetMistake", "Interacted with {0} - expected {1}"),
				USimFlowIdentityStatics::GetIdentityDisplayName(Cast<AActor>(Payload)),
				ExpectedPayload.Describe());

			ApplyMismatchPolicy(MismatchPolicy, SimFlowTags::Mistake_WrongTarget, Payload, Description, Quality);
			return;
		}
	}

	if (!PayloadToBlackboardKey.IsNone() && Payload)
	{
		if (USimFlowBlackboard* Blackboard = GetBlackboard())
		{
			Blackboard->SetObject(PayloadToBlackboardKey, Payload);
		}
	}

	FinishTask(ESimFlowResult::Succeeded);
}

// ---------------------------------------------------------- WaitForCondition

USimFlowTask_WaitForCondition::USimFlowTask_WaitForCondition()
{
	DisplayName = LOCTEXT("WaitForConditionName", "Wait For Condition");
}

void USimFlowTask_WaitForCondition::NativeTaskStart()
{
	CheckAccumulator = 0.f;
	HoldAccumulator = 0.f;

	if (!Condition)
	{
		UE_LOG(LogSimFlow, Warning, TEXT("WaitForCondition task has no condition - finishing immediately."));
		FinishTask(ESimFlowResult::Succeeded);
	}
}

void USimFlowTask_WaitForCondition::NativeTaskTick(float DeltaTime)
{
	if (!Condition)
	{
		return;
	}

	CheckAccumulator += DeltaTime;
	if (CheckAccumulator < CheckInterval)
	{
		return;
	}

	const float Slice = CheckAccumulator;
	CheckAccumulator = 0.f;

	if (Condition->Evaluate(FlowInstance))
	{
		HoldAccumulator += Slice;
		if (HoldAccumulator >= RequiredHoldTime)
		{
			FinishTask(ESimFlowResult::Succeeded);
		}
	}
	else
	{
		HoldAccumulator = 0.f;
	}
}

// ------------------------------------------------------------- GoToLocation

USimFlowTask_GoToLocation::USimFlowTask_GoToLocation()
{
	DisplayName = LOCTEXT("GoToLocationName", "Go To Location");
}

FVector USimFlowTask_GoToLocation::GetResolvedTargetLocation() const
{
	FVector Target = TargetLocation;

	if (!TargetFromBlackboardKey.IsNone())
	{
		if (USimFlowBlackboard* Blackboard = GetBlackboard())
		{
			Target = Blackboard->GetVector(TargetFromBlackboardKey, TargetLocation);
			return Target;
		}
	}

	if (bRelativeToFlowOwner)
	{
		if (const AActor* Owner = GetFlowOwner())
		{
			Target = Owner->GetActorTransform().TransformPosition(TargetLocation);
		}
	}

	return Target;
}

void USimFlowTask_GoToLocation::NativeTaskTick(float /*DeltaTime*/)
{
	const APawn* Pawn = GetPlayerPawn();
	if (!Pawn)
	{
		return;
	}

	const FVector Target = GetResolvedTargetLocation();

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugSphere)
	{
		if (const UWorld* World = GetWorld())
		{
			DrawDebugSphere(World, Target, AcceptanceRadius, 16, FColor::Green, false, -1.f, 0, 2.f);
		}
	}
#endif

	FVector Delta = Pawn->GetActorLocation() - Target;
	if (bIgnoreZ)
	{
		Delta.Z = 0.f;
	}

	if (Delta.SizeSquared() <= FMath::Square(AcceptanceRadius))
	{
		FinishTask(ESimFlowResult::Succeeded);
	}
}

// ------------------------------------------------------------------------ Quiz

USimFlowTask_Quiz::USimFlowTask_Quiz()
{
	DisplayName = LOCTEXT("QuizName", "Quiz");
	bAllowRetry = true;
}

void USimFlowTask_Quiz::NativeTaskStart()
{
	OnQuizPresented.Broadcast(this);

	if (FlowInstance)
	{
		FlowInstance->NotifyQuizPresented(this);
	}
}

void USimFlowTask_Quiz::NativeTaskEnd(ESimFlowResult /*Result*/)
{
	// Nothing to unbind - UI listens to OnQuizPresented and calls SubmitAnswer.
}

bool USimFlowTask_Quiz::IsCorrectIndex(int32 OptionIndex) const
{
	return OptionIndex == CorrectOptionIndex || AdditionalCorrectIndices.Contains(OptionIndex);
}

void USimFlowTask_Quiz::SubmitAnswer(int32 OptionIndex)
{
	if (!bIsRunning)
	{
		UE_LOG(LogSimFlow, Warning, TEXT("SubmitAnswer called on a quiz that is not running."));
		return;
	}

	const bool bCorrect = IsCorrectIndex(OptionIndex);

	if (USimFlowBlackboard* Blackboard = GetBlackboard())
	{
		Blackboard->SetInt(SimFlowKeys::LastAnswerIndex, OptionIndex);
		Blackboard->SetBool(SimFlowKeys::LastAnswerCorrect, bCorrect);

		if (!AnswerBlackboardKey.IsNone())
		{
			Blackboard->SetInt(AnswerBlackboardKey, OptionIndex);
		}
	}

	if (!bCorrect && bCountMistakes)
	{
		// RecordMistake bumps the Mistakes key itself, so the quiz lands in the same
		// debrief list as a wrong item or an out-of-order step.
		RecordMistake(SimFlowTags::Mistake_WrongAnswer, this,
			FText::Format(LOCTEXT("WrongAnswerMistake", "Answered option {0} - expected option {1}"),
				FText::AsNumber(OptionIndex + 1), FText::AsNumber(CorrectOptionIndex + 1)),
			ESimFlowMatchQuality::Related);
	}

	OnQuizAnswered.Broadcast(OptionIndex, bCorrect);

	if (bCorrect)
	{
		FinishTask(ESimFlowResult::Succeeded);
	}
	else
	{
		FinishTask(bFailOnWrongAnswer ? ESimFlowResult::Failed : ESimFlowResult::Succeeded);
	}
}

// -------------------------------------------------------------- ParallelGroup

USimFlowTask_ParallelGroup::USimFlowTask_ParallelGroup()
{
	DisplayName = LOCTEXT("ParallelGroupName", "Parallel Group");
}

void USimFlowTask_ParallelGroup::NativeTaskStart()
{
	FinishedCount = 0;
	bAnyChildFailed = false;

	if (Tasks.Num() == 0)
	{
		FinishTask(ESimFlowResult::Succeeded);
		return;
	}

	// Snapshot: children may finish synchronously inside StartTask.
	TArray<TObjectPtr<USimFlowTask>> Snapshot = Tasks;
	for (const TObjectPtr<USimFlowTask>& Child : Snapshot)
	{
		if (!Child)
		{
			FinishedCount++;
			continue;
		}
		Child->InitializeTask(FlowInstance, nullptr);
		Child->OnTaskFinished.AddDynamic(this, &USimFlowTask_ParallelGroup::HandleChildFinished);
		Child->StartTask();

		if (!bIsRunning)
		{
			// A child finished the group already.
			return;
		}
	}
}

void USimFlowTask_ParallelGroup::NativeTaskTick(float DeltaTime)
{
	for (const TObjectPtr<USimFlowTask>& Child : Tasks)
	{
		if (Child && Child->bIsRunning)
		{
			Child->TickTask(DeltaTime);
			if (!bIsRunning)
			{
				return;
			}
		}
	}
}

void USimFlowTask_ParallelGroup::NativeTaskPause()
{
	for (const TObjectPtr<USimFlowTask>& Child : Tasks)
	{
		if (Child)
		{
			Child->PauseTask();
		}
	}
}

void USimFlowTask_ParallelGroup::NativeTaskResume()
{
	for (const TObjectPtr<USimFlowTask>& Child : Tasks)
	{
		if (Child)
		{
			Child->ResumeTask();
		}
	}
}

void USimFlowTask_ParallelGroup::NativeTaskEnd(ESimFlowResult /*Result*/)
{
	for (const TObjectPtr<USimFlowTask>& Child : Tasks)
	{
		if (Child)
		{
			Child->OnTaskFinished.RemoveDynamic(this, &USimFlowTask_ParallelGroup::HandleChildFinished);
			if (Child->bIsRunning)
			{
				Child->AbortTask();
			}
		}
	}
}

void USimFlowTask_ParallelGroup::HandleChildFinished(ESimFlowResult Result)
{
	FinishedCount++;

	if (Result == ESimFlowResult::Failed || Result == ESimFlowResult::TimedOut)
	{
		bAnyChildFailed = true;
	}

	const bool bDone = bWaitForAll ? (FinishedCount >= Tasks.Num()) : true;
	if (bDone && bIsRunning)
	{
		const bool bFailed = bAnyChildFailed && bFailIfAnyChildFails;
		FinishTask(bFailed ? ESimFlowResult::Failed : ESimFlowResult::Succeeded);
	}
}

// -------------------------------------------------------------- PlaceObject

USimFlowTask_PlaceObject::USimFlowTask_PlaceObject()
{
	DisplayName = LOCTEXT("PlaceObjectName", "Place Object In Zone");
}

void USimFlowTask_PlaceObject::NativeTaskStart()
{
	ReportedWrongItems.Reset();

	ResolvedZone = ResolveZone();
	if (!ResolvedZone)
	{
		LogZoneResolveFailure();
		FinishTask(ESimFlowResult::Failed);
		return;
	}

	ResolvedZone->OnActorSettled.AddDynamic(this, &USimFlowTask_PlaceObject::HandleZoneSettled);
	ResolvedZone->OnActorExited.AddDynamic(this, &USimFlowTask_PlaceObject::HandleZoneExited);

	// The right item may already be sitting there before the task started.
	EvaluateZoneContents();
}

void USimFlowTask_PlaceObject::NativeTaskEnd(ESimFlowResult /*Result*/)
{
	if (ResolvedZone)
	{
		ResolvedZone->OnActorSettled.RemoveDynamic(this, &USimFlowTask_PlaceObject::HandleZoneSettled);
		ResolvedZone->OnActorExited.RemoveDynamic(this, &USimFlowTask_PlaceObject::HandleZoneExited);
	}
	ResolvedZone = nullptr;
	ReportedWrongItems.Reset();
}

ASimFlowZone* USimFlowTask_PlaceObject::ResolveZone() const
{
	// Specific Actor and Blackboard Key each name one actor, so the generic resolver
	// is the right answer for them - and being told the named actor is not a zone is
	// useful, because naming the wrong actor is the mistake.
	if (!Zone.SpecificActor.IsNull() || !Zone.BlackboardKey.IsNone())
	{
		return Cast<ASimFlowZone>(Zone.Resolve(FlowInstance));
	}

	// A tag query describes a set, and the only member of that set this task can use
	// is a zone. Scanning every actor instead would let any mistagged prop win on
	// iteration order alone and take the task down with it.
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<ASimFlowZone> It(World); It; ++It)
		{
			if (Zone.MatchActor(*It, FlowInstance) == ESimFlowMatchQuality::Exact)
			{
				return *It;
			}
		}
	}

	return nullptr;
}

void USimFlowTask_PlaceObject::LogZoneResolveFailure() const
{
	const FString TaskName = GetDisplayNameText().ToString();
	const FString QueryText = Zone.Describe().ToString();

	// Four different mistakes used to share one message. They need different fixes.
	if (!Zone.IsSet())
	{
		UE_LOG(LogSimFlow, Warning,
			TEXT("PlaceObject task '%s' has an empty Zone query, so there is nothing to watch - failing. ")
			TEXT("Set Zone > Required Tags to the zone's identity tag, e.g. Zone.PartsBin."),
			*TaskName);
		return;
	}

	// The named-actor forms: say what was named and what it turned out to be.
	if (!Zone.SpecificActor.IsNull() || !Zone.BlackboardKey.IsNone())
	{
		if (const AActor* Named = Zone.Resolve(FlowInstance))
		{
			UE_LOG(LogSimFlow, Warning,
				TEXT("PlaceObject task '%s' resolved its Zone query (%s) to '%s', which is a %s and not a SimFlow Zone - failing. ")
				TEXT("A plain trigger volume will not do; the actor has to be an ASimFlowZone or a Blueprint child of one."),
				*TaskName, *QueryText, *Named->GetName(), *Named->GetClass()->GetName());
		}
		else
		{
			UE_LOG(LogSimFlow, Warning,
				TEXT("PlaceObject task '%s' could not resolve its Zone query (%s) to any actor - failing. ")
				TEXT("A Specific Actor pointing into a level that is not loaded resolves to nothing."),
				*TaskName, *QueryText);
		}
		return;
	}

	// The tag form. Name the zones that do exist and what they are actually tagged,
	// because the answer is almost always visible in that list.
	FString Present;
	int32 ZoneCount = 0;
	const UWorld* World = GetWorld();
	if (World)
	{
		for (TActorIterator<ASimFlowZone> It(World); It; ++It)
		{
			++ZoneCount;
			if (ZoneCount > 8)
			{
				continue;
			}
			const FGameplayTagContainer Tags = USimFlowIdentityStatics::GetIdentityTags(*It);
			Present += FString::Printf(TEXT("\n    %s  tagged: %s"),
				*It->GetName(),
				Tags.IsEmpty() ? TEXT("(no identity tags)") : *Tags.ToStringSimple());
		}
	}

	if (ZoneCount == 0)
	{
		UE_LOG(LogSimFlow, Warning,
			TEXT("PlaceObject task '%s' found no SimFlow Zone anywhere in the level to match its Zone query (%s) - failing. ")
			TEXT("Check you are playing the level the zone is in."),
			*TaskName, *QueryText);
		return;
	}

	// A non-zone wearing the zone's tag no longer breaks the lookup, but it is still
	// worth naming - it is usually the thing the author meant to tag differently.
	FString Decoy;
	if (const AActor* AnyMatch = Zone.Resolve(FlowInstance))
	{
		if (!AnyMatch->IsA<ASimFlowZone>())
		{
			Decoy = FString::Printf(
				TEXT(" Note that '%s' (a %s) also carries this tag and is not a zone."),
				*AnyMatch->GetName(), *AnyMatch->GetClass()->GetName());
		}
	}

	UE_LOG(LogSimFlow, Warning,
		TEXT("PlaceObject task '%s' matched none of the %d SimFlow Zone(s) in the level against its Zone query (%s) - failing. ")
		TEXT("The tag has to be on the zone's SimFlow Identity component, not the actor's own Tags array.%s Zones present:%s"),
		*TaskName, ZoneCount, *QueryText, *Decoy, *Present);
}

ESimFlowMatchQuality USimFlowTask_PlaceObject::JudgeItem(const AActor* Actor) const
{
	if (!Actor)
	{
		return ESimFlowMatchQuality::NoMatch;
	}

	const ESimFlowMatchQuality Quality = AcceptedItems.MatchActor(Actor, FlowInstance);

	// An explicit rejection vetoes an accept - that is the whole job of RejectedItems,
	// and with it empty anything AcceptedItems does not match is wrong anyway. How
	// wrong stays a question about the tags: a rejected item is only a near miss when
	// it actually looks like the answer, so a wrench in the extinguisher bay is not
	// told it was close.
	if (Quality == ESimFlowMatchQuality::Exact
		&& RejectedItems.IsSet()
		&& RejectedItems.MatchActor(Actor, FlowInstance) == ESimFlowMatchQuality::Exact)
	{
		return ESimFlowMatchQuality::Related;
	}

	return Quality;
}

int32 USimFlowTask_PlaceObject::GetAcceptedCount() const
{
	if (!ResolvedZone)
	{
		return 0;
	}

	const TArray<AActor*> Candidates = bRequireSettled
		? ResolvedZone->GetSettledActors()
		: ResolvedZone->GetContainedActors();

	int32 Count = 0;
	for (const AActor* Actor : Candidates)
	{
		if (JudgeItem(Actor) == ESimFlowMatchQuality::Exact)
		{
			++Count;
		}
	}
	return Count;
}

void USimFlowTask_PlaceObject::HandleZoneSettled(ASimFlowZone* /*InZone*/, AActor* Actor)
{
	if (!bIsRunning || bIsPaused || !Actor)
	{
		return;
	}

	const ESimFlowMatchQuality Quality = JudgeItem(Actor);

	if (Quality == ESimFlowMatchQuality::Exact)
	{
		if (!PlacedItemBlackboardKey.IsNone())
		{
			if (USimFlowBlackboard* Blackboard = GetBlackboard())
			{
				Blackboard->SetObject(PlacedItemBlackboardKey, Actor);
			}
		}

		OnCorrectItemPlaced.Broadcast(Actor, GetAcceptedCount());
		EvaluateZoneContents();
		return;
	}

	// Wrong thing. Report it once per item unless the author asked for every attempt.
	if (bReportEachWrongItemOnce && ReportedWrongItems.Contains(Actor))
	{
		return;
	}
	ReportedWrongItems.Add(Actor);

	OnWrongItemPlaced.Broadcast(Actor, Quality);

	const FText Description = FText::Format(
		LOCTEXT("WrongItemMistake", "Placed {0} in {1} - expected {2}"),
		USimFlowIdentityStatics::GetIdentityDisplayName(Actor),
		ResolvedZone ? ResolvedZone->GetDisplayNameText() : LOCTEXT("TheZone", "the zone"),
		AcceptedItems.Describe());

	ApplyMismatchPolicy(WrongItemPolicy, SimFlowTags::Mistake_WrongItem, Actor, Description, Quality);
}

void USimFlowTask_PlaceObject::HandleZoneExited(ASimFlowZone* /*InZone*/, AActor* Actor)
{
	if (!bIsRunning || bIsPaused)
	{
		return;
	}

	// Taking a wrong item back out lets the trainee be told about it again.
	ReportedWrongItems.Remove(Actor);
}

void USimFlowTask_PlaceObject::EvaluateZoneContents()
{
	if (!bIsRunning || bIsPaused)
	{
		return;
	}

	if (GetAcceptedCount() >= FMath::Max(1, RequiredCount))
	{
		FinishTask(ESimFlowResult::Succeeded);
	}
}

// ----------------------------------------------------------- OrderedSequence

USimFlowTask_OrderedSequence::USimFlowTask_OrderedSequence()
{
	DisplayName = LOCTEXT("OrderedSequenceName", "Ordered Sequence");
}

void USimFlowTask_OrderedSequence::NativeTaskStart()
{
	if (Steps.Num() == 0)
	{
		UE_LOG(LogSimFlow, Warning, TEXT("Ordered Sequence task has no steps - finishing immediately."));
		FinishTask(ESimFlowResult::Succeeded);
		return;
	}

	if (!EventTag.IsValid())
	{
		UE_LOG(LogSimFlow, Warning, TEXT("Ordered Sequence task has no event tag set - failing."));
		FinishTask(ESimFlowResult::Failed);
		return;
	}

	SetCurrentStep(0);

	if (FlowInstance)
	{
		FlowInstance->OnEventRaised.AddDynamic(this, &USimFlowTask_OrderedSequence::HandleEvent);
	}
}

void USimFlowTask_OrderedSequence::NativeTaskEnd(ESimFlowResult /*Result*/)
{
	if (FlowInstance)
	{
		FlowInstance->OnEventRaised.RemoveDynamic(this, &USimFlowTask_OrderedSequence::HandleEvent);
	}
}

void USimFlowTask_OrderedSequence::SetCurrentStep(int32 NewStep)
{
	CurrentStep = NewStep;

	if (USimFlowBlackboard* Blackboard = GetBlackboard())
	{
		Blackboard->SetInt(SimFlowKeys::CurrentStep, CurrentStep);

		if (!StepBlackboardKey.IsNone())
		{
			Blackboard->SetInt(StepBlackboardKey, CurrentStep);
		}
	}
}

FText USimFlowTask_OrderedSequence::GetCurrentStepInstruction() const
{
	return Steps.IsValidIndex(CurrentStep) ? Steps[CurrentStep].Instruction : FText::GetEmpty();
}

void USimFlowTask_OrderedSequence::HandleEvent(FGameplayTag Tag, UObject* Payload)
{
	if (!bIsRunning || bIsPaused || !Steps.IsValidIndex(CurrentStep))
	{
		return;
	}

	const bool bTagMatches = bMatchChildTags ? Tag.MatchesTag(EventTag) : (Tag == EventTag);
	if (!bTagMatches)
	{
		return;
	}

	// The step we are actually waiting for.
	if (Steps[CurrentStep].Target.MatchObject(Payload, FlowInstance) == ESimFlowMatchQuality::Exact)
	{
		OnStepCompleted.Broadcast(CurrentStep, Cast<AActor>(Payload));

		const int32 NextStep = CurrentStep + 1;
		SetCurrentStep(NextStep);

		if (NextStep >= Steps.Num())
		{
			FinishTask(ESimFlowResult::Succeeded);
		}
		return;
	}

	// Not the expected step. Is it one of the other steps, or something unrelated?
	bool bBelongsToSequence = false;
	for (int32 Index = 0; Index < Steps.Num(); ++Index)
	{
		if (Index != CurrentStep && Steps[Index].Target.MatchObject(Payload, FlowInstance) == ESimFlowMatchQuality::Exact)
		{
			bBelongsToSequence = true;
			break;
		}
	}

	if (!bBelongsToSequence && !bUnlistedInputIsMistake)
	{
		return;
	}

	if (OutOfOrderPolicy == ESimFlowOutOfOrderPolicy::Ignore)
	{
		return;
	}

	OnWrongInput.Broadcast(Payload, CurrentStep);

	const FText Description = FText::Format(
		LOCTEXT("WrongOrderMistake", "Step {0}: used {1} - expected {2}"),
		FText::AsNumber(CurrentStep + 1),
		USimFlowIdentityStatics::GetIdentityDisplayName(Cast<AActor>(Payload)),
		Steps[CurrentStep].Target.Describe());

	// A step of the procedure done at the wrong moment is a near miss; a prop that
	// was never part of it at all is not.
	const ESimFlowMatchQuality Severity = bBelongsToSequence
		? ESimFlowMatchQuality::Related
		: ESimFlowMatchQuality::NoMatch;

	RecordMistake(SimFlowTags::Mistake_WrongOrder, Payload, Description, Severity);

	if (USimFlowBlackboard* Blackboard = GetBlackboard())
	{
		Blackboard->AddToValue(SimFlowKeys::WrongAttempts, FSimFlowValue::MakeInt(1));
	}

	switch (OutOfOrderPolicy)
	{
	case ESimFlowOutOfOrderPolicy::RestartSequence:
		SetCurrentStep(0);
		break;

	case ESimFlowOutOfOrderPolicy::FailTask:
		FinishTask(ESimFlowResult::Failed);
		break;

	default:
		// CountMistake: stay where we are and let them try again.
		break;
	}
}

#undef LOCTEXT_NAMESPACE
