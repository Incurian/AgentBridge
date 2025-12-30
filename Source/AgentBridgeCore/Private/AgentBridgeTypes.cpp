#include "AgentBridgeTypes.h"

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
	Result.StringValue = FString::Printf(TEXT("%f"), Value);
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
