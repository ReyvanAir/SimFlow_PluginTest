// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SimFlowCondition.h"
#include "GameplayTagContainer.h"
#include "SimFlowConditions.generated.h"

/** True when a blackboard key compares as requested against a literal value. */
UCLASS(DisplayName = "Blackboard Compare", meta = (ToolTip = "Compares a blackboard key against a literal value."))
class SIMFLOWRUNTIME_API USimFlowCondition_BlackboardCompare : public USimFlowCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FName Key = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	ESimFlowCompareOp Operation = ESimFlowCompareOp::Equal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FSimFlowValue Value;

	/** Returned when the key does not exist at all. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition", AdvancedDisplay)
	bool bResultWhenKeyMissing = false;

	virtual bool NativeEvaluate(USimFlowInstance* Instance) const override;
	virtual FText GetConditionDescription_Implementation() const override;
};

/** True when the run score passes a threshold. Shorthand for a compare on "Score". */
UCLASS(DisplayName = "Score Threshold")
class SIMFLOWRUNTIME_API USimFlowCondition_Score : public USimFlowCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	ESimFlowCompareOp Operation = ESimFlowCompareOp::GreaterOrEqual;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	float Threshold = 100.f;

	virtual bool NativeEvaluate(USimFlowInstance* Instance) const override;
	virtual FText GetConditionDescription_Implementation() const override;
};

/** True when the most recently finished task ended with the given result. */
UCLASS(DisplayName = "Last Task Result Is")
class SIMFLOWRUNTIME_API USimFlowCondition_LastResult : public USimFlowCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	ESimFlowResult ExpectedResult = ESimFlowResult::Succeeded;

	virtual bool NativeEvaluate(USimFlowInstance* Instance) const override;
	virtual FText GetConditionDescription_Implementation() const override;
};

/** True when the flow has been running for longer / shorter than a given time. */
UCLASS(DisplayName = "Elapsed Time")
class SIMFLOWRUNTIME_API USimFlowCondition_ElapsedTime : public USimFlowCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	ESimFlowCompareOp Operation = ESimFlowCompareOp::GreaterOrEqual;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition", meta = (ClampMin = "0.0", Units = "s"))
	float Seconds = 60.f;

	virtual bool NativeEvaluate(USimFlowInstance* Instance) const override;
	virtual FText GetConditionDescription_Implementation() const override;
};

/** True when a gameplay tag has been raised on the flow (see USimFlowComponent::SendEvent). */
UCLASS(DisplayName = "Event Was Raised")
class SIMFLOWRUNTIME_API USimFlowCondition_EventRaised : public USimFlowCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FGameplayTag EventTag;

	virtual bool NativeEvaluate(USimFlowInstance* Instance) const override;
	virtual FText GetConditionDescription_Implementation() const override;
};

/** Logical AND / OR over a list of child conditions. */
UCLASS(DisplayName = "All Of (AND)")
class SIMFLOWRUNTIME_API USimFlowCondition_AllOf : public USimFlowCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Condition")
	TArray<TObjectPtr<USimFlowCondition>> Conditions;

	virtual bool NativeEvaluate(USimFlowInstance* Instance) const override;
	virtual FText GetConditionDescription_Implementation() const override;
};

UCLASS(DisplayName = "Any Of (OR)")
class SIMFLOWRUNTIME_API USimFlowCondition_AnyOf : public USimFlowCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Condition")
	TArray<TObjectPtr<USimFlowCondition>> Conditions;

	virtual bool NativeEvaluate(USimFlowInstance* Instance) const override;
	virtual FText GetConditionDescription_Implementation() const override;
};

/** Always returns the configured constant. Useful as a placeholder / default branch. */
UCLASS(DisplayName = "Constant")
class SIMFLOWRUNTIME_API USimFlowCondition_Constant : public USimFlowCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	bool bValue = true;

	virtual bool NativeEvaluate(USimFlowInstance* Instance) const override;
	virtual FText GetConditionDescription_Implementation() const override;
};

/** True when the player pawn is within Radius of a world location (very common in VR flows). */
UCLASS(DisplayName = "Player Near Location")
class SIMFLOWRUNTIME_API USimFlowCondition_PlayerNearLocation : public USimFlowCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition", meta = (ClampMin = "1.0", Units = "cm"))
	float Radius = 150.f;

	/** When set, the radius test uses this blackboard key's vector instead of Location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition", AdvancedDisplay)
	FName LocationFromBlackboardKey = NAME_None;

	/** Ignore the vertical axis - useful when the HMD height varies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition", AdvancedDisplay)
	bool bIgnoreZ = true;

	virtual bool NativeEvaluate(USimFlowInstance* Instance) const override;
	virtual FText GetConditionDescription_Implementation() const override;
};
