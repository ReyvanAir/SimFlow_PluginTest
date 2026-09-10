// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "SimFlowGraph.generated.h"

class USimFlowAsset;
class USimFlowGraphNode;

/**
 * The editor graph for a flow asset.
 *
 * The graph is the authoring surface; the runtime only ever reads the pin links
 * stored on the runtime nodes. CompileToAsset copies one into the other.
 */
UCLASS()
class SIMFLOWEDITOR_API USimFlowGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	USimFlowAsset* GetFlowAsset() const;

	/** Copies node positions and pin connections from the graph into the runtime data. */
	void CompileToAsset();

	/** Rebuilds the graph from the runtime data. Used when an asset is opened. */
	void RebuildFromAsset();

	/** Creates the graph for an asset that does not have one yet. */
	static USimFlowGraph* CreateGraphForAsset(USimFlowAsset* Asset);

	USimFlowGraphNode* FindGraphNodeForRuntimeNode(const class USimFlowNode* Node) const;
};
