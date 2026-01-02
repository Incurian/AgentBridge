#include "PropertyAccessor.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"
#include "UObject/SoftObjectPath.h"

//~==============================================================================
// Core Read/Write Operations
//~==============================================================================

FAgentPropertyValue FPropertyAccessor::ReadProperty(
	const void* Container,
	FProperty* Property,
	int32 MaxDepth)
{
	if (!Container || !Property)
	{
		FAgentPropertyValue Result;
		Result.Type = EAgentPropertyType::Unknown;
		return Result;
	}

	// Get pointer to the actual value within the container
	const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);

	// Dispatch based on property type
	// Order matters - check more specific types before base types

	// Boolean
	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		return ReadBoolProperty(Container, BoolProp);
	}

	// Enum (check before numeric since FByteProperty can be an enum)
	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		return ReadEnumProperty(Container, Property);
	}
	if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		if (ByteProp->Enum)
		{
			return ReadEnumProperty(Container, Property);
		}
	}

	// Numeric types
	if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		return ReadNumericProperty(Container, NumProp);
	}

	// String types
	if (CastField<FStrProperty>(Property) ||
		CastField<FNameProperty>(Property) ||
		CastField<FTextProperty>(Property))
	{
		return ReadStringProperty(Container, Property);
	}

	// Object references (check all object property types)
	if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
	{
		return ReadObjectProperty(Container, ObjProp);
	}

	// Struct
	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		return ReadStructProperty(Container, StructProp, 0, MaxDepth);
	}

	// Containers
	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		return ReadArrayProperty(Container, ArrayProp, 0, MaxDepth);
	}
	if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
	{
		return ReadMapProperty(Container, MapProp, 0, MaxDepth);
	}
	if (FSetProperty* SetProp = CastField<FSetProperty>(Property))
	{
		return ReadSetProperty(Container, SetProp, 0, MaxDepth);
	}

	// Fallback - use ExportText for unknown types
	FAgentPropertyValue Result;
	Result.Type = EAgentPropertyType::Unknown;
	Property->ExportTextItem_Direct(Result.StringValue, ValuePtr, nullptr, nullptr, PPF_None);
	return Result;
}

bool FPropertyAccessor::WriteProperty(
	void* Container,
	FProperty* Property,
	const FAgentPropertyValue& Value)
{
	if (!Container || !Property)
	{
		return false;
	}

	// Calculate value pointer from container and delegate to direct write
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	return WritePropertyDirect(ValuePtr, Property, Value);
}

bool FPropertyAccessor::WritePropertyDirect(
	void* ValuePtr,
	FProperty* Property,
	const FAgentPropertyValue& Value)
{
	if (!ValuePtr || !Property)
	{
		return false;
	}

	// Check if property is writable
	if (!IsPropertyWritable(Property))
	{
		return false;
	}

	// Dispatch based on property type
	// NOTE: All Write*Property helpers now receive the direct ValuePtr, NOT a container.
	// They must NOT call ContainerPtrToValuePtr internally.
	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		return WriteBoolProperty(ValuePtr, BoolProp, Value);
	}

	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		return WriteEnumProperty(ValuePtr, Property, Value);
	}
	if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		if (ByteProp->Enum)
		{
			return WriteEnumProperty(ValuePtr, Property, Value);
		}
	}

	if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		return WriteNumericProperty(ValuePtr, NumProp, Value);
	}

	if (CastField<FStrProperty>(Property) ||
		CastField<FNameProperty>(Property) ||
		CastField<FTextProperty>(Property))
	{
		return WriteStringProperty(ValuePtr, Property, Value);
	}

	if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
	{
		return WriteObjectProperty(ValuePtr, ObjProp, Value);
	}

	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		return WriteStructProperty(ValuePtr, StructProp, Value);
	}

	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		return WriteArrayProperty(ValuePtr, ArrayProp, Value);
	}

	if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
	{
		return WriteMapProperty(ValuePtr, MapProp, Value);
	}

	if (FSetProperty* SetProp = CastField<FSetProperty>(Property))
	{
		return WriteSetProperty(ValuePtr, SetProp, Value);
	}

	// Fallback - try ImportText for unknown types
	const TCHAR* ImportResult = Property->ImportText_Direct(*Value.StringValue, ValuePtr, nullptr, PPF_None);
	return ImportResult != nullptr;
}

//~==============================================================================
// Type Introspection
//~==============================================================================

