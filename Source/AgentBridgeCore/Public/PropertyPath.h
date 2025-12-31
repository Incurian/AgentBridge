#pragma once

#include "CoreMinimal.h"
#include "AgentBridgeTypes.h"

/**
 * EPropertyPathSegmentType - Types of segments in a property path.
 */
enum class EPropertyPathSegmentType : uint8
{
	/** Invalid segment. */
	None,

	/** Named property access (e.g., "Location", "Health"). */
	Property,

	/** Array index access (e.g., "[0]", "[5]"). */
	ArrayIndex,

	/** Map key access (e.g., "[\"KeyName\"]"). */
	MapKey
};

/**
 * FPropertyPathSegment - A single segment in a property path.
 *
 * Property paths consist of one or more segments separated by dots:
 * - "Location" -> Single property segment
 * - "Location.X" -> Property, then property
 * - "Components[0]" -> Property, then array index
 * - "Inventory[\"Sword\"]" -> Property, then map key
 */
struct AGENTBRIDGECORE_API FPropertyPathSegment
{
	/** Type of this segment. */
	EPropertyPathSegmentType Type = EPropertyPathSegmentType::None;

	/** Property or key name (for Property and MapKey types). */
	FString Name;

	/** Index value (for ArrayIndex type). */
	int32 Index = INDEX_NONE;

	/** Creates a property segment. */
	static FPropertyPathSegment Property(const FString& InName)
	{
		FPropertyPathSegment Seg;
		Seg.Type = EPropertyPathSegmentType::Property;
		Seg.Name = InName;
		return Seg;
	}

	/** Creates an array index segment. */
	static FPropertyPathSegment ArrayIndex(int32 InIndex)
	{
		FPropertyPathSegment Seg;
		Seg.Type = EPropertyPathSegmentType::ArrayIndex;
		Seg.Index = InIndex;
		return Seg;
	}

	/** Creates a map key segment. */
	static FPropertyPathSegment MapKey(const FString& InKey)
	{
		FPropertyPathSegment Seg;
		Seg.Type = EPropertyPathSegmentType::MapKey;
		Seg.Name = InKey;
		return Seg;
	}

	/** Converts segment to string representation. */
	FString ToString() const
	{
		switch (Type)
		{
		case EPropertyPathSegmentType::Property:
			return Name;
		case EPropertyPathSegmentType::ArrayIndex:
			return FString::Printf(TEXT("[%d]"), Index);
		case EPropertyPathSegmentType::MapKey:
			return FString::Printf(TEXT("[\"%s\"]"), *Name);
		default:
			return TEXT("");
		}
	}

	/** Checks if segment is valid. */
	bool IsValid() const
	{
		return Type != EPropertyPathSegmentType::None;
	}
};

/**
 * FPropertyPathResult - Result of a property path resolution.
 */
struct AGENTBRIDGECORE_API FPropertyPathResult
{
	/** Whether resolution succeeded. */
	bool bSuccess = false;

	/** Error message if resolution failed. */
	FString ErrorMessage;

	/** The resolved property value. */
	FAgentPropertyValue Value;

	/** Pointer to the container object (for write operations). */
	void* ContainerPtr = nullptr;

	/** The final property in the path (for write operations). */
	FProperty* FinalProperty = nullptr;

	/** Offset within container for final value (for arrays/maps). */
	void* ValuePtr = nullptr;
};

/**
 * FPropertyPath - Parses and resolves nested property paths.
 *
 * This class enables access to deeply nested properties using dot-notation paths:
 *
 * Simple Examples:
 * - "Location" -> Actor's Location property
 * - "Location.X" -> X component of Location vector
 * - "Health" -> Simple float property
 *
 * Array Examples:
 * - "Components[0]" -> First element of Components array
 * - "Inventory[3].Name" -> Name property of 4th inventory item
 *
 * Map Examples:
 * - "Stats[\"Strength\"]" -> Value for "Strength" key in Stats map
 * - "Equipment[\"Weapon\"].Damage" -> Nested access through map
 *
 * Path Syntax:
 * - Properties are separated by dots: "Outer.Inner.Leaf"
 * - Array indices use brackets: "Array[0]"
 * - Map keys use quoted brackets: "Map[\"Key\"]"
 * - Segments can be chained: "Outer.Array[0].Map[\"Key\"].Value"
 *
 * Blueprint Compatibility:
 * - Supports both C++ names and Blueprint display names
 * - Handles GUID-suffixed Blueprint property names
 *
 * Thread Safety:
 * - All methods should be called from the Game Thread
 *
 * @see FPropertyAccessor for single-property access
 */
