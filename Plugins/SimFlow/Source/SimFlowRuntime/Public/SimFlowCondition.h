// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SimFlowTypes.h"
#include "SimFlowCondition.generated.h"

class USimFlowInstance;
class USimFlowBlackboard;

/**
 * Base class for anything that answers a yes/no question about the current run.
 * Subclass in C++ or Blueprint and override Evaluate.
 *
 * Conditions are instanced sub-objects: drop them straight into a Branch node,
 * a Wait node or a Task's early-out list in the details panel.
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced, CollapseCategories)
class SIMFLOWRUNTIME_API USimFlowCondition : public UObject
{
	GENERATED_BODY()

public:
	/** Inverts the result of Evaluate. Handy so you rarely need a NOT wrapper. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	bool bInvert = false;

	/** Evaluates the condition. Override ReceiveEvaluate in Blueprint or NativeEvaluate in C++. */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Condition")
	bool Evaluate(USimFlowInstance* Instance);

	/** Blueprint hook. */
	UFUNCTION(BlueprintImplementableEvent, Category = "SimFlow|Condition", meta = (DisplayName = "Evaluate"))
	bool ReceiveEvaluate(USimFlowInstance* Instance);

	/** C++ hook. */
	virtual bool NativeEvaluate(USimFlowInstance* Instance) const;

	/** Short human readable form, shown on graph nodes and in the debug HUD. */
	UFUNCTION(BlueprintNativeEvent, Category = "SimFlow|Condition")
	FText GetConditionDescription() const;
	virtual FText GetConditionDescription_Implementation() const;

	/** Convenience accessor used by most built-in conditions. */
	UFUNCTION(BlueprintPure, Category = "SimFlow|Condition")
	static USimFlowBlackboard* GetBlackboardFrom(USimFlowInstance* Instance);

	virtual UWorld* GetWorld() const override;

protected:
	/** Set by the runtime just before evaluation so Blueprint graphs have world context. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "SimFlow|Condition")
	TObjectPtr<USimFlowInstance> CachedInstance = nullptr;
};