EAgentPropertyType FPropertyAccessor::GetPropertyType(FProperty* Property)
{
	if (!Property)
	{
		return EAgentPropertyType::None;
	}

	// Boolean
	if (CastField<FBoolProperty>(Property))
	{
		return EAgentPropertyType::Bool;
	}

	// Numeric types - check specific sizes
	if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		if (NumProp->IsFloatingPoint())
		{
			if (CastField<FFloatProperty>(Property))
			{
				return EAgentPropertyType::Float;
			}
			return EAgentPropertyType::Double;
		}
		if (NumProp->IsInteger())
		{
			if (CastField<FInt8Property>(Property)) return EAgentPropertyType::Int8;
			if (CastField<FInt16Property>(Property)) return EAgentPropertyType::Int16;
			if (CastField<FIntProperty>(Property)) return EAgentPropertyType::Int32;
			if (CastField<FInt64Property>(Property)) return EAgentPropertyType::Int64;
			if (CastField<FByteProperty>(Property))
			{
				FByteProperty* ByteProp = CastField<FByteProperty>(Property);
				if (ByteProp->Enum)
				{
					return EAgentPropertyType::Enum;
				}
				return EAgentPropertyType::UInt8;
			}
			if (CastField<FUInt16Property>(Property)) return EAgentPropertyType::UInt16;
			if (CastField<FUInt32Property>(Property)) return EAgentPropertyType::UInt32;
			if (CastField<FUInt64Property>(Property)) return EAgentPropertyType::UInt64;
		}
	}

	// String types
	if (CastField<FStrProperty>(Property)) return EAgentPropertyType::String;
	if (CastField<FNameProperty>(Property)) return EAgentPropertyType::Name;
	if (CastField<FTextProperty>(Property)) return EAgentPropertyType::Text;

	// Enum
	if (CastField<FEnumProperty>(Property)) return EAgentPropertyType::Enum;

	// Object types
	if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
	{
		if (CastField<FSoftObjectProperty>(Property)) return EAgentPropertyType::SoftObject;
		if (CastField<FWeakObjectProperty>(Property)) return EAgentPropertyType::WeakObject;
		if (CastField<FClassProperty>(Property)) return EAgentPropertyType::Class;
		if (CastField<FSoftClassProperty>(Property)) return EAgentPropertyType::Class;
		return EAgentPropertyType::Object;
	}

	// Struct - check for special types
	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		UScriptStruct* Struct = StructProp->Struct;
		if (Struct == TBaseStructure<FVector>::Get()) return EAgentPropertyType::Vector;
		if (Struct == TBaseStructure<FRotator>::Get()) return EAgentPropertyType::Rotator;
		if (Struct == TBaseStructure<FTransform>::Get()) return EAgentPropertyType::Transform;
		if (Struct == TBaseStructure<FColor>::Get()) return EAgentPropertyType::Color;
		if (Struct == TBaseStructure<FLinearColor>::Get()) return EAgentPropertyType::Color;
		return EAgentPropertyType::Struct;
	}

	// Containers
	if (CastField<FArrayProperty>(Property)) return EAgentPropertyType::Array;
	if (CastField<FMapProperty>(Property)) return EAgentPropertyType::Map;
	if (CastField<FSetProperty>(Property)) return EAgentPropertyType::Set;

	return EAgentPropertyType::Unknown;
}

FString FPropertyAccessor::GetPropertyTypeName(FProperty* Property)
{
	if (!Property)
	{
		return TEXT("None");
	}
	return Property->GetCPPType();
}

bool FPropertyAccessor::IsPropertyWritable(FProperty* Property)
{
	if (!Property)
	{
		return false;
	}

	// Check for truly read-only flags
	// NOTE: We intentionally do NOT check CPF_BlueprintReadOnly here!
	// BlueprintReadOnly only prevents Blueprint scripts from writing - C++ and Editor
	// code can still modify these properties. Since AgentBridge acts as editor code
	// (not as a Blueprint script), we should allow writing to BlueprintReadOnly properties.
	//
	// CPF_EditConst is the flag for truly read-only properties (computed/derived values).
	const EPropertyFlags ReadOnlyFlags = CPF_EditConst;

	return !Property->HasAnyPropertyFlags(ReadOnlyFlags);
}

FString FPropertyAccessor::GetPropertyDisplayName(FProperty* Property)
{
	if (!Property)
	{
		return TEXT("");
	}

	// GetAuthoredName strips the GUID suffix from Blueprint properties
	FString DisplayName = Property->GetAuthoredName();
	if (DisplayName.IsEmpty())
	{
		DisplayName = Property->GetName();
	}
	return DisplayName;
}

//~==============================================================================
// Object Reference Handling
//~==============================================================================

FString FPropertyAccessor::SerializeObjectReference(UObject* Object)
{
	if (!Object)
	{
		return TEXT("");
	}

	FSoftObjectPath SoftPath(Object);
	return SoftPath.ToString();
}

UObject* FPropertyAccessor::ResolveObjectReference(const FString& Reference, UClass* ExpectedClass)
{
	if (Reference.IsEmpty())
	{
		return nullptr;
	}

	FSoftObjectPath SoftPath(Reference);
	UObject* Result = SoftPath.ResolveObject();

	// Optionally filter by class
	if (Result && ExpectedClass && !Result->IsA(ExpectedClass))
	{
		return nullptr;
	}

	return Result;
}

