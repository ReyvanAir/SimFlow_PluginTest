// Copyright SimFlow. All Rights Reserved.

#include "SimFlowTypes.h"

namespace SimFlowPins
{
	const FName In(TEXT("In"));
	const FName Out(TEXT("Out"));
	const FName Completed(TEXT("Completed"));
	const FName Failed(TEXT("Failed"));
	const FName Skipped(TEXT("Skipped"));
	const FName TimedOut(TEXT("TimedOut"));
	const FName Default(TEXT("Default"));
	const FName LoopBody(TEXT("LoopBody"));
}

namespace SimFlowKeys
{
	const FName Score(TEXT("Score"));
	const FName Mistakes(TEXT("Mistakes"));
	const FName LastResult(TEXT("LastResult"));
	const FName LastAnswerIndex(TEXT("LastAnswerIndex"));
	const FName LastAnswerCorrect(TEXT("LastAnswerCorrect"));
	const FName WrongAttempts(TEXT("WrongAttempts"));
	const FName CurrentStep(TEXT("CurrentStep"));
}

FSimFlowValue FSimFlowValue::MakeBool(bool In)
{
	FSimFlowValue V; V.Type = ESimFlowValueType::Bool; V.BoolValue = In; return V;
}

FSimFlowValue FSimFlowValue::MakeInt(int32 In)
{
	FSimFlowValue V; V.Type = ESimFlowValueType::Int; V.IntValue = In; return V;
}

FSimFlowValue FSimFlowValue::MakeFloat(float In)
{
	FSimFlowValue V; V.Type = ESimFlowValueType::Float; V.FloatValue = In; return V;
}

FSimFlowValue FSimFlowValue::MakeString(const FString& In)
{
	FSimFlowValue V; V.Type = ESimFlowValueType::String; V.StringValue = In; return V;
}

FSimFlowValue FSimFlowValue::MakeName(FName In)
{
	FSimFlowValue V; V.Type = ESimFlowValueType::Name; V.NameValue = In; return V;
}

FSimFlowValue FSimFlowValue::MakeVector(const FVector& In)
{
	FSimFlowValue V; V.Type = ESimFlowValueType::Vector; V.VectorValue = In; return V;
}

FSimFlowValue FSimFlowValue::MakeObject(UObject* In)
{
	FSimFlowValue V; V.Type = ESimFlowValueType::Object; V.ObjectValue = In; return V;
}

double FSimFlowValue::AsNumber() const
{
	switch (Type)
	{
	case ESimFlowValueType::Bool:	return BoolValue ? 1.0 : 0.0;
	case ESimFlowValueType::Int:	return static_cast<double>(IntValue);
	case ESimFlowValueType::Float:	return static_cast<double>(FloatValue);
	case ESimFlowValueType::String:	return FCString::Atod(*StringValue);
	case ESimFlowValueType::Name:	return FCString::Atod(*NameValue.ToString());
	case ESimFlowValueType::Vector:	return VectorValue.Size();
	default:						return 0.0;
	}
}

bool FSimFlowValue::AsBool() const
{
	switch (Type)
	{
	case ESimFlowValueType::Bool:	return BoolValue;
	case ESimFlowValueType::Int:	return IntValue != 0;
	case ESimFlowValueType::Float:	return !FMath::IsNearlyZero(FloatValue);
	case ESimFlowValueType::String:	return StringValue.ToBool() || StringValue.Len() > 0;
	case ESimFlowValueType::Name:	return NameValue != NAME_None;
	case ESimFlowValueType::Vector:	return !VectorValue.IsNearlyZero();
	case ESimFlowValueType::Object:	return ObjectValue != nullptr;
	default:						return false;
	}
}

FString FSimFlowValue::AsString() const
{
	switch (Type)
	{
	case ESimFlowValueType::Bool:	return BoolValue ? TEXT("true") : TEXT("false");
	case ESimFlowValueType::Int:	return FString::FromInt(IntValue);
	case ESimFlowValueType::Float:	return FString::SanitizeFloat(FloatValue);
	case ESimFlowValueType::String:	return StringValue;
	case ESimFlowValueType::Name:	return NameValue.ToString();
	case ESimFlowValueType::Vector:	return VectorValue.ToString();
	case ESimFlowValueType::Object:	return ObjectValue ? ObjectValue->GetName() : TEXT("None");
	default:						return TEXT("");
	}
}

