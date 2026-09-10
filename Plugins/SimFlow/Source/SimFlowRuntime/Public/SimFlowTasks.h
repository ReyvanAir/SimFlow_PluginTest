// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SimFlowTask.h"
#include "SimFlowIdentity.h"
#include "GameplayTagContainer.h"
#include "SimFlowTasks.generated.h"

class USimFlowCondition;
class ASimFlowZone;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSimFlowPayloadRejected, UObject*, Payload, ESimFlowMatchQuality, Quality);

/** Waits a fixed number of seconds. Respects pause. */
UCLASS(DisplayName = "Delay", meta = (ToolTip = "Waits for a number of seconds, then succeeds."))
class SIMFLOWRUNTIME_API USimFlowTask_Delay : public USimFlowTask
{
	GENERATED_BODY()

public:
	USimFlowTask_Delay();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delay", meta = (ClampMin = "0.0", Units = "s"))
	float Duration = 1.f;

	/** Randomly adds up to this many seconds on top of Duration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delay", meta = (ClampMin = "0.0", Units = "s"), AdvancedDisplay)
	float RandomExtra = 0.f;

	virtual void NativeTaskStart() override;
	virtual void NativeTaskTick(float DeltaTime) override;

private:
	UPROPERTY(Transient)
	float TargetTime = 0.f;
};

/** Prints a message. Great for bringing a flow up before real tasks exist. */
UCLASS(DisplayName = "Log Message")
class SIMFLOWRUNTIME_API USimFlowTask_Log : public USimFlowTask
{
	GENERATED_BODY()

public:
	USimFlowTask_Log();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log")
	FString Message = TEXT("SimFlow");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log")
	bool bPrintToScreen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log", meta = (ClampMin = "0.0", Units = "s"))
	float ScreenDuration = 3.f;

	virtual void NativeTaskStart() override;
};

/** Writes (or adds to) a blackboard key, then finishes immediately. */
UCLASS(DisplayName = "Set Blackboard Value")
class SIMFLOWRUNTIME_API USimFlowTask_SetBlackboard : public USimFlowTask
{
	GENERATED_BODY()

public:
	USimFlowTask_SetBlackboard();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackboard")
	FName Key = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackboard")
	FSimFlowValue Value;

	/** When true the value is added to whatever is already stored instead of replacing it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackboard")
	bool bAdd = false;

	virtual void NativeTaskStart() override;
};

/**
 * Blocks until an event tag is raised on the flow.
 * Raise it with USimFlowComponent::SendEvent or USimFlowStatics::SendFlowEvent.
 */
UCLASS(DisplayName = "Wait For Event")
class SIMFLOWRUNTIME_API USimFlowTask_WaitForEvent : public USimFlowTask
{
	GENERATED_BODY()

public:
	USimFlowTask_WaitForEvent();

	/** The tag this task is listening for. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	FGameplayTag EventTag;

	/** Also accept child tags, e.g. listening for Sim.Grab accepts Sim.Grab.Extinguisher. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	bool bMatchChildTags = true;

	/**
	 * If the tag was already raised earlier in this run, finish straight away.
	 * Ignored when ExpectedPayload is set - a past event carries no payload to check.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event", AdvancedDisplay)
	bool bAcceptAlreadyRaised = false;

	/**
	 * Which object the event has to be about.
	 *
	 * Leave empty and any sender satisfies the task. Fill it in and ten buttons can
	 * all broadcast the same tag while only the intended one counts - which is what
	 * lets the level stay neutral and the flow asset hold the answer.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event|Payload")
	FSimFlowActorQuery ExpectedPayload;

	/** What happens when the tag is right but the object is not. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event|Payload")
	ESimFlowMismatchPolicy MismatchPolicy = ESimFlowMismatchPolicy::CountMistake;

	/** Stores the accepted payload here, so later tasks can refer to "the thing they picked". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event|Payload", AdvancedDisplay)
	FName PayloadToBlackboardKey = NAME_None;

	/** Fires on a wrong object. Bind it for a buzzer, a red outline or a hint. */
	UPROPERTY(BlueprintAssignable, Category = "SimFlow|Event")
	FSimFlowPayloadRejected OnPayloadRejected;