//~==============================================================================
// Internal Read Helpers
//~==============================================================================

FAgentPropertyValue FPropertyAccessor::ReadBoolProperty(const void* Container, FBoolProperty* Property)
{
	FAgentPropertyValue Result;
	Result.Type = EAgentPropertyType::Bool;

	const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	bool Value = Property->GetPropertyValue(ValuePtr);
	Result.StringValue = Value ? TEXT("true") : TEXT("false");

	return Result;
}

FAgentPropertyValue FPropertyAccessor::ReadNumericProperty(const void* Container, FNumericProperty* Property)
{
	FAgentPropertyValue Result;
	const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);

	if (Property->IsFloatingPoint())
	{
		Result.Type = CastField<FFloatProperty>(Property) ? EAgentPropertyType::Float : EAgentPropertyType::Double;
		double Value = Property->GetFloatingPointPropertyValue(ValuePtr);
		Result.StringValue = FString::Printf(TEXT("%.17g"), Value);
	}
	else if (Property->IsInteger())
	{
		// Determine signedness and set appropriate type
		if (Property->IsA<FByteProperty>() ||
			Property->IsA<FUInt16Property>() ||
			Property->IsA<FUInt32Property>() ||
			Property->IsA<FUInt64Property>())
		{
			Result.Type = EAgentPropertyType::UInt64;
			uint64 Value = Property->GetUnsignedIntPropertyValue(ValuePtr);
			Result.StringValue = FString::Printf(TEXT("%llu"), Value);
		}
		else
		{
			Result.Type = EAgentPropertyType::Int64;
			int64 Value = Property->GetSignedIntPropertyValue(ValuePtr);
			Result.StringValue = FString::Printf(TEXT("%lld"), Value);
		}
	}

	return Result;
}

FAgentPropertyValue FPropertyAccessor::ReadStringProperty(const void* Container, FProperty* Property)
{
	FAgentPropertyValue Result;
	const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);

	if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		Result.Type = EAgentPropertyType::String;
		Result.StringValue = StrProp->GetPropertyValue(ValuePtr);
	}
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		Result.Type = EAgentPropertyType::Name;
		Result.StringValue = NameProp->GetPropertyValue(ValuePtr).ToString();
	}
	else if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
	{
		Result.Type = EAgentPropertyType::Text;
		Result.StringValue = TextProp->GetPropertyValue(ValuePtr).ToString();
	}

	return Result;
}

FAgentPropertyValue FPropertyAccessor::ReadEnumProperty(const void* Container, FProperty* Property)
{
	FAgentPropertyValue Result;
	Result.Type = EAgentPropertyType::Enum;

	const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	UEnum* Enum = nullptr;
	int64 NumericValue = 0;

	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		Enum = EnumProp->GetEnum();
		FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
		NumericValue = UnderlyingProp->GetSignedIntPropertyValue(
			EnumProp->ContainerPtrToValuePtr<void>(Container));
	}
	else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		Enum = ByteProp->Enum;
		NumericValue = ByteProp->GetPropertyValue(ValuePtr);
	}

	if (Enum)
	{
		FString EnumName = Enum->GetNameStringByValue(NumericValue);
		// Return as "EnumName::ValueName" format
		Result.StringValue = FString::Printf(TEXT("%s::%s"), *Enum->GetName(), *EnumName);
	}
	else
	{
		Result.StringValue = FString::Printf(TEXT("%lld"), NumericValue);
	}

	return Result;
}

FAgentPropertyValue FPropertyAccessor::ReadObjectProperty(const void* Container, FObjectPropertyBase* Property)
{
	FAgentPropertyValue Result;

	// Determine specific object type
	if (CastField<FSoftObjectProperty>(Property))
	{
		Result.Type = EAgentPropertyType::SoftObject;
	}
	else if (CastField<FWeakObjectProperty>(Property))
	{
		Result.Type = EAgentPropertyType::WeakObject;
	}
	else if (CastField<FClassProperty>(Property) || CastField<FSoftClassProperty>(Property))
	{
		Result.Type = EAgentPropertyType::Class;
	}
	else
	{
		Result.Type = EAgentPropertyType::Object;
	}

	// Get the object and serialize its path
	UObject* Object = Property->GetObjectPropertyValue(Property->ContainerPtrToValuePtr<void>(Container));
	Result.StringValue = SerializeObjectReference(Object);

	// Also store class info in the struct value for additional context
	if (Object)
	{
		Result.StructValue.Add(TEXT("_class"), MakeShared<FAgentPropertyValue>(
			FAgentPropertyValue::FromString(Object->GetClass()->GetName())));
		Result.StructValue.Add(TEXT("_name"), MakeShared<FAgentPropertyValue>(
			FAgentPropertyValue::FromString(Object->GetName())));
	}

	return Result;
}

