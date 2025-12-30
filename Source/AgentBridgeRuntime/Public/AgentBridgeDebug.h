#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAgentBridge, Log, All);

/**
 * Debug utilities for AgentBridge - console commands and inspection helpers.
 */
class AGENTBRIDGERUNTIME_API FAgentBridgeDebug
{
public:
	// Register console commands - call from module startup
	static void RegisterCommands();

	// Unregister console commands - call from module shutdown
	static void UnregisterCommands();

	// Dump all properties of an object to log
	static void DumpObject(UObject* Object, int32 MaxDepth = 3);

	// Dump class schema (properties + functions) to log
	static void DumpClassSchema(UClass* Class);

	// List all world contexts
	static void ListWorlds();

private:
	// Console command handlers
	static void Cmd_DumpActor(const TArray<FString>& Args, UWorld* World);
	static void Cmd_DumpClass(const TArray<FString>& Args, UWorld* World);
	static void Cmd_ListWorlds(const TArray<FString>& Args);

	// Property value to string conversion
	static FString PropertyValueToString(FProperty* Property, const void* ValuePtr, int32 Depth, int32 MaxDepth);

	// Property flags to readable string
	static FString PropertyFlagsToString(EPropertyFlags Flags);

	// Function flags to readable string
	static FString FunctionFlagsToString(EFunctionFlags Flags);

	// Registered command handles for cleanup
	static TArray<IConsoleObject*> RegisteredCommands;
};
