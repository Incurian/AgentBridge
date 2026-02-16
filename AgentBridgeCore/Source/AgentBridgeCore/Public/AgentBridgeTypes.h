#pragma once

#include "CoreMinimal.h"

// Property type enumeration for transport
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

// Forward declare for pointer members
struct FAgentPropertyValue;

// Transport type for property values - supports nested structures
struct AGENTBRIDGECORE_API FAgentPropertyValue
{
	EAgentPropertyType Type = EAgentPropertyType::None;
	FString StringValue;
	TArray<uint8> BinaryValue;
	TArray<TSharedPtr<FAgentPropertyValue>> ArrayValue;
	TMap<FString, TSharedPtr<FAgentPropertyValue>> StructValue;

	// Default constructor
	FAgentPropertyValue() = default;

	// Convenience constructors
	explicit FAgentPropertyValue(bool Value);
	explicit FAgentPropertyValue(int64 Value);
	explicit FAgentPropertyValue(double Value);
	explicit FAgentPropertyValue(const FString& Value);
	explicit FAgentPropertyValue(const FVector& Value);
	explicit FAgentPropertyValue(const FRotator& Value);
	explicit FAgentPropertyValue(const FTransform& Value);

	// Static factory methods
	static FAgentPropertyValue FromBool(bool Value);
	static FAgentPropertyValue FromInt(int64 Value);
	static FAgentPropertyValue FromFloat(double Value);
	static FAgentPropertyValue FromString(const FString& Value);
	static FAgentPropertyValue FromVector(const FVector& Value);
	static FAgentPropertyValue FromRotator(const FRotator& Value);
	static FAgentPropertyValue FromTransform(const FTransform& Value);
	static FAgentPropertyValue FromObject(UObject* Object);

	// Convenience extractors
	bool AsBool() const;
	int64 AsInt() const;
	double AsFloat() const;
	FString AsString() const;
	FVector AsVector() const;
	FRotator AsRotator() const;
	FTransform AsTransform() const;

	// Alias methods for readability
	bool GetBool() const { return AsBool(); }
	int64 GetInt() const { return AsInt(); }
	double GetFloat() const { return AsFloat(); }
	FString GetString() const { return AsString(); }
};

// Property metadata for discovery
struct AGENTBRIDGECORE_API FAgentPropertyInfo
{
	FString PropertyName;
	FString DisplayName;
	EAgentPropertyType Type = EAgentPropertyType::None;
	FString TypeName;
	FString ElementType;      // For TArray/TSet/TMap: the inner type name
	FString KeyType;          // For TMap: the key type name
	bool bIsReadOnly = false;
	bool bIsEditorOnly = false;
	FString Category;
	FString Description;
};

// Class metadata for discovery
struct AGENTBRIDGECORE_API FAgentClassInfo
{
	FString ClassName;
	FString DisplayName;
	FString ClassPath;
	bool bIsBlueprintClass = false;
	bool bIsAbstract = false;
	FString ParentClassName;
	TArray<FString> ImplementedInterfaces;
};

// Function signature for discovery
struct AGENTBRIDGECORE_API FAgentFunctionSignature
{
	FString FunctionName;
	TArray<FAgentPropertyInfo> Parameters;
	FAgentPropertyInfo ReturnValue;
	bool bIsStatic = false;
	bool bIsBlueprintCallable = false;
	bool bNeedsWorldContext = false;
	FString Description;
};

// Function invocation result
struct AGENTBRIDGECORE_API FAgentFunctionResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	FAgentPropertyValue ReturnValue;
	TMap<FString, TSharedPtr<FAgentPropertyValue>> OutParams;
};

// Enum value for discovery
struct AGENTBRIDGECORE_API FAgentEnumValue
{
	FString Name;
	FString DisplayName;
	int64 Value = 0;
};
