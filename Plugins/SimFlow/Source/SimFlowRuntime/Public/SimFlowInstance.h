// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "SimFlowTypes.h"
#include "SimFlowSaveGame.h"
#include "SimFlowInstance.generated.h"

class USimFlowAsset;
class USimFlowNode;
class USimFlowNode_Task;
class USimFlowNode_Checkpoint;
class USimFlowTask;
class USimFlowTask_Quiz;
class USimFlowBlackboard;
class USimFlowComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSimFlowFinishedSignature, ESimFlowRunState, FinalState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSimFlowSimpleSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSimFlowEventSignature, FGameplayTag, EventTag, UObject*, Payload);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSimFlowTaskSignature, USimFlowNode_Task*, Node, USimFlowTask*, Task);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSimFlowTaskResultSignature, USimFlowNode_Task*, Node, USimFlowTask*, Task, ESimFlowResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSimFlowCheckpointSignature, USimFlowNode_Checkpoint*, Checkpoint);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSimFlowQuizSignature, USimFlowTask_Quiz*, Quiz);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSimFlowMistakeSignature, const FSimFlowMistake&, Mistake);

/**
 * A running copy of a USimFlowAsset.
 *
 * The instance owns duplicated nodes, the blackboard, and the activation queue.
 * Execution is queue driven rather than recursive, so long chains of instant
 * nodes cannot blow the stack.
 */
UCLASS(BlueprintType)
class SIMFLOWRUNTIME_API USimFlowInstance : public UObject
{
	GENERATED_BODY()

public:
	USimFlowInstance();

	// -------------------------------------------------------------- Delegates

	UPROPERTY(BlueprintAssignable, Category = "SimFlow") FSimFlowSimpleSignature		OnFlowStarted;
	UPROPERTY(BlueprintAssignable, Category = "SimFlow") FSimFlowSimpleSignature		OnFlowPaused;
	UPROPERTY(BlueprintAssignable, Category = "SimFlow") FSimFlowSimpleSignature		OnFlowResumed;
	UPROPERTY(BlueprintAssignable, Category = "SimFlow") FSimFlowFinishedSignature	OnFlowFinished;
	UPROPERTY(BlueprintAssignable, Category = "SimFlow") FSimFlowTaskSignature		OnTaskStarted;
	UPROPERTY(BlueprintAssignable, Category = "SimFlow") FSimFlowTaskResultSignature	OnTaskFinished;
	UPROPERTY(BlueprintAssignable, Category = "SimFlow") FSimFlowTaskSignature		OnTaskRetried;
	UPROPERTY(BlueprintAssignable, Category = "SimFlow") FSimFlowCheckpointSignature	OnCheckpointReached;
	UPROPERTY(BlueprintAssignable, Category = "SimFlow") FSimFlowQuizSignature		OnQuizPresented;
	UPROPERTY(BlueprintAssignable, Category = "SimFlow") FSimFlowEventSignature		OnEventRaised;
	UPROPERTY(BlueprintAssignable, Category = "SimFlow") FSimFlowMistakeSignature	OnMistakeRecorded;

	// -------------------------------------------------------------- Lifecycle

	void InitializeInstance(USimFlowAsset* InTemplate, USimFlowComponent* InComponent, USimFlowInstance* InParent = nullptr);

	UFUNCTION(BlueprintCallable, Category = "SimFlow")
	bool StartInstance(FName EntryName = NAME_None);

	void TickInstance(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "SimFlow")
	void PauseInstance();

	UFUNCTION(BlueprintCallable, Category = "SimFlow")
	void ResumeInstance();

	/** Stops everything and reports Aborted. */
	UFUNCTION(BlueprintCallable, Category = "SimFlow")
	void StopInstance();

	/** Ends the flow with an explicit result. Called by Finish nodes. */
	void FinishInstance(ESimFlowRunState FinalState, bool bStopOtherBranches = true);

	// ------------------------------------------------------------- Execution

	void ActivateNode(USimFlowNode* Node, FName InputPin);
	void DeactivateNode(USimFlowNode* Node);
	void TriggerNodeOutput(USimFlowNode* Node, FName PinName, bool bStayActive);

	// ------------------------------------------------------- Player controls

	/** Restarts the active task(s). Returns how many were restarted. */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Control")
	int32 RetryActiveTasks();

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Control")
	int32 SkipActiveTasks();

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Control")
	int32 FailActiveTasks();

	/** Raises an event tag. Wakes any Wait For Event tasks listening for it. */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Control")
	void RaiseEvent(FGameplayTag EventTag, UObject* Payload = nullptr);

	UFUNCTION(BlueprintPure, Category = "SimFlow")
	bool WasEventRaised(FGameplayTag EventTag) const;

	UFUNCTION(BlueprintCallable, Category = "SimFlow")
	void ClearRaisedEvents();

	// ------------------------------------------------------------- Mistakes

	/**
	 * Logs something the trainee got wrong.
	 *
	 * Also bumps the "Mistakes" blackboard key, so conditions written against that
	 * counter keep working - the array is the richer record a debrief screen needs.
	 */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Mistakes")
	void RecordMistake(FGameplayTag Kind, UObject* Involved, const FText& Description,
		ESimFlowMatchQuality Severity = ESimFlowMatchQuality::NoMatch, FName TaskId = NAME_None);

	UFUNCTION(BlueprintPure, Category = "SimFlow|Mistakes")
	const TArray<FSimFlowMistake>& GetMistakes() const { return Mistakes; }

