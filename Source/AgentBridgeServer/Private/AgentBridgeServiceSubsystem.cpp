#include "AgentBridgeServiceSubsystem.h"
#include "CommandExecutor.h"
#include "AgentCommands.h"
#include "WorldContextManager.h"
#include "ActorOperations.h"
#include "TempoScriptingServer.h"

// gRPC includes
#include <grpcpp/grpcpp.h>

// Generated proto headers (created by GenProtos.sh during build)
#include "AgentBridgeServer/AgentBridge.pb.h"
#include "AgentBridgeServer/AgentBridge.grpc.pb.h"
#include "TempoScripting/Empty.pb.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAgentBridgeGrpc, Log, All);
DEFINE_LOG_CATEGORY(LogAgentBridgeGrpc);

using namespace AgentBridgeServer;
using AgentBridgeAsyncService = AgentBridgeService::AsyncService;

//~==============================================================================
// USubsystem Interface
//~==============================================================================

bool UAgentBridgeServiceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Create in editor and PIE worlds
	if (UWorld* World = Cast<UWorld>(Outer))
	{
		return World->WorldType == EWorldType::Editor ||
			   World->WorldType == EWorldType::PIE ||
			   World->WorldType == EWorldType::Game;
	}
	return false;
}

void UAgentBridgeServiceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogAgentBridgeGrpc, Log, TEXT("AgentBridge gRPC service activating for world: %s"),
		*GetWorld()->GetName());

	FTempoScriptingServer::Get().ActivateService<AgentBridgeService>(this);
}

void UAgentBridgeServiceSubsystem::Deinitialize()
{
	UE_LOG(LogAgentBridgeGrpc, Log, TEXT("AgentBridge gRPC service deactivating"));

	FTempoScriptingServer::Get().DeactivateService<AgentBridgeService>();

	Super::Deinitialize();
}

//~==============================================================================
// ITempoScriptable Interface
//~==============================================================================

void UAgentBridgeServiceSubsystem::RegisterScriptingServices(FTempoScriptingServer& ScriptingServer)
{
	ScriptingServer.RegisterService<AgentBridgeService>(
		// World Operations
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestListWorlds,
			&UAgentBridgeServiceSubsystem::ListWorlds),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestSetTargetWorld,
			&UAgentBridgeServiceSubsystem::SetTargetWorld),

		// Actor Discovery
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestQueryActors,
			&UAgentBridgeServiceSubsystem::QueryActors),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestGetActor,
			&UAgentBridgeServiceSubsystem::GetActor),

		// Actor Manipulation
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestSpawnActor,
			&UAgentBridgeServiceSubsystem::SpawnActor),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestDeleteActor,
			&UAgentBridgeServiceSubsystem::DeleteActor),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestSetActorTransform,
			&UAgentBridgeServiceSubsystem::SetActorTransform),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestSetActorProperties,
			&UAgentBridgeServiceSubsystem::SetActorProperties),

		// Property Path Operations
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestGetPropertyPath,
			&UAgentBridgeServiceSubsystem::GetPropertyPath),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestSetPropertyPath,
			&UAgentBridgeServiceSubsystem::SetPropertyPath),

		// Function Invocation
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestCallFunction,
			&UAgentBridgeServiceSubsystem::CallFunction),

		// Type Discovery
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestFindClass,
			&UAgentBridgeServiceSubsystem::FindClass),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestGetClassSchema,
			&UAgentBridgeServiceSubsystem::GetClassSchema),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestListClasses,
			&UAgentBridgeServiceSubsystem::ListClasses)
	);
}

//~==============================================================================
// Helper Functions
//~==============================================================================

namespace
{
	// Convert FVector to proto Vector
	void SetProtoVector(TempoScripting::Vector* Proto, const FVector& V)
	{
		Proto->set_x(V.X);
		Proto->set_y(V.Y);
		Proto->set_z(V.Z);
	}

	// Convert FRotator to proto Rotation (r=roll, p=pitch, y=yaw)
	void SetProtoRotation(TempoScripting::Rotation* Proto, const FRotator& R)
	{
		Proto->set_r(R.Roll);
		Proto->set_p(R.Pitch);
		Proto->set_y(R.Yaw);
	}

