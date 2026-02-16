#pragma once

#include "CoreMinimal.h"
#include "AgentBridgeTypes.h"

/**
 * FPropertyAccessor - Low-level property read/write using Unreal's reflection system.
 *
 * This class provides the foundation for all property access in AgentBridge. It handles
 * the conversion between Unreal's native property values and our transport format
 * (FAgentPropertyValue), supporting all FProperty types including nested containers.
 *
 * Key Design Decisions:
 * - Uses FProperty's ContainerPtrToValuePtr for safe pointer arithmetic
 * - Recursively handles nested structures with configurable depth limits
 * - Object references are serialized as FSoftObjectPath strings for stability
 * - Blueprint property names are normalized via GetAuthoredName()
 *
 * Thread Safety:
 * - All methods must be called from the Game Thread
 * - UObject pointers should be validated before use
 *
 * @see FAgentPropertyValue for the transport format
 * @see AgentBridge_Handover.md for architectural context
 */
class AGENTBRIDGECORE_API FPropertyAccessor
{
public:
	//~==============================================================================
	// Core Read/Write Operations
	//~==============================================================================

	/**
	 * Reads a property value from a container and converts it to transport format.
	 *
	 * This is the primary entry point for reading property values. It handles all
	 * FProperty types and recursively processes nested structures up to MaxDepth.
	 *
	 * @param Container		Pointer to the UObject or struct instance containing the property.
	 *						For UObjects, pass the UObject pointer directly.
	 *						For nested structs, pass the struct's memory address.
	 * @param Property		The FProperty to read. Must not be null.
	 * @param MaxDepth		Maximum recursion depth for nested containers (default: 10).
	 *						Prevents infinite recursion and limits output size.
	 * @return				FAgentPropertyValue containing the serialized value.
	 *						Type will be EAgentPropertyType::Unknown on failure.
	 *
	 * @note Container pointer type depends on context - for direct UObject properties,
	 *       pass the UObject*. For struct members, pass the struct memory pointer.
	 *
	 * Example:
	 * @code
	 *     // Reading from a UObject
	 *     FProperty* HealthProp = ActorClass->FindPropertyByName("Health");
	 *     FAgentPropertyValue Value = FPropertyAccessor::ReadProperty(MyActor, HealthProp);
	 *
	 *     // Reading from a struct
	 *     FProperty* XProp = VectorStruct->FindPropertyByName("X");
	 *     FAgentPropertyValue Value = FPropertyAccessor::ReadProperty(&MyVector, XProp);
	 * @endcode
	 */
	static FAgentPropertyValue ReadProperty(
		const void* Container,
		FProperty* Property,
		int32 MaxDepth = 10
	);

	/**
	 * Writes a transport-format value to a property in a container.
	 *
	 * Converts the FAgentPropertyValue back to the native property type and writes
	 * it to the specified location. Handles type coercion where reasonable (e.g.,
	 * string "123" to int32).
	 *
	 * @param Container		Pointer to the UObject or struct instance to modify.
	 * @param Property		The FProperty to write. Must not be null.
	 * @param Value			The value to write in transport format.
	 * @return				True if the write succeeded, false otherwise.
	 *
	 * @warning This method modifies object state. Ensure proper transaction wrapping
	 *          for editor operations that should support undo.
	 *
	 * @note Type mismatches are handled gracefully where possible:
	 *       - Numeric types are converted (int to float, etc.)
	 *       - Strings are parsed for numeric/vector/rotator types
	 *       - Invalid conversions return false without modifying the property
	 */
	static bool WriteProperty(
		void* Container,
		FProperty* Property,
		const FAgentPropertyValue& Value
	);

