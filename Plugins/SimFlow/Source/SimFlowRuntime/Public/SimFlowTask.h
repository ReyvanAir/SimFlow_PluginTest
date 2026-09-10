// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SimFlowTypes.h"
#include "SimFlowTask.generated.h"

class USimFlowInstance;
class USimFlowBlackboard;
class USimFlowNode_Task;
class AActor;
class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSimFlowTaskFinished, ESimFlowResult, Result);

/**
 * The unit of work in a flow.
 *
 * Subclass this in Blueprint (or C++) and implement OnTaskStart. Call FinishTask()
 * when the work is done. Everything else - retry, skip, timeout, pause - is handled
 * for you by the owning node.
 *
 * Tasks are instanced sub-objects of a Task node, and are duplicated per running
 * flow, so it is safe to keep mutable state in them.
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced, CollapseCategories)
class SIMFLOWRUNTIME_API USimFlowTask : public UObject
{
	GENERATED_BODY()

public:
	USimFlowTask();

	// ------------------------------------------------------------- Authoring

	/** Shown in the graph, in the debug HUD and in any tutorial UI you build. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task|Presentation")
	FText DisplayName;

	/** Longer instruction text, e.g. "Pick up the fire extinguisher". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task|Presentation", meta = (MultiLine = "true"))
	FText Instruction;

	/** Optional tag so UI / analytics can identify this task. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task|Presentation")
	FName TaskId = NAME_None;

	/** Whether the player is allowed to retry this task through the flow component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task|Rules")
	bool bAllowRetry = true;

	/** 0 = unlimited. When exceeded, a retry request fails the task instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task|Rules", meta = (ClampMin = "0", EditCondition = "bAllowRetry"))
	int32 MaxRetries = 0;

	/** Whether the player / instructor may skip this task. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task|Rules")
	bool bAllowSkip = true;

	/** When true the task keeps ticking while the flow is paused. Almost always false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task|Rules", AdvancedDisplay)
	bool bTickWhilePaused = false;

	/** How much score to add to the blackboard when this task succeeds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task|Scoring")
	float ScoreOnSuccess = 0.f;

	/** How much score to add (usually negative) when this task fails or times out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task|Scoring")
	float ScoreOnFailure = 0.f;

	// -------------------------------------------------------------- Delegates

	UPROPERTY(BlueprintAssignable, Category = "SimFlow|Task")
	FSimFlowTaskFinished OnTaskFinished;

	// ------------------------------------------------------- Runtime read-only

	UPROPERTY(Transient, BlueprintReadOnly, Category = "SimFlow|Task")
	TObjectPtr<USimFlowInstance> FlowInstance = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "SimFlow|Task")
	TObjectPtr<USimFlowNode_Task> OwningNode = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "SimFlow|Task")
	bool bIsRunning = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "SimFlow|Task")
	bool bIsPaused = false;

	/** Seconds this task has been active, excluding paused time. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "SimFlow|Task")
	float ElapsedTime = 0.f;

	/** How many times the task has been restarted through Retry. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "SimFlow|Task")
	int32 RetryCount = 0;

	// ------------------------------------------------------------ Blueprint API

	/** Call this from your task Blueprint the moment the work is done. */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Task", meta = (DisplayName = "Finish Task"))
	void FinishTask(ESimFlowResult Result = ESimFlowResult::Succeeded);

	UFUNCTION(BlueprintPure, Category = "SimFlow|Task")
	USimFlowBlackboard* GetBlackboard() const;

	/** The actor that owns the flow component running this task. */
	UFUNCTION(BlueprintPure, Category = "SimFlow|Task")
	AActor* GetFlowOwner() const;

	/** The local player pawn, which in a VR project is the VR pawn. */
	UFUNCTION(BlueprintPure, Category = "SimFlow|Task")
	APawn* GetPlayerPawn() const;

	UFUNCTION(BlueprintPure, Category = "SimFlow|Task")
	bool CanRetry() const;

	UFUNCTION(BlueprintPure, Category = "SimFlow|Task")
	FText GetDisplayNameText() const;

	/** Logs something the trainee got wrong against this task. */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Task")
	void RecordMistake(FGameplayTag Kind, UObject* Involved, const FText& Description,
		ESimFlowMatchQuality Severity = ESimFlowMatchQuality::NoMatch);

	/**
	 * Shared handling for "the right event arrived carrying the wrong object".
	 * Returns true when the policy ended the task, so callers can stop early.
	 */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Task")
	bool ApplyMismatchPolicy(ESimFlowMismatchPolicy Policy, FGameplayTag MistakeKind, UObject* Involved,
		const FText& Description, ESimFlowMatchQuality Severity);

	// ------------------------------------------------------------ Blueprint hooks

	/** Called when the task becomes active. Do your setup here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "SimFlow|Task", meta = (DisplayName = "On Task Start"))
	void ReceiveTaskStart();

	/** Called every frame while the task runs and the flow is not paused. */
	UFUNCTION(BlueprintImplementableEvent, Category = "SimFlow|Task", meta = (DisplayName = "On Task Tick"))
	void ReceiveTaskTick(float DeltaTime);

	UFUNCTION(BlueprintImplementableEvent, Category = "SimFlow|Task", meta = (DisplayName = "On Task Pause"))
	void ReceiveTaskPause();

	UFUNCTION(BlueprintImplementableEvent, Category = "SimFlow|Task", meta = (DisplayName = "On Task Resume"))
	void ReceiveTaskResume();

	/** Called after the task ends for any reason. Clean up bindings / actors here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "SimFlow|Task", meta = (DisplayName = "On Task End"))
	void ReceiveTaskEnd(ESimFlowResult Result);

	/** Called right before the task is restarted by a retry request. */
	UFUNCTION(BlueprintImplementableEvent, Category = "SimFlow|Task", meta = (DisplayName = "On Task Retry"))
	void ReceiveTaskRetry(int32 NewRetryCount);

	/** Save any extra state you need to survive a save/load here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "SimFlow|Task", meta = (DisplayName = "On Save Task State"))
	void ReceiveSaveTaskState();

	UFUNCTION(BlueprintImplementableEvent, Category = "SimFlow|Task", meta = (DisplayName = "On Load Task State"))
	void ReceiveLoadTaskState();

	/** Key/value bag written by ReceiveSaveTaskState and restored before ReceiveLoadTaskState. */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Task")
	void SetSavedValue(FName Key, const FSimFlowValue& Value);

	UFUNCTION(BlueprintPure, Category = "SimFlow|Task")
	FSimFlowValue GetSavedValue(FName Key) const;

	// ------------------------------------------------------------- C++ surface

	virtual void NativeTaskStart() {}
	virtual void NativeTaskTick(float DeltaTime) {}
	virtual void NativeTaskPause() {}
	virtual void NativeTaskResume() {}
	virtual void NativeTaskEnd(ESimFlowResult Result) {}

	/** Internal: called by the owning node. */
	void InitializeTask(USimFlowInstance* InInstance, USimFlowNode_Task* InNode);
	void StartTask();
	void TickTask(float DeltaTime);
	void PauseTask();
	void ResumeTask();
	void RestartTask();
	void AbortTask();

	/** Internal: save support. */
	void SaveTaskState(TArray<FSimFlowBlackboardEntry>& OutData);
	void LoadTaskState(const TArray<FSimFlowBlackboardEntry>& InData);

	virtual UWorld* GetWorld() const override;

protected:
	UPROPERTY(Transient)
	TMap<FName, FSimFlowValue> SavedState;

	/** Guards against FinishTask being called twice in the same activation. */
	UPROPERTY(Transient)
	bool bFinishRequested = false;
};