	// Convert FVector to proto Scale
	void SetProtoScale(Scale* Proto, const FVector& S)
	{
		Proto->set_x(S.X);
		Proto->set_y(S.Y);
		Proto->set_z(S.Z);
	}

	// Convert FTransform to proto ActorTransform
	void SetProtoTransform(ActorTransform* Proto, const FTransform& T)
	{
		SetProtoVector(Proto->mutable_location(), T.GetLocation());
		SetProtoRotation(Proto->mutable_rotation(), T.Rotator());
		SetProtoScale(Proto->mutable_scale(), T.GetScale3D());
	}

	// Convert proto Vector to FVector
	FVector FromProtoVector(const TempoScripting::Vector& V)
	{
		return FVector(V.x(), V.y(), V.z());
	}

	// Convert proto Rotation to FRotator
	FRotator FromProtoRotation(const TempoScripting::Rotation& R)
	{
		return FRotator(R.p(), R.y(), R.r()); // Pitch, Yaw, Roll
	}

	// Convert proto Scale to FVector
	FVector FromProtoScale(const Scale& S)
	{
		return FVector(S.x(), S.y(), S.z());
	}

	// Convert proto ActorTransform to FTransform
	FTransform FromProtoTransform(const ActorTransform& T)
	{
		FTransform Result;
		Result.SetLocation(FromProtoVector(T.location()));
		Result.SetRotation(FromProtoRotation(T.rotation()).Quaternion());
		if (T.has_scale())
		{
			Result.SetScale3D(FromProtoScale(T.scale()));
		}
		return Result;
	}

	// Fill ActorDescriptor from FActorInfo
	void FillActorDescriptor(ActorDescriptor* Desc, const FActorInfo& Info)
	{
		Desc->set_guid(TCHAR_TO_UTF8(*Info.Guid));
		Desc->set_path(TCHAR_TO_UTF8(*Info.Path));
		Desc->set_name(TCHAR_TO_UTF8(*Info.Name));
		Desc->set_label(TCHAR_TO_UTF8(*Info.Label));
		Desc->set_class_name(TCHAR_TO_UTF8(*Info.ClassName));
		Desc->set_is_hidden(Info.bHidden);
		Desc->set_parent_actor_id(TCHAR_TO_UTF8(*Info.ParentActorId));

		ActorTransform* Transform = Desc->mutable_transform();
		SetProtoVector(Transform->mutable_location(), Info.Location);
		SetProtoRotation(Transform->mutable_rotation(), Info.Rotation);
		SetProtoScale(Transform->mutable_scale(), Info.Scale);
	}
}

//~==============================================================================
// World Operations
//~==============================================================================

void UAgentBridgeServiceSubsystem::ListWorlds(
	const ListWorldsRequest& Request,
	const TResponseDelegate<ListWorldsResponse>& ResponseContinuation)
{
	FListWorldsCommand Cmd;
	FListWorldsResponse CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	ListWorldsResponse Response;
	Response.set_current_world_index(CmdResponse.CurrentWorldIndex);

	for (const FWorldInfo& World : CmdResponse.Worlds)
	{
		WorldInfo* Info = Response.add_worlds();
		Info->set_world_type(TCHAR_TO_UTF8(*World.WorldType));
		Info->set_world_name(TCHAR_TO_UTF8(*World.WorldName));
		Info->set_pie_instance(World.PIEInstance);
		Info->set_has_begun_play(World.bHasBegunPlay);
		Info->set_actor_count(World.ActorCount);
	}

	ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
}

void UAgentBridgeServiceSubsystem::SetTargetWorld(
	const SetTargetWorldRequest& Request,
	const TResponseDelegate<TempoScripting::Empty>& ResponseContinuation)
{
	FSetTargetWorldCommand Cmd;
	Cmd.WorldIdentifier = UTF8_TO_TCHAR(Request.world_identifier().c_str());

	FAgentResponseBase CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	TempoScripting::Empty Response;

	if (CmdResponse.bSuccess)
	{
		ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
	}
	else
	{
		ResponseContinuation.ExecuteIfBound(Response,
			grpc::Status(grpc::NOT_FOUND, TCHAR_TO_UTF8(*CmdResponse.ErrorMessage)));
	}
}

//~==============================================================================
// Actor Discovery
//~==============================================================================

