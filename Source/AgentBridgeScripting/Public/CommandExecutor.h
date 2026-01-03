#pragma once

#include "CoreMinimal.h"
#include "AgentCommands.h"

/**
 * FCommandExecutor - Executes agent commands against the Runtime layer.
 *
 * This class is the main entry point for the AgentBridgeScripting module.
 * It receives commands (typically from gRPC), dispatches them to the
 * appropriate Runtime operations, and returns structured responses.
 *
 * Features:
 * - Command dispatch based on type
 * - Automatic timing of command execution
 * - Error handling and response wrapping
 * - Transaction support for undo/redo
 * - Batch command execution
 *
 * JSON Serialization:
 * - Commands and responses can be serialized to/from JSON
 * - Property values use JSON encoding for complex types
 * - This enables language-agnostic gRPC communication
 *
 * Thread Safety:
 * - All commands must be executed on the Game Thread
 * - Use AsyncTask or FGraphEvent for async execution from other threads
 *
 * Usage:
 * @code
 * FQueryActorsCommand Cmd;
 * Cmd.NamePattern = "Light";
 * Cmd.Limit = 10;
 *
 * FQueryActorsResponse Response;
 * FCommandExecutor::Execute(Cmd, Response);
 *
 * if (Response.bSuccess)
 * {
 *     for (const FActorInfo& Actor : Response.Actors)
 *     {
 *         UE_LOG(LogTemp, Log, TEXT("Found: %s"), *Actor.Name);
 *     }
 * }
 * @endcode
 *
 * @see AgentCommands.h for command and response structures
 */
class AGENTBRIDGESCRIPTING_API FCommandExecutor
{
public:
	//~==============================================================================
	// Generic Execution
	//~==============================================================================

	/**
	 * Executes a command from JSON and returns JSON response.
	 *
	 * This is the primary entry point for gRPC calls.
	 *
	 * @param CommandJson	JSON-encoded command.
	 * @return				JSON-encoded response.
	 */
	static FString ExecuteJson(const FString& CommandJson);

	/**
	 * Executes a batch of commands from JSON.
	 *
	 * @param CommandsJson	JSON array of commands.
	 * @param bStopOnError	Whether to stop on first error.
	 * @return				JSON array of responses.
	 */
	static FString ExecuteBatchJson(const FString& CommandsJson, bool bStopOnError = true);

	//~==============================================================================
	// Typed Execution - World Commands
	//~==============================================================================

	static void Execute(const FListWorldsCommand& Command, FListWorldsResponse& Response);
	static void Execute(const FSetTargetWorldCommand& Command, FAgentResponseBase& Response);
	static void Execute(const FGetCapabilitiesCommand& Command, FGetCapabilitiesResponse& Response);

	//~==============================================================================
	// Typed Execution - Actor Queries
	//~==============================================================================

	static void Execute(const FQueryActorsCommand& Command, FQueryActorsResponse& Response);
	static void Execute(const FGetActorCommand& Command, FGetActorResponse& Response);
	static void Execute(const FGetActorPropertiesCommand& Command, FPropertyValueResponse& Response);

	//~==============================================================================
	// Typed Execution - Actor Modifications
	//~==============================================================================

	static void Execute(const FSpawnActorCommand& Command, FSpawnActorResponse& Response);
	static void Execute(const FDeleteActorCommand& Command, FAgentResponseBase& Response);
	static void Execute(const FDuplicateActorCommand& Command, FSpawnActorResponse& Response);
	static void Execute(const FSetActorPropertiesCommand& Command, FAgentResponseBase& Response);
	static void Execute(const FSetActorTransformCommand& Command, FAgentResponseBase& Response);

	//~==============================================================================
	// Typed Execution - Property Paths
	//~==============================================================================

	static void Execute(const FGetPropertyPathCommand& Command, FPropertyValueResponse& Response);
	static void Execute(const FSetPropertyPathCommand& Command, FAgentResponseBase& Response);

	//~==============================================================================
	// Typed Execution - Functions
	//~==============================================================================

	static void Execute(const FCallFunctionCommand& Command, FFunctionCallResponse& Response);
	static void Execute(const FCallAssetFunctionCommand& Command, FCallAssetFunctionResponse& Response);
	static void Execute(const FGetFunctionSignatureCommand& Command, FAgentResponseBase& Response);

	//~==============================================================================
	// Typed Execution - Type Discovery
	//~==============================================================================

	static void Execute(const FFindClassCommand& Command, FAgentResponseBase& Response);
	static void Execute(const FGetClassSchemaCommand& Command, FGetClassSchemaResponse& Response);
	static void Execute(const FListClassesCommand& Command, FListClassesResponse& Response);

	//~==============================================================================
	// Typed Execution - DataAsset Commands
	//~==============================================================================

	static void Execute(const FListDataAssetsCommand& Command, FListDataAssetsResponse& Response);
	static void Execute(const FGetDataAssetCommand& Command, FGetDataAssetResponse& Response);
	static void Execute(const FGetDataTableRowCommand& Command, FGetDataTableRowResponse& Response);

