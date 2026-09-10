// Copyright SimFlow. All Rights Reserved.

#include "SimFlowConditions.h"
#include "SimFlowInstance.h"
#include "SimFlowBlackboard.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "SimFlowConditions"

static FString LexOp(ESimFlowCompareOp Op)
{
	switch (Op)
	{
	case ESimFlowCompareOp::Equal:			return TEXT("==");
	case ESimFlowCompareOp::NotEqual:		return TEXT("!=");
	case ESimFlowCompareOp::Less:			return TEXT("<");
	case ESimFlowCompareOp::LessOrEqual:	return TEXT("<=");
	case ESimFlowCompareOp::Greater:		return TEXT(">");
	case ESimFlowCompareOp::GreaterOrEqual:	return TEXT(">=");
	default:								return TEXT("?");
	}
}

// ---------------------------------------------------------------- Blackboard

bool USimFlowCondition_BlackboardCompare::NativeEvaluate(USimFlowInstance* Instance) const
{
	USimFlowBlackboard* Blackboard = GetBlackboardFrom(Instance);
	if (!Blackboard)
	{
		return bResultWhenKeyMissing;
	}
	if (!Blackboard->HasValue(Key))
	{
		return bResultWhenKeyMissing;
	}
	return Blackboard->GetValue(Key).Compare(Operation, Value);
}

FText USimFlowCondition_BlackboardCompare::GetConditionDescription_Implementation() const
{
	return FText::FromString(FString::Printf(TEXT("%s %s %s"),
		*Key.ToString(), *LexOp(Operation), *Value.ToDisplayString()));
}

// --------------------------------------------------------------------- Score

bool USimFlowCondition_Score::NativeEvaluate(USimFlowInstance* Instance) const
{
	USimFlowBlackboard* Blackboard = GetBlackboardFrom(Instance);
	if (!Blackboard)
	{
		return false;
	}
	return FSimFlowValue::MakeFloat(Blackboard->GetScore()).Compare(Operation, FSimFlowValue::MakeFloat(Threshold));
}

FText USimFlowCondition_Score::GetConditionDescription_Implementation() const
{
	return FText::FromString(FString::Printf(TEXT("Score %s %g"), *LexOp(Operation), Threshold));
}

// --------------------------------------------------------------- Last result

bool USimFlowCondition_LastResult::NativeEvaluate(USimFlowInstance* Instance) const
{
	return Instance && Instance->GetLastTaskResult() == ExpectedResult;
}

FText USimFlowCondition_LastResult::GetConditionDescription_Implementation() const
{
	return FText::FromString(FString::Printf(TEXT("Last result == %s"),
		*StaticEnum<ESimFlowResult>()->GetNameStringByValue(static_cast<int64>(ExpectedResult))));
}

// -------------------------------------------------------------- Elapsed time

bool USimFlowCondition_ElapsedTime::NativeEvaluate(USimFlowInstance* Instance) const
{
	if (!Instance)
	{
		return false;
	}
	return FSimFlowValue::MakeFloat(Instance->GetElapsedTime()).Compare(Operation, FSimFlowValue::MakeFloat(Seconds));
}

FText USimFlowCondition_ElapsedTime::GetConditionDescription_Implementation() const
{
	return FText::FromString(FString::Printf(TEXT("Elapsed %s %gs"), *LexOp(Operation), Seconds));
}

// --------------------------------------------------------------------- Event

bool USimFlowCondition_EventRaised::NativeEvaluate(USimFlowInstance* Instance) const
{
	return Instance && EventTag.IsValid() && Instance->WasEventRaised(EventTag);
}

FText USimFlowCondition_EventRaised::GetConditionDescription_Implementation() const
{
	return FText::FromString(FString::Printf(TEXT("Event %s raised"), *EventTag.ToString()));
}

// ---------------------------------------------------------------- Composites

bool USimFlowCondition_AllOf::NativeEvaluate(USimFlowInstance* Instance) const
{
	for (const TObjectPtr<USimFlowCondition>& Child : Conditions)
	{
		if (Child && !Child->Evaluate(Instance))
		{
			return false;
		}
	}
	return true;
}

FText USimFlowCondition_AllOf::GetConditionDescription_Implementation() const
{
	return FText::FromString(FString::Printf(TEXT("All of (%d)"), Conditions.Num()));
}

bool USimFlowCondition_AnyOf::NativeEvaluate(USimFlowInstance* Instance) const
{
	if (Conditions.Num() == 0)
	{
		return false;
	}
	for (const TObjectPtr<USimFlowCondition>& Child : Conditions)
	{
		if (Child && Child->Evaluate(Instance))
		{
			return true;
		}
	}
	return false;
}

FText USimFlowCondition_AnyOf::GetConditionDescription_Implementation() const
{
	return FText::FromString(FString::Printf(TEXT("Any of (%d)"), Conditions.Num()));
}

// ------------------------------------------------------------------ Constant

bool USimFlowCondition_Constant::NativeEvaluate(USimFlowInstance* /*Instance*/) const
{
	return bValue;
}

FText USimFlowCondition_Constant::GetConditionDescription_Implementation() const
{
	return bValue ? LOCTEXT("ConstTrue", "Always true") : LOCTEXT("ConstFalse", "Always false");
}

// -------------------------------------------------------- Player near a spot

bool USimFlowCondition_PlayerNearLocation::NativeEvaluate(USimFlowInstance* Instance) const
{
	if (!Instance)
	{
		return false;
	}

	const UWorld* World = Instance->GetWorld();
	if (!World)
	{
		return false;
	}

	APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!Pawn)
	{
		return false;
	}

	FVector Target = Location;
	if (!LocationFromBlackboardKey.IsNone())
	{
		if (USimFlowBlackboard* Blackboard = Instance->GetBlackboard())
		{
			Target = Blackboard->GetVector(LocationFromBlackboardKey, Location);
		}
	}

	FVector Delta = Pawn->GetActorLocation() - Target;
	if (bIgnoreZ)
	{
		Delta.Z = 0.f;
	}
	return Delta.SizeSquared() <= FMath::Square(Radius);
}

FText USimFlowCondition_PlayerNearLocation::GetConditionDescription_Implementation() const
{
	return FText::FromString(FString::Printf(TEXT("Player within %gcm"), Radius));
}

#undef LOCTEXT_NAMESPACE
