// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SimFlowNode.h"
#include "SimFlowNodes.generated.h"

class USimFlowTask;
class USimFlowCondition;
class USimFlowAsset;
class USimFlowInstance;

// =====================================================================
//  Entry
// =====================================================================

/** Where execution begins. A graph may have several, each with its own name. */
UCLASS(DisplayName = "Start", meta = (ToolTip = "Entry point of the flow."))
class SIMFLOWRUNTIME_API USimFlowNode_Entry : public USimFlowNode
{
	GENERATED_BODY()

public:
	USimFlowNode_Entry();

	/** Pass this name to StartFlow to begin from this particular entry. */
	UPROPERTY(EditAnywhere, Category = "Entry")
	FName EntryName = TEXT("Default");

	virtual void BuildPins() override;
	virtual void ExecuteInput(FName PinName) override;
	virtual bool IsEntryNode() const override { return true; }
	virtual FText GetNodeTitle() const override;
	virtual FText GetNodeSubtitle() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual FText GetNodeCategory() const override;
};

// =====================================================================
//  Task
// =====================================================================

/**
 * Runs a single USimFlowTask. This is the node designers use most.
 *
 * Outputs: Completed / Failed / Skipped / TimedOut. Anything left unwired
 * falls back to Completed, so simple linear flows stay tidy.
 */
UCLASS(DisplayName = "Task")
class SIMFLOWRUNTIME_API USimFlowNode_Task : public USimFlowNode
{
	GENERATED_BODY()

public:
	USimFlowNode_Task();

	UPROPERTY(EditAnywhere, Instanced, Category = "Task")
	TObjectPtr<USimFlowTask> Task = nullptr;

	/** Seconds before the task times out and leaves through the TimedOut pin. 0 = no limit. */
	UPROPERTY(EditAnywhere, Category = "Task", meta = (ClampMin = "0.0", Units = "s"))
	float TimeLimit = 0.f;

	/** Automatically restart the task when it fails, up to AutoRetryLimit times. */
	UPROPERTY(EditAnywhere, Category = "Task")
	bool bAutoRetryOnFailure = false;

	UPROPERTY(EditAnywhere, Category = "Task", meta = (ClampMin = "1", EditCondition = "bAutoRetryOnFailure"))
	int32 AutoRetryLimit = 1;

	/** Also restart on a timeout when auto retry is on. */
	UPROPERTY(EditAnywhere, Category = "Task", meta = (EditCondition = "bAutoRetryOnFailure"))
	bool bAutoRetryOnTimeout = false;

	/** Unwired Failed/Skipped/TimedOut pins fall through to Completed. */
	UPROPERTY(EditAnywhere, Category = "Task", AdvancedDisplay)
	bool bFallbackToCompleted = true;

	/** Optional conditions that abort the task early (evaluated every frame). */
	UPROPERTY(EditAnywhere, Instanced, Category = "Task|Early Out")
	TObjectPtr<USimFlowCondition> AbortCondition = nullptr;

	/** Which pin the abort condition leaves through. */
	UPROPERTY(EditAnywhere, Category = "Task|Early Out", meta = (EditCondition = "AbortCondition != nullptr"))
	ESimFlowResult AbortResult = ESimFlowResult::Failed;

	UPROPERTY(Transient)
	int32 AutoRetryCount = 0;

	virtual void BuildPins() override;
	virtual void ExecuteInput(FName PinName) override;
	virtual void TickNode(float DeltaTime) override;
	virtual void OnPauseNode() override;
	virtual void OnResumeNode() override;
	virtual void Cleanup() override;

	virtual bool CanRetry() const override;
	virtual bool CanSkip() const override;
	virtual void RequestRetry() override;
	virtual void RequestSkip() override;
	virtual void RequestFail() override;

	virtual FText GetNodeTitle() const override;
	virtual FText GetNodeSubtitle() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual FText GetNodeCategory() const override;
	virtual FString GetDebugStatus() const override;

	virtual void SaveNodeState(FSimFlowNodeSaveState& OutState) const override;
	virtual void LoadNodeState(const FSimFlowNodeSaveState& InState) override;