	//~==============================================================================
	// Typed Execution - Capture Commands
	//~==============================================================================

	static void Execute(const FCaptureViewportCommand& Command, FCaptureViewportResponse& Response);
	static void Execute(const FCaptureSceneCommand& Command, FCaptureSceneResponse& Response);

	//~==============================================================================
	// Typed Execution - Audio Commands
	//~==============================================================================

	static void Execute(const FGetAudioAnalysisCommand& Command, FAudioAnalysisResponse& Response);
	static void Execute(const FStartAudioCaptureCommand& Command, FStartAudioCaptureResponse& Response);
	static void Execute(const FStopAudioCaptureCommand& Command, FStopAudioCaptureResponse& Response);

	//~==============================================================================
	// Typed Execution - Material Commands
	//~==============================================================================

	static void Execute(const FListMaterialsCommand& Command, FListMaterialsResponse& Response);
	static void Execute(const FGetMaterialInfoCommand& Command, FGetMaterialInfoResponse& Response);
	static void Execute(const FCreateMaterialInstanceCommand& Command, FCreateMaterialInstanceResponse& Response);
	static void Execute(const FSetMaterialParameterCommand& Command, FAgentResponseBase& Response);
	static void Execute(const FApplyMaterialToActorCommand& Command, FAgentResponseBase& Response);

	//~==============================================================================
	// Typed Execution - PCG Commands
	//~==============================================================================

	static void Execute(const FListPCGActorsCommand& Command, FListPCGActorsResponse& Response);
	static void Execute(const FRegeneratePCGCommand& Command, FRegeneratePCGResponse& Response);
	static void Execute(const FSetPCGParameterCommand& Command, FAgentResponseBase& Response);

	//~==============================================================================
	// Typed Execution - Asset Commands (P0)
	//~==============================================================================

	static void Execute(const FCreateAssetCommand& Command, FCreateAssetResponse& Response);
	static void Execute(const FSaveAssetCommand& Command, FSaveAssetResponse& Response);
	static void Execute(const FSaveActorAsBlueprintCommand& Command, FSaveActorAsBlueprintResponse& Response);
	static void Execute(const FDuplicateAssetCommand& Command, FDuplicateAssetResponse& Response);
	static void Execute(const FGetAssetThumbnailCommand& Command, FGetAssetThumbnailResponse& Response);

	//~==============================================================================
	// Typed Execution - Component Commands (P1)
	//~==============================================================================

	static void Execute(const FGetComponentTransformCommand& Command, FGetComponentTransformResponse& Response);
	static void Execute(const FSetComponentTransformCommand& Command, FAgentResponseBase& Response);
	static void Execute(const FAttachComponentCommand& Command, FAgentResponseBase& Response);
	static void Execute(const FAttachActorCommand& Command, FAgentResponseBase& Response);
	static void Execute(const FDetachComponentCommand& Command, FAgentResponseBase& Response);
	static void Execute(const FDetachActorCommand& Command, FAgentResponseBase& Response);

	//~==============================================================================
	// Typed Execution - File Commands (P1)
	//~==============================================================================

	static void Execute(const FReadProjectFileCommand& Command, FReadProjectFileResponse& Response);
	static void Execute(const FWriteProjectFileCommand& Command, FWriteProjectFileResponse& Response);
	static void Execute(const FListProjectDirectoryCommand& Command, FListProjectDirectoryResponse& Response);
	static void Execute(const FCopyProjectFileCommand& Command, FCopyProjectFileResponse& Response);
	static void Execute(const FDeleteProjectFileCommand& Command, FAgentResponseBase& Response);

	//~==============================================================================
	// Typed Execution - Blueprint Node Commands (P2)
	//~==============================================================================

	static void Execute(const FCreateBlueprintNodeCommand& Command, FCreateBlueprintNodeResponse& Response);
	static void Execute(const FConnectBlueprintPinsCommand& Command, FAgentResponseBase& Response);
	static void Execute(const FDisconnectBlueprintPinsCommand& Command, FAgentResponseBase& Response);
	static void Execute(const FDeleteBlueprintNodeCommand& Command, FAgentResponseBase& Response);
	static void Execute(const FListBlueprintNodesCommand& Command, FListBlueprintNodesResponse& Response);
	static void Execute(const FListBlueprintPinsCommand& Command, FListBlueprintPinsResponse& Response);

	//~==============================================================================
	// JSON Serialization Helpers
	//~==============================================================================

	/**
	 * Converts an FAgentPropertyValue to a JSON string.
	 */
	static FString PropertyValueToJson(const FAgentPropertyValue& Value);

	/**
	 * Parses a JSON string into an FAgentPropertyValue.
	 */
	static FAgentPropertyValue JsonToPropertyValue(const FString& Json, EAgentPropertyType TypeHint = EAgentPropertyType::None);

	/**
	 * Converts an FActorInfo to a JSON object string.
	 */
	static FString ActorInfoToJson(const FActorInfo& Info);