	/**
	 * Writes a value directly to a memory location without calling ContainerPtrToValuePtr.
	 *
	 * This is used when the caller has already resolved the value pointer via property
	 * path resolution. Unlike WriteProperty, this does NOT apply ContainerPtrToValuePtr
	 * offset - the ValuePtr must already point to the exact memory location of the value.
	 *
	 * @param ValuePtr		Direct pointer to the value's memory location.
	 * @param Property		The FProperty describing the value's type. Must not be null.
	 * @param Value			The value to write in transport format.
	 * @return				True if the write succeeded, false otherwise.
	 *
	 * @note Use this ONLY when you've already calculated the correct value address.
	 *       For standard property access, use WriteProperty instead.
	 */
	static bool WritePropertyDirect(
		void* ValuePtr,
		FProperty* Property,
		const FAgentPropertyValue& Value
	);

	//~==============================================================================
	// Type Introspection
	//~==============================================================================

	/**
	 * Determines the EAgentPropertyType for a given FProperty.
	 *
	 * Maps Unreal's property types to our simplified transport enum. This is used
	 * for schema discovery and type-aware serialization.
	 *
	 * @param Property		The property to inspect.
	 * @return				The corresponding EAgentPropertyType.
	 */
	static EAgentPropertyType GetPropertyType(FProperty* Property);

	/**
	 * Gets a human-readable type name for a property.
	 *
	 * Returns a string like "TArray<FVector>" or "TMap<FString, int32>" that
	 * describes the full property type including template parameters.
	 *
	 * @param Property		The property to describe.
	 * @return				Human-readable type string.
	 */
	static FString GetPropertyTypeName(FProperty* Property);

	/**
	 * Checks if a property can be written to.
	 *
	 * Properties may be read-only due to:
	 * - CPF_BlueprintReadOnly flag
	 * - CPF_EditConst flag
	 * - Being a computed/derived value
	 *
	 * @param Property		The property to check.
	 * @return				True if the property can be written.
	 */
	static bool IsPropertyWritable(FProperty* Property);

	/**
	 * Gets the clean display name for a property.
	 *
	 * Blueprint properties have internal names with GUID suffixes like
	 * "MyVariable_42_ABC123DEF". This returns the authored name "MyVariable".
	 *
	 * @param Property		The property to get the name for.
	 * @return				Clean display name suitable for external use.
	 */
	static FString GetPropertyDisplayName(FProperty* Property);

	//~==============================================================================
	// Object Reference Handling
	//~==============================================================================

	/**
	 * Serializes a UObject reference to a stable string path.
	 *
	 * Uses FSoftObjectPath internally for maximum compatibility across sessions.
	 * The resulting string can be used with ResolveObjectReference() to retrieve
	 * the object later.
	 *
	 * @param Object		The object to serialize. May be null.
	 * @return				Path string like "/Game/Maps/Level.Level:PersistentLevel.Actor_0"
	 *						Returns empty string for null objects.
	 */
	static FString SerializeObjectReference(UObject* Object);

	/**
	 * Resolves an object reference string back to a UObject pointer.
	 *
	 * Attempts to find the object without loading it from disk. For assets that
	 * may not be loaded, consider using async loading patterns instead.
	 *
	 * @param Reference		The path string from SerializeObjectReference().
	 * @param ExpectedClass	Optional class filter. If provided, returns null if
	 *						the found object isn't of this class.
	 * @return				The resolved object, or null if not found/wrong type.
	 *
	 * @note This uses FindObject, not LoadObject - it won't load assets from disk.
	 */
	static UObject* ResolveObjectReference(const FString& Reference, UClass* ExpectedClass = nullptr);

private:
	//~==============================================================================
	// Internal Read Helpers - Recursive handlers for complex types
	//~==============================================================================

	/** Reads a boolean property value. */
	static FAgentPropertyValue ReadBoolProperty(const void* Container, FBoolProperty* Property);

	/** Reads any numeric property (int8-64, uint8-64, float, double). */
	static FAgentPropertyValue ReadNumericProperty(const void* Container, FNumericProperty* Property);

	/** Reads string-like properties (FString, FName, FText). */
	static FAgentPropertyValue ReadStringProperty(const void* Container, FProperty* Property);

	/** Reads enum properties (both FEnumProperty and legacy FByteProperty enums). */
	static FAgentPropertyValue ReadEnumProperty(const void* Container, FProperty* Property);