	virtual void NativeTaskStart() override;
	virtual void NativeTaskEnd(ESimFlowResult Result) override;

private:
	UFUNCTION()
	void HandleEvent(FGameplayTag Tag, UObject* Payload);
};

/**
 * Blocks until a condition becomes true. Useful for "player is holding the drill",
 * "valve rotation > 90 degrees" and similar continuous checks in a VR sim.
 */
UCLASS(DisplayName = "Wait For Condition")
class SIMFLOWRUNTIME_API USimFlowTask_WaitForCondition : public USimFlowTask
{
	GENERATED_BODY()

public:
	USimFlowTask_WaitForCondition();

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Condition")
	TObjectPtr<USimFlowCondition> Condition = nullptr;

	/** Seconds between evaluations. 0 evaluates every frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition", meta = (ClampMin = "0.0", Units = "s"))
	float CheckInterval = 0.1f;

	/** The condition must hold for this long before the task succeeds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition", meta = (ClampMin = "0.0", Units = "s"))
	float RequiredHoldTime = 0.f;

	virtual void NativeTaskStart() override;
	virtual void NativeTaskTick(float DeltaTime) override;

private:
	UPROPERTY(Transient) float CheckAccumulator = 0.f;
	UPROPERTY(Transient) float HoldAccumulator = 0.f;
};

/** Blocks until the player pawn reaches a location. The bread and butter of VR tutorials. */
UCLASS(DisplayName = "Go To Location")
class SIMFLOWRUNTIME_API USimFlowTask_GoToLocation : public USimFlowTask
{
	GENERATED_BODY()

public:
	USimFlowTask_GoToLocation();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location", meta = (MakeEditWidget = "true"))
	FVector TargetLocation = FVector::ZeroVector;

	/** When set, the target is read from this blackboard key instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location", AdvancedDisplay)
	FName TargetFromBlackboardKey = NAME_None;

	/** Treat TargetLocation as relative to the flow owner actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
	bool bRelativeToFlowOwner = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location", meta = (ClampMin = "1.0", Units = "cm"))
	float AcceptanceRadius = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
	bool bIgnoreZ = true;

	/** Draws a debug sphere at the target while the task runs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location", AdvancedDisplay)
	bool bDrawDebugSphere = false;

	UFUNCTION(BlueprintPure, Category = "SimFlow|Task")
	FVector GetResolvedTargetLocation() const;

	virtual void NativeTaskTick(float DeltaTime) override;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSimFlowQuizPresented, USimFlowTask_Quiz*, Quiz);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSimFlowQuizAnswered, int32, AnswerIndex, bool, bCorrect);

/**
 * A multiple choice question. Bind OnQuizPresented from your VR widget, show the
 * options, then call SubmitAnswer. Wrong answers can retry, fail, or just continue -
 * wire the Failed pin of the Task node to whatever remediation branch you want.
 */
UCLASS(DisplayName = "Quiz")
class SIMFLOWRUNTIME_API USimFlowTask_Quiz : public USimFlowTask
{
	GENERATED_BODY()

public:
	USimFlowTask_Quiz();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quiz", meta = (MultiLine = "true"))
	FText Question;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quiz")
	TArray<FText> Options;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quiz", meta = (ClampMin = "0"))
	int32 CorrectOptionIndex = 0;

	/** Optional: also treat these indices as correct (multi-answer questions). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quiz", AdvancedDisplay)
	TArray<int32> AdditionalCorrectIndices;

	/** Blackboard key that receives the submitted index. Leave None to skip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quiz", AdvancedDisplay)
	FName AnswerBlackboardKey = NAME_None;

	/** When true a wrong answer immediately fails the task (drives the Failed pin). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quiz")
	bool bFailOnWrongAnswer = true;

	/** Increments the "Mistakes" blackboard key on a wrong answer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quiz")
	bool bCountMistakes = true;

	UPROPERTY(BlueprintAssignable, Category = "SimFlow|Quiz")
	FSimFlowQuizPresented OnQuizPresented;

	UPROPERTY(BlueprintAssignable, Category = "SimFlow|Quiz")
	FSimFlowQuizAnswered OnQuizAnswered;

	/** Call this from your answer buttons. */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Quiz")
	void SubmitAnswer(int32 OptionIndex);

