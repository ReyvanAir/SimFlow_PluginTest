// Copyright SimFlow. All Rights Reserved.

#include "SimFlowCondition.h"
#include "SimFlowInstance.h"
#include "SimFlowBlackboard.h"

bool USimFlowCondition::Evaluate(USimFlowInstance* Instance)
{
	CachedInstance = Instance;

	bool bResult = NativeEvaluate(Instance);

	// If a Blueprint subclass implements the event, let it have the final word.
	if (GetClass()->IsFunctionImplementedInScript(TEXT("ReceiveEvaluate")))
	{
		bResult = ReceiveEvaluate(Instance);
	}

	return bInvert ? !bResult : bResult;
}

bool USimFlowCondition::NativeEvaluate(USimFlowInstance* /*Instance*/) const
{
	return true;
}

FText USimFlowCondition::GetConditionDescription_Implementation() const
{
	return FText::FromString(GetClass()->GetName());
}

USimFlowBlackboard* USimFlowCondition::GetBlackboardFrom(USimFlowInstance* Instance)
{
	return Instance ? Instance->GetBlackboard() : nullptr;
}

UWorld* USimFlowCondition::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}
	if (CachedInstance)
	{
		return CachedInstance->GetWorld();
	}
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}