FAgentPropertyValue FPropertyAccessor::ReadStructProperty(
	const void* Container,
	FStructProperty* Property,
	int32 Depth,
	int32 MaxDepth)
{
	const void* StructPtr = Property->ContainerPtrToValuePtr<void>(Container);

	// Try special handling for common types first
	FAgentPropertyValue Result;
	if (TryReadSpecialStruct(Property, StructPtr, Result))
	{
		return Result;
	}

	// Generic struct handling
	Result.Type = EAgentPropertyType::Struct;

	if (Depth >= MaxDepth)
	{
		Result.StringValue = TEXT("{...}");
		return Result;
	}

	// Iterate all properties in the struct
	UScriptStruct* Struct = Property->Struct;
	for (TFieldIterator<FProperty> PropIt(Struct); PropIt; ++PropIt)
	{
		FProperty* MemberProp = *PropIt;
		FString MemberName = GetPropertyDisplayName(MemberProp);

		// Recursively read the member (note: StructPtr is the container for members)
		FAgentPropertyValue MemberValue = ReadProperty(StructPtr, MemberProp, MaxDepth - Depth);
		Result.StructValue.Add(MemberName, MakeShared<FAgentPropertyValue>(MoveTemp(MemberValue)));
	}

	return Result;
}

FAgentPropertyValue FPropertyAccessor::ReadArrayProperty(
	const void* Container,
	FArrayProperty* Property,
	int32 Depth,
	int32 MaxDepth)
{
	FAgentPropertyValue Result;
	Result.Type = EAgentPropertyType::Array;

	if (Depth >= MaxDepth)
	{
		Result.StringValue = TEXT("[...]");
		return Result;
	}

	// Get the array helper
	const void* ArrayPtr = Property->ContainerPtrToValuePtr<void>(Container);
	FScriptArrayHelper ArrayHelper(Property, ArrayPtr);

	// Read each element
	FProperty* InnerProp = Property->Inner;
	for (int32 i = 0; i < ArrayHelper.Num(); i++)
	{
		// For array elements, the element pointer IS the container for the inner property
		void* ElementPtr = ArrayHelper.GetRawPtr(i);

		// Create a temporary value and read directly into it
		FAgentPropertyValue ElementValue;
		ElementValue.Type = GetPropertyType(InnerProp);

		// Read the element - pass ElementPtr as container, but we need to handle this specially
		// since the element IS the value, not a container with the value
		const void* ElementValuePtr = ElementPtr;

		// Dispatch based on inner type
		if (FBoolProperty* BoolProp = CastField<FBoolProperty>(InnerProp))
		{
			ElementValue.Type = EAgentPropertyType::Bool;
			ElementValue.StringValue = BoolProp->GetPropertyValue(ElementValuePtr) ? TEXT("true") : TEXT("false");
		}
		else if (FNumericProperty* NumProp = CastField<FNumericProperty>(InnerProp))
		{
			if (NumProp->IsFloatingPoint())
			{
				ElementValue.Type = EAgentPropertyType::Double;
				ElementValue.StringValue = FString::Printf(TEXT("%.17g"), NumProp->GetFloatingPointPropertyValue(ElementValuePtr));
			}
			else
			{
				ElementValue.Type = EAgentPropertyType::Int64;
				ElementValue.StringValue = FString::Printf(TEXT("%lld"), NumProp->GetSignedIntPropertyValue(ElementValuePtr));
			}
		}
		else if (FStrProperty* StrProp = CastField<FStrProperty>(InnerProp))
		{
			ElementValue.Type = EAgentPropertyType::String;
			ElementValue.StringValue = StrProp->GetPropertyValue(ElementValuePtr);
		}
		else if (FStructProperty* StructProp = CastField<FStructProperty>(InnerProp))
		{
			// For structs, ElementPtr points directly to the struct data
			if (!TryReadSpecialStruct(StructProp, ElementValuePtr, ElementValue))
			{
				ElementValue.Type = EAgentPropertyType::Struct;
				if (Depth + 1 < MaxDepth)
				{
					UScriptStruct* Struct = StructProp->Struct;
					for (TFieldIterator<FProperty> PropIt(Struct); PropIt; ++PropIt)
					{
						FProperty* MemberProp = *PropIt;
						FString MemberName = GetPropertyDisplayName(MemberProp);
						FAgentPropertyValue MemberValue = ReadProperty(ElementValuePtr, MemberProp, MaxDepth - Depth - 1);
						ElementValue.StructValue.Add(MemberName, MakeShared<FAgentPropertyValue>(MoveTemp(MemberValue)));
					}
				}
			}
		}
		else if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(InnerProp))
		{
			UObject* Obj = ObjProp->GetObjectPropertyValue(ElementValuePtr);
			ElementValue.Type = EAgentPropertyType::Object;
			ElementValue.StringValue = SerializeObjectReference(Obj);
		}
		else
		{
			// Fallback
			ElementValue.Type = EAgentPropertyType::Unknown;
			InnerProp->ExportTextItem_Direct(ElementValue.StringValue, ElementValuePtr, nullptr, nullptr, PPF_None);
		}

		Result.ArrayValue.Add(MakeShared<FAgentPropertyValue>(MoveTemp(ElementValue)));
	}

	return Result;
}