	UFUNCTION(BlueprintPure, Category = "SimFlow|Mistakes")
	int32 GetMistakeCount() const { return Mistakes.Num(); }

	/** Mistakes of one kind, e.g. everything tagged SimFlow.Mistake.WrongItem. */
	UFUNCTION(BlueprintPure, Category = "SimFlow|Mistakes")
	TArray<FSimFlowMistake> GetMistakesOfKind(FGameplayTag Kind, bool bMatchChildTags = true) const;

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Mistakes")
	void ClearMistakes();

	// ---------------------------------------------------------- Save & load

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Save")
	FSimFlowSaveState SaveInstanceState() const;

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Save")
	bool LoadInstanceState(const FSimFlowSaveState& State, ESimFlowLoadMode LoadMode = ESimFlowLoadMode::ExactState);

	// ------------------------------------------------------------ Accessors

	UFUNCTION(BlueprintPure, Category = "SimFlow") USimFlowAsset* GetTemplate() const { return Template; }
	UFUNCTION(BlueprintPure, Category = "SimFlow") USimFlowBlackboard* GetBlackboard() const { return Blackboard; }
	UFUNCTION(BlueprintPure, Category = "SimFlow") USimFlowComponent* GetOwningComponent() const;
	UFUNCTION(BlueprintPure, Category = "SimFlow") ESimFlowRunState GetRunState() const { return RunState; }
	UFUNCTION(BlueprintPure, Category = "SimFlow") bool IsRunning() const { return RunState == ESimFlowRunState::Running; }
	UFUNCTION(BlueprintPure, Category = "SimFlow") bool IsPaused() const { return RunState == ESimFlowRunState::Paused; }
	UFUNCTION(BlueprintPure, Category = "SimFlow") float GetElapsedTime() const { return ElapsedTime; }
	UFUNCTION(BlueprintPure, Category = "SimFlow") ESimFlowResult GetLastTaskResult() const { return LastTaskResult; }
	UFUNCTION(BlueprintPure, Category = "SimFlow") FName GetEntryName() const { return ActiveEntryName; }

	UFUNCTION(BlueprintPure, Category = "SimFlow")
	TArray<USimFlowNode*> GetActiveNodes() const;

	UFUNCTION(BlueprintPure, Category = "SimFlow")
	TArray<USimFlowNode_Task*> GetActiveTaskNodes() const;

	/** The first active task, which is what most single-track tutorial UIs want. */
	UFUNCTION(BlueprintPure, Category = "SimFlow")
	USimFlowTask* GetCurrentTask() const;

	UFUNCTION(BlueprintPure, Category = "SimFlow")
	USimFlowNode* FindRuntimeNode(const FGuid& Guid) const;

	/** Percentage of task nodes already completed, 0..1. Rough but useful for a progress bar. */
	UFUNCTION(BlueprintPure, Category = "SimFlow")
	float GetProgress() const;

	FString BuildDebugString() const;

	virtual UWorld* GetWorld() const override;

	// ------------------------------------------------------------ Node hooks

	void NotifyTaskStarted(USimFlowNode_Task* Node, USimFlowTask* Task);
	void NotifyTaskFinished(USimFlowNode_Task* Node, USimFlowTask* Task, ESimFlowResult Result);
	void NotifyTaskRetried(USimFlowNode_Task* Node, USimFlowTask* Task);
	void NotifyCheckpointReached(USimFlowNode_Checkpoint* Checkpoint);
	void NotifyQuizPresented(USimFlowTask_Quiz* Quiz);

	UFUNCTION(BlueprintPure, Category = "SimFlow")
	FGuid GetLastCheckpointGuid() const { return LastCheckpointGuid; }

private:
	void BuildRuntimeNodes();
	void EnqueueActivation(const FGuid& NodeGuid, FName InputPin);
	void ProcessPendingActivations();
	void DeactivateAllNodes();

	struct FPendingActivation
	{
		FGuid NodeGuid;
		FName InputPin;
	};

	UPROPERTY(Transient)
	TObjectPtr<USimFlowAsset> Template = nullptr;

	UPROPERTY(Transient)
	TWeakObjectPtr<USimFlowComponent> OwningComponent;

	UPROPERTY(Transient)
	TObjectPtr<USimFlowInstance> ParentInstance = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USimFlowBlackboard> Blackboard = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USimFlowNode>> RuntimeNodes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USimFlowNode>> ActiveNodes;

	UPROPERTY(Transient)
	ESimFlowRunState RunState = ESimFlowRunState::NotStarted;

	UPROPERTY(Transient)
	float ElapsedTime = 0.f;

	UPROPERTY(Transient)
	ESimFlowResult LastTaskResult = ESimFlowResult::Succeeded;

	UPROPERTY(Transient)
	FName ActiveEntryName = NAME_None;

	UPROPERTY(Transient)
	FGuid LastCheckpointGuid;

	UPROPERTY(Transient)
	FGameplayTagContainer RaisedEvents;

	UPROPERTY(Transient)
	TSet<FGuid> CompletedTaskNodes;

	UPROPERTY(Transient)
	TArray<FSimFlowMistake> Mistakes;

	TMap<FGuid, TObjectPtr<USimFlowNode>> NodeLookup;
	TArray<FPendingActivation> PendingActivations;
	bool bProcessingActivations = false;
	int32 ActivationsThisFrame = 0;

	static constexpr int32 MaxActivationsPerProcess = 10000;
};
