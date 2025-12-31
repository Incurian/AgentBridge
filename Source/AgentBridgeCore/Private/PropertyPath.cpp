#include "PropertyPath.h"
#include "PropertyAccessor.h"
#include "TypeDiscovery.h"
#include "UObject/UnrealType.h"

//~==============================================================================
// Path Parsing
//~==============================================================================

TArray<FPropertyPathSegment> FPropertyPath::ParsePath(const FString& PathString)
{
	TArray<FPropertyPathSegment> Segments;

	if (PathString.IsEmpty())
	{
		return Segments;
	}

	int32 Position = 0;
	const int32 Length = PathString.Len();

	while (Position < Length)
	{
		// Skip leading dots
		if (PathString[Position] == TEXT('.'))
		{
			Position++;
			continue;
		}

		// Check for array/map index: [...]
		if (PathString[Position] == TEXT('['))
		{
			Position++; // Skip '['

			// Check if it's a string key (map) or numeric index (array)
			if (Position < Length && PathString[Position] == TEXT('"'))
			{
				// Map key: ["KeyName"]
				Position++; // Skip opening quote

				FString Key;
				while (Position < Length && PathString[Position] != TEXT('"'))
				{
					// Handle escaped quotes
					if (PathString[Position] == TEXT('\\') && Position + 1 < Length)
					{
						Position++;
					}
					Key.AppendChar(PathString[Position]);
					Position++;
				}

				if (Position < Length && PathString[Position] == TEXT('"'))
				{
					Position++; // Skip closing quote
				}

				if (Position < Length && PathString[Position] == TEXT(']'))
				{
					Position++; // Skip ']'
				}

				Segments.Add(FPropertyPathSegment::MapKey(Key));
			}
			else
			{
				// Array index: [0]
				FString IndexStr;
				while (Position < Length && PathString[Position] != TEXT(']'))
				{
					IndexStr.AppendChar(PathString[Position]);
					Position++;
				}

				if (Position < Length && PathString[Position] == TEXT(']'))
				{
					Position++; // Skip ']'
				}

				int32 Index = FCString::Atoi(*IndexStr);
				Segments.Add(FPropertyPathSegment::ArrayIndex(Index));
			}
		}
		else
		{
			// Property name: continues until '.', '[', or end
			FString PropertyName;
			while (Position < Length &&
				PathString[Position] != TEXT('.') &&
				PathString[Position] != TEXT('['))
			{
				PropertyName.AppendChar(PathString[Position]);
				Position++;
			}

			if (!PropertyName.IsEmpty())
			{
				Segments.Add(FPropertyPathSegment::Property(PropertyName));
			}
		}
	}

	return Segments;
}

FString FPropertyPath::SegmentsToString(const TArray<FPropertyPathSegment>& Segments)
{
	FString Result;

	for (int32 i = 0; i < Segments.Num(); i++)
	{
		const FPropertyPathSegment& Seg = Segments[i];

		// Add dot separator before property segments (not first, not after array/map)
		if (Seg.Type == EPropertyPathSegmentType::Property && i > 0)
		{
			const FPropertyPathSegment& Prev = Segments[i - 1];
			if (Prev.Type == EPropertyPathSegmentType::Property)
			{
				Result += TEXT(".");
			}
		}

		Result += Seg.ToString();
	}

	return Result;
}

bool FPropertyPath::ValidatePath(const FString& PathString, FString* OutError)
{
	if (PathString.IsEmpty())
	{
		if (OutError) *OutError = TEXT("Path is empty");
		return false;
	}

	int32 BracketDepth = 0;
	bool bInQuote = false;
	bool bEscaped = false;

	for (int32 i = 0; i < PathString.Len(); i++)
	{
		TCHAR C = PathString[i];

		if (bEscaped)
		{
			bEscaped = false;
			continue;
		}

		if (C == TEXT('\\'))
		{
			bEscaped = true;
			continue;
		}

		if (C == TEXT('"'))
		{
			bInQuote = !bInQuote;
			continue;
		}

		if (bInQuote)
		{
			continue;
		}

		if (C == TEXT('['))
		{
			BracketDepth++;
			if (BracketDepth > 1)
			{
				if (OutError) *OutError = FString::Printf(TEXT("Nested brackets at position %d"), i);
				return false;
			}
		}
		else if (C == TEXT(']'))
		{
			BracketDepth--;
			if (BracketDepth < 0)
			{
				if (OutError) *OutError = FString::Printf(TEXT("Unmatched ']' at position %d"), i);
				return false;
			}
		}
	}

	if (bInQuote)
	{
		if (OutError) *OutError = TEXT("Unclosed quote");
		return false;
	}

	if (BracketDepth != 0)
	{
		if (OutError) *OutError = TEXT("Unclosed bracket");
		return false;
	}

	return true;
}