	/** Called by the task when it finishes. */
	void HandleTaskFinished(ESimFlowResult Result);

	UFUNCTION(BlueprintPure, Category = "SimFlow|Node")
	USimFlowTask* GetTask() const { return Task; }

	UFUNCTION(BlueprintPure, Category = "SimFlow|Node")
	float GetRemainingTime() const;
};

// =====================================================================
//  Delay
// =====================================================================

/** Waits, then continues. Respects pause. */
UCLASS(DisplayName = "Delay")
class SIMFLOWRUNTIME_API USimFlowNode_Delay : public USimFlowNode
{
	GENERATED_BODY()

public:
	USimFlowNode_Delay();

	UPROPERTY(EditAnywhere, Category = "Delay", meta = (ClampMin = "0.0", Units = "s"))
	float Duration = 1.f;

	virtual void BuildPins() override;
	virtual void ExecuteInput(FName PinName) override;
	virtual void TickNode(float DeltaTime) override;
	virtual FText GetNodeSubtitle() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual FText GetNodeCategory() const override;
	virtual FString GetDebugStatus() const override;
};

// =====================================================================
//  Branch
// =====================================================================

USTRUCT(BlueprintType)
struct SIMFLOWRUNTIME_API FSimFlowBranchCase
{
	GENERATED_BODY()

	/** Label shown on the output pin. Leave empty for "Case N". */
	UPROPERTY(EditAnywhere, Category = "Case")
	FString Label;

	UPROPERTY(EditAnywhere, Instanced, Category = "Case")
	TObjectPtr<USimFlowCondition> Condition = nullptr;
};

/**
 * Condition based execution. Evaluates each case in order and leaves through
 * the first one that passes, or through Default when none do.
 */
UCLASS(DisplayName = "Branch")
class SIMFLOWRUNTIME_API USimFlowNode_Branch : public USimFlowNode
{
	GENERATED_BODY()

public:
	USimFlowNode_Branch();

	UPROPERTY(EditAnywhere, Category = "Branch")
	TArray<FSimFlowBranchCase> Cases;

	/** Fire every case that passes instead of only the first (fans out in parallel). */
	UPROPERTY(EditAnywhere, Category = "Branch")
	bool bFireAllMatchingCases = false;

	/** Show a Default pin taken when nothing matched. */
	UPROPERTY(EditAnywhere, Category = "Branch")
	bool bHasDefaultPin = true;

	virtual void BuildPins() override;
	virtual void ExecuteInput(FName PinName) override;
	virtual FText GetNodeSubtitle() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual FText GetNodeCategory() const override;

	static FName MakeCasePinName(int32 Index);
};

// =====================================================================
//  Random Branch
// =====================================================================

/** Picks one output at random, optionally weighted. Good for varied scenarios. */
UCLASS(DisplayName = "Random Branch")
class SIMFLOWRUNTIME_API USimFlowNode_RandomBranch : public USimFlowNode
{
	GENERATED_BODY()

public:
	USimFlowNode_RandomBranch();

	UPROPERTY(EditAnywhere, Category = "Random", meta = (ClampMin = "2", ClampMax = "16"))
	int32 NumOutputs = 2;

	/** Optional per-output weights. Missing entries count as 1. */
	UPROPERTY(EditAnywhere, Category = "Random")
	TArray<float> Weights;

	/** Never pick the same output twice in a row (within one run). */
	UPROPERTY(EditAnywhere, Category = "Random")
	bool bAvoidRepeats = false;

	virtual void BuildPins() override;
	virtual void ExecuteInput(FName PinName) override;
	virtual FLinearColor GetNodeColor() const override;
	virtual FText GetNodeCategory() const override;

private:
	UPROPERTY(Transient)
	int32 LastPicked = INDEX_NONE;
};

// =====================================================================
//  Parallel / Join
// =====================================================================

/** Fires every output at once. Pair with a Join node to converge. */
UCLASS(DisplayName = "Parallel")
class SIMFLOWRUNTIME_API USimFlowNode_Parallel : public USimFlowNode
{
	GENERATED_BODY()

public:
	USimFlowNode_Parallel();