void UAgentBridgeServiceSubsystem::QueryActors(
	const QueryActorsRequest& Request,
	const TResponseDelegate<QueryActorsResponse>& ResponseContinuation)
{
	FQueryActorsCommand Cmd;
	Cmd.ClassName = UTF8_TO_TCHAR(Request.class_name().c_str());
	Cmd.NamePattern = UTF8_TO_TCHAR(Request.name_pattern().c_str());
	Cmd.Tag = UTF8_TO_TCHAR(Request.tag().c_str());
	Cmd.Limit = Request.limit() > 0 ? Request.limit() : 1000;
	Cmd.bIncludeHidden = Request.include_hidden();

	FQueryActorsResponse CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	QueryActorsResponse Response;
	Response.set_total_count(CmdResponse.TotalCount);

	for (const FActorInfo& Actor : CmdResponse.Actors)
	{
		FillActorDescriptor(Response.add_actors(), Actor);
	}

	if (CmdResponse.bSuccess)
	{
		ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
	}
	else
	{
		ResponseContinuation.ExecuteIfBound(Response,
			grpc::Status(grpc::INTERNAL, TCHAR_TO_UTF8(*CmdResponse.ErrorMessage)));
	}
}

void UAgentBridgeServiceSubsystem::GetActor(
	const GetActorRequest& Request,
	const TResponseDelegate<GetActorResponse>& ResponseContinuation)
{
	FGetActorCommand Cmd;
	Cmd.ActorId = UTF8_TO_TCHAR(Request.actor_id().c_str());
	Cmd.bIncludeProperties = Request.include_properties();
	Cmd.bIncludeComponents = Request.include_components();
	Cmd.PropertyDepth = Request.property_depth();

	FGetActorResponse CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	GetActorResponse Response;

	if (CmdResponse.bSuccess)
	{
		ActorDetails* Details = Response.mutable_actor();
		FillActorDescriptor(Details->mutable_actor_info(), CmdResponse.Actor);

		// TODO: Fill properties and components

		ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
	}
	else
	{
		ResponseContinuation.ExecuteIfBound(Response,
			grpc::Status(grpc::NOT_FOUND, TCHAR_TO_UTF8(*CmdResponse.ErrorMessage)));
	}
}

//~==============================================================================
// Actor Manipulation
//~==============================================================================

void UAgentBridgeServiceSubsystem::SpawnActor(
	const SpawnActorRequest& Request,
	const TResponseDelegate<SpawnActorResponse>& ResponseContinuation)
{
	FSpawnActorCommand Cmd;
	Cmd.ClassName = UTF8_TO_TCHAR(Request.class_name().c_str());
	Cmd.Label = UTF8_TO_TCHAR(Request.label().c_str());
	Cmd.FolderPath = UTF8_TO_TCHAR(Request.folder_path().c_str());

	if (Request.has_transform())
	{
		const ActorTransform& T = Request.transform();
		Cmd.Location = FromProtoVector(T.location());
		Cmd.Rotation = FromProtoRotation(T.rotation());
		if (T.has_scale())
		{
			Cmd.Scale = FromProtoScale(T.scale());
		}
	}

	FSpawnActorResponse CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	SpawnActorResponse Response;

	if (CmdResponse.bSuccess)
	{
		FillActorDescriptor(Response.mutable_spawned_actor(), CmdResponse.Actor);
		ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
	}
	else
	{
		ResponseContinuation.ExecuteIfBound(Response,
			grpc::Status(grpc::INVALID_ARGUMENT, TCHAR_TO_UTF8(*CmdResponse.ErrorMessage)));
	}
}

void UAgentBridgeServiceSubsystem::DeleteActor(
	const DeleteActorRequest& Request,
	const TResponseDelegate<TempoScripting::Empty>& ResponseContinuation)
{
	FDeleteActorCommand Cmd;
	Cmd.ActorId = UTF8_TO_TCHAR(Request.actor_id().c_str());

	FAgentResponseBase CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	TempoScripting::Empty Response;

	if (CmdResponse.bSuccess)
	{
		ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
	}
	else
	{
		ResponseContinuation.ExecuteIfBound(Response,
			grpc::Status(grpc::NOT_FOUND, TCHAR_TO_UTF8(*CmdResponse.ErrorMessage)));
	}
}

