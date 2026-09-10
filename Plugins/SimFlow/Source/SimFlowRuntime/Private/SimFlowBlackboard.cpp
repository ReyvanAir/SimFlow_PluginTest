// Copyright SimFlow. All Rights Reserved.

#include "SimFlowBlackboard.h"
#include "SimFlowRuntimeModule.h"

void USimFlowBlackboard::SetValue(FName Key, const FSimFlowValue& Value)
{
	if (Key.IsNone())
	{
		return;
	}
	Values.Add(Key, Value);
	OnValueChanged.Broadcast(Key, Value);
}

FSimFlowValue USimFlowBlackboard::GetValue(FName Key) const
{
	if (const FSimFlowValue* Found = Values.Find(Key))
	{
		return *Found;
	}
	return FSimFlowValue();
}

bool USimFlowBlackboard::HasValue(FName Key) const
{
	return Values.Contains(Key);
}

void USimFlowBlackboard::RemoveValue(FName Key)
{
	if (Values.Remove(Key) > 0)
	{
		OnValueChanged.Broadcast(Key, FSimFlowValue());
	}
}

void USimFlowBlackboard::ClearAll()
{
	Values.Reset();
}

void USimFlowBlackboard::AddToValue(FName Key, const FSimFlowValue& Delta)
{
	FSimFlowValue Current = GetValue(Key);
	Current.Add(Delta);
	SetValue(Key, Current);
}

void USimFlowBlackboard::SetBool(FName Key, bool Value)					{ SetValue(Key, FSimFlowValue::MakeBool(Value)); }
void USimFlowBlackboard::SetInt(FName Key, int32 Value)					{ SetValue(Key, FSimFlowValue::MakeInt(Value)); }
void USimFlowBlackboard::SetFloat(FName Key, float Value)				{ SetValue(Key, FSimFlowValue::MakeFloat(Value)); }
void USimFlowBlackboard::SetString(FName Key, const FString& Value)		{ SetValue(Key, FSimFlowValue::MakeString(Value)); }
void USimFlowBlackboard::SetName(FName Key, FName Value)					{ SetValue(Key, FSimFlowValue::MakeName(Value)); }
void USimFlowBlackboard::SetVector(FName Key, FVector Value)				{ SetValue(Key, FSimFlowValue::MakeVector(Value)); }
void USimFlowBlackboard::SetObject(FName Key, UObject* Value)			{ SetValue(Key, FSimFlowValue::MakeObject(Value)); }

bool USimFlowBlackboard::GetBool(FName Key, bool DefaultValue) const
{
	const FSimFlowValue* Found = Values.Find(Key);
	return Found ? Found->AsBool() : DefaultValue;
}

int32 USimFlowBlackboard::GetInt(FName Key, int32 DefaultValue) const
{
	const FSimFlowValue* Found = Values.Find(Key);
	return Found ? FMath::RoundToInt(Found->AsNumber()) : DefaultValue;
}

float USimFlowBlackboard::GetFloat(FName Key, float DefaultValue) const
{
	const FSimFlowValue* Found = Values.Find(Key);
	return Found ? static_cast<float>(Found->AsNumber()) : DefaultValue;
}

FString USimFlowBlackboard::GetString(FName Key, const FString& DefaultValue) const
{
	const FSimFlowValue* Found = Values.Find(Key);
	return Found ? Found->AsString() : DefaultValue;
}

FName USimFlowBlackboard::GetName(FName Key, FName DefaultValue) const
{
	const FSimFlowValue* Found = Values.Find(Key);
	if (!Found)
	{
		return DefaultValue;
	}
	return (Found->Type == ESimFlowValueType::Name) ? Found->NameValue : FName(*Found->AsString());
}

FVector USimFlowBlackboard::GetVector(FName Key, FVector DefaultValue) const
{
	const FSimFlowValue* Found = Values.Find(Key);
	return (Found && Found->Type == ESimFlowValueType::Vector) ? Found->VectorValue : DefaultValue;
}

UObject* USimFlowBlackboard::GetObject(FName Key) const
{
	const FSimFlowValue* Found = Values.Find(Key);
	return (Found && Found->Type == ESimFlowValueType::Object) ? Found->ObjectValue.Get() : nullptr;
}

void USimFlowBlackboard::AddScore(float Delta)
{
	SetFloat(SimFlowKeys::Score, GetScore() + Delta);
}

float USimFlowBlackboard::GetScore() const
{
	return GetFloat(SimFlowKeys::Score, 0.f);
}

TArray<FSimFlowBlackboardEntry> USimFlowBlackboard::ToEntries(bool bStripObjectReferences) const
{
	TArray<FSimFlowBlackboardEntry> Out;
	Out.Reserve(Values.Num());
	for (const TPair<FName, FSimFlowValue>& Pair : Values)
	{
		if (bStripObjectReferences && Pair.Value.Type == ESimFlowValueType::Object)
		{
			// Object references cannot be meaningfully restored from a save file.
			continue;
		}
		Out.Emplace(Pair.Key, Pair.Value);
	}
	Out.Sort([](const FSimFlowBlackboardEntry& A, const FSimFlowBlackboardEntry& B)
	{
		return A.Key.LexicalLess(B.Key);
	});
	return Out;
}

void USimFlowBlackboard::FromEntries(const TArray<FSimFlowBlackboardEntry>& Entries, bool bClearFirst)
{
	if (bClearFirst)
	{
		Values.Reset();
	}
	for (const FSimFlowBlackboardEntry& Entry : Entries)
	{
		if (!Entry.Key.IsNone())
		{
			Values.Add(Entry.Key, Entry.Value);
		}
	}
}

void USimFlowBlackboard::MergeFrom(const USimFlowBlackboard* Other, bool bOverwriteExisting)
{
	if (!Other)
	{
		return;
	}
	for (const TPair<FName, FSimFlowValue>& Pair : Other->Values)
	{
		if (bOverwriteExisting || !Values.Contains(Pair.Key))
		{
			Values.Add(Pair.Key, Pair.Value);
		}
	}
}

FString USimFlowBlackboard::ToDebugString() const
{
	TArray<FName> Keys;
	Values.GetKeys(Keys);
	Keys.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });

	FString Result;
	for (const FName& Key : Keys)
	{
		Result += FString::Printf(TEXT("  %s = %s\n"), *Key.ToString(), *Values[Key].ToDisplayString());
	}
	return Result;
}
