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
	GetCapabilities,

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

	// DataAsset Commands
	ListDataAssets,
	GetDataAsset,
	GetDataTableRow,

	// Capture Commands
	CaptureViewport,
	CaptureScene,

	// Audio Commands
	GetAudioAnalysis,
	StartAudioCapture,
	StopAudioCapture,

	// Material Commands
	ListMaterials,
	GetMaterialInfo,
	CreateMaterialInstance,
	SetMaterialParameter,
	ApplyMaterialToActor,

	// PCG Commands
	ListPCGActors,
	RegeneratePCG,
	SetPCGParameter,

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

/**
 * FGetCapabilitiesCommand - Gets context capabilities.
 *
 * Returns information about what operations are available in the current
 * world context. This is useful for agents to understand what actions
 * are possible (e.g., setting actor labels is only available in editor).
 */
struct AGENTBRIDGESCRIPTING_API FGetCapabilitiesCommand : FAgentCommandBase
{
	FGetCapabilitiesCommand() { Type = EAgentCommandType::GetCapabilities; }
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
// DataAsset Commands
//~==============================================================================

/**
 * FListDataAssetsCommand - Lists DataAssets matching criteria.
 */
struct AGENTBRIDGESCRIPTING_API FListDataAssetsCommand : FAgentCommandBase
{
	FListDataAssetsCommand() { Type = EAgentCommandType::ListDataAssets; }

	/** Base class to filter by (e.g., "DataAsset", "DataTable", "PrimaryDataAsset"). */
	FString BaseClassName;

	/** Path pattern to filter (e.g., "/Game/Data/*"). */
	FString PathFilter;

	/** Maximum results. */
	int32 Limit = 100;
};

/**
 * FGetDataAssetCommand - Gets a DataAsset and its properties.
 */
struct AGENTBRIDGESCRIPTING_API FGetDataAssetCommand : FAgentCommandBase
{
	FGetDataAssetCommand() { Type = EAgentCommandType::GetDataAsset; }

	/** Asset path (e.g., "/Game/Data/MyData.MyData"). */
	FString AssetPath;

	/** Max depth for nested property reading. */
	int32 PropertyDepth = 3;
};

/**
 * FGetDataTableRowCommand - Gets a specific row from a DataTable.
 */
struct AGENTBRIDGESCRIPTING_API FGetDataTableRowCommand : FAgentCommandBase
{
	FGetDataTableRowCommand() { Type = EAgentCommandType::GetDataTableRow; }

	/** DataTable asset path. */
	FString TablePath;

	/** Row name to get (empty = all rows). */
	FString RowName;

	/** Maximum rows to return if RowName is empty. */
	int32 Limit = 100;
};

//~==============================================================================
// Capture Commands
//~==============================================================================

/**
 * FCaptureViewportCommand - Captures the current viewport.
 *
 * Note: Only available in Editor or PIE contexts where a viewport exists.
 */
struct AGENTBRIDGESCRIPTING_API FCaptureViewportCommand : FAgentCommandBase
{
	FCaptureViewportCommand() { Type = EAgentCommandType::CaptureViewport; }

	/** Output file path (empty = return base64 in response). */
	FString OutputPath;

	/** Width override (0 = current viewport width). */
	int32 Width = 0;

	/** Height override (0 = current viewport height). */
	int32 Height = 0;

	/** Include UI elements in capture. */
	bool bShowUI = false;

	/** Image format (PNG, JPG, EXR). */
	FString Format = TEXT("PNG");
};

/**
 * FCaptureSceneCommand - Captures through a SceneCaptureComponent2D.
 *
 * This allows capturing from an arbitrary camera position without
 * requiring a viewport. Works in all contexts (Editor, PIE, packaged).
 */
struct AGENTBRIDGESCRIPTING_API FCaptureSceneCommand : FAgentCommandBase
{
	FCaptureSceneCommand() { Type = EAgentCommandType::CaptureScene; }

	/** Actor ID with SceneCaptureComponent2D (empty = create temporary). */
	FString ActorId;

	/** Component name if actor has multiple capture components. */
	FString ComponentName;

	/** Camera location (required if ActorId is empty). */
	FVector Location = FVector::ZeroVector;

	/** Camera rotation (required if ActorId is empty). */
	FRotator Rotation = FRotator::ZeroRotator;

	/** Field of view in degrees (0 = default 90). */
	float FOV = 90.0f;

	/** Capture width. */
	int32 Width = 1280;