void UAgentBridgeServiceSubsystem::SetActorTransform(
	const SetActorTransformRequest& Request,
	const TResponseDelegate<TempoScripting::Empty>& ResponseContinuation)
{
	FSetActorTransformCommand Cmd;
	Cmd.ActorId = UTF8_TO_TCHAR(Request.actor_id().c_str());
	Cmd.bSweep = Request.sweep();

	if (Request.has_transform())
	{
		const ActorTransform& T = Request.transform();
		Cmd.Location = FromProtoVector(T.location());
		Cmd.Rotation = FromProtoRotation(T.rotation());
		if (T.has_scale())
		{
			Cmd.Scale = FromProtoScale(T.scale());
		}
	}

	FAgentResponseBase CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	TempoScripting::Empty Response;

	if (CmdResponse.bSuccess)
	{
		ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
	}
	else
	{
		ResponseContinuation.ExecuteIfBound(Response,
			grpc::Status(grpc::NOT_FOUND, TCHAR_TO_UTF8(*CmdResponse.ErrorMessage)));
	}
}

void UAgentBridgeServiceSubsystem::SetActorProperties(
	const SetActorPropertiesRequest& Request,
	const TResponseDelegate<SetActorPropertiesResponse>& ResponseContinuation)
{
	FSetActorPropertiesCommand Cmd;
	Cmd.ActorId = UTF8_TO_TCHAR(Request.actor_id().c_str());

	// TODO: Convert PropertyValue proto to JSON strings
	// For now, just report success/failure

	FAgentResponseBase CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	SetActorPropertiesResponse Response;

	if (CmdResponse.bSuccess)
	{
		Response.set_properties_set(Request.properties_size());
		ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
	}
	else
	{
		ResponseContinuation.ExecuteIfBound(Response,
			grpc::Status(grpc::NOT_FOUND, TCHAR_TO_UTF8(*CmdResponse.ErrorMessage)));
	}
}

//~==============================================================================
// Property Path Operations
//~==============================================================================

void UAgentBridgeServiceSubsystem::GetPropertyPath(
	const GetPropertyPathRequest& Request,
	const TResponseDelegate<GetPropertyPathResponse>& ResponseContinuation)
{
	FGetPropertyPathCommand Cmd;
	Cmd.ActorId = UTF8_TO_TCHAR(Request.actor_id().c_str());
	Cmd.Path = UTF8_TO_TCHAR(Request.path().c_str());

	FPropertyValueResponse CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	GetPropertyPathResponse Response;

	if (CmdResponse.bSuccess)
	{
		Response.set_type_name(TCHAR_TO_UTF8(*CmdResponse.TypeName));
		// TODO: Convert value properly
		Response.mutable_value()->set_string_value(TCHAR_TO_UTF8(*CmdResponse.Value));
		ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
	}
	else
	{
		ResponseContinuation.ExecuteIfBound(Response,
			grpc::Status(grpc::NOT_FOUND, TCHAR_TO_UTF8(*CmdResponse.ErrorMessage)));
	}
}

void UAgentBridgeServiceSubsystem::SetPropertyPath(
	const SetPropertyPathRequest& Request,
	const TResponseDelegate<TempoScripting::Empty>& ResponseContinuation)
{
	FSetPropertyPathCommand Cmd;
	Cmd.ActorId = UTF8_TO_TCHAR(Request.actor_id().c_str());
	Cmd.Path = UTF8_TO_TCHAR(Request.path().c_str());

	// Convert PropertyValue to JSON string
	// TODO: Proper conversion
	if (Request.has_value())
	{
		Cmd.Value = UTF8_TO_TCHAR(Request.value().string_value().c_str());
	}

	FAgentResponseBase CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	TempoScripting::Empty Response;

	if (CmdResponse.bSuccess)
	{
		ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
	}
	else
	{
		ResponseContinuation.ExecuteIfBound(Response,
			grpc::Status(grpc::INVALID_ARGUMENT, TCHAR_TO_UTF8(*CmdResponse.ErrorMessage)));
	}
}

//~==============================================================================
// Function Invocation
//~==============================================================================

