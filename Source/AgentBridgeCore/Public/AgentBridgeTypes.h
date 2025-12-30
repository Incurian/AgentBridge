#pragma once

#include "CoreMinimal.h"
#include "AgentBridgeTypes.generated.h"

UENUM(BlueprintType)
enum class EAgentPropertyType : uint8
{
	None,
	Bool,
	Int8, Int16, Int32, Int64,
	UInt8, UInt16, UInt32, UInt64,
	Float, Double,
	String,
	Name,
	Text,
	Vector,
	Rotator,
	Transform,
	Color,
	Object,
	SoftObject,
	WeakObject,
	Class,
	Struct,
	Enum,
	Array,
	Map,
	Set,
	Unknown
};

// Forward declaration for pointer usage
struct FAgentPropertyValue;

// Container types for nested values (avoids incomplete type issues)
using FAgentPropertyValueArray = TArray<TSharedPtr<FAgentPropertyValue>>;
using FAgentPropertyValueMap = TMap<FString, TSharedPtr<FAgentPropertyValue>>;

USTRUCT(BlueprintType)
struct AGENTBRIDGECORE_API FAgentPropertyValue
{
	GENERATED_BODY()

	UPROPERTY()
	EAgentPropertyType Type = EAgentPropertyType::None;

	UPROPERTY()
	FString StringValue;

	UPROPERTY()
	TArray<uint8> BinaryValue;

	// Note: Self-referential members use TSharedPtr to avoid incomplete type issues
	// Cannot be UPROPERTY due to UE reflection limitation with self-referential types
	FAgentPropertyValueArray ArrayValue;
	FAgentPropertyValueMap StructValue;

	// Convenience constructors
	static FAgentPropertyValue FromBool(bool Value);
	static FAgentPropertyValue FromInt(int64 Value);
	static FAgentPropertyValue FromFloat(double Value);
	static FAgentPropertyValue FromString(const FString& Value);
	static FAgentPropertyValue FromVector(const FVector& Value);
	static FAgentPropertyValue FromObject(UObject* Object);

	// Convenience extractors
	bool AsBool() const;
	int64 AsInt() const;
	double AsFloat() const;
	FString AsString() const;
	FVector AsVector() const;
};

USTRUCT(BlueprintType)
struct AGENTBRIDGECORE_API FAgentPropertyInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FString PropertyName;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	EAgentPropertyType Type = EAgentPropertyType::None;

	UPROPERTY()
	FString TypeName;

	UPROPERTY()
	bool bIsReadOnly = false;

	UPROPERTY()
	bool bIsEditorOnly = false;

	UPROPERTY()
	FString Category;

	UPROPERTY()
	FString Description;
};

USTRUCT(BlueprintType)
struct AGENTBRIDGECORE_API FAgentClassInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FString ClassName;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	FString ClassPath;

	UPROPERTY()
	bool bIsBlueprintClass = false;

	UPROPERTY()
	bool bIsAbstract = false;

	UPROPERTY()
	FString ParentClassName;

	UPROPERTY()
	TArray<FString> ImplementedInterfaces;
};

USTRUCT(BlueprintType)
struct AGENTBRIDGECORE_API FAgentFunctionSignature
{
	GENERATED_BODY()

	UPROPERTY()
	FString FunctionName;

	UPROPERTY()
	TArray<FAgentPropertyInfo> Parameters;

	UPROPERTY()
	FAgentPropertyInfo ReturnValue;

	UPROPERTY()
	bool bIsStatic = false;

	UPROPERTY()
	bool bIsBlueprintCallable = false;

	UPROPERTY()
	bool bNeedsWorldContext = false;

	UPROPERTY()
	FString Description;
};

USTRUCT(BlueprintType)
struct AGENTBRIDGECORE_API FAgentFunctionResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bSuccess = false;

	UPROPERTY()
	FString ErrorMessage;

	// Note: Contains nested types, cannot be UPROPERTY
	FAgentPropertyValue ReturnValue;

	FAgentPropertyValueMap OutParams;
};

USTRUCT(BlueprintType)
struct AGENTBRIDGECORE_API FAgentEnumValue
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	int64 Value = 0;
};