	UFUNCTION(BlueprintPure, Category = "SimFlow|Quiz")
	bool IsCorrectIndex(int32 OptionIndex) const;

	virtual void NativeTaskStart() override;
	virtual void NativeTaskEnd(ESimFlowResult Result) override;
};

/**
 * Runs several child tasks at once inside a single node. Handy when you want a
 * small parallel group without cluttering the graph with Parallel/Join nodes.
 */
UCLASS(DisplayName = "Parallel Group")
class SIMFLOWRUNTIME_API USimFlowTask_ParallelGroup : public USimFlowTask
{
	GENERATED_BODY()

public:
	USimFlowTask_ParallelGroup();

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Group")
	TArray<TObjectPtr<USimFlowTask>> Tasks;

	/** When false the group finishes as soon as the first child finishes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Group")
	bool bWaitForAll = true;

	/** Any child failing fails the whole group. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Group")
	bool bFailIfAnyChildFails = true;

	virtual void NativeTaskStart() override;
	virtual void NativeTaskTick(float DeltaTime) override;
	virtual void NativeTaskPause() override;
	virtual void NativeTaskResume() override;
	virtual void NativeTaskEnd(ESimFlowResult Result) override;

private:
	UFUNCTION()
	void HandleChildFinished(ESimFlowResult Result);

	UPROPERTY(Transient) int32 FinishedCount = 0;
	UPROPERTY(Transient) bool bAnyChildFailed = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSimFlowItemPlaced, AActor*, Item, int32, PlacedCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSimFlowWrongItemPlaced, AActor*, Item, ESimFlowMatchQuality, Quality);

/**
 * "Put the foam extinguisher in the bay."
 *
 * Watches a SimFlow Zone and judges what turns up in it. The zone reports what is
 * there without any opinion; this task holds the opinion, so the same bay can be
 * the right answer in one exercise and a distractor in the next without touching
 * a single Blueprint.
 */
UCLASS(DisplayName = "Place Object In Zone")
class SIMFLOWRUNTIME_API USimFlowTask_PlaceObject : public USimFlowTask
{
	GENERATED_BODY()

public:
	USimFlowTask_PlaceObject();

	/** Which zone to watch. Usually a tag such as Zone.PartsBin, or the zone actor itself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FSimFlowActorQuery Zone;

	/** What belongs there. Tags are the useful form here - they cover spawned copies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FSimFlowActorQuery AcceptedItems;

	/** How many accepted items have to be in the zone at once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	/** What happens when the wrong thing is placed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	ESimFlowMismatchPolicy WrongItemPolicy = ESimFlowMismatchPolicy::CountMistake;

	/**
	 * Optional. Leave it empty and anything AcceptedItems does not match is already
	 * a mistake. Fill it in to subtract from an open-ended accepted family - accept
	 * Item.Extinguisher, reject Item.Extinguisher.CO2 - which keeps working as new
	 * variants of that family are added.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", AdvancedDisplay)
	FSimFlowActorQuery RejectedItems;

	/**
	 * Wait for the zone to report the item as settled - put down and let go - rather
	 * than reacting the instant it overlaps while still in the trainee's hand. Leave
	 * this on and let the zone's Settle Mode decide how patient to be.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", AdvancedDisplay)
	bool bRequireSettled = true;

	/** Only report a given wrong item once, instead of every time it settles again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", AdvancedDisplay)
	bool bReportEachWrongItemOnce = true;

	/** Stores the last accepted item here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", AdvancedDisplay)
	FName PlacedItemBlackboardKey = NAME_None;

	UPROPERTY(BlueprintAssignable, Category = "SimFlow|Placement")
	FSimFlowItemPlaced OnCorrectItemPlaced;

	/** Bind for feedback: a near miss deserves a different hint from a random object. */
	UPROPERTY(BlueprintAssignable, Category = "SimFlow|Placement")
	FSimFlowWrongItemPlaced OnWrongItemPlaced;