	/** Capture height. */
	int32 Height = 720;

	/** Output file path (empty = return base64 in response). */
	FString OutputPath;

	/** Image format (PNG, JPG). */
	FString Format = TEXT("PNG");
};

//~==============================================================================
// Audio Commands
//~==============================================================================

/**
 * EAudioCaptureSource - Source of audio to capture.
 */
enum class EAudioCaptureSource : uint8
{
	/** Audio from world/game (submix output). */
	WorldAudio,

	/** Audio from player's microphone. */
	PlayerMic,

	/** Audio from a specific actor with audio component. */
	Actor
};

/**
 * FGetAudioAnalysisCommand - Get real-time audio analysis data.
 *
 * This provides frequency bands, volume levels, and beat detection
 * without recording full audio. Useful for visualizations or reactions.
 */
struct AGENTBRIDGESCRIPTING_API FGetAudioAnalysisCommand : FAgentCommandBase
{
	FGetAudioAnalysisCommand() { Type = EAgentCommandType::GetAudioAnalysis; }

	/** Source of audio to analyze. */
	EAudioCaptureSource Source = EAudioCaptureSource::WorldAudio;

	/** Actor ID if Source is Actor. */
	FString ActorId;

	/** Number of frequency bands to analyze (e.g., 4 for bass/low-mid/high-mid/treble). */
	int32 FrequencyBands = 8;
};

/**
 * FStartAudioCaptureCommand - Starts recording audio.
 *
 * Audio is captured until StopAudioCapture is called or max duration reached.
 */
struct AGENTBRIDGESCRIPTING_API FStartAudioCaptureCommand : FAgentCommandBase
{
	FStartAudioCaptureCommand() { Type = EAgentCommandType::StartAudioCapture; }

	/** Source of audio to capture. */
	EAudioCaptureSource Source = EAudioCaptureSource::WorldAudio;

	/** Actor ID if Source is Actor. */
	FString ActorId;

	/** Maximum capture duration in seconds (0 = unlimited until stop). */
	float MaxDuration = 30.0f;

	/** Sample rate (44100, 48000, etc). */
	int32 SampleRate = 44100;

	/** Number of audio channels (1 = mono, 2 = stereo). */
	int32 Channels = 2;
};

/**
 * FStopAudioCaptureCommand - Stops recording and retrieves audio data.
 */
struct AGENTBRIDGESCRIPTING_API FStopAudioCaptureCommand : FAgentCommandBase
{
	FStopAudioCaptureCommand() { Type = EAgentCommandType::StopAudioCapture; }

	/** Capture ID returned by StartAudioCapture. */
	FString CaptureId;

	/** Output file path (empty = return base64 WAV in response). */
	FString OutputPath;
};

//~==============================================================================
// Material Commands
//~==============================================================================

/**
 * EMaterialParameterType - Type of material parameter.
 */
enum class EMaterialParameterType : uint8
{
	Scalar,
	Vector,
	Texture,
	StaticSwitch
};

/**
 * FListMaterialsCommand - List materials in the project.
 */
struct AGENTBRIDGESCRIPTING_API FListMaterialsCommand : FAgentCommandBase
{
	FListMaterialsCommand() { Type = EAgentCommandType::ListMaterials; }

	/** Wildcard filter for asset paths (e.g., "/Game/Materials/*"). */
	FString PathFilter;

	/** Filter for material instances only. */
	bool bInstancesOnly = false;

	/** Maximum results. */
	int32 Limit = 100;
};

/**
 * FGetMaterialInfoCommand - Get information about a material.
 */
struct AGENTBRIDGESCRIPTING_API FGetMaterialInfoCommand : FAgentCommandBase
{
	FGetMaterialInfoCommand() { Type = EAgentCommandType::GetMaterialInfo; }

	/** Material asset path or name. */
	FString MaterialPath;

	/** Include parameter values. */
	bool bIncludeParameters = true;
};

/**
 * FCreateMaterialInstanceCommand - Create a dynamic material instance.
 *
 * Note: Creates UMaterialInstanceDynamic which persists only in current session.
 * For persistent material instances, use the Editor's asset creation tools.
 */
struct AGENTBRIDGESCRIPTING_API FCreateMaterialInstanceCommand : FAgentCommandBase
{
	FCreateMaterialInstanceCommand() { Type = EAgentCommandType::CreateMaterialInstance; }

	/** Parent material asset path. */
	FString ParentMaterialPath;

