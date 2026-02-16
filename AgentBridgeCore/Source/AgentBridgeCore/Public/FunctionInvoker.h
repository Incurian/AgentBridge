#pragma once

#include "CoreMinimal.h"
#include "AgentBridgeTypes.h"

/**
 * FFunctionInvoker - Dynamic UFunction invocation with parameter marshaling.
 *
 * This class enables calling any Blueprint-exposed UFunction dynamically, handling
 * the complex parameter setup, invocation, and result extraction that UE's reflection
 * system requires.
 *
 * Key Features:
 * - Automatic parameter memory allocation and initialization
 * - Hidden parameter handling (WorldContext, self pins)
 * - Return value and output parameter extraction
 * - Support for both instance and static/CDO calls
 *
 * Hidden Parameters:
 * Some Blueprint functions have "hidden" parameters that are automatically filled:
 * - WorldContext: Filled from the World parameter or target object
 * - Self: Automatically set to the target instance
 *
 * Thread Safety:
 * - All invocation must happen on the Game Thread
 * - UObject targets must be valid for the duration of the call
 *
 * Memory Management:
 * - Parameter memory is allocated on the stack where possible
 * - Complex types (strings, arrays) are properly initialized/destroyed
 *
 * @see FPropertyAccessor for parameter value conversion
 * @see FAgentFunctionResult for invocation results
 */
class AGENTBRIDGECORE_API FFunctionInvoker
{
public:
	//~==============================================================================
	// Function Discovery
	//~==============================================================================

	/**
	 * Gets all callable functions on a class.
	 *
	 * Returns functions that can be invoked via the reflection system. This includes
	 * BlueprintCallable, Exec, and Event functions.
	 *
	 * @param Class				The class to enumerate functions for.
	 * @param bIncludeParents	Include functions from parent classes.
	 * @param bBlueprintOnly	Only return BlueprintCallable functions.
	 * @return					Array of callable UFunction pointers.
	 */
	static TArray<UFunction*> GetCallableFunctions(
		UClass* Class,
		bool bIncludeParents = true,
		bool bBlueprintOnly = false
	);

	/**
	 * Finds a specific function by name on a class.
	 *
	 * @param Class			The class to search.
	 * @param FunctionName	The name of the function.
	 * @return				The found function, or nullptr.
	 */
	static UFunction* FindFunction(UClass* Class, const FString& FunctionName);

	/**
	 * Gets the function signature in transport format.
	 *
	 * Extracts parameter types, return type, and metadata for a function.
	 *
	 * @param Function	The function to describe.
	 * @return			Populated signature structure.
	 */
	static FAgentFunctionSignature GetFunctionSignature(UFunction* Function);

	//~==============================================================================
	// Hidden Parameter Detection
	//~==============================================================================

	/**
	 * Checks if a function requires a WorldContext parameter.
	 *
	 * WorldContext is a hidden parameter on many static Blueprint functions
	 * that needs to be filled in automatically.
	 *
	 * @param Function	The function to check.
	 * @return			True if the function needs a WorldContext.
	 */
	static bool FunctionNeedsWorldContext(UFunction* Function);

	/**
	 * Gets the name of the WorldContext parameter if one exists.
	 *
	 * @param Function	The function to check.
	 * @return			Parameter name, or empty string if none.
	 */
	static FString GetWorldContextParamName(UFunction* Function);

	/**
	 * Checks if a function has a hidden "self" pin.
	 *
	 * Some Blueprint functions have a hidden self reference that is
	 * automatically bound to the target object.
	 *
	 * @param Function	The function to check.
	 * @return			True if function has hidden self.
	 */
	static bool FunctionHasHiddenSelfPin(UFunction* Function);

	//~==============================================================================
	// Function Invocation
	//~==============================================================================

	/**
	 * Invokes a function on a target object.
	 *
	 * This is the primary method for calling functions. It handles:
	 * - Parameter memory allocation and initialization
	 * - Converting FAgentPropertyValue params to native types
	 * - Setting WorldContext if needed
	 * - Extracting return value and out parameters
	 *
	 * @param Target		The object to call the function on. Required for non-static functions.
	 * @param Function		The function to invoke.
	 * @param Params		Map of parameter name -> value. Only provide non-default parameters.
	 * @param WorldContext	Optional world for WorldContext parameter. If null, derived from Target.
	 * @return				Result containing success status, return value, and out params.
	 *
	 * Example:
	 * @code
	 *     TMap<FString, FAgentPropertyValue> Params;
	 *     Params.Add("DeltaLocation", FAgentPropertyValue::FromVector(FVector(100, 0, 0)));
	 *     Params.Add("bSweep", FAgentPropertyValue::FromBool(true));
	 *
	 *     FAgentFunctionResult Result = FFunctionInvoker::InvokeFunction(
	 *         MyActor, AddActorWorldOffsetFunc, Params);
	 * @endcode
	 */
	static FAgentFunctionResult InvokeFunction(
		UObject* Target,
		UFunction* Function,
		const TMap<FString, FAgentPropertyValue>& Params,
		UWorld* WorldContext = nullptr
	);

	/**
	 * Invokes a static-like function using the Class Default Object.
	 *
	 * For static functions or functions that don't require a specific instance,
	 * this method creates a temporary CDO instance and calls the function on it.
	 *
	 * @param Class			The class containing the function.
	 * @param Function		The function to invoke.
	 * @param Params		Parameter map.
	 * @param WorldContext	World for WorldContext parameter.
	 * @return				Invocation result.
	 */
	static FAgentFunctionResult InvokeStaticFunction(
		UClass* Class,
		UFunction* Function,
		const TMap<FString, FAgentPropertyValue>& Params,
		UWorld* WorldContext = nullptr
	);

private:
	//~==============================================================================
	// Internal Helpers
	//~==============================================================================

	/**
	 * Allocates and initializes parameter memory for a function call.
	 *
	 * This allocates a block of memory sized for all function parameters,
	 * initializes default values, and fills in provided parameters.
	 *
	 * @param Function			The function being called.
	 * @param Params			User-provided parameter values.
	 * @param WorldContextObject Object to use for WorldContext if needed.
	 * @return					Allocated and initialized parameter memory. Caller must free.
	 */
	static void* PrepareParameters(
		UFunction* Function,
		const TMap<FString, FAgentPropertyValue>& Params,
		UObject* WorldContextObject
	);

	/**
	 * Extracts return value and output parameters after a function call.
	 *
	 * @param Function		The function that was called.
	 * @param ParamBuffer	The parameter memory block after execution.
	 * @return				Result structure with extracted values.
	 */
	static FAgentFunctionResult ExtractResults(
		UFunction* Function,
		void* ParamBuffer
	);

	/**
	 * Cleans up parameter memory after function execution.
	 *
	 * Properly destroys complex types (strings, arrays, objects) and frees memory.
	 *
	 * @param Function		The function that was called.
	 * @param ParamBuffer	The parameter memory to clean up.
	 */
	static void CleanupParameters(UFunction* Function, void* ParamBuffer);

	/**
	 * Finds a parameter property by display name.
	 *
	 * Handles Blueprint parameter name mangling.
	 *
	 * @param Function		The function to search.
	 * @param ParamName		The display name to search for.
	 * @return				The found property, or nullptr.
	 */
	static FProperty* FindParameterByName(UFunction* Function, const FString& ParamName);
};