	/** The zone this task resolved at start, once it is running. */
	UFUNCTION(BlueprintPure, Category = "SimFlow|Placement")
	ASimFlowZone* GetResolvedZone() const { return ResolvedZone; }

	/** How many accepted items are in the zone right now. */
	UFUNCTION(BlueprintPure, Category = "SimFlow|Placement")
	int32 GetAcceptedCount() const;

	virtual void NativeTaskStart() override;
	virtual void NativeTaskEnd(ESimFlowResult Result) override;

private:
	UFUNCTION()
	void HandleZoneSettled(ASimFlowZone* InZone, AActor* Actor);

	UFUNCTION()
	void HandleZoneExited(ASimFlowZone* InZone, AActor* Actor);

	/** Re-counts what is in the zone and finishes the task when the target is met. */
	void EvaluateZoneContents();

	/**
	 * Finds the zone the Zone query means. The tag form only ever considers actual
	 * zones, so a mistagged prop cannot shadow the real one.
	 */
	ASimFlowZone* ResolveZone() const;

	/** Says which of the four ways the Zone query failed, and what to do about it. */
	void LogZoneResolveFailure() const;

	ESimFlowMatchQuality JudgeItem(const AActor* Actor) const;

	UPROPERTY(Transient)
	TObjectPtr<ASimFlowZone> ResolvedZone = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> ReportedWrongItems;
};

/** One step of an Ordered Sequence task. */
USTRUCT(BlueprintType)
struct SIMFLOWRUNTIME_API FSimFlowSequenceStep
{
	GENERATED_BODY()

	/** The object this step expects, e.g. the second valve or the green button. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
	FSimFlowActorQuery Target;

	/** Shown while this step is the current one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step", meta = (MultiLine = "true"))
	FText Instruction;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSimFlowSequenceStepDone, int32, StepIndex, AActor*, Target);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSimFlowSequenceWrongInput, UObject*, Payload, int32, ExpectedStepIndex);

/**
 * "Press these three buttons, in this order."
 *
 * Every button broadcasts the same tag with itself as the payload and knows
 * nothing about the procedure; this task holds the order. Out-of-order input is a
 * first class outcome rather than something you have to notice by accident.
 */
UCLASS(DisplayName = "Ordered Sequence")
class SIMFLOWRUNTIME_API USimFlowTask_OrderedSequence : public USimFlowTask
{
	GENERATED_BODY()

public:
	USimFlowTask_OrderedSequence();

	/** The event every candidate raises, e.g. SimFlow.Event.ButtonPressed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequence")
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequence")
	bool bMatchChildTags = true;

	/** The expected order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequence")
	TArray<FSimFlowSequenceStep> Steps;

	/** What happens when the trainee acts out of turn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequence")
	ESimFlowOutOfOrderPolicy OutOfOrderPolicy = ESimFlowOutOfOrderPolicy::CountMistake;

	/**
	 * Treat input that is not part of the sequence at all as an out-of-order mistake.
	 * Leave false when unrelated props share the event tag and should just be ignored.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequence")
	bool bUnlistedInputIsMistake = false;

	/** Publishes the current step index here, for a progress widget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequence", AdvancedDisplay)
	FName StepBlackboardKey = NAME_None;

	UPROPERTY(BlueprintAssignable, Category = "SimFlow|Sequence")
	FSimFlowSequenceStepDone OnStepCompleted;

	UPROPERTY(BlueprintAssignable, Category = "SimFlow|Sequence")
	FSimFlowSequenceWrongInput OnWrongInput;

	UFUNCTION(BlueprintPure, Category = "SimFlow|Sequence")
	int32 GetCurrentStepIndex() const { return CurrentStep; }

	/** Instruction text for the step being waited on, for tutorial UI. */
	UFUNCTION(BlueprintPure, Category = "SimFlow|Sequence")
	FText GetCurrentStepInstruction() const;

	virtual void NativeTaskStart() override;
	virtual void NativeTaskEnd(ESimFlowResult Result) override;

private:
	UFUNCTION()
	void HandleEvent(FGameplayTag Tag, UObject* Payload);

	void SetCurrentStep(int32 NewStep);

	UPROPERTY(Transient)
	int32 CurrentStep = 0;
};