FAgentPropertyValue FPropertyAccessor::ReadMapProperty(
	const void* Container,
	FMapProperty* Property,
	int32 Depth,
	int32 MaxDepth)
{
	FAgentPropertyValue Result;
	Result.Type = EAgentPropertyType::Map;

	if (Depth >= MaxDepth)
	{
		Result.StringValue = TEXT("{...}");
		return Result;
	}

	const void* MapPtr = Property->ContainerPtrToValuePtr<void>(Container);
	FScriptMapHelper MapHelper(Property, MapPtr);

	// Iterate valid indices (maps can have gaps)
	for (int32 i = 0; i < MapHelper.GetMaxIndex(); i++)
	{
		if (!MapHelper.IsValidIndex(i))
		{
			continue;
		}

		// Get key and value pointers
		const uint8* KeyPtr = MapHelper.GetKeyPtr(i);
		const uint8* ValuePtr = MapHelper.GetValuePtr(i);

		// Read key as string for the map key
		FString KeyString;
		Property->KeyProp->ExportTextItem_Direct(KeyString, KeyPtr, nullptr, nullptr, PPF_None);

		// Read value recursively
		FAgentPropertyValue ValueResult;
		FProperty* ValueProp = Property->ValueProp;

		// Similar dispatch as array elements
		if (FStructProperty* StructProp = CastField<FStructProperty>(ValueProp))
		{
			if (!TryReadSpecialStruct(StructProp, ValuePtr, ValueResult))
			{
				ValueResult.Type = EAgentPropertyType::Struct;
				if (Depth + 1 < MaxDepth)
				{
					for (TFieldIterator<FProperty> PropIt(StructProp->Struct); PropIt; ++PropIt)
					{
						FProperty* MemberProp = *PropIt;
						FString MemberName = GetPropertyDisplayName(MemberProp);
						FAgentPropertyValue MemberValue = ReadProperty(ValuePtr, MemberProp, MaxDepth - Depth - 1);
						ValueResult.StructValue.Add(MemberName, MakeShared<FAgentPropertyValue>(MoveTemp(MemberValue)));
					}
				}
			}
		}
		else
		{
			// Use ExportText for simple types
			ValueResult.Type = GetPropertyType(ValueProp);
			ValueProp->ExportTextItem_Direct(ValueResult.StringValue, ValuePtr, nullptr, nullptr, PPF_None);
		}

		Result.StructValue.Add(KeyString, MakeShared<FAgentPropertyValue>(MoveTemp(ValueResult)));
	}

	return Result;
}

FAgentPropertyValue FPropertyAccessor::ReadSetProperty(
	const void* Container,
	FSetProperty* Property,
	int32 Depth,
	int32 MaxDepth)
{
	FAgentPropertyValue Result;
	Result.Type = EAgentPropertyType::Set;

	if (Depth >= MaxDepth)
	{
		Result.StringValue = TEXT("{...}");
		return Result;
	}

	const void* SetPtr = Property->ContainerPtrToValuePtr<void>(Container);
	FScriptSetHelper SetHelper(Property, SetPtr);

	for (int32 i = 0; i < SetHelper.GetMaxIndex(); i++)
	{
		if (!SetHelper.IsValidIndex(i))
		{
			continue;
		}

		const uint8* ElementPtr = SetHelper.GetElementPtr(i);
		FProperty* ElementProp = Property->ElementProp;

		FAgentPropertyValue ElementValue;
		ElementValue.Type = GetPropertyType(ElementProp);
		ElementProp->ExportTextItem_Direct(ElementValue.StringValue, ElementPtr, nullptr, nullptr, PPF_None);

		Result.ArrayValue.Add(MakeShared<FAgentPropertyValue>(MoveTemp(ElementValue)));
	}

	return Result;
}

//~==============================================================================
// Internal Write Helpers
// NOTE: All these helpers now receive the direct ValuePtr, NOT a container.
// They must NOT call ContainerPtrToValuePtr internally.
//~==============================================================================

bool FPropertyAccessor::WriteBoolProperty(void* ValuePtr, FBoolProperty* Property, const FAgentPropertyValue& Value)
{
	// ValuePtr is already the direct pointer to the bool value
	bool BoolValue = Value.AsBool();
	Property->SetPropertyValue(ValuePtr, BoolValue);
	return true;
}

