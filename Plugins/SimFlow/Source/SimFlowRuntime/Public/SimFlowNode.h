// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SimFlowTypes.h"
#include "SimFlowNode.generated.h"

class USimFlowInstance;
class USimFlowAsset;
class USimFlowBlackboard;

/** Serialised runtime state for a single node. */
USTRUCT(BlueprintType)
struct SIMFLOWRUNTIME_API FSimFlowNodeSaveState
{
	GENERATED_BODY()

	UPROPERTY() FGuid NodeGuid;
	UPROPERTY() bool bActive = false;
	UPROPERTY() float ActiveTime = 0.f;
	UPROPERTY() FName ActiveInputPin = NAME_None;
	UPROPERTY() int32 IntState = 0;
	UPROPERTY() TArray<FName> ReceivedInputs;
	UPROPERTY() TArray<FSimFlowBlackboardEntry> CustomData;
};

/**
 * Base class for every node in a flow graph.
 *
 * A node is activated through one of its input pins, does something, then pushes
 * execution on through one of its output pins. Nodes are duplicated per running
 * flow so they can safely hold runtime state.
 */
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class SIMFLOWRUNTIME_API USimFlowNode : public UObject
{
	GENERATED_BODY()

public:
	USimFlowNode();

	// ------------------------------------------------------------ Identity

	/** Stable identity used for links and save data. */
	UPROPERTY()
	FGuid NodeGuid;

	UPROPERTY()
	TArray<FSimFlowInputPin> InputPins;

	UPROPERTY()
	TArray<FSimFlowOutputPin> OutputPins;

	/** Free-form designer note, shown on the node in the graph. */
	UPROPERTY(EditAnywhere, Category = "Node", meta = (MultiLine = "true"))
	FString NodeComment;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FVector2D GraphPosition = FVector2D::ZeroVector;

	/** Broadcast when a property change altered this node's pin layout. */
	FSimpleMulticastDelegate OnPinsChanged;
#endif

	// ------------------------------------------------------------ Description

	/** Title drawn on the graph node. */
	virtual FText GetNodeTitle() const;

	/** Second line drawn on the graph node - usually a summary of the settings. */
	virtual FText GetNodeSubtitle() const;

	/** Tooltip / menu description. */
	virtual FText GetNodeTooltip() const;

	/** Where this node appears in the "add node" context menu. */
	virtual FText GetNodeCategory() const;

	virtual FLinearColor GetNodeColor() const;

	/** True when this node can be the entry point of a graph. */
	virtual bool IsEntryNode() const { return false; }

	/** Hidden from the node creation menu when false. */
	virtual bool IsPlaceableInGraph() const { return true; }

	/** Line shown by the debug HUD while this node is active. */
	virtual FString GetDebugStatus() const;

	// ------------------------------------------------------------ Pin helpers

	/**
	 * Rebuilds InputPins / OutputPins from the node's current settings,
	 * preserving any links whose pin name still exists afterwards.
	 * Subclasses override BuildPins(), not this.
	 */
	void RebuildPins();

	/** Subclasses declare their pins here by calling AddInputPin / AddOutputPin. */
	virtual void BuildPins();

	const FSimFlowOutputPin* FindOutputPin(FName PinName) const;
	FSimFlowOutputPin* FindOutputPin(FName PinName);
	bool HasInputPin(FName PinName) const;

	void AddInputPin(FName PinName, const FString& DisplayName = FString());
	void AddOutputPin(FName PinName, const FString& DisplayName = FString());

	/** Every node this node's outputs are wired to. */
	TArray<FGuid> GetConnectedNodeGuids() const;

	// ------------------------------------------------------------ Execution

	/** Called by the instance when execution arrives at PinName. */
	virtual void ExecuteInput(FName PinName);

	/** Called every frame while bActive. */
	virtual void TickNode(float DeltaTime);

	virtual void OnPauseNode() {}
	virtual void OnResumeNode() {}

	/** Called when the node stops being active for any reason. */
	virtual void Cleanup();

	/**
	 * When true, execution arriving at an already-active node calls ExecuteInput
	 * again instead of being ignored. Join and Loop nodes rely on this.
	 */
	virtual bool AllowsReentrantActivation() const { return false; }

	/** External control - default implementations do nothing except on Task nodes. */
	virtual bool CanRetry() const { return false; }
	virtual bool CanSkip() const { return false; }
	virtual void RequestRetry() {}
	virtual void RequestSkip() {}
	virtual void RequestFail() {}

	/** Pushes execution out of PinName. Deactivates this node unless bStayActive. */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Node")
	void TriggerOutput(FName PinName, bool bStayActive = false);

	/** Marks this node finished without pushing execution anywhere. */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Node")
	void FinishNode();

	// ------------------------------------------------------------ Save / load

	virtual void SaveNodeState(FSimFlowNodeSaveState& OutState) const;
	virtual void LoadNodeState(const FSimFlowNodeSaveState& InState);

	// ------------------------------------------------------------ Accessors

	UFUNCTION(BlueprintPure, Category = "SimFlow|Node")
	USimFlowInstance* GetFlowInstance() const { return FlowInstance; }

	UFUNCTION(BlueprintPure, Category = "SimFlow|Node")
	USimFlowBlackboard* GetBlackboard() const;

	UFUNCTION(BlueprintPure, Category = "SimFlow|Node")
	bool IsNodeActive() const { return bActive; }

	UFUNCTION(BlueprintPure, Category = "SimFlow|Node")
	float GetNodeActiveTime() const { return ActiveTime; }

	/** The asset this node was authored in (or the template of the running copy). */
	USimFlowAsset* GetOwningAsset() const;

	void SetFlowInstance(USimFlowInstance* InInstance) { FlowInstance = InInstance; }
	void SetActiveInternal(bool bInActive) { bActive = bInActive; }

	virtual UWorld* GetWorld() const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

protected:
	UPROPERTY(Transient)
	TObjectPtr<USimFlowInstance> FlowInstance = nullptr;

	UPROPERTY(Transient)
	bool bActive = false;

	UPROPERTY(Transient)
	float ActiveTime = 0.f;

	/** Which input pin execution arrived through most recently. */
	UPROPERTY(Transient)
	FName ActiveInputPin = NAME_None;

	friend class USimFlowInstance;
};
