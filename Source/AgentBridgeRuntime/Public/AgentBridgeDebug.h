#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAgentBridge, Log, All);

/**
 * FAgentBridgeDebug - Console commands for testing and debugging AgentBridge systems.
 *
 * Available Commands:
 *
 * Reflection & Inspection:
 * - AgentBridge.DumpActor <Name> [Depth] - Dump actor properties
 * - AgentBridge.DumpClass <Name> - Dump class schema (properties + functions)
 * - AgentBridge.ListWorlds - List all world contexts
 *
 * PropertyPath Testing:
 * - AgentBridge.GetPath <ActorName> <Path> - Read nested property path
 * - AgentBridge.SetPath <ActorName> <Path> <Value> - Write to property path
 *
 * ActorOperations Testing:
 * - AgentBridge.QueryActors [Pattern] [Limit] - Query actors by name pattern
 * - AgentBridge.SpawnActor <Class> [X Y Z] [Label] - Spawn actor at location
 *
 * FunctionInvoker Testing:
 * - AgentBridge.CallFunc <ActorName> <FunctionName> - Call a function
 *
 * Usage Notes:
 * - Run from editor console or via -ExecCmds for headless testing
 * - All output goes to LogAgentBridge category
 * - Use "Log LogAgentBridge Log" to see output in console
 *
 * @see AgentBridge_Handover.md for detailed system documentation
 */
class AGENTBRIDGERUNTIME_API FAgentBridgeDebug
{
public:
	//~==============================================================================
	// Command Registration
	//~==============================================================================

	/**
	 * Registers all debug console commands.
	 * Call from module StartupModule().
	 */
	static void RegisterCommands();

	/**
	 * Unregisters all debug console commands.
	 * Call from module ShutdownModule().
	 */
	static void UnregisterCommands();

	//~==============================================================================
	// Inspection Utilities
	//~==============================================================================

	/**
	 * Dumps all properties of an object to the log.
	 *
	 * @param Object	The object to inspect.
	 * @param MaxDepth	Maximum recursion depth for nested types.
	 */
	static void DumpObject(UObject* Object, int32 MaxDepth = 3);

	/**
	 * Dumps class schema (properties + functions) to the log.
	 *
	 * @param Class		The class to inspect.
	 */
	static void DumpClassSchema(UClass* Class);

	/**
	 * Lists all world contexts to the log.
	 */
	static void ListWorlds();

private:
	//~==============================================================================
	// Console Command Handlers
	//~==============================================================================

	// Reflection commands
	static void Cmd_DumpActor(const TArray<FString>& Args, UWorld* World);
	static void Cmd_DumpClass(const TArray<FString>& Args, UWorld* World);
	static void Cmd_ListWorlds(const TArray<FString>& Args);

	// PropertyPath commands
	static void Cmd_GetPath(const TArray<FString>& Args, UWorld* World);
	static void Cmd_SetPath(const TArray<FString>& Args, UWorld* World);

	// ActorOperations commands
	static void Cmd_QueryActors(const TArray<FString>& Args, UWorld* World);
	static void Cmd_SpawnActor(const TArray<FString>& Args, UWorld* World);

	// FunctionInvoker commands
	static void Cmd_CallFunc(const TArray<FString>& Args, UWorld* World);

	//~==============================================================================
	// Internal Helpers
	//~==============================================================================

	/**
	 * Converts a property value to a human-readable string.
	 */
	static FString PropertyValueToString(FProperty* Property, const void* ValuePtr, int32 Depth, int32 MaxDepth);

	/**
	 * Converts property flags to a human-readable string.
	 */
	static FString PropertyFlagsToString(EPropertyFlags Flags);

	/**
	 * Converts function flags to a human-readable string.
	 */
	static FString FunctionFlagsToString(EFunctionFlags Flags);

	/**
	 * Converts an FAgentPropertyValue to a human-readable string.
	 */
	static FString AgentValueToString(const struct FAgentPropertyValue& Value);

	/**
	 * Finds an actor by name in the world.
	 */
	static AActor* FindActorByName(UWorld* World, const FString& SearchName);

	/** Registered command handles for cleanup. */
	static TArray<IConsoleObject*> RegisteredCommands;
};
