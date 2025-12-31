#pragma once

#include "CoreMinimal.h"
#include "AgentBridgeTypes.h"

/**
 * EAgentCommandType - Types of commands that agents can execute.
 */
enum class EAgentCommandType : uint8
{
	None,

	// World Commands
	ListWorlds,
	SetTargetWorld,

	// Actor Query Commands
	QueryActors,
	GetActor,
	GetActorProperties,
	GetActorComponents,

	// Actor Modification Commands
	SpawnActor,
	DeleteActor,
	SetActorProperties,
	SetActorTransform,
	DuplicateActor,

	// Property Path Commands
	GetPropertyPath,
	SetPropertyPath,

	// Function Commands
	CallFunction,
	GetFunctionSignature,

	// Type Discovery Commands
	FindClass,
	GetClassSchema,
	ListClasses,

	// Batch Commands
	BatchExecute,

	// Transaction Commands
	BeginTransaction,
	CommitTransaction,
	RollbackTransaction
};

/**
 * FAgentCommandBase - Base structure for all agent commands.
 *
 * Commands are serializable structures that describe operations for the agent to perform.
 * Each command type has a corresponding response type.
 */
struct AGENTBRIDGESCRIPTING_API FAgentCommandBase
{
	/** Unique ID for this command (for correlation with responses). */
	FString CommandId;

	/** Type of command. */
	EAgentCommandType Type = EAgentCommandType::None;

	virtual ~FAgentCommandBase() = default;
};

//~==============================================================================
// World Commands
//~==============================================================================

/**
 * FListWorldsCommand - Lists all available world contexts.
 */
struct AGENTBRIDGESCRIPTING_API FListWorldsCommand : FAgentCommandBase
{
	FListWorldsCommand() { Type = EAgentCommandType::ListWorlds; }
};

/**
 * FSetTargetWorldCommand - Sets the target world for subsequent operations.
 */
struct AGENTBRIDGESCRIPTING_API FSetTargetWorldCommand : FAgentCommandBase
{
	FSetTargetWorldCommand() { Type = EAgentCommandType::SetTargetWorld; }

	/** World identifier (index, name, or "editor"/"pie"). */
	FString WorldIdentifier;
};

//~==============================================================================
// Actor Query Commands
//~==============================================================================

/**
 * FQueryActorsCommand - Queries actors matching criteria.
 */
struct AGENTBRIDGESCRIPTING_API FQueryActorsCommand : FAgentCommandBase
{
	FQueryActorsCommand() { Type = EAgentCommandType::QueryActors; }

	/** Class name to filter by (empty = all). */
	FString ClassName;

	/** Name/label pattern to match. */
	FString NamePattern;

	/** Tag to filter by. */
	FString Tag;

	/** Maximum results. */
	int32 Limit = 100;

	/** Include hidden actors. */
	bool bIncludeHidden = false;
};

/**
 * FGetActorCommand - Gets detailed info about a specific actor.
 */
struct AGENTBRIDGESCRIPTING_API FGetActorCommand : FAgentCommandBase
{
	FGetActorCommand() { Type = EAgentCommandType::GetActor; }

	/** Actor identifier (name, label, path, or GUID). */
	FString ActorId;

	/** Whether to include all properties. */
	bool bIncludeProperties = true;

	/** Whether to include component list. */
	bool bIncludeComponents = true;

	/** Max depth for nested property reading. */
	int32 PropertyDepth = 2;
};

/**
 * FGetActorPropertiesCommand - Gets specific properties from an actor.
 */
struct AGENTBRIDGESCRIPTING_API FGetActorPropertiesCommand : FAgentCommandBase
{
	FGetActorPropertiesCommand() { Type = EAgentCommandType::GetActorProperties; }

	/** Actor identifier. */
	FString ActorId;

	/** Property names to retrieve (empty = all). */
	TArray<FString> PropertyNames;
};

//~==============================================================================
// Actor Modification Commands
//~==============================================================================

/**
 * FSpawnActorCommand - Spawns a new actor.
 */
struct AGENTBRIDGESCRIPTING_API FSpawnActorCommand : FAgentCommandBase
{
	FSpawnActorCommand() { Type = EAgentCommandType::SpawnActor; }

	/** Class to spawn. */
	FString ClassName;

	/** Spawn location. */
	FVector Location = FVector::ZeroVector;

	/** Spawn rotation. */
	FRotator Rotation = FRotator::ZeroRotator;

	/** Spawn scale. */
	FVector Scale = FVector::OneVector;

	/** Editor label for the actor. */
	FString Label;

	/** Folder path in World Outliner. */
	FString FolderPath;

	/** Initial property values. */
	TMap<FString, FString> Properties;
};

/**
 * FDeleteActorCommand - Deletes an actor.
 */
struct AGENTBRIDGESCRIPTING_API FDeleteActorCommand : FAgentCommandBase
{
	FDeleteActorCommand() { Type = EAgentCommandType::DeleteActor; }

	/** Actor identifier. */
	FString ActorId;
};

