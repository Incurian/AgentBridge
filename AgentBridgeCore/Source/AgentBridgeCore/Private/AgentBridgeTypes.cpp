#include "AgentBridgeTypes.h"

//~==============================================================================
// Convenience Constructors
//~==============================================================================

FAgentPropertyValue::FAgentPropertyValue(bool Value)
{
	Type = EAgentPropertyType::Bool;
	StringValue = Value ? TEXT("true") : TEXT("false");
}

FAgentPropertyValue::FAgentPropertyValue(int64 Value)
{
	Type = EAgentPropertyType::Int64;
	StringValue = FString::Printf(TEXT("%lld"), Value);
}

FAgentPropertyValue::FAgentPropertyValue(double Value)
{
	Type = EAgentPropertyType::Double;
	StringValue = FString::Printf(TEXT("%.17g"), Value);
}

FAgentPropertyValue::FAgentPropertyValue(const FString& Value)
{
	Type = EAgentPropertyType::String;
	StringValue = Value;
}

FAgentPropertyValue::FAgentPropertyValue(const FVector& Value)
{
	Type = EAgentPropertyType::Vector;
	StringValue = Value.ToString();
}

FAgentPropertyValue::FAgentPropertyValue(const FRotator& Value)
{
	Type = EAgentPropertyType::Rotator;
	StringValue = Value.ToString();
}

FAgentPropertyValue::FAgentPropertyValue(const FTransform& Value)
{
	Type = EAgentPropertyType::Transform;
	StringValue = Value.ToString();
}

//~==============================================================================
// Static Factory Methods
//~==============================================================================

FAgentPropertyValue FAgentPropertyValue::FromBool(bool Value)
{
	FAgentPropertyValue Result;
	Result.Type = EAgentPropertyType::Bool;
	Result.StringValue = Value ? TEXT("true") : TEXT("false");
	return Result;
}

FAgentPropertyValue FAgentPropertyValue::FromInt(int64 Value)
{
	FAgentPropertyValue Result;
	Result.Type = EAgentPropertyType::Int64;
	Result.StringValue = FString::Printf(TEXT("%lld"), Value);
	return Result;
}

FAgentPropertyValue FAgentPropertyValue::FromFloat(double Value)
{
	FAgentPropertyValue Result;
	Result.Type = EAgentPropertyType::Double;
	Result.StringValue = FString::Printf(TEXT("%.17g"), Value);
	return Result;
}

FAgentPropertyValue FAgentPropertyValue::FromString(const FString& Value)
{
	FAgentPropertyValue Result;
	Result.Type = EAgentPropertyType::String;
	Result.StringValue = Value;
	return Result;
}

FAgentPropertyValue FAgentPropertyValue::FromVector(const FVector& Value)
{
	FAgentPropertyValue Result;
	Result.Type = EAgentPropertyType::Vector;
	Result.StringValue = Value.ToString();
	return Result;
}

FAgentPropertyValue FAgentPropertyValue::FromRotator(const FRotator& Value)
{
	FAgentPropertyValue Result;
	Result.Type = EAgentPropertyType::Rotator;
	Result.StringValue = Value.ToString();
	return Result;
}

FAgentPropertyValue FAgentPropertyValue::FromTransform(const FTransform& Value)
{
	FAgentPropertyValue Result;
	Result.Type = EAgentPropertyType::Transform;
	Result.StringValue = Value.ToString();
	return Result;
}

FAgentPropertyValue FAgentPropertyValue::FromObject(UObject* Object)
{
	FAgentPropertyValue Result;
	Result.Type = EAgentPropertyType::Object;
	if (Object)
	{
		Result.StringValue = Object->GetPathName();
	}
	return Result;
}

bool FAgentPropertyValue::AsBool() const
{
	return StringValue.Equals(TEXT("true"), ESearchCase::IgnoreCase) || StringValue == TEXT("1");
}

int64 FAgentPropertyValue::AsInt() const
{
	return FCString::Atoi64(*StringValue);
}

double FAgentPropertyValue::AsFloat() const
{
	return FCString::Atod(*StringValue);
}

FString FAgentPropertyValue::AsString() const
{
	return StringValue;
}

FVector FAgentPropertyValue::AsVector() const
{
	FVector Result;
	Result.InitFromString(StringValue);
	return Result;
}

FRotator FAgentPropertyValue::AsRotator() const
{
	FRotator Result;
	Result.InitFromString(StringValue);
	return Result;
}

FTransform FAgentPropertyValue::AsTransform() const
{
	FTransform Result;
	Result.InitFromString(StringValue);
	return Result;
}