//~==============================================================================
// Path Resolution (Read)
//~==============================================================================

FPropertyPathResult FPropertyPath::GetValue(UObject* Object, const FString& PathString)
{
	FPropertyPathResult Result;

	if (!Object)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Object is null");
		return Result;
	}

	TArray<FPropertyPathSegment> Segments = ParsePath(PathString);
	if (Segments.Num() == 0)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Path is empty or invalid");
		return Result;
	}

	return GetValue(Object, Segments);
}

FPropertyPathResult FPropertyPath::GetValue(UObject* Object, const TArray<FPropertyPathSegment>& Segments)
{
	FPropertyPathResult Result;

	if (!Object)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Object is null");
		return Result;
	}

	if (Segments.Num() == 0)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("No segments provided");
		return Result;
	}

	// First segment must be a property
	if (Segments[0].Type != EPropertyPathSegmentType::Property)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Path must start with a property name");
		return Result;
	}

	// Find the first property
	FProperty* FirstProp = FindPropertyByName(Object->GetClass(), Segments[0].Name);
	if (!FirstProp)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = FString::Printf(TEXT("Property '%s' not found"), *Segments[0].Name);
		return Result;
	}

	// Resolve remaining segments
	return ResolveSegments(Object, FirstProp, Segments, 1, false);
}

//~==============================================================================
// Path Resolution (Write)
//~==============================================================================

bool FPropertyPath::SetValue(UObject* Object, const FString& PathString, const FAgentPropertyValue& Value)
{
	TArray<FPropertyPathSegment> Segments = ParsePath(PathString);
	return SetValue(Object, Segments, Value);
}

bool FPropertyPath::SetValue(
	UObject* Object,
	const TArray<FPropertyPathSegment>& Segments,
	const FAgentPropertyValue& Value)
{
	if (!Object)
	{
		return false;
	}

	if (Segments.Num() == 0)
	{
		return false;
	}

	// First segment must be a property
	if (Segments[0].Type != EPropertyPathSegmentType::Property)
	{
		return false;
	}

	// Find the first property
	FProperty* FirstProp = FindPropertyByName(Object->GetClass(), Segments[0].Name);
	if (!FirstProp)
	{
		return false;
	}

	// If this is the only segment, write directly
	if (Segments.Num() == 1)
	{
		return FPropertyAccessor::WriteProperty(Object, FirstProp, Value);
	}

	// Resolve to get the container and final property
	FPropertyPathResult Resolution = ResolveSegments(Object, FirstProp, Segments, 1, true);

	if (!Resolution.bSuccess || !Resolution.ValuePtr || !Resolution.FinalProperty)
	{
		return false;
	}

	// Write the value to the resolved location
	return FPropertyAccessor::WriteProperty(Resolution.ValuePtr, Resolution.FinalProperty, Value);
}

//~==============================================================================
// Path Existence
//~==============================================================================

bool FPropertyPath::PathExists(UObject* Object, const FString& PathString)
{
	FPropertyPathResult Result = GetValue(Object, PathString);
	return Result.bSuccess;
}

