// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SimFlowTypes.h"
#include "SimFlowNetTypes.generated.h"

/**
 * Control requests a client can ask the server to perform on a flow.
 * Routed through USimFlowPlayerComponent, which lives on the PlayerController
 * and therefore always has an owning connection to send RPCs over.
 */
UENUM(BlueprintType)
enum class ESimFlowControlRequest : uint8
{
	Start		UMETA(DisplayName = "Start"),
	Stop		UMETA(DisplayName = "Stop"),
	Restart		UMETA(DisplayName = "Restart"),
	Pause		UMETA(DisplayName = "Pause"),
	Resume		UMETA(DisplayName = "Resume"),
	TogglePause	UMETA(DisplayName = "Toggle Pause"),
	Retry		UMETA(DisplayName = "Retry Current Task"),
	Skip		UMETA(DisplayName = "Skip Current Task"),
	Fail		UMETA(DisplayName = "Fail Current Task")
};

/**
 * One active task, as seen by a client.
 *
 * Only the node's Guid travels. Clients already have the flow asset loaded, so
 * they resolve the Guid back to the authored node and read the display name,
 * instruction, quiz question and options straight off it. No text is replicated.
 */
USTRUCT(BlueprintType)
struct SIMFLOWRUNTIME_API FSimFlowNetTask
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SimFlow|Net")
	FGuid NodeGuid;

	/** Server world time the task started, for computing countdowns client-side. */
	UPROPERTY(BlueprintReadOnly, Category = "SimFlow|Net")
	float StartServerTime = 0.f;

	/** Time limit copied from the node, so clients need not look it up. 0 = none. */
	UPROPERTY(BlueprintReadOnly, Category = "SimFlow|Net")
	float TimeLimit = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "SimFlow|Net")
	int32 RetryCount = 0;

	bool operator==(const FSimFlowNetTask& Other) const
	{
		return NodeGuid == Other.NodeGuid
			&& RetryCount == Other.RetryCount
			&& FMath::IsNearlyEqual(StartServerTime, Other.StartServerTime)
			&& FMath::IsNearlyEqual(TimeLimit, Other.TimeLimit);
	}

	bool operator!=(const FSimFlowNetTask& Other) const { return !(*this == Other); }
};

/**
 * The replicated summary of a running flow.
 *
 * Nodes, tasks and conditions never go over the wire - they are authored content
 * every client already has. Only this compact snapshot of *where the flow is*
 * replicates, which keeps bandwidth flat regardless of how large the graph is.
 */
USTRUCT(BlueprintType)
struct SIMFLOWRUNTIME_API FSimFlowNetState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SimFlow|Net")
	ESimFlowRunState RunState = ESimFlowRunState::NotStarted;

	/** Active task nodes, in activation order. */
	UPROPERTY(BlueprintReadOnly, Category = "SimFlow|Net")
	TArray<FSimFlowNetTask> ActiveTasks;

	/** Every active node, including non-task nodes such as Delay and Join. */
	UPROPERTY(BlueprintReadOnly, Category = "SimFlow|Net")
	TArray<FGuid> ActiveNodeGuids;

	UPROPERTY(BlueprintReadOnly, Category = "SimFlow|Net")
	FName EntryName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "SimFlow|Net")
	float Progress = 0.f;

	/** Seconds the flow has been running. Refreshed on a slow cadence, not every frame. */
	UPROPERTY(BlueprintReadOnly, Category = "SimFlow|Net")
	float ElapsedTime = 0.f;

	/** True once the flow has ended, so late joiners can tell "not started" from "over". */
	UPROPERTY(BlueprintReadOnly, Category = "SimFlow|Net")
	bool bHasEverStarted = false;

	/**
	 * Compares everything that describes *where* the flow is, deliberately
	 * ignoring ElapsedTime - otherwise the state would be dirty every single
	 * frame and we would replicate constantly for no benefit.
	 */
	bool EqualsIgnoringTime(const FSimFlowNetState& Other) const
	{
		return RunState == Other.RunState
			&& EntryName == Other.EntryName
			&& bHasEverStarted == Other.bHasEverStarted
			&& FMath::IsNearlyEqual(Progress, Other.Progress, 0.001f)
			&& ActiveTasks == Other.ActiveTasks
			&& ActiveNodeGuids == Other.ActiveNodeGuids;
	}
};