/**
 * FSetActorPropertiesCommand - Sets properties on an actor.
 */
struct AGENTBRIDGESCRIPTING_API FSetActorPropertiesCommand : FAgentCommandBase
{
	FSetActorPropertiesCommand() { Type = EAgentCommandType::SetActorProperties; }

	/** Actor identifier. */
	FString ActorId;

	/** Properties to set (name -> JSON value). */
	TMap<FString, FString> Properties;
};

/**
 * FSetActorTransformCommand - Sets an actor's transform.
 */
struct AGENTBRIDGESCRIPTING_API FSetActorTransformCommand : FAgentCommandBase
{
	FSetActorTransformCommand() { Type = EAgentCommandType::SetActorTransform; }

	/** Actor identifier. */
	FString ActorId;

	/** New location (optional). */
	TOptional<FVector> Location;

	/** New rotation (optional). */
	TOptional<FRotator> Rotation;

	/** New scale (optional). */
	TOptional<FVector> Scale;

	/** Whether to sweep for collision. */
	bool bSweep = false;
};

//~==============================================================================
// Property Path Commands
//~==============================================================================

/**
 * FGetPropertyPathCommand - Gets a value at a property path.
 */
struct AGENTBRIDGESCRIPTING_API FGetPropertyPathCommand : FAgentCommandBase
{
	FGetPropertyPathCommand() { Type = EAgentCommandType::GetPropertyPath; }

	/** Actor identifier. */
	FString ActorId;

	/** Property path (e.g., "RootComponent.RelativeLocation.X"). */
	FString Path;
};

/**
 * FSetPropertyPathCommand - Sets a value at a property path.
 */
struct AGENTBRIDGESCRIPTING_API FSetPropertyPathCommand : FAgentCommandBase
{
	FSetPropertyPathCommand() { Type = EAgentCommandType::SetPropertyPath; }

	/** Actor identifier. */
	FString ActorId;

	/** Property path. */
	FString Path;

	/** Value to set (JSON encoded). */
	FString Value;
};

//~==============================================================================
// Function Commands
//~==============================================================================

/**
 * FCallFunctionCommand - Calls a function on an actor.
 */
struct AGENTBRIDGESCRIPTING_API FCallFunctionCommand : FAgentCommandBase
{
	FCallFunctionCommand() { Type = EAgentCommandType::CallFunction; }

	/** Actor identifier (empty for static functions). */
	FString ActorId;

	/** Class name (for static functions). */
	FString ClassName;

	/** Function name. */
	FString FunctionName;

	/** Parameters (name -> JSON value). */
	TMap<FString, FString> Parameters;
};

/**
 * FGetFunctionSignatureCommand - Gets function signature info.
 */
struct AGENTBRIDGESCRIPTING_API FGetFunctionSignatureCommand : FAgentCommandBase
{
	FGetFunctionSignatureCommand() { Type = EAgentCommandType::GetFunctionSignature; }

	/** Class name. */
	FString ClassName;

	/** Function name. */
	FString FunctionName;
};

//~==============================================================================
// Type Discovery Commands
//~==============================================================================

/**
 * FFindClassCommand - Finds a class by name.
 */
struct AGENTBRIDGESCRIPTING_API FFindClassCommand : FAgentCommandBase
{
	FFindClassCommand() { Type = EAgentCommandType::FindClass; }

	/** Class name to find. */
	FString ClassName;
};

/**
 * FGetClassSchemaCommand - Gets full schema for a class.
 */
struct AGENTBRIDGESCRIPTING_API FGetClassSchemaCommand : FAgentCommandBase
{
	FGetClassSchemaCommand() { Type = EAgentCommandType::GetClassSchema; }

	/** Class name. */
	FString ClassName;

	/** Include inherited members. */
	bool bIncludeInherited = true;

	/** Include functions. */
	bool bIncludeFunctions = true;
};

/**
 * FListClassesCommand - Lists classes matching criteria.
 */
struct AGENTBRIDGESCRIPTING_API FListClassesCommand : FAgentCommandBase
{
	FListClassesCommand() { Type = EAgentCommandType::ListClasses; }

	/** Base class to filter by (empty = AActor). */
	FString BaseClassName;

	/** Name pattern to match. */
	FString NamePattern;

	/** Include Blueprint classes. */
	bool bIncludeBlueprint = true;

	/** Include abstract classes. */
	bool bIncludeAbstract = false;

	/** Maximum results. */
	int32 Limit = 100;
};

//~==============================================================================
// Batch Commands
//~==============================================================================

/**
 * FBatchExecuteCommand - Executes multiple commands in sequence.
 */
struct AGENTBRIDGESCRIPTING_API FBatchExecuteCommand : FAgentCommandBase
{
	FBatchExecuteCommand() { Type = EAgentCommandType::BatchExecute; }

	/** Commands to execute (JSON encoded). */
	TArray<FString> Commands;

	/** Stop on first error. */
	bool bStopOnError = true;

	/** Wrap in transaction (for undo). */
	bool bUseTransaction = false;
};