bool FPropertyAccessor::WriteNumericProperty(void* ValuePtr, FNumericProperty* Property, const FAgentPropertyValue& Value)
{
	// ValuePtr is already the direct pointer to the numeric value
	if (Property->IsFloatingPoint())
	{
		double DoubleValue = Value.AsFloat();
		Property->SetFloatingPointPropertyValue(ValuePtr, DoubleValue);
	}
	else if (Property->IsInteger())
	{
		int64 IntValue = Value.AsInt();
		Property->SetIntPropertyValue(ValuePtr, IntValue);
	}
	else
	{
		return false;
	}

	return true;
}

bool FPropertyAccessor::WriteStringProperty(void* ValuePtr, FProperty* Property, const FAgentPropertyValue& Value)
{
	// ValuePtr is already the direct pointer to the string value
	if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		StrProp->SetPropertyValue(ValuePtr, Value.StringValue);
		return true;
	}
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		NameProp->SetPropertyValue(ValuePtr, FName(*Value.StringValue));
		return true;
	}
	else if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
	{
		TextProp->SetPropertyValue(ValuePtr, FText::FromString(Value.StringValue));
		return true;
	}

	return false;
}

bool FPropertyAccessor::WriteEnumProperty(void* ValuePtr, FProperty* Property, const FAgentPropertyValue& Value)
{
	// ValuePtr is already the direct pointer to the enum value
	UEnum* Enum = nullptr;

	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		Enum = EnumProp->GetEnum();
	}
	else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		Enum = ByteProp->Enum;
	}

	if (!Enum)
	{
		return false;
	}

	// Try to parse the value - could be "EnumName::Value", "Value", or a number
	FString ValueStr = Value.StringValue;
	int64 NumericValue = INDEX_NONE;

	// Remove enum name prefix if present
	int32 ColonPos;
	if (ValueStr.FindLastChar(':', ColonPos))
	{
		ValueStr = ValueStr.Mid(ColonPos + 1);
	}

	// Try name lookup first
	NumericValue = Enum->GetValueByNameString(ValueStr);

	// If not found, try as numeric
	if (NumericValue == INDEX_NONE)
	{
		NumericValue = FCString::Atoi64(*ValueStr);
	}

	// Write the value
	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, NumericValue);
	}
	else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		ByteProp->SetPropertyValue(ValuePtr, static_cast<uint8>(NumericValue));
	}

	return true;
}

bool FPropertyAccessor::WriteObjectProperty(void* ValuePtr, FObjectPropertyBase* Property, const FAgentPropertyValue& Value)
{
	// ValuePtr is already the direct pointer to the object reference

	// Handle FSoftObjectProperty specially - it stores a path, not a loaded object
	if (FSoftObjectProperty* SoftProp = CastField<FSoftObjectProperty>(Property))
	{
		FSoftObjectPath SoftPath(Value.StringValue);
		FSoftObjectPtr SoftPtr(SoftPath);
		SoftProp->SetPropertyValue(ValuePtr, SoftPtr);
		return true;
	}

	// Handle FSoftClassProperty similarly
	if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
	{
		FSoftObjectPath SoftPath(Value.StringValue);
		FSoftObjectPtr SoftPtr(SoftPath);
		SoftClassProp->SetPropertyValue(ValuePtr, SoftPtr);
		return true;
	}

	// For regular object properties (TObjectPtr, raw pointers), resolve and load the object
	FSoftObjectPath SoftPath(Value.StringValue);
	UObject* Object = nullptr;

	if (!Value.StringValue.IsEmpty())
	{
		// Try to resolve (already loaded)
		Object = SoftPath.ResolveObject();

		// If not loaded, try to load it
		if (!Object)
		{
			Object = SoftPath.TryLoad();
		}

		// If still not found, check asset registry and load
		if (!Object)
		{
			Object = ResolveObjectReference(Value.StringValue, Property->PropertyClass);
		}

		// If we couldn't find the object, fail
		if (!Object)
		{
			return false;
		}

		// Type check
		if (Property->PropertyClass && !Object->IsA(Property->PropertyClass))
		{
			return false;
		}
	}

	Property->SetObjectPropertyValue(ValuePtr, Object);
	return true;
}

bool FPropertyAccessor::WriteStructProperty(void* ValuePtr, FStructProperty* Property, const FAgentPropertyValue& Value)
{
	// ValuePtr is already the direct pointer to the struct data (StructPtr)
	void* StructPtr = ValuePtr;

	// Try special types first
	if (TryWriteSpecialStruct(Property, StructPtr, Value))
	{
		return true;
	}

	// Generic struct writing - set each member from StructValue map
	if (Value.Type != EAgentPropertyType::Struct || Value.StructValue.Num() == 0)
	{
		return false;
	}

	UScriptStruct* Struct = Property->Struct;
	bool bAllSuccess = true;

	for (const auto& Pair : Value.StructValue)
	{
		// Find the property by display name
		FProperty* MemberProp = nullptr;
		for (TFieldIterator<FProperty> PropIt(Struct); PropIt; ++PropIt)
		{
			if (GetPropertyDisplayName(*PropIt) == Pair.Key)
			{
				MemberProp = *PropIt;
				break;
			}
		}

		if (MemberProp && Pair.Value.IsValid())
		{
			// For struct members, StructPtr IS the container, so use WriteProperty (not WritePropertyDirect)
			if (!WriteProperty(StructPtr, MemberProp, *Pair.Value))
			{
				bAllSuccess = false;
			}
		}
	}

	return bAllSuccess;
}