	/** Name for the new instance (used for lookup). */
	FString InstanceName;

	/** Actor to own the material instance (for lifecycle management). */
	FString OwnerActorId;

	/** Initial scalar parameter values. */
	TMap<FString, float> ScalarParameters;

	/** Initial vector parameter values (as JSON objects with R,G,B,A). */
	TMap<FString, FString> VectorParameters;
};

/**
 * FSetMaterialParameterCommand - Set a parameter on a material instance.
 */
struct AGENTBRIDGESCRIPTING_API FSetMaterialParameterCommand : FAgentCommandBase
{
	FSetMaterialParameterCommand() { Type = EAgentCommandType::SetMaterialParameter; }

	/** Actor with the material or material instance name. */
	FString TargetId;

	/** Component name (if actor has multiple mesh components). */
	FString ComponentName;

	/** Material slot index (0-based). */
	int32 SlotIndex = 0;

	/** Parameter name. */
	FString ParameterName;

	/** Parameter type. */
	EMaterialParameterType ParameterType = EMaterialParameterType::Scalar;

	/** Value (scalar as number, vector as JSON object, texture as asset path). */
	FString Value;
};

/**
 * FApplyMaterialToActorCommand - Apply a material to an actor's mesh.
 */
struct AGENTBRIDGESCRIPTING_API FApplyMaterialToActorCommand : FAgentCommandBase
{
	FApplyMaterialToActorCommand() { Type = EAgentCommandType::ApplyMaterialToActor; }

	/** Target actor. */
	FString ActorId;

	/** Component name (optional, uses first mesh component if empty). */
	FString ComponentName;

	/** Material asset path or instance name. */
	FString MaterialPath;

	/** Material slot index (-1 for all slots). */
	int32 SlotIndex = -1;
};

//~==============================================================================
// PCG Commands
//~==============================================================================

/**
 * FListPCGActorsCommand - List PCG actors in the world.
 */
struct AGENTBRIDGESCRIPTING_API FListPCGActorsCommand : FAgentCommandBase
{
	FListPCGActorsCommand() { Type = EAgentCommandType::ListPCGActors; }

	/** Wildcard filter for actor names. */
	FString NamePattern;

	/** Include graph info. */
	bool bIncludeGraphInfo = true;

	/** Maximum results. */
	int32 Limit = 100;
};

/**
 * FRegeneratePCGCommand - Trigger PCG regeneration.
 */
struct AGENTBRIDGESCRIPTING_API FRegeneratePCGCommand : FAgentCommandBase
{
	FRegeneratePCGCommand() { Type = EAgentCommandType::RegeneratePCG; }

	/** PCG actor identifier (name, label, or GUID). */
	FString ActorId;

	/** Component name if actor has multiple PCG components. */
	FString ComponentName;

	/** Force full regeneration (vs incremental). */
	bool bForceRefresh = false;
};

/**
 * FSetPCGParameterCommand - Set a PCG graph parameter.
 *
 * PCG graphs expose parameters through their PCGGraphParametersStruct.
 * This command modifies those parameters before regeneration.
 */
struct AGENTBRIDGESCRIPTING_API FSetPCGParameterCommand : FAgentCommandBase
{
	FSetPCGParameterCommand() { Type = EAgentCommandType::SetPCGParameter; }

	/** PCG actor identifier. */
	FString ActorId;

	/** Parameter name. */
	FString ParameterName;

	/** Value (JSON encoded). */
	FString Value;

	/** Auto-regenerate after setting parameter. */
	bool bAutoRegenerate = true;
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

/**
 * FDataAssetInfo - Information about a DataAsset.
 */
struct AGENTBRIDGESCRIPTING_API FDataAssetInfo
{
	/** Asset path. */
	FString AssetPath;

	/** Asset name. */
	FString AssetName;

	/** Class name. */
	FString ClassName;

	/** Is this a DataTable. */
	bool bIsDataTable = false;

	/** Is this a PrimaryDataAsset. */
	bool bIsPrimaryDataAsset = false;

	/** Row count (for DataTables). */
	int32 RowCount = 0;

	/** Properties (name -> JSON value). */
	TMap<FString, FString> Properties;
};

/**
 * FDataTableRowInfo - Information about a DataTable row.
 */
struct AGENTBRIDGESCRIPTING_API FDataTableRowInfo
{
	/** Row name. */
	FString RowName;