//~==============================================================================
// Response Structures
//~==============================================================================

/**
 * FAgentResponseBase - Base structure for command responses.
 */
struct AGENTBRIDGESCRIPTING_API FAgentResponseBase
{
	/** ID of the command this responds to. */
	FString CommandId;

	/** Whether the command succeeded. */
	bool bSuccess = false;

	/** Error message if failed. */
	FString ErrorMessage;

	/** Execution time in milliseconds. */
	double ExecutionTimeMs = 0.0;

	virtual ~FAgentResponseBase() = default;
};

/**
 * FWorldInfo - Information about a world context.
 */
struct AGENTBRIDGESCRIPTING_API FWorldInfo
{
	/** World type (Editor, PIE, Game). */
	FString WorldType;

	/** World name. */
	FString WorldName;

	/** PIE instance index (-1 if not PIE). */
	int32 PIEInstance = -1;

	/** Whether gameplay has begun. */
	bool bHasBegunPlay = false;

	/** Number of actors. */
	int32 ActorCount = 0;
};

/**
 * FListWorldsResponse - Response to ListWorlds command.
 */
struct AGENTBRIDGESCRIPTING_API FListWorldsResponse : FAgentResponseBase
{
	/** Available worlds. */
	TArray<FWorldInfo> Worlds;

	/** Index of current target world. */
	int32 CurrentWorldIndex = -1;
};

/**
 * FActorInfo - Detailed information about an actor.
 */
struct AGENTBRIDGESCRIPTING_API FActorInfo
{
	/** Actor GUID. */
	FString Guid;

	/** Full path. */
	FString Path;

	/** Internal name. */
	FString Name;

	/** Editor label. */
	FString Label;

	/** Class name. */
	FString ClassName;

	/** World location. */
	FVector Location = FVector::ZeroVector;

	/** World rotation. */
	FRotator Rotation = FRotator::ZeroRotator;

	/** World scale. */
	FVector Scale = FVector::OneVector;

	/** Is hidden. */
	bool bHidden = false;

	/** Parent actor (if attached). */
	FString ParentActorId;

	/** Properties (name -> JSON value). */
	TMap<FString, FString> Properties;

	/** Components (name -> class name). */
	TMap<FString, FString> Components;
};

/**
 * FQueryActorsResponse - Response to QueryActors command.
 */
struct AGENTBRIDGESCRIPTING_API FQueryActorsResponse : FAgentResponseBase
{
	/** Matching actors (summary info). */
	TArray<FActorInfo> Actors;

	/** Total count (may be more than returned due to limit). */
	int32 TotalCount = 0;
};

/**
 * FGetActorResponse - Response to GetActor command.
 */
struct AGENTBRIDGESCRIPTING_API FGetActorResponse : FAgentResponseBase
{
	/** Actor details. */
	FActorInfo Actor;
};

/**
 * FSpawnActorResponse - Response to SpawnActor command.
 */
struct AGENTBRIDGESCRIPTING_API FSpawnActorResponse : FAgentResponseBase
{
	/** Spawned actor info. */
	FActorInfo Actor;
};

/**
 * FPropertyValueResponse - Response containing a property value.
 */
struct AGENTBRIDGESCRIPTING_API FPropertyValueResponse : FAgentResponseBase
{
	/** Property value (JSON encoded). */
	FString Value;

	/** Property type name. */
	FString TypeName;
};

/**
 * FFunctionCallResponse - Response to function call.
 */
struct AGENTBRIDGESCRIPTING_API FFunctionCallResponse : FAgentResponseBase
{
	/** Return value (JSON encoded). */
	FString ReturnValue;

	/** Out parameters (name -> JSON value). */
	TMap<FString, FString> OutParameters;
};

/**
 * FClassInfo - Information about a class.
 */
struct AGENTBRIDGESCRIPTING_API FClassInfo
{
	/** Class name. */
	FString ClassName;

	/** Display name. */
	FString DisplayName;

	/** Full class path. */
	FString ClassPath;

	/** Parent class name. */
	FString ParentClassName;

	/** Is Blueprint class. */
	bool bIsBlueprint = false;

	/** Is abstract. */
	bool bIsAbstract = false;

	/** Properties (for schema response). */
	TArray<FAgentPropertyInfo> Properties;

	/** Functions (for schema response). */
	TArray<FAgentFunctionSignature> Functions;
};

/**
 * FListClassesResponse - Response to ListClasses command.
 */
struct AGENTBRIDGESCRIPTING_API FListClassesResponse : FAgentResponseBase
{
	/** Matching classes. */
	TArray<FClassInfo> Classes;
};

/**
 * FBatchExecuteResponse - Response to batch execution.
 */
struct AGENTBRIDGESCRIPTING_API FBatchExecuteResponse : FAgentResponseBase
{
	/** Individual command responses (JSON encoded). */
	TArray<FString> Responses;

	/** Index of first failed command (-1 if all succeeded). */
	int32 FirstFailedIndex = -1;
};
