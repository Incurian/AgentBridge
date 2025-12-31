#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TempoScriptable.h"
#include "TempoScriptingServer.h"
#include "AgentBridgeServiceSubsystem.generated.h"

// Forward declarations for generated proto types
namespace AgentBridgeServer
{
	class ListWorldsRequest;
	class ListWorldsResponse;
	class SetTargetWorldRequest;
	class QueryActorsRequest;
	class QueryActorsResponse;
	class GetActorRequest;
	class GetActorResponse;
	class SpawnActorRequest;
	class SpawnActorResponse;
	class DeleteActorRequest;
	class SetActorTransformRequest;
	class SetActorPropertiesRequest;
	class SetActorPropertiesResponse;
	class GetPropertyPathRequest;
	class GetPropertyPathResponse;
	class SetPropertyPathRequest;
	class CallFunctionRequest;
	class CallFunctionResponse;
	class FindClassRequest;
	class FindClassResponse;
	class GetClassSchemaRequest;
	class GetClassSchemaResponse;
	class ListClassesRequest;
	class ListClassesResponse;
}

namespace TempoScripting
{
	class Empty;
}

/**
 * UAgentBridgeServiceSubsystem - gRPC service subsystem for AgentBridge.
 *
 * This subsystem exposes the AgentBridge API via gRPC using Tempo's
 * TempoScripting infrastructure. It bridges between the gRPC service
 * handlers and the existing CommandExecutor/Runtime layer.
 *
 * The subsystem:
 * - Implements ITempoScriptable to register with FTempoScriptingServer
 * - Handles all gRPC requests on the game thread
 * - Reuses existing FCommandExecutor logic for actual operations
 */
UCLASS()
class AGENTBRIDGESERVER_API UAgentBridgeServiceSubsystem :
	public UWorldSubsystem,
	public ITempoScriptable
{
	GENERATED_BODY()

public:
	//~==============================================================================
	// USubsystem Interface
	//~==============================================================================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	//~==============================================================================
	// ITempoScriptable Interface
	//~==============================================================================

	virtual void RegisterScriptingServices(FTempoScriptingServer& ScriptingServer) override;

	//~==============================================================================
	// gRPC Service Handlers - World Operations
	//~==============================================================================

	void ListWorlds(
		const AgentBridgeServer::ListWorldsRequest& Request,
		const TResponseDelegate<AgentBridgeServer::ListWorldsResponse>& ResponseContinuation);

	void SetTargetWorld(
		const AgentBridgeServer::SetTargetWorldRequest& Request,
		const TResponseDelegate<TempoScripting::Empty>& ResponseContinuation);

	//~==============================================================================
	// gRPC Service Handlers - Actor Discovery
	//~==============================================================================

	void QueryActors(
		const AgentBridgeServer::QueryActorsRequest& Request,
		const TResponseDelegate<AgentBridgeServer::QueryActorsResponse>& ResponseContinuation);

	void GetActor(
		const AgentBridgeServer::GetActorRequest& Request,
		const TResponseDelegate<AgentBridgeServer::GetActorResponse>& ResponseContinuation);

	//~==============================================================================
	// gRPC Service Handlers - Actor Manipulation
	//~==============================================================================

	void SpawnActor(
		const AgentBridgeServer::SpawnActorRequest& Request,
		const TResponseDelegate<AgentBridgeServer::SpawnActorResponse>& ResponseContinuation);

	void DeleteActor(
		const AgentBridgeServer::DeleteActorRequest& Request,
		const TResponseDelegate<TempoScripting::Empty>& ResponseContinuation);

	void SetActorTransform(
		const AgentBridgeServer::SetActorTransformRequest& Request,
		const TResponseDelegate<TempoScripting::Empty>& ResponseContinuation);

	void SetActorProperties(
		const AgentBridgeServer::SetActorPropertiesRequest& Request,
		const TResponseDelegate<AgentBridgeServer::SetActorPropertiesResponse>& ResponseContinuation);

	//~==============================================================================
	// gRPC Service Handlers - Property Path Operations
	//~==============================================================================

	void GetPropertyPath(
		const AgentBridgeServer::GetPropertyPathRequest& Request,
		const TResponseDelegate<AgentBridgeServer::GetPropertyPathResponse>& ResponseContinuation);

	void SetPropertyPath(
		const AgentBridgeServer::SetPropertyPathRequest& Request,
		const TResponseDelegate<TempoScripting::Empty>& ResponseContinuation);

	//~==============================================================================
	// gRPC Service Handlers - Function Invocation
	//~==============================================================================

	void CallFunction(
		const AgentBridgeServer::CallFunctionRequest& Request,
		const TResponseDelegate<AgentBridgeServer::CallFunctionResponse>& ResponseContinuation);

	//~==============================================================================
	// gRPC Service Handlers - Type Discovery
	//~==============================================================================

	void FindClass(
		const AgentBridgeServer::FindClassRequest& Request,
		const TResponseDelegate<AgentBridgeServer::FindClassResponse>& ResponseContinuation);

	void GetClassSchema(
		const AgentBridgeServer::GetClassSchemaRequest& Request,
		const TResponseDelegate<AgentBridgeServer::GetClassSchemaResponse>& ResponseContinuation);

	void ListClasses(
		const AgentBridgeServer::ListClassesRequest& Request,
		const TResponseDelegate<AgentBridgeServer::ListClassesResponse>& ResponseContinuation);
};