	/** Row data (property name -> JSON value). */
	TMap<FString, FString> Data;
};

/**
 * FListDataAssetsResponse - Response to ListDataAssets command.
 */
struct AGENTBRIDGESCRIPTING_API FListDataAssetsResponse : FAgentResponseBase
{
	/** Matching assets. */
	TArray<FDataAssetInfo> Assets;

	/** Total count. */
	int32 TotalCount = 0;
};

/**
 * FGetDataAssetResponse - Response to GetDataAsset command.
 */
struct AGENTBRIDGESCRIPTING_API FGetDataAssetResponse : FAgentResponseBase
{
	/** Asset info. */
	FDataAssetInfo Asset;
};

/**
 * FGetDataTableRowResponse - Response to GetDataTableRow command.
 */
struct AGENTBRIDGESCRIPTING_API FGetDataTableRowResponse : FAgentResponseBase
{
	/** Row struct type name. */
	FString RowStructName;

	/** Rows data. */
	TArray<FDataTableRowInfo> Rows;

	/** Total row count in table. */
	int32 TotalRowCount = 0;
};

/**
 * FGetCapabilitiesResponse - Response to GetCapabilities command.
 *
 * Reports what operations are available in the current world context.
 * This allows agents to adapt their behavior based on whether they're
 * running in Editor, PIE, or packaged game contexts.
 */
struct AGENTBRIDGESCRIPTING_API FGetCapabilitiesResponse : FAgentResponseBase
{
	// Context identification
	FString WorldType;           // "Editor", "PIE", "Game", "EditorPreview", "None"
	FString WorldName;           // Human-readable world name
	bool bIsGameplayActive = false;  // True if HasBegunPlay (PIE or Game)
	int32 PIEInstance = -1;      // PIE instance number (-1 if not PIE)

	// Core reflection (always available)
	bool bCanIterateProperties = true;
	bool bCanInvokeFunctions = true;
	bool bCanSpawnActors = true;
	bool bCanDestroyActors = true;
	bool bCanModifyTransforms = true;
	bool bCanModifyProperties = true;

	// Editor-only features
	bool bCanSetActorLabel = false;
	bool bCanSetActorFolder = false;
	bool bCanUseTransactions = false;
	bool bHasPropertyMetadata = false;
	bool bCanAccessEditorWorld = false;

	// Explanations for unavailable features
	FString LabelUnavailableReason;
	FString FolderUnavailableReason;
	FString TransactionUnavailableReason;
	FString MetadataUnavailableReason;
};

/**
 * FCaptureViewportResponse - Response to CaptureViewport command.
 */
struct AGENTBRIDGESCRIPTING_API FCaptureViewportResponse : FAgentResponseBase
{
	/** File path if saved to disk. */
	FString FilePath;

	/** Base64-encoded image data if not saved to file. */
	FString ImageData;

	/** Image format (PNG, JPG, EXR). */
	FString Format;

	/** Image width. */
	int32 Width = 0;

	/** Image height. */
	int32 Height = 0;

	/** Size in bytes. */
	int64 SizeBytes = 0;
};

/**
 * FCaptureSceneResponse - Response to CaptureScene command.
 */
struct AGENTBRIDGESCRIPTING_API FCaptureSceneResponse : FAgentResponseBase
{
	/** File path if saved to disk. */
	FString FilePath;

	/** Base64-encoded image data if not saved to file. */
	FString ImageData;

	/** Image format (PNG, JPG). */
	FString Format;

	/** Image width. */
	int32 Width = 0;

	/** Image height. */
	int32 Height = 0;

	/** Size in bytes. */
	int64 SizeBytes = 0;

	/** Camera location used for capture. */
	FVector CameraLocation = FVector::ZeroVector;

	/** Camera rotation used for capture. */
	FRotator CameraRotation = FRotator::ZeroRotator;
};

/**
 * FAudioAnalysisResponse - Response to GetAudioAnalysis command.
 */
struct AGENTBRIDGESCRIPTING_API FAudioAnalysisResponse : FAgentResponseBase
{
	/** Frequency band values (normalized 0-1). */
	TArray<float> FrequencyBands;

	/** Current average volume (0-1). */
	float AverageVolume = 0.0f;

	/** Peak volume in this sample (0-1). */
	float PeakVolume = 0.0f;

	/** Whether a beat was detected (simple beat detection). */
	bool bBeatDetected = false;