bool FPropertyAccessor::WriteArrayProperty(void* ValuePtr, FArrayProperty* Property, const FAgentPropertyValue& Value)
{
	if (Value.Type != EAgentPropertyType::Array)
	{
		return false;
	}

	// ValuePtr is already the direct pointer to the array data
	void* ArrayPtr = ValuePtr;
	FScriptArrayHelper ArrayHelper(Property, ArrayPtr);

	// Resize array to match input
	ArrayHelper.Resize(Value.ArrayValue.Num());

	FProperty* InnerProp = Property->Inner;
	bool bAllSuccess = true;

	for (int32 i = 0; i < Value.ArrayValue.Num(); i++)
	{
		if (!Value.ArrayValue[i].IsValid())
		{
			continue;
		}

		void* ElementPtr = ArrayHelper.GetRawPtr(i);
		const FAgentPropertyValue& ElementValue = *Value.ArrayValue[i];

		// Dispatch to type-specific writers for proper handling
		// Following "tools should just work" philosophy: object references need proper resolution
		bool bElementSuccess = false;

		if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(InnerProp))
		{
			// Object properties need our WriteObjectProperty which loads/resolves objects
			bElementSuccess = WriteObjectProperty(ElementPtr, ObjProp, ElementValue);
		}
		else if (FStructProperty* StructProp = CastField<FStructProperty>(InnerProp))
		{
			// Structs need WriteStructProperty for proper member handling
			bElementSuccess = WriteStructProperty(ElementPtr, StructProp, ElementValue);
		}
		else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(InnerProp))
		{
			bElementSuccess = WriteBoolProperty(ElementPtr, BoolProp, ElementValue);
		}
		else if (FNumericProperty* NumProp = CastField<FNumericProperty>(InnerProp))
		{
			bElementSuccess = WriteNumericProperty(ElementPtr, NumProp, ElementValue);
		}
		else if (CastField<FStrProperty>(InnerProp) ||
				 CastField<FNameProperty>(InnerProp) ||
				 CastField<FTextProperty>(InnerProp))
		{
			bElementSuccess = WriteStringProperty(ElementPtr, InnerProp, ElementValue);
		}
		else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(InnerProp))
		{
			bElementSuccess = WriteEnumProperty(ElementPtr, InnerProp, ElementValue);
		}
		else if (FByteProperty* ByteProp = CastField<FByteProperty>(InnerProp))
		{
			if (ByteProp->Enum)
			{
				bElementSuccess = WriteEnumProperty(ElementPtr, InnerProp, ElementValue);
			}
			else
			{
				bElementSuccess = WriteNumericProperty(ElementPtr, ByteProp, ElementValue);
			}
		}
		else if (FArrayProperty* NestedArrayProp = CastField<FArrayProperty>(InnerProp))
		{
			bElementSuccess = WriteArrayProperty(ElementPtr, NestedArrayProp, ElementValue);
		}
		else
		{
			// Fallback to ImportText for unknown types
			const TCHAR* Result = InnerProp->ImportText_Direct(*ElementValue.StringValue, ElementPtr, nullptr, PPF_None);
			bElementSuccess = (Result != nullptr);
		}

		if (!bElementSuccess)
		{
			bAllSuccess = false;
		}
	}

	return bAllSuccess;
}

bool FPropertyAccessor::WriteMapProperty(void* ValuePtr, FMapProperty* Property, const FAgentPropertyValue& Value)
{
	if (Value.Type != EAgentPropertyType::Map && Value.StructValue.Num() == 0)
	{
		return false;
	}

	// ValuePtr is already the direct pointer to the map data
	void* MapPtr = ValuePtr;
	FScriptMapHelper MapHelper(Property, MapPtr);

	// Clear existing entries
	MapHelper.EmptyValues();

	bool bAllSuccess = true;

	for (const auto& Pair : Value.StructValue)
	{
		if (!Pair.Value.IsValid())
		{
			continue;
		}

		// Add a new entry
		int32 Index = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
		uint8* KeyPtr = MapHelper.GetKeyPtr(Index);
		uint8* MapValuePtr = MapHelper.GetValuePtr(Index);

		// Set key
		const TCHAR* KeyResult = Property->KeyProp->ImportText_Direct(*Pair.Key, KeyPtr, nullptr, PPF_None);
		if (!KeyResult)
		{
			bAllSuccess = false;
		}

		// Set value
		const TCHAR* ValueResult = Property->ValueProp->ImportText_Direct(*Pair.Value->StringValue, MapValuePtr, nullptr, PPF_None);
		if (!ValueResult)
		{
			bAllSuccess = false;
		}
	}

	MapHelper.Rehash();
	return bAllSuccess;
}