EAgentPropertyType FPropertyPath::GetPathType(UClass* Class, const FString& PathString)
{
	if (!Class)
	{
		return EAgentPropertyType::None;
	}

	TArray<FPropertyPathSegment> Segments = ParsePath(PathString);
	if (Segments.Num() == 0)
	{
		return EAgentPropertyType::None;
	}

	// Navigate through the type hierarchy
	UStruct* CurrentStruct = Class;
	FProperty* CurrentProp = nullptr;

	for (const FPropertyPathSegment& Seg : Segments)
	{
		switch (Seg.Type)
		{
		case EPropertyPathSegmentType::Property:
		{
			if (!CurrentStruct)
			{
				return EAgentPropertyType::None;
			}

			CurrentProp = FindPropertyByName(CurrentStruct, Seg.Name);
			if (!CurrentProp)
			{
				return EAgentPropertyType::None;
			}

			// Update CurrentStruct if this is a struct property
			if (FStructProperty* StructProp = CastField<FStructProperty>(CurrentProp))
			{
				CurrentStruct = StructProp->Struct;
			}
			else if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(CurrentProp))
			{
				CurrentStruct = ObjProp->PropertyClass;
			}
			else
			{
				CurrentStruct = nullptr;
			}
			break;
		}

		case EPropertyPathSegmentType::ArrayIndex:
		{
			if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(CurrentProp))
			{
				CurrentProp = ArrayProp->Inner;

				if (FStructProperty* InnerStruct = CastField<FStructProperty>(CurrentProp))
				{
					CurrentStruct = InnerStruct->Struct;
				}
				else if (FObjectPropertyBase* InnerObj = CastField<FObjectPropertyBase>(CurrentProp))
				{
					CurrentStruct = InnerObj->PropertyClass;
				}
				else
				{
					CurrentStruct = nullptr;
				}
			}
			else
			{
				return EAgentPropertyType::None;
			}
			break;
		}

		case EPropertyPathSegmentType::MapKey:
		{
			if (FMapProperty* MapProp = CastField<FMapProperty>(CurrentProp))
			{
				CurrentProp = MapProp->ValueProp;

				if (FStructProperty* ValueStruct = CastField<FStructProperty>(CurrentProp))
				{
					CurrentStruct = ValueStruct->Struct;
				}
				else if (FObjectPropertyBase* ValueObj = CastField<FObjectPropertyBase>(CurrentProp))
				{
					CurrentStruct = ValueObj->PropertyClass;
				}
				else
				{
					CurrentStruct = nullptr;
				}
			}
			else
			{
				return EAgentPropertyType::None;
			}
			break;
		}

		default:
			return EAgentPropertyType::None;
		}
	}

	if (!CurrentProp)
	{
		return EAgentPropertyType::None;
	}

	return FPropertyAccessor::GetPropertyType(CurrentProp);
}

//~==============================================================================
// Internal Helpers
//~==============================================================================