	/**
	 * Converts an FWorldInfo to a JSON object string.
	 */
	static FString WorldInfoToJson(const FWorldInfo& Info);

private:
	/**
	 * Populates an FActorInfo from an AActor.
	 */
	static FActorInfo BuildActorInfo(AActor* Actor, bool bIncludeProperties, bool bIncludeComponents, int32 PropertyDepth);

	/**
	 * Finds an actor by various identifiers.
	 */
	static AActor* ResolveActor(const FString& ActorId, FString* OutError = nullptr);

	/**
	 * Finds any UObject by identifier - supports both actors and assets.
	 * Following "tools should just work" philosophy: tries actor resolution first,
	 * then falls back to asset path loading.
	 *
	 * @param ObjectId	Actor name/label/path OR asset path (e.g., "/Game/Data/MyAsset.MyAsset")
	 * @param OutError	Optional error message on failure
	 * @return			The found UObject (actor or asset), or nullptr
	 */
	static UObject* ResolveObject(const FString& ObjectId, FString* OutError = nullptr);

	/**
	 * Starts execution timing.
	 */
	static double StartTiming();

	/**
	 * Ends timing and returns elapsed milliseconds.
	 */
	static double EndTiming(double StartTime);

	//~==============================================================================
	// Response Serialization
	//~==============================================================================

	static FString SerializeBaseResponse(const FAgentResponseBase& Response);
	static FString SerializeListWorldsResponse(const FListWorldsResponse& Response);
	static FString SerializeQueryActorsResponse(const FQueryActorsResponse& Response);
	static FString SerializeGetActorResponse(const FGetActorResponse& Response);
	static FString SerializeSpawnActorResponse(const FSpawnActorResponse& Response);
	static FString SerializePropertyValueResponse(const FPropertyValueResponse& Response);
	static FString SerializeFunctionCallResponse(const FFunctionCallResponse& Response);
	static FString SerializeListClassesResponse(const FListClassesResponse& Response);
	static FString SerializeGetCapabilitiesResponse(const FGetCapabilitiesResponse& Response);
	static FString SerializeListDataAssetsResponse(const FListDataAssetsResponse& Response);
	static FString SerializeGetDataAssetResponse(const FGetDataAssetResponse& Response);
	static FString SerializeGetDataTableRowResponse(const FGetDataTableRowResponse& Response);
	static FString SerializeCaptureViewportResponse(const FCaptureViewportResponse& Response);
	static FString SerializeCaptureSceneResponse(const FCaptureSceneResponse& Response);
	static FString SerializeAudioAnalysisResponse(const FAudioAnalysisResponse& Response);
	static FString SerializeStartAudioCaptureResponse(const FStartAudioCaptureResponse& Response);
	static FString SerializeStopAudioCaptureResponse(const FStopAudioCaptureResponse& Response);
	static FString SerializeListMaterialsResponse(const FListMaterialsResponse& Response);
	static FString SerializeGetMaterialInfoResponse(const FGetMaterialInfoResponse& Response);
	static FString SerializeCreateMaterialInstanceResponse(const FCreateMaterialInstanceResponse& Response);
	static FString SerializeListPCGActorsResponse(const FListPCGActorsResponse& Response);
	static FString SerializeRegeneratePCGResponse(const FRegeneratePCGResponse& Response);

	// Asset response serialization (P0)
	static FString SerializeCreateAssetResponse(const FCreateAssetResponse& Response);
	static FString SerializeSaveAssetResponse(const FSaveAssetResponse& Response);
	static FString SerializeSaveActorAsBlueprintResponse(const FSaveActorAsBlueprintResponse& Response);
	static FString SerializeDuplicateAssetResponse(const FDuplicateAssetResponse& Response);
	static FString SerializeGetAssetThumbnailResponse(const FGetAssetThumbnailResponse& Response);

	// Component response serialization (P1)
	static FString SerializeGetComponentTransformResponse(const FGetComponentTransformResponse& Response);

	// File response serialization (P1)
	static FString SerializeReadProjectFileResponse(const FReadProjectFileResponse& Response);
	static FString SerializeWriteProjectFileResponse(const FWriteProjectFileResponse& Response);
	static FString SerializeListProjectDirectoryResponse(const FListProjectDirectoryResponse& Response);
	static FString SerializeCopyProjectFileResponse(const FCopyProjectFileResponse& Response);

	// Blueprint node response serialization (P2)
	static FString SerializeCreateBlueprintNodeResponse(const FCreateBlueprintNodeResponse& Response);
	static FString SerializeListBlueprintNodesResponse(const FListBlueprintNodesResponse& Response);
	static FString SerializeListBlueprintPinsResponse(const FListBlueprintPinsResponse& Response);

	//~==============================================================================
	// File Path Validation (P1)
	//~==============================================================================

	/**
	 * Validates a relative path is safe for file operations.
	 * Returns true if the path is within allowed project directories.
	 */
	static bool IsPathAllowed(const FString& RelativePath, FString* OutError = nullptr);

	/**
	 * Converts a relative path to absolute project path.
	 * Returns empty if path is not allowed.
	 */
	static FString ToAbsoluteProjectPath(const FString& RelativePath, FString* OutError = nullptr);
};