bool FPropertyAccessor::WriteSetProperty(void* ValuePtr, FSetProperty* Property, const FAgentPropertyValue& Value)
{
	if (Value.Type != EAgentPropertyType::Set && Value.ArrayValue.Num() == 0)
	{
		return false;
	}

	// ValuePtr is already the direct pointer to the set data
	void* SetPtr = ValuePtr;
	FScriptSetHelper SetHelper(Property, SetPtr);

	// Clear existing entries
	SetHelper.EmptyElements();

	bool bAllSuccess = true;

	for (const TSharedPtr<FAgentPropertyValue>& ElementValue : Value.ArrayValue)
	{
		if (!ElementValue.IsValid())
		{
			continue;
		}

		int32 Index = SetHelper.AddDefaultValue_Invalid_NeedsRehash();
		uint8* ElementPtr = SetHelper.GetElementPtr(Index);

		const TCHAR* Result = Property->ElementProp->ImportText_Direct(*ElementValue->StringValue, ElementPtr, nullptr, PPF_None);
		if (!Result)
		{
			bAllSuccess = false;
		}
	}

	SetHelper.Rehash();
	return bAllSuccess;
}

//~==============================================================================
// Special Struct Handling
//~==============================================================================

bool FPropertyAccessor::TryReadSpecialStruct(FStructProperty* StructProperty, const void* ValuePtr, FAgentPropertyValue& OutValue)
{
	UScriptStruct* Struct = StructProperty->Struct;

	// FVector
	if (Struct == TBaseStructure<FVector>::Get())
	{
		const FVector* Vec = static_cast<const FVector*>(ValuePtr);
		OutValue.Type = EAgentPropertyType::Vector;
		OutValue.StringValue = Vec->ToString();
		return true;
	}

	// FRotator
	if (Struct == TBaseStructure<FRotator>::Get())
	{
		const FRotator* Rot = static_cast<const FRotator*>(ValuePtr);
		OutValue.Type = EAgentPropertyType::Rotator;
		OutValue.StringValue = Rot->ToString();
		return true;
	}

	// FTransform
	if (Struct == TBaseStructure<FTransform>::Get())
	{
		const FTransform* Trans = static_cast<const FTransform*>(ValuePtr);
		OutValue.Type = EAgentPropertyType::Transform;
		OutValue.StringValue = Trans->ToString();
		return true;
	}

	// FColor
	if (Struct == TBaseStructure<FColor>::Get())
	{
		const FColor* Col = static_cast<const FColor*>(ValuePtr);
		OutValue.Type = EAgentPropertyType::Color;
		OutValue.StringValue = Col->ToString();
		return true;
	}

	// FLinearColor
	if (Struct == TBaseStructure<FLinearColor>::Get())
	{
		const FLinearColor* Col = static_cast<const FLinearColor*>(ValuePtr);
		OutValue.Type = EAgentPropertyType::Color;
		OutValue.StringValue = Col->ToString();
		return true;
	}

	return false;
}

bool FPropertyAccessor::TryWriteSpecialStruct(FStructProperty* StructProperty, void* ValuePtr, const FAgentPropertyValue& Value)
{
	UScriptStruct* Struct = StructProperty->Struct;

	// FVector
	if (Struct == TBaseStructure<FVector>::Get())
	{
		FVector* Vec = static_cast<FVector*>(ValuePtr);
		Vec->InitFromString(Value.StringValue);
		return true;
	}

	// FRotator
	if (Struct == TBaseStructure<FRotator>::Get())
	{
		FRotator* Rot = static_cast<FRotator*>(ValuePtr);
		Rot->InitFromString(Value.StringValue);
		return true;
	}

	// FTransform
	if (Struct == TBaseStructure<FTransform>::Get())
	{
		FTransform* Trans = static_cast<FTransform*>(ValuePtr);
		Trans->InitFromString(Value.StringValue);
		return true;
	}

	// FColor
	if (Struct == TBaseStructure<FColor>::Get())
	{
		FColor* Col = static_cast<FColor*>(ValuePtr);
		Col->InitFromString(Value.StringValue);
		return true;
	}

	// FLinearColor
	if (Struct == TBaseStructure<FLinearColor>::Get())
	{
		FLinearColor* Col = static_cast<FLinearColor*>(ValuePtr);
		Col->InitFromString(Value.StringValue);
		return true;
	}

	return false;
}