FPropertyPathResult FPropertyPath::ResolveSegments(
	void* Container,
	FProperty* Property,
	const TArray<FPropertyPathSegment>& Segments,
	int32 SegmentIndex,
	bool bForWrite)
{
	FPropertyPathResult Result;

	if (!Container || !Property)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Invalid container or property");
		return Result;
	}

	// Get the current value pointer
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);

	// If we've processed all segments, return the current value
	if (SegmentIndex >= Segments.Num())
	{
		if (bForWrite)
		{
			Result.bSuccess = true;
			Result.ContainerPtr = Container;
			Result.FinalProperty = Property;
			Result.ValuePtr = ValuePtr;
		}
		else
		{
			Result.Value = FPropertyAccessor::ReadProperty(Container, Property);
			Result.bSuccess = true;
		}
		return Result;
	}

	const FPropertyPathSegment& Seg = Segments[SegmentIndex];

	switch (Seg.Type)
	{
	case EPropertyPathSegmentType::Property:
	{
		// Navigate into a struct or object
		if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			// Find the nested property
			FProperty* NestedProp = FindPropertyByName(StructProp->Struct, Seg.Name);
			if (!NestedProp)
			{
				Result.bSuccess = false;
				Result.ErrorMessage = FString::Printf(TEXT("Property '%s' not found in struct '%s'"),
					*Seg.Name, *StructProp->Struct->GetName());
				return Result;
			}

			return ResolveSegments(ValuePtr, NestedProp, Segments, SegmentIndex + 1, bForWrite);
		}
		else if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
		{
			UObject* NestedObj = ObjProp->GetObjectPropertyValue(ValuePtr);
			if (!NestedObj)
			{
				Result.bSuccess = false;
				Result.ErrorMessage = FString::Printf(TEXT("Object property '%s' is null"),
					*Property->GetName());
				return Result;
			}

			FProperty* NestedProp = FindPropertyByName(NestedObj->GetClass(), Seg.Name);
			if (!NestedProp)
			{
				Result.bSuccess = false;
				Result.ErrorMessage = FString::Printf(TEXT("Property '%s' not found in object of class '%s'"),
					*Seg.Name, *NestedObj->GetClass()->GetName());
				return Result;
			}

			return ResolveSegments(NestedObj, NestedProp, Segments, SegmentIndex + 1, bForWrite);
		}
		else
		{
			Result.bSuccess = false;
			Result.ErrorMessage = FString::Printf(TEXT("Cannot navigate into property '%s' (not a struct or object)"),
				*Property->GetName());
			return Result;
		}
	}

	case EPropertyPathSegmentType::ArrayIndex:
	{
		if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);

			if (Seg.Index < 0 || Seg.Index >= ArrayHelper.Num())
			{
				Result.bSuccess = false;
				Result.ErrorMessage = FString::Printf(TEXT("Array index %d out of bounds (size: %d)"),
					Seg.Index, ArrayHelper.Num());
				return Result;
			}

			void* ElementPtr = ArrayHelper.GetRawPtr(Seg.Index);

			// If this is the last segment
			if (SegmentIndex + 1 >= Segments.Num())
			{
				if (bForWrite)
				{
					Result.bSuccess = true;
					Result.ContainerPtr = ValuePtr;
					Result.FinalProperty = ArrayProp->Inner;
					Result.ValuePtr = ElementPtr;
				}
				else
				{
					Result.Value = FPropertyAccessor::ReadProperty(ElementPtr, ArrayProp->Inner);
					Result.bSuccess = true;
				}
				return Result;
			}

			// Continue resolution with the inner property
			return ResolveSegments(ElementPtr, ArrayProp->Inner, Segments, SegmentIndex + 1, bForWrite);
		}
		else
		{
			Result.bSuccess = false;
			Result.ErrorMessage = FString::Printf(TEXT("Cannot index into property '%s' (not an array)"),
				*Property->GetName());
			return Result;
		}
	}

	case EPropertyPathSegmentType::MapKey:
	{
		if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
		{
			FScriptMapHelper MapHelper(MapProp, ValuePtr);

			// Find the key in the map
			// We need to convert the string key to the appropriate type
			void* FoundValue = nullptr;

			for (int32 i = 0; i < MapHelper.GetMaxIndex(); i++)
			{
				if (!MapHelper.IsValidIndex(i))
				{
					continue;
				}

				void* KeyPtr = MapHelper.GetKeyPtr(i);

				// Convert key to string for comparison
				FString KeyString;
				MapProp->KeyProp->ExportTextItem_Direct(KeyString, KeyPtr, nullptr, nullptr, 0);

				// Remove quotes if present (from string export)
				KeyString.TrimStartAndEndInline();
				if (KeyString.StartsWith(TEXT("\"")) && KeyString.EndsWith(TEXT("\"")))
				{
					KeyString = KeyString.Mid(1, KeyString.Len() - 2);
				}

				if (KeyString == Seg.Name)
				{
					FoundValue = MapHelper.GetValuePtr(i);
					break;
				}
			}

			if (!FoundValue)
			{
				Result.bSuccess = false;
				Result.ErrorMessage = FString::Printf(TEXT("Key '%s' not found in map"), *Seg.Name);
				return Result;
			}

			// If this is the last segment
			if (SegmentIndex + 1 >= Segments.Num())
			{
				if (bForWrite)
				{
					Result.bSuccess = true;
					Result.ContainerPtr = ValuePtr;
					Result.FinalProperty = MapProp->ValueProp;
					Result.ValuePtr = FoundValue;
				}
				else
				{
					Result.Value = FPropertyAccessor::ReadProperty(FoundValue, MapProp->ValueProp);
					Result.bSuccess = true;
				}
				return Result;
			}

			// Continue resolution with the value property
			return ResolveSegments(FoundValue, MapProp->ValueProp, Segments, SegmentIndex + 1, bForWrite);
		}
		else
		{
			Result.bSuccess = false;
			Result.ErrorMessage = FString::Printf(TEXT("Cannot use key access on property '%s' (not a map)"),
				*Property->GetName());
			return Result;
		}
	}

	default:
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Invalid segment type");
		return Result;
	}
}

FProperty* FPropertyPath::FindPropertyByName(UStruct* Struct, const FString& PropertyName)
{
	if (!Struct || PropertyName.IsEmpty())
	{
		return nullptr;
	}

	// Try direct lookup first
	FProperty* Found = Struct->FindPropertyByName(*PropertyName);
	if (Found)
	{
		return Found;
	}

	// Search by display name (case-insensitive)
	for (TFieldIterator<FProperty> PropIt(Struct); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;

		// Check display name
		FString DisplayName = FTypeDiscovery::GetDisplayPropertyName(Prop);
		if (DisplayName.Equals(PropertyName, ESearchCase::IgnoreCase))
		{
			return Prop;
		}

		// Check raw name case-insensitively
		if (Prop->GetName().Equals(PropertyName, ESearchCase::IgnoreCase))
		{
			return Prop;
		}
	}

	return nullptr;
}
