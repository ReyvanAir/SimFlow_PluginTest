// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SimFlowTypes.h"
#include "SimFlowNode.h"
#include "SimFlowSaveGame.generated.h"

/** A complete snapshot of one running flow. */
USTRUCT(BlueprintType)
struct SIMFLOWRUNTIME_API FSimFlowSaveState
{
	GENERATED_BODY()

	/** Soft path of the flow asset this state belongs to. */
	UPROPERTY(BlueprintReadWrite, Category = "SimFlow|Save")
	FString FlowAssetPath;

	/** Identifier of the component the state came from (see USimFlowComponent::FlowSaveId). */
	UPROPERTY(BlueprintReadWrite, Category = "SimFlow|Save")
	FName FlowSaveId = NAME_None;

	UPROPERTY(BlueprintReadWrite, Category = "SimFlow|Save")
	ESimFlowRunState RunState = ESimFlowRunState::NotStarted;

	UPROPERTY(BlueprintReadWrite, Category = "SimFlow|Save")
	FName EntryName = NAME_None;

	UPROPERTY(BlueprintReadWrite, Category = "SimFlow|Save")
	float ElapsedTime = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "SimFlow|Save")
	TArray<FSimFlowNodeSaveState> NodeStates;

	UPROPERTY(BlueprintReadWrite, Category = "SimFlow|Save")
	TArray<FSimFlowBlackboardEntry> Blackboard;

	/** Guid of the last checkpoint node passed, used by the FromLastCheckpoint load mode. */
	UPROPERTY(BlueprintReadWrite, Category = "SimFlow|Save")
	FGuid LastCheckpointGuid;

	UPROPERTY(BlueprintReadWrite, Category = "SimFlow|Save")
	TArray<FName> RaisedEvents;

	/** Everything the trainee got wrong, for a debrief that survives a reload. */
	UPROPERTY(BlueprintReadWrite, Category = "SimFlow|Save")
	TArray<FSimFlowMistake> Mistakes;

	UPROPERTY(BlueprintReadWrite, Category = "SimFlow|Save")
	FDateTime SavedAt = FDateTime(0);

	/** Free text you can show in a "continue" menu, e.g. the current task name. */
	UPROPERTY(BlueprintReadWrite, Category = "SimFlow|Save")
	FString Label;

	bool IsValid() const { return !FlowAssetPath.IsEmpty(); }
};

/**
 * SaveGame object that can hold several flows at once, keyed by save id.
 * Use USimFlowStatics / USimFlowComponent rather than touching this directly.
 */
UCLASS(BlueprintType)
class SIMFLOWRUNTIME_API USimFlowSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = "SimFlow|Save")
	TMap<FName, FSimFlowSaveState> Flows;

	/** Anything else your project wants to persist alongside the flow. */
	UPROPERTY(BlueprintReadWrite, Category = "SimFlow|Save")
	TArray<FSimFlowBlackboardEntry> ExtraData;

	UPROPERTY(BlueprintReadWrite, Category = "SimFlow|Save")
	int32 SaveVersion = 1;

	UFUNCTION(BlueprintPure, Category = "SimFlow|Save")
	bool HasFlow(FName FlowSaveId) const { return Flows.Contains(FlowSaveId); }

	UFUNCTION(BlueprintPure, Category = "SimFlow|Save")
	FSimFlowSaveState GetFlow(FName FlowSaveId) const;
};
