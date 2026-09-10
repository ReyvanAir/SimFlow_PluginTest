// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectSaveContext.h"
#include "SimFlowTypes.h"
#include "SimFlowAsset.generated.h"

class USimFlowNode;
class USimFlowNode_Entry;
class UEdGraph;

/**
 * A flow: the authored graph of nodes that drives a tutorial or a gameplay sequence.
 *
 * Assets are templates. At runtime a USimFlowComponent creates a USimFlowInstance,
 * which duplicates the nodes so each running copy has its own state.
 */
UCLASS(BlueprintType, meta = (DisplayName = "SimFlow Graph"))
class SIMFLOWRUNTIME_API USimFlowAsset : public UObject
{
	GENERATED_BODY()

public:
	USimFlowAsset();

	/** Shown in tutorial UI and in the debug HUD. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flow")
	FText FlowDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flow", meta = (MultiLine = "true"))
	FText FlowDescription;

	/** Values written into a fresh blackboard whenever this flow starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flow")
	TArray<FSimFlowBlackboardEntry> InitialBlackboard;

	/** Every node in the graph. Owned by this asset. */
	UPROPERTY()
	TArray<TObjectPtr<USimFlowNode>> Nodes;

#if WITH_EDITORONLY_DATA
	/** The visual graph. Editor only - the runtime uses Nodes and their pin links. */
	UPROPERTY()
	TObjectPtr<UEdGraph> EdGraph = nullptr;

	UPROPERTY()
	FGuid AssetGuid;
#endif

	// ------------------------------------------------------------ Queries

	UFUNCTION(BlueprintPure, Category = "SimFlow")
	USimFlowNode* FindNodeByGuid(const FGuid& Guid) const;

	/** Returns the entry node with the given name, or the first entry when EntryName is None. */
	USimFlowNode_Entry* FindEntryNode(FName EntryName = NAME_None) const;

	UFUNCTION(BlueprintPure, Category = "SimFlow")
	TArray<FName> GetEntryNames() const;

	UFUNCTION(BlueprintPure, Category = "SimFlow")
	int32 GetNodeCount() const { return Nodes.Num(); }

	/** Human readable name for UI: FlowDisplayName if set, otherwise the asset name. */
	UFUNCTION(BlueprintPure, Category = "SimFlow")
	FText GetDisplayNameText() const;

	// ------------------------------------------------------------ Authoring

	/** Adds a node of the given class and returns it. Safe to call at runtime for procedural flows. */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Authoring", meta = (DeterminesOutputType = "NodeClass"))
	USimFlowNode* AddNode(TSubclassOf<USimFlowNode> NodeClass);

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Authoring")
	void RemoveNode(USimFlowNode* Node);

	/** Wires FromNode.FromPin into ToNode.ToPin. */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Authoring")
	bool ConnectNodes(USimFlowNode* FromNode, FName FromPin, USimFlowNode* ToNode, FName ToPin);

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Authoring")
	bool DisconnectNodes(USimFlowNode* FromNode, FName FromPin, USimFlowNode* ToNode, FName ToPin);

	/** Drops links that point at nodes which no longer exist. */
	void SanitizeLinks();

	/**
	 * Reports problems a designer should know about: no entry node, dangling
	 * links, task nodes with no task, unreachable nodes.
	 */
	UFUNCTION(BlueprintCallable, Category = "SimFlow")
	void ValidateFlow(TArray<FString>& OutErrors, TArray<FString>& OutWarnings) const;

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
#endif
};
