#pragma once

#include "CoreMinimal.h"
#include "AgentBridgeTypes.h"

/**
 * FTypeDiscovery - Class, struct, and enum discovery and introspection.
 *
 * This class provides the discovery layer for AgentBridge, allowing external agents
 * to explore the type system and find classes/structs/enums by name. It handles
 * the complexities of Blueprint vs C++ naming conventions.
 *
 * Key Features:
 * - Class lookup with automatic BP "_C" suffix handling
 * - Property enumeration with display name normalization
 * - Function signature extraction
 * - Struct and enum discovery
 *
 * Blueprint Naming Conventions:
 * - Blueprint classes exist as two objects:
 *   - "BP_MyActor" - The UBlueprint asset (editor-only)
 *   - "BP_MyActor_C" - The UBlueprintGeneratedClass (runtime)
 * - This class automatically handles the "_C" suffix when searching
 *
 * Thread Safety:
 * - All methods should be called from the Game Thread
 * - Results should not be cached across frames without validation
 *
 * @see FPropertyAccessor for reading/writing property values
 * @see AgentBridge_Handover.md for architectural context
 */
class AGENTBRIDGECORE_API FTypeDiscovery
{
public:
	//~==============================================================================
	// Class Discovery
	//~==============================================================================

	/**
	 * Finds a UClass by name, handling both native C++ and Blueprint classes.
	 *
	 * This method handles multiple input formats:
	 * - Short name: "StaticMeshActor", "BP_MyActor"
	 * - With prefix: "AStaticMeshActor", "ABP_MyActor_C"
	 * - Full path: "/Script/Engine.StaticMeshActor", "/Game/BP_MyActor.BP_MyActor_C"
	 *
	 * For Blueprint classes, automatically tries the "_C" suffix if not provided.
	 *
	 * @param ClassName		The class name to search for.
	 * @return				The found UClass, or nullptr if not found.
	 *
	 * Example:
	 * @code
	 *     // All of these find the same class
	 *     UClass* A = FTypeDiscovery::FindClassByName("StaticMeshActor");
	 *     UClass* B = FTypeDiscovery::FindClassByName("AStaticMeshActor");
	 *
	 *     // Blueprint classes
	 *     UClass* C = FTypeDiscovery::FindClassByName("BP_MyActor");    // Adds _C automatically
	 *     UClass* D = FTypeDiscovery::FindClassByName("BP_MyActor_C");  // Works directly
	 * @endcode
	 */
	static UClass* FindClassByName(const FString& ClassName);

	/**
	 * Gets all classes derived from a base class.
	 *
	 * Useful for discovering available actor types, component types, etc.
	 *
	 * @param BaseClass			The base class to search from. Use AActor::StaticClass() for all actors.
	 * @param bIncludeAbstract	If true, includes abstract classes in results.
	 * @param bBlueprintOnly	If true, only returns Blueprint-generated classes.
	 * @return					Array of matching UClass pointers.
	 */
	static TArray<UClass*> GetAllClassesOfType(
		UClass* BaseClass,
		bool bIncludeAbstract = false,
		bool bBlueprintOnly = false
	);

	/**
	 * Gets detailed information about a class.
	 *
	 * Returns metadata suitable for external consumption including display name,
	 * path, parent class, and whether it's a Blueprint class.
	 *
	 * @param Class		The class to inspect.
	 * @return			Populated FAgentClassInfo structure.
	 */
	static FAgentClassInfo GetClassInfo(UClass* Class);

	/**
	 * Gets all properties defined on a class.
	 *
	 * @param Class				The class to enumerate properties for.
	 * @param bIncludeParents	If true, includes properties from parent classes.
	 * @param bIncludeHidden	If true, includes properties not exposed to Blueprints.
	 * @return					Array of property information.
	 */
	static TArray<FAgentPropertyInfo> GetClassProperties(
		UClass* Class,
		bool bIncludeParents = true,
		bool bIncludeHidden = false
	);