	UPROPERTY(EditAnywhere, Category = "Parallel", meta = (ClampMin = "2", ClampMax = "16"))
	int32 NumOutputs = 2;

	virtual void BuildPins() override;
	virtual void ExecuteInput(FName PinName) override;
	virtual FText GetNodeSubtitle() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual FText GetNodeCategory() const override;
};

/**
 * Converges parallel branches.
 * Wait For All  - continues once every input has fired.
 * Wait For Any  - continues on the first input and ignores the rest (race / timeout).
 */
UCLASS(DisplayName = "Join")
class SIMFLOWRUNTIME_API USimFlowNode_Join : public USimFlowNode
{
	GENERATED_BODY()

public:
	USimFlowNode_Join();

	UPROPERTY(EditAnywhere, Category = "Join", meta = (ClampMin = "2", ClampMax = "16"))
	int32 NumInputs = 2;

	UPROPERTY(EditAnywhere, Category = "Join")
	ESimFlowJoinMode Mode = ESimFlowJoinMode::WaitForAll;

	virtual void BuildPins() override;
	virtual void ExecuteInput(FName PinName) override;
	virtual void Cleanup() override;
	virtual bool AllowsReentrantActivation() const override { return true; }
	virtual FText GetNodeSubtitle() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual FText GetNodeCategory() const override;
	virtual FString GetDebugStatus() const override;

	virtual void SaveNodeState(FSimFlowNodeSaveState& OutState) const override;
	virtual void LoadNodeState(const FSimFlowNodeSaveState& InState) override;

private:
	UPROPERTY(Transient)
	TArray<FName> ReceivedInputs;

	UPROPERTY(Transient)
	bool bHasFired = false;
};

// =====================================================================
//  Loop
// =====================================================================

/**
 * Repeats a section of the graph.
 * Wire LoopBody into the section, and the end of the section back into Continue.
 */
UCLASS(DisplayName = "Loop")
class SIMFLOWRUNTIME_API USimFlowNode_Loop : public USimFlowNode
{
	GENERATED_BODY()

public:
	USimFlowNode_Loop();

	/** Number of iterations. 0 means "until the break condition passes". */
	UPROPERTY(EditAnywhere, Category = "Loop", meta = (ClampMin = "0"))
	int32 Iterations = 3;

	/** Evaluated before each iteration. When it passes the loop exits through Completed. */
	UPROPERTY(EditAnywhere, Instanced, Category = "Loop")
	TObjectPtr<USimFlowCondition> BreakCondition = nullptr;

	/** Safety valve so an infinite loop cannot hang the game. */
	UPROPERTY(EditAnywhere, Category = "Loop", AdvancedDisplay, meta = (ClampMin = "1"))
	int32 MaxIterations = 1000;

	/** Writes the current 0-based iteration into this blackboard key. */
	UPROPERTY(EditAnywhere, Category = "Loop", AdvancedDisplay)
	FName IterationBlackboardKey = NAME_None;

	UPROPERTY(Transient)
	int32 CurrentIteration = 0;

	virtual void BuildPins() override;
	virtual void ExecuteInput(FName PinName) override;
	virtual bool AllowsReentrantActivation() const override { return true; }
	virtual void Cleanup() override;
	virtual FText GetNodeSubtitle() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual FText GetNodeCategory() const override;
	virtual FString GetDebugStatus() const override;

	virtual void SaveNodeState(FSimFlowNodeSaveState& OutState) const override;
	virtual void LoadNodeState(const FSimFlowNodeSaveState& InState) override;

	static const FName ContinuePin;
};

// =====================================================================
//  Sub Flow
// =====================================================================

/** Runs another SimFlow asset as a child. This is what makes flows modular. */
UCLASS(DisplayName = "Sub Flow")
class SIMFLOWRUNTIME_API USimFlowNode_SubFlow : public USimFlowNode
{
	GENERATED_BODY()

public:
	USimFlowNode_SubFlow();

	UPROPERTY(EditAnywhere, Category = "Sub Flow")
	TObjectPtr<USimFlowAsset> SubFlow = nullptr;