	/** Reads object reference properties (UObject*, TObjectPtr, TSoftObjectPtr, etc). */
	static FAgentPropertyValue ReadObjectProperty(const void* Container, FObjectPropertyBase* Property);

	/** Reads struct properties, recursively processing members. */
	static FAgentPropertyValue ReadStructProperty(const void* Container, FStructProperty* Property, int32 Depth, int32 MaxDepth);

	/** Reads TArray properties, recursively processing elements. */
	static FAgentPropertyValue ReadArrayProperty(const void* Container, FArrayProperty* Property, int32 Depth, int32 MaxDepth);

	/** Reads TMap properties, recursively processing key-value pairs. */
	static FAgentPropertyValue ReadMapProperty(const void* Container, FMapProperty* Property, int32 Depth, int32 MaxDepth);

	/** Reads TSet properties, recursively processing elements. */
	static FAgentPropertyValue ReadSetProperty(const void* Container, FSetProperty* Property, int32 Depth, int32 MaxDepth);

	//~==============================================================================
	// Internal Write Helpers
	// NOTE: All write helpers receive a direct ValuePtr (already resolved), not a container.
	// They must NOT call ContainerPtrToValuePtr internally.
	//~==============================================================================

	/** Writes a boolean value directly to a property location. */
	static bool WriteBoolProperty(void* ValuePtr, FBoolProperty* Property, const FAgentPropertyValue& Value);

	/** Writes a numeric value directly, handling type conversion. */
	static bool WriteNumericProperty(void* ValuePtr, FNumericProperty* Property, const FAgentPropertyValue& Value);

	/** Writes string-like values directly. */
	static bool WriteStringProperty(void* ValuePtr, FProperty* Property, const FAgentPropertyValue& Value);

	/** Writes enum values by name or numeric value directly. */
	static bool WriteEnumProperty(void* ValuePtr, FProperty* Property, const FAgentPropertyValue& Value);

	/** Writes object references directly, resolving from path strings. */
	static bool WriteObjectProperty(void* ValuePtr, FObjectPropertyBase* Property, const FAgentPropertyValue& Value);

	/** Writes struct values directly, recursively setting members. */
	static bool WriteStructProperty(void* ValuePtr, FStructProperty* Property, const FAgentPropertyValue& Value);

	/** Writes array values directly, resizing and populating elements. */
	static bool WriteArrayProperty(void* ValuePtr, FArrayProperty* Property, const FAgentPropertyValue& Value);

	/** Writes map values directly, clearing and repopulating. */
	static bool WriteMapProperty(void* ValuePtr, FMapProperty* Property, const FAgentPropertyValue& Value);

	/** Writes set values directly, clearing and repopulating. */
	static bool WriteSetProperty(void* ValuePtr, FSetProperty* Property, const FAgentPropertyValue& Value);

	//~==============================================================================
	// Utility Helpers
	//~==============================================================================

	/**
	 * Handles special struct types with known serialization formats.
	 *
	 * Common types like FVector, FRotator, FTransform, FColor have well-defined
	 * string representations. This method checks if a struct is one of these
	 * and handles it specially for cleaner output.
	 *
	 * @param StructProperty	The struct property being processed.
	 * @param ValuePtr			Pointer to the struct data.
	 * @param OutValue			Output parameter filled if this is a known type.
	 * @return					True if handled as a special type, false to use generic struct handling.
	 */
	static bool TryReadSpecialStruct(FStructProperty* StructProperty, const void* ValuePtr, FAgentPropertyValue& OutValue);

	/**
	 * Attempts to write a value to a special struct type.
	 *
	 * @param StructProperty	The struct property being written.
	 * @param ValuePtr			Pointer to the struct data.
	 * @param Value				The value to write.
	 * @return					True if handled as a special type.
	 */
	static bool TryWriteSpecialStruct(FStructProperty* StructProperty, void* ValuePtr, const FAgentPropertyValue& Value);
};