void FSimFlowValue::Add(const FSimFlowValue& Other)
{
	switch (Type)
	{
	case ESimFlowValueType::None:
		*this = Other;
		break;
	case ESimFlowValueType::Int:
		IntValue += FMath::RoundToInt(Other.AsNumber());
		break;
	case ESimFlowValueType::Float:
		FloatValue += static_cast<float>(Other.AsNumber());
		break;
	case ESimFlowValueType::Bool:
		BoolValue = BoolValue || Other.AsBool();
		break;
	case ESimFlowValueType::String:
		StringValue += Other.AsString();
		break;
	case ESimFlowValueType::Vector:
		VectorValue += (Other.Type == ESimFlowValueType::Vector) ? Other.VectorValue : FVector::ZeroVector;
		break;
	default:
		break;
	}
}

bool FSimFlowValue::Compare(ESimFlowCompareOp Op, const FSimFlowValue& Other) const
{
	// String / Name comparisons stay textual for equality, numeric otherwise.
	const bool bTextual =
		(Type == ESimFlowValueType::String || Type == ESimFlowValueType::Name) &&
		(Other.Type == ESimFlowValueType::String || Other.Type == ESimFlowValueType::Name);

	if (Type == ESimFlowValueType::Object || Other.Type == ESimFlowValueType::Object)
	{
		const bool bEqual = ObjectValue == Other.ObjectValue;
		return (Op == ESimFlowCompareOp::NotEqual) ? !bEqual : bEqual;
	}

	if (Type == ESimFlowValueType::Vector && Other.Type == ESimFlowValueType::Vector)
	{
		const bool bEqual = VectorValue.Equals(Other.VectorValue);
		switch (Op)
		{
		case ESimFlowCompareOp::Equal:		return bEqual;
		case ESimFlowCompareOp::NotEqual:	return !bEqual;
		default: break;
		}
	}

	if (bTextual && (Op == ESimFlowCompareOp::Equal || Op == ESimFlowCompareOp::NotEqual))
	{
		const bool bEqual = AsString().Equals(Other.AsString(), ESearchCase::IgnoreCase);
		return (Op == ESimFlowCompareOp::Equal) ? bEqual : !bEqual;
	}

	const double A = AsNumber();
	const double B = Other.AsNumber();

	switch (Op)
	{
	case ESimFlowCompareOp::Equal:			return FMath::IsNearlyEqual(A, B, UE_KINDA_SMALL_NUMBER);
	case ESimFlowCompareOp::NotEqual:		return !FMath::IsNearlyEqual(A, B, UE_KINDA_SMALL_NUMBER);
	case ESimFlowCompareOp::Less:			return A < B;
	case ESimFlowCompareOp::LessOrEqual:	return A <= B;
	case ESimFlowCompareOp::Greater:		return A > B;
	case ESimFlowCompareOp::GreaterOrEqual:	return A >= B;
	default:								return false;
	}
}

FString FSimFlowValue::ToDisplayString() const
{
	return AsString();
}

void FSimFlowValue::StripObjectReferences()
{
	if (Type == ESimFlowValueType::Object)
	{
		ObjectValue = nullptr;
	}
}

bool FSimFlowValue::operator==(const FSimFlowValue& Other) const
{
	if (Type != Other.Type)
	{
		return false;
	}
	switch (Type)
	{
	case ESimFlowValueType::None:	return true;
	case ESimFlowValueType::Bool:	return BoolValue == Other.BoolValue;
	case ESimFlowValueType::Int:	return IntValue == Other.IntValue;
	case ESimFlowValueType::Float:	return FMath::IsNearlyEqual(FloatValue, Other.FloatValue);
	case ESimFlowValueType::String:	return StringValue == Other.StringValue;
	case ESimFlowValueType::Name:	return NameValue == Other.NameValue;
	case ESimFlowValueType::Vector:	return VectorValue.Equals(Other.VectorValue);
	case ESimFlowValueType::Object:	return ObjectValue == Other.ObjectValue;
	default:						return false;
	}
}