	/**
	 * Gets all functions defined on a class.
	 *
	 * @param Class				The class to enumerate functions for.
	 * @param bIncludeParents	If true, includes functions from parent classes.
	 * @param bBlueprintOnly	If true, only returns BlueprintCallable functions.
	 * @return					Array of function signatures.
	 */
	static TArray<FAgentFunctionSignature> GetClassFunctions(
		UClass* Class,
		bool bIncludeParents = true,
		bool bBlueprintOnly = false
	);

	//~==============================================================================
	// Struct Discovery
	//~==============================================================================

	/**
	 * Finds a UScriptStruct by name.
	 *
	 * Searches for both native and user-defined structs.
	 *
	 * @param StructName	The struct name to search for.
	 * @return				The found struct, or nullptr if not found.
	 */
	static UScriptStruct* FindStructByName(const FString& StructName);

	/**
	 * Gets all properties in a struct.
	 *
	 * @param Struct		The struct to enumerate.
	 * @return				Array of property information.
	 */
	static TArray<FAgentPropertyInfo> GetStructProperties(UScriptStruct* Struct);

	/**
	 * Checks if a struct is user-defined (Blueprint) vs native C++.
	 *
	 * @param Struct		The struct to check.
	 * @return				True if this is a user-defined struct.
	 */
	static bool IsUserDefinedStruct(UScriptStruct* Struct);

	//~==============================================================================
	// Enum Discovery
	//~==============================================================================

	/**
	 * Finds a UEnum by name.
	 *
	 * @param EnumName		The enum name to search for.
	 * @return				The found enum, or nullptr if not found.
	 */
	static UEnum* FindEnumByName(const FString& EnumName);

	/**
	 * Gets all values defined in an enum.
	 *
	 * @param Enum			The enum to enumerate.
	 * @return				Array of enum values with names and numeric values.
	 */
	static TArray<FAgentEnumValue> GetEnumValues(UEnum* Enum);

	/**
	 * Checks if an enum is user-defined (Blueprint) vs native C++.
	 *
	 * @param Enum			The enum to check.
	 * @return				True if this is a user-defined enum.
	 */
	static bool IsUserDefinedEnum(UEnum* Enum);

	//~==============================================================================
	// Name Normalization Utilities
	//~==============================================================================

	/**
	 * Gets a clean display name for a class.
	 *
	 * Removes the "_C" suffix from Blueprint classes and the "A" or "U" prefix
	 * from native classes for cleaner external display.
	 *
	 * @param Class		The class to get a display name for.
	 * @return			Clean display name.
	 */
	static FString GetDisplayClassName(UClass* Class);

	/**
	 * Gets a clean display name for a property.
	 *
	 * Blueprint properties have GUID suffixes like "MyVar_23_ABC123". This
	 * returns the authored name without the suffix.
	 *
	 * @param Property	The property to get a display name for.
	 * @return			Clean display name.
	 */
	static FString GetDisplayPropertyName(FProperty* Property);

	/**
	 * Normalizes a class name for lookup.
	 *
	 * Handles various input formats and converts to a consistent form for searching.
	 *
	 * @param Input		The input class name in any format.
	 * @return			Normalized name suitable for FindObject/FindClass.
	 */
	static FString NormalizeClassName(const FString& Input);

	/**
	 * Checks if a class is a Blueprint-generated class.
	 *
	 * @param Class		The class to check.
	 * @return			True if this is a Blueprint class.
	 */
	static bool IsBlueprintClass(UClass* Class);

	/**
	 * Gets the full object path for a class.
	 *
	 * @param Class		The class to get the path for.
	 * @return			Full path like "/Script/Engine.StaticMeshActor".
	 */
	static FString GetClassPath(UClass* Class);

private:
	/**
	 * Builds property info from an FProperty.
	 *
	 * @param Property	The property to extract info from.
	 * @return			Populated property info structure.
	 */
	static FAgentPropertyInfo BuildPropertyInfo(FProperty* Property);

	/**
	 * Builds function signature from a UFunction.
	 *
	 * @param Function	The function to extract signature from.
	 * @return			Populated function signature structure.
	 */
	static FAgentFunctionSignature BuildFunctionSignature(UFunction* Function);
};