	/** Which entry point of the sub flow to start from. */
	UPROPERTY(EditAnywhere, Category = "Sub Flow")
	FName EntryName = TEXT("Default");

	/** Copy the parent blackboard into the child when it starts. */
	UPROPERTY(EditAnywhere, Category = "Sub Flow")
	bool bInheritBlackboard = true;

	/** Copy the child blackboard back into the parent when it ends. */
	UPROPERTY(EditAnywhere, Category = "Sub Flow")
	bool bWriteBackBlackboard = true;

	virtual void BuildPins() override;
	virtual void ExecuteInput(FName PinName) override;
	virtual void TickNode(float DeltaTime) override;
	virtual void OnPauseNode() override;
	virtual void OnResumeNode() override;
	virtual void Cleanup() override;
	virtual bool CanSkip() const override { return true; }
	virtual void RequestSkip() override;
	virtual void RequestFail() override;
	virtual FText GetNodeSubtitle() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual FText GetNodeCategory() const override;
	virtual FString GetDebugStatus() const override;

	UFUNCTION(BlueprintPure, Category = "SimFlow|Node")
	USimFlowInstance* GetChildInstance() const { return ChildInstance; }

private:
	UFUNCTION()
	void HandleChildFinished(ESimFlowRunState FinalState);

	UPROPERTY(Transient)
	TObjectPtr<USimFlowInstance> ChildInstance = nullptr;
};

// =====================================================================
//  Checkpoint
// =====================================================================

/** Marks a safe resume point, and can auto-save the flow when reached. */
UCLASS(DisplayName = "Checkpoint")
class SIMFLOWRUNTIME_API USimFlowNode_Checkpoint : public USimFlowNode
{
	GENERATED_BODY()

public:
	USimFlowNode_Checkpoint();

	UPROPERTY(EditAnywhere, Category = "Checkpoint")
	FName CheckpointId = NAME_None;

	/** Write the flow to a SaveGame slot as soon as this node is reached. */
	UPROPERTY(EditAnywhere, Category = "Checkpoint")
	bool bAutoSave = false;

	/** Leave empty to use the flow component's default slot. */
	UPROPERTY(EditAnywhere, Category = "Checkpoint", meta = (EditCondition = "bAutoSave"))
	FString SaveSlotName;

	UPROPERTY(EditAnywhere, Category = "Checkpoint", meta = (EditCondition = "bAutoSave"))
	int32 SaveUserIndex = 0;

	virtual void BuildPins() override;
	virtual void ExecuteInput(FName PinName) override;
	virtual FText GetNodeSubtitle() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual FText GetNodeCategory() const override;
};

// =====================================================================
//  Set Blackboard
// =====================================================================

/** Writes a blackboard key inline in the graph, no task object needed. */
UCLASS(DisplayName = "Set Blackboard Value")
class SIMFLOWRUNTIME_API USimFlowNode_SetBlackboard : public USimFlowNode
{
	GENERATED_BODY()

public:
	USimFlowNode_SetBlackboard();

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName Key = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FSimFlowValue Value;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	bool bAdd = false;

	virtual void BuildPins() override;
	virtual void ExecuteInput(FName PinName) override;
	virtual FText GetNodeSubtitle() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual FText GetNodeCategory() const override;
};

// =====================================================================
//  Finish
// =====================================================================

/** Ends the whole flow with a result. */
UCLASS(DisplayName = "Finish")
class SIMFLOWRUNTIME_API USimFlowNode_Finish : public USimFlowNode
{
	GENERATED_BODY()

public:
	USimFlowNode_Finish();

	UPROPERTY(EditAnywhere, Category = "Finish")
	ESimFlowFinishMode FinishMode = ESimFlowFinishMode::Complete;

	/** Stop any other still-running branches. Almost always what you want. */
	UPROPERTY(EditAnywhere, Category = "Finish")
	bool bStopOtherBranches = true;

	virtual void BuildPins() override;
	virtual void ExecuteInput(FName PinName) override;
	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual FText GetNodeCategory() const override;
};
