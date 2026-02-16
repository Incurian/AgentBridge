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
 * - AgentBridge.Capabilities - Show current context capabilities
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
 * Material Testing:
 * - AgentBridge.ListMaterials [Filter] [Limit] - List project materials
 * - AgentBridge.GetMaterial <Path> - Get material info and parameters
 * - AgentBridge.SetMaterialParam <Actor> <Param> <Value> [Type] - Set material parameter
 *
 * PCG Testing:
 * - AgentBridge.ListPCG [Pattern] - List PCG actors in the world
 *
 * CVar Manipulation:
 * - AgentBridge.GetCVar <Name> - Get console variable value
 * - AgentBridge.SetCVar <Name> <Value> - Set console variable value
 * - AgentBridge.ListCVars [Pattern] [Limit] - List console variables
 *
 * Command Discovery:
 * - AgentBridge.SearchCommands <Keyword> [Limit] [SearchHelp] - Search console commands by keyword
 *
 * World Partition & Streaming:
 * - AgentBridge.IsPartitioned - Check if current world uses World Partition
 * - AgentBridge.QueryAllActors [Pattern] [Limit] - Query all actors including unloaded
 * - AgentBridge.StreamingState <ActorGuid> - Get streaming state of an actor
 * - AgentBridge.QueryLandscape - List all landscape proxies (including streaming)
 * - AgentBridge.DataLayers - List all data layers in the world
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
	static void Cmd_Capabilities(const TArray<FString>& Args);

	// PropertyPath commands
	static void Cmd_GetPath(const TArray<FString>& Args, UWorld* World);
	static void Cmd_SetPath(const TArray<FString>& Args, UWorld* World);

	// ActorOperations commands
	static void Cmd_QueryActors(const TArray<FString>& Args, UWorld* World);
	static void Cmd_SpawnActor(const TArray<FString>& Args, UWorld* World);

	// FunctionInvoker commands
	static void Cmd_CallFunc(const TArray<FString>& Args, UWorld* World);

	// Material commands
	static void Cmd_ListMaterials(const TArray<FString>& Args);
	static void Cmd_GetMaterial(const TArray<FString>& Args);
	static void Cmd_SetMaterialParam(const TArray<FString>& Args, UWorld* World);

	// PCG commands
	static void Cmd_ListPCG(const TArray<FString>& Args, UWorld* World);

	// CVar commands
	static void Cmd_GetCVar(const TArray<FString>& Args);
	static void Cmd_SetCVar(const TArray<FString>& Args);
	static void Cmd_ListCVars(const TArray<FString>& Args);

	// Command discovery
	static void Cmd_SearchCommands(const TArray<FString>& Args);

	// World Partition commands
	static void Cmd_IsPartitioned(const TArray<FString>& Args, UWorld* World);
	static void Cmd_QueryAllActors(const TArray<FString>& Args, UWorld* World);
	static void Cmd_StreamingState(const TArray<FString>& Args, UWorld* World);
	static void Cmd_QueryLandscape(const TArray<FString>& Args, UWorld* World);
	static void Cmd_DataLayers(const TArray<FString>& Args, UWorld* World);

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