	/** Current playback position in seconds (for audio components). */
	float CurrentTime = 0.0f;
};

/**
 * FStartAudioCaptureResponse - Response to StartAudioCapture command.
 */
struct AGENTBRIDGESCRIPTING_API FStartAudioCaptureResponse : FAgentResponseBase
{
	/** Unique capture ID for stopping/retrieving later. */
	FString CaptureId;

	/** Actual sample rate being used. */
	int32 SampleRate = 0;

	/** Actual number of channels. */
	int32 Channels = 0;

	/** Maximum duration in seconds. */
	float MaxDuration = 0.0f;
};

/**
 * FStopAudioCaptureResponse - Response to StopAudioCapture command.
 */
struct AGENTBRIDGESCRIPTING_API FStopAudioCaptureResponse : FAgentResponseBase
{
	/** File path if saved to disk. */
	FString FilePath;

	/** Base64-encoded WAV data if not saved to file. */
	FString AudioData;

	/** Audio format (WAV). */
	FString Format = TEXT("WAV");

	/** Duration in seconds. */
	float Duration = 0.0f;

	/** Sample rate. */
	int32 SampleRate = 0;

	/** Number of channels. */
	int32 Channels = 0;

	/** Size in bytes. */
	int64 SizeBytes = 0;
};

//~==============================================================================
// Material Response Structures
//~==============================================================================

/**
 * FMaterialParameterInfo - Information about a material parameter.
 */
struct AGENTBRIDGESCRIPTING_API FMaterialParameterInfo
{
	/** Parameter name. */
	FString Name;

	/** Parameter type (Scalar, Vector, Texture). */
	FString Type;

	/** Current value as string. */
	FString Value;

	/** Parameter group. */
	FString Group;
};

/**
 * FMaterialInfo - Information about a material.
 */
struct AGENTBRIDGESCRIPTING_API FMaterialInfo
{
	/** Material asset path. */
	FString AssetPath;

	/** Material name. */
	FString Name;

	/** Whether this is a material instance. */
	bool bIsMaterialInstance = false;

	/** Parent material (if instance). */
	FString ParentPath;

	/** Two-sided rendering. */
	bool bTwoSided = false;

	/** Blend mode as string. */
	FString BlendMode;
};

/**
 * FListMaterialsResponse - Response to ListMaterials command.
 */
struct AGENTBRIDGESCRIPTING_API FListMaterialsResponse : FAgentResponseBase
{
	/** List of materials. */
	TArray<FMaterialInfo> Materials;

	/** Total count (may exceed limit). */
	int32 TotalCount = 0;
};

/**
 * FGetMaterialInfoResponse - Response to GetMaterialInfo command.
 */
struct AGENTBRIDGESCRIPTING_API FGetMaterialInfoResponse : FAgentResponseBase
{
	/** Material information. */
	FMaterialInfo Material;

	/** Parameters (if requested). */
	TArray<FMaterialParameterInfo> Parameters;
};

/**
 * FCreateMaterialInstanceResponse - Response to CreateMaterialInstance command.
 */
struct AGENTBRIDGESCRIPTING_API FCreateMaterialInstanceResponse : FAgentResponseBase
{
	/** Instance name (for lookup). */
	FString InstanceName;

	/** Whether it's applied to owner actor. */
	bool bAppliedToOwner = false;
};

//~==============================================================================
// PCG Response Structures
//~==============================================================================

/**
 * FPCGActorInfo - Information about a PCG actor.
 */
struct AGENTBRIDGESCRIPTING_API FPCGActorInfo
{
	/** Actor GUID. */
	FString Guid;

	/** Actor name. */
	FString Name;

	/** Actor label. */
	FString Label;

	/** Graph name. */
	FString GraphName;

	/** Is generated. */
	bool bIsGenerated = false;

	/** Generation status. */
	FString Status;
};

/**
 * FListPCGActorsResponse - Response to ListPCGActors command.
 */
struct AGENTBRIDGESCRIPTING_API FListPCGActorsResponse : FAgentResponseBase
{
	/** List of PCG actors. */
	TArray<FPCGActorInfo> Actors;
};

/**
 * FRegeneratePCGResponse - Response to RegeneratePCG command.
 */
struct AGENTBRIDGESCRIPTING_API FRegeneratePCGResponse : FAgentResponseBase
{
	/** Number of points/instances generated. */
	int32 GeneratedCount = 0;

	/** Generation time in milliseconds. */
	double GenerationTimeMs = 0.0;
};