class AGENTBRIDGECORE_API FPropertyPath
{
public:
	//~==============================================================================
	// Path Parsing
	//~==============================================================================

	/**
	 * Parses a path string into segments.
	 *
	 * @param PathString	The path to parse (e.g., "Components[0].Location.X").
	 * @return				Array of parsed segments.
	 */
	static TArray<FPropertyPathSegment> ParsePath(const FString& PathString);

	/**
	 * Converts segments back to a path string.
	 *
	 * @param Segments		The segments to convert.
	 * @return				Path string representation.
	 */
	static FString SegmentsToString(const TArray<FPropertyPathSegment>& Segments);

	/**
	 * Validates that a path string has correct syntax.
	 *
	 * @param PathString	The path to validate.
	 * @param OutError		Error message if invalid.
	 * @return				True if syntax is valid.
	 */
	static bool ValidatePath(const FString& PathString, FString* OutError = nullptr);

	//~==============================================================================
	// Path Resolution (Read)
	//~==============================================================================

	/**
	 * Resolves a property path and reads the value.
	 *
	 * @param Object		The root object to start from.
	 * @param PathString	The path to resolve.
	 * @return				Result containing value or error.
	 */
	static FPropertyPathResult GetValue(UObject* Object, const FString& PathString);

	/**
	 * Resolves a property path using pre-parsed segments.
	 *
	 * @param Object		The root object to start from.
	 * @param Segments		Pre-parsed path segments.
	 * @return				Result containing value or error.
	 */
	static FPropertyPathResult GetValue(UObject* Object, const TArray<FPropertyPathSegment>& Segments);

	//~==============================================================================
	// Path Resolution (Write)
	//~==============================================================================

	/**
	 * Resolves a property path and writes a value.
	 *
	 * @param Object		The root object to modify.
	 * @param PathString	The path to resolve.
	 * @param Value			The value to write.
	 * @return				True if write succeeded.
	 */
	static bool SetValue(UObject* Object, const FString& PathString, const FAgentPropertyValue& Value);

	/**
	 * Resolves a property path using pre-parsed segments and writes a value.
	 *
	 * @param Object		The root object to modify.
	 * @param Segments		Pre-parsed path segments.
	 * @param Value			The value to write.
	 * @return				True if write succeeded.
	 */
	static bool SetValue(
		UObject* Object,
		const TArray<FPropertyPathSegment>& Segments,
		const FAgentPropertyValue& Value);

	//~==============================================================================
	// Path Existence
	//~==============================================================================

	/**
	 * Checks if a property path exists and is accessible.
	 *
	 * @param Object		The root object to check.
	 * @param PathString	The path to check.
	 * @return				True if path exists and is readable.
	 */
	static bool PathExists(UObject* Object, const FString& PathString);

	/**
	 * Gets type information for a property path.
	 *
	 * @param Class			The class to introspect.
	 * @param PathString	The path to analyze.
	 * @return				Property type at end of path, or None if invalid.
	 */
	static EAgentPropertyType GetPathType(UClass* Class, const FString& PathString);

private:
	/**
	 * Internal resolution implementation.
	 *
	 * @param Container		Current container pointer.
	 * @param Property		Current property being accessed.
	 * @param Segments		Remaining segments to process.
	 * @param SegmentIndex	Current segment index.
	 * @param bForWrite		Whether this is for a write operation.
	 * @return				Resolution result.
	 */
	static FPropertyPathResult ResolveSegments(
		void* Container,
		FProperty* Property,
		const TArray<FPropertyPathSegment>& Segments,
		int32 SegmentIndex,
		bool bForWrite);

	/**
	 * Finds a property by name, checking both C++ and display names.
	 *
	 * @param Struct		The struct/class to search.
	 * @param PropertyName	Name to search for.
	 * @return				Found property or nullptr.
	 */
	static FProperty* FindPropertyByName(UStruct* Struct, const FString& PropertyName);
};