void UAgentBridgeServiceSubsystem::CallFunction(
	const CallFunctionRequest& Request,
	const TResponseDelegate<CallFunctionResponse>& ResponseContinuation)
{
	FCallFunctionCommand Cmd;
	Cmd.ActorId = UTF8_TO_TCHAR(Request.actor_id().c_str());
	Cmd.ClassName = UTF8_TO_TCHAR(Request.class_name().c_str());
	Cmd.FunctionName = UTF8_TO_TCHAR(Request.function_name().c_str());

	// TODO: Convert parameters

	FFunctionCallResponse CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	CallFunctionResponse Response;

	if (CmdResponse.bSuccess)
	{
		// TODO: Convert return value and out params
		Response.mutable_return_value()->set_string_value(TCHAR_TO_UTF8(*CmdResponse.ReturnValue));
		ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
	}
	else
	{
		ResponseContinuation.ExecuteIfBound(Response,
			grpc::Status(grpc::INTERNAL, TCHAR_TO_UTF8(*CmdResponse.ErrorMessage)));
	}
}

//~==============================================================================
// Type Discovery
//~==============================================================================

void UAgentBridgeServiceSubsystem::FindClass(
	const FindClassRequest& Request,
	const TResponseDelegate<FindClassResponse>& ResponseContinuation)
{
	FFindClassCommand Cmd;
	Cmd.ClassName = UTF8_TO_TCHAR(Request.class_name().c_str());

	FAgentResponseBase CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	FindClassResponse Response;

	if (CmdResponse.bSuccess)
	{
		ClassInfo* Info = Response.mutable_class_info();
		Info->set_class_name(TCHAR_TO_UTF8(*Cmd.ClassName));
		// TODO: Fill in more info
		ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
	}
	else
	{
		ResponseContinuation.ExecuteIfBound(Response,
			grpc::Status(grpc::NOT_FOUND, TCHAR_TO_UTF8(*CmdResponse.ErrorMessage)));
	}
}

void UAgentBridgeServiceSubsystem::GetClassSchema(
	const GetClassSchemaRequest& Request,
	const TResponseDelegate<GetClassSchemaResponse>& ResponseContinuation)
{
	FGetClassSchemaCommand Cmd;
	Cmd.ClassName = UTF8_TO_TCHAR(Request.class_name().c_str());
	Cmd.bIncludeInherited = Request.include_inherited();
	Cmd.bIncludeFunctions = Request.include_functions();

	FAgentResponseBase CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	GetClassSchemaResponse Response;

	if (CmdResponse.bSuccess)
	{
		// TODO: Fill in schema details
		Response.mutable_schema()->mutable_class_info()->set_class_name(
			TCHAR_TO_UTF8(*Cmd.ClassName));
		ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
	}
	else
	{
		ResponseContinuation.ExecuteIfBound(Response,
			grpc::Status(grpc::NOT_FOUND, TCHAR_TO_UTF8(*CmdResponse.ErrorMessage)));
	}
}

void UAgentBridgeServiceSubsystem::ListClasses(
	const ListClassesRequest& Request,
	const TResponseDelegate<ListClassesResponse>& ResponseContinuation)
{
	FListClassesCommand Cmd;
	Cmd.BaseClassName = UTF8_TO_TCHAR(Request.base_class_name().c_str());
	Cmd.NamePattern = UTF8_TO_TCHAR(Request.name_pattern().c_str());
	Cmd.bIncludeBlueprint = Request.include_blueprint();
	Cmd.bIncludeAbstract = Request.include_abstract();
	Cmd.Limit = Request.limit() > 0 ? Request.limit() : 100;

	FListClassesResponse CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	ListClassesResponse Response;
	Response.set_total_count(CmdResponse.Classes.Num());

	for (const FClassInfo& Class : CmdResponse.Classes)
	{
		ClassInfo* Info = Response.add_classes();
		Info->set_class_name(TCHAR_TO_UTF8(*Class.ClassName));
		Info->set_display_name(TCHAR_TO_UTF8(*Class.DisplayName));
		Info->set_class_path(TCHAR_TO_UTF8(*Class.ClassPath));
		Info->set_parent_class_name(TCHAR_TO_UTF8(*Class.ParentClassName));
		Info->set_is_blueprint(Class.bIsBlueprint);
		Info->set_is_abstract(Class.bIsAbstract);
	}

	if (CmdResponse.bSuccess)
	{
		ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
	}
	else
	{
		ResponseContinuation.ExecuteIfBound(Response,
			grpc::Status(grpc::INTERNAL, TCHAR_TO_UTF8(*CmdResponse.ErrorMessage)));
	}
}
