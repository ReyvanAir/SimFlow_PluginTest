// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "SimFlowGraphNode.generated.h"

class USimFlowNode;
class USimFlowAsset;

/** Editor-side wrapper around a runtime USimFlowNode. */
UCLASS()
class SIMFLOWEDITOR_API USimFlowGraphNode : public UEdGraphNode
{
	GENERATED_BODY()

public:
	/** The runtime node this graph node represents. Outered to the flow asset. */
	UPROPERTY(VisibleAnywhere, Instanced, Category = "SimFlow")
	TObjectPtr<USimFlowNode> RuntimeNode = nullptr;

	void SetRuntimeNode(USimFlowNode* InNode);

	/** Rebuilds the Slate pins from the runtime node's pin arrays, keeping connections. */
	void SyncPinsWithRuntimeNode();

	USimFlowAsset* GetFlowAsset() const;

	// ------------------------------------------------------------ UEdGraphNode

	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual void PrepareForCopying() override;
	virtual void PostPasteNode() override;

	/** Puts the runtime node back under the asset after a copy. */
	void PostCopyNode();
	virtual void DestroyNode() override;
	virtual void ReconstructNode() override;
	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;
	virtual bool CanUserDeleteNode() const override;
	virtual bool CanDuplicateNode() const override;
	virtual void OnUpdateCommentText(const FString& NewComment) override;
	virtual void NodeConnectionListChanged() override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UEdGraphPin* FindPinByRuntimeName(FName PinName, EEdGraphPinDirection Direction) const;

private:
	void HandleRuntimePinsChanged();

	FDelegateHandle PinsChangedHandle;
};
