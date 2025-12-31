#include "AgentBridgeServiceSubsystem.h"
#include "CommandExecutor.h"
#include "AgentCommands.h"
#include "WorldContextManager.h"
#include "ActorOperations.h"
#include "TempoScriptingServer.h"

// JSON includes for property value conversion
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

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

	//--------------------------------------------------------------------------
	// PropertyValue Conversion Helpers
	//--------------------------------------------------------------------------

	// Convert JSON value string to proto PropertyValue
	// Handles the JSON format produced by CommandExecutor::PropertyValueToJson
	void JsonToProtoPropertyValue(const FString& JsonStr, const FString& TypeName, PropertyValue* OutValue)
	{
		if (JsonStr.IsEmpty() || JsonStr == TEXT("null"))
		{
			OutValue->set_type(PROPERTY_TYPE_NONE);
			return;
		}

		// Detect type from TypeName hint or JSON structure
		if (TypeName.Contains(TEXT("Bool")) || JsonStr == TEXT("true") || JsonStr == TEXT("false"))
		{
			OutValue->set_type(PROPERTY_TYPE_BOOL);
			OutValue->set_bool_value(JsonStr == TEXT("true"));
		}
		else if (TypeName.Contains(TEXT("Int")) || TypeName.Contains(TEXT("Byte")))
		{
			OutValue->set_type(PROPERTY_TYPE_INT);
			OutValue->set_int_value(FCString::Atoi64(*JsonStr));
		}
		else if (TypeName.Contains(TEXT("Float")) || TypeName.Contains(TEXT("Double")))
		{
			OutValue->set_type(PROPERTY_TYPE_FLOAT);
			OutValue->set_float_value(FCString::Atod(*JsonStr));
		}
		else if (TypeName.Contains(TEXT("Vector")) && JsonStr.StartsWith(TEXT("{")))
		{
			// Parse {"X":..., "Y":..., "Z":...}
			OutValue->set_type(PROPERTY_TYPE_VECTOR);
			TSharedPtr<FJsonObject> JsonObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
			if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
			{
				TempoScripting::Vector* Vec = OutValue->mutable_vector_value();
				Vec->set_x(JsonObj->GetNumberField(TEXT("X")));
				Vec->set_y(JsonObj->GetNumberField(TEXT("Y")));
				Vec->set_z(JsonObj->GetNumberField(TEXT("Z")));
			}
		}
		else if (TypeName.Contains(TEXT("Rotator")) && JsonStr.StartsWith(TEXT("{")))
		{
			// Parse {"Pitch":..., "Yaw":..., "Roll":...}
			OutValue->set_type(PROPERTY_TYPE_ROTATOR);
			TSharedPtr<FJsonObject> JsonObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
			if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
			{
				TempoScripting::Rotation* Rot = OutValue->mutable_rotation_value();
				Rot->set_p(JsonObj->GetNumberField(TEXT("Pitch")));
				Rot->set_y(JsonObj->GetNumberField(TEXT("Yaw")));
				Rot->set_r(JsonObj->GetNumberField(TEXT("Roll")));
			}
		}
		else if (TypeName.Contains(TEXT("Transform")) && JsonStr.StartsWith(TEXT("{")))
		{
			// Parse {"Location":{...}, "Rotation":{...}, "Scale":{...}}
			OutValue->set_type(PROPERTY_TYPE_TRANSFORM);
			TSharedPtr<FJsonObject> JsonObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
			if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
			{
				ActorTransform* T = OutValue->mutable_transform_value();
				if (const TSharedPtr<FJsonObject>* LocObj = nullptr; JsonObj->TryGetObjectField(TEXT("Location"), LocObj))
				{
					SetProtoVector(T->mutable_location(), FVector(
						(*LocObj)->GetNumberField(TEXT("X")),
						(*LocObj)->GetNumberField(TEXT("Y")),
						(*LocObj)->GetNumberField(TEXT("Z"))
					));
				}
				if (const TSharedPtr<FJsonObject>* RotObj = nullptr; JsonObj->TryGetObjectField(TEXT("Rotation"), RotObj))
				{
					T->mutable_rotation()->set_p((*RotObj)->GetNumberField(TEXT("Pitch")));
					T->mutable_rotation()->set_y((*RotObj)->GetNumberField(TEXT("Yaw")));
					T->mutable_rotation()->set_r((*RotObj)->GetNumberField(TEXT("Roll")));
				}
				if (const TSharedPtr<FJsonObject>* ScaleObj = nullptr; JsonObj->TryGetObjectField(TEXT("Scale"), ScaleObj))
				{
					SetProtoScale(T->mutable_scale(), FVector(
						(*ScaleObj)->GetNumberField(TEXT("X")),
						(*ScaleObj)->GetNumberField(TEXT("Y")),
						(*ScaleObj)->GetNumberField(TEXT("Z"))
					));
				}
			}
		}
		else if (TypeName.Contains(TEXT("Color")) && JsonStr.StartsWith(TEXT("{")))
		{
			// Parse {"r":..., "g":..., "b":..., "a":...}
			OutValue->set_type(PROPERTY_TYPE_COLOR);
			TSharedPtr<FJsonObject> JsonObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
			if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
			{
				Color* C = OutValue->mutable_color_value();
				C->set_r(static_cast<int32>(JsonObj->GetNumberField(TEXT("r")) * 255));
				C->set_g(static_cast<int32>(JsonObj->GetNumberField(TEXT("g")) * 255));
				C->set_b(static_cast<int32>(JsonObj->GetNumberField(TEXT("b")) * 255));
				C->set_a(static_cast<int32>(JsonObj->GetNumberField(TEXT("a")) * 255));
			}
		}
		else if (JsonStr.StartsWith(TEXT("[")))
		{
			// Array
			OutValue->set_type(PROPERTY_TYPE_ARRAY);
			TArray<TSharedPtr<FJsonValue>> JsonArray;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
			if (FJsonSerializer::Deserialize(Reader, JsonArray))
			{
				for (const TSharedPtr<FJsonValue>& Elem : JsonArray)
				{
					PropertyValue* ElemValue = OutValue->add_array_values();
					FString ElemStr;
					if (Elem->Type == EJson::String)
					{
						ElemStr = Elem->AsString();
						ElemValue->set_type(PROPERTY_TYPE_STRING);
						ElemValue->set_string_value(TCHAR_TO_UTF8(*ElemStr));
					}
					else if (Elem->Type == EJson::Number)
					{
						ElemValue->set_type(PROPERTY_TYPE_FLOAT);
						ElemValue->set_float_value(Elem->AsNumber());
					}
					else if (Elem->Type == EJson::Boolean)
					{
						ElemValue->set_type(PROPERTY_TYPE_BOOL);
						ElemValue->set_bool_value(Elem->AsBool());
					}
					else if (Elem->Type == EJson::Object)
					{
						// Nested object - recursively convert
						const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
						if (Elem->TryGetObject(ObjPtr) && ObjPtr && ObjPtr->IsValid())
						{
							ElemValue->set_type(PROPERTY_TYPE_STRUCT);
							for (const auto& KV : (*ObjPtr)->Values)
							{
								PropertyKeyValue* SubKV = ElemValue->add_struct_values();
								SubKV->set_key(TCHAR_TO_UTF8(*KV.Key));
								if (KV.Value->Type == EJson::String)
								{
									SubKV->mutable_value()->set_type(PROPERTY_TYPE_STRING);
									SubKV->mutable_value()->set_string_value(TCHAR_TO_UTF8(*KV.Value->AsString()));
								}
								else if (KV.Value->Type == EJson::Number)
								{
									SubKV->mutable_value()->set_type(PROPERTY_TYPE_FLOAT);
									SubKV->mutable_value()->set_float_value(KV.Value->AsNumber());
								}
								else
								{
									SubKV->mutable_value()->set_type(PROPERTY_TYPE_STRING);
									SubKV->mutable_value()->set_string_value("(complex)");
								}
							}
						}
					}
					else
					{
						// Other types - store as string placeholder
						ElemValue->set_type(PROPERTY_TYPE_STRING);
						ElemValue->set_string_value("(array/null)");
					}
				}
			}
		}
		else if (JsonStr.StartsWith(TEXT("{")))
		{
			// Struct/Map - store as key-value pairs
			OutValue->set_type(PROPERTY_TYPE_STRUCT);
			TSharedPtr<FJsonObject> JsonObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
			if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
			{
				for (const auto& Pair : JsonObj->Values)
				{
					PropertyKeyValue* KV = OutValue->add_struct_values();
					KV->set_key(TCHAR_TO_UTF8(*Pair.Key));

					FString ValStr;
					if (Pair.Value->Type == EJson::String)
					{
						ValStr = Pair.Value->AsString();
						KV->mutable_value()->set_type(PROPERTY_TYPE_STRING);
						KV->mutable_value()->set_string_value(TCHAR_TO_UTF8(*ValStr));
					}
					else if (Pair.Value->Type == EJson::Number)
					{
						KV->mutable_value()->set_type(PROPERTY_TYPE_FLOAT);
						KV->mutable_value()->set_float_value(Pair.Value->AsNumber());
					}
					else if (Pair.Value->Type == EJson::Boolean)
					{
						KV->mutable_value()->set_type(PROPERTY_TYPE_BOOL);
						KV->mutable_value()->set_bool_value(Pair.Value->AsBool());
					}
					else if (Pair.Value->Type == EJson::Object)
					{
						// Nested object - store as struct
						const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
						if (Pair.Value->TryGetObject(ObjPtr) && ObjPtr && ObjPtr->IsValid())
						{
							KV->mutable_value()->set_type(PROPERTY_TYPE_STRUCT);
							for (const auto& SubPair : (*ObjPtr)->Values)
							{
								PropertyKeyValue* SubKV = KV->mutable_value()->add_struct_values();
								SubKV->set_key(TCHAR_TO_UTF8(*SubPair.Key));
								if (SubPair.Value->Type == EJson::Number)
								{
									SubKV->mutable_value()->set_type(PROPERTY_TYPE_FLOAT);
									SubKV->mutable_value()->set_float_value(SubPair.Value->AsNumber());
								}
								else
								{
									SubKV->mutable_value()->set_type(PROPERTY_TYPE_STRING);
									SubKV->mutable_value()->set_string_value(
										SubPair.Value->Type == EJson::String ?
										TCHAR_TO_UTF8(*SubPair.Value->AsString()) : "(complex)");
								}
							}
						}
					}
					else
					{
						// Other types - store as placeholder
						KV->mutable_value()->set_type(PROPERTY_TYPE_STRING);
						KV->mutable_value()->set_string_value("(array/null)");
					}
				}
			}
		}
		else if (JsonStr.StartsWith(TEXT("\"")) && JsonStr.EndsWith(TEXT("\"")))
		{
			// Quoted string
			OutValue->set_type(PROPERTY_TYPE_STRING);
			FString Unquoted = JsonStr.Mid(1, JsonStr.Len() - 2);
			OutValue->set_string_value(TCHAR_TO_UTF8(*Unquoted));
		}
		else if (JsonStr.IsNumeric() || (JsonStr.StartsWith(TEXT("-")) && JsonStr.Mid(1).IsNumeric()))
		{
			// Numeric - check for decimal
			if (JsonStr.Contains(TEXT(".")))
			{
				OutValue->set_type(PROPERTY_TYPE_FLOAT);
				OutValue->set_float_value(FCString::Atod(*JsonStr));
			}
			else
			{
				OutValue->set_type(PROPERTY_TYPE_INT);
				OutValue->set_int_value(FCString::Atoi64(*JsonStr));
			}
		}
		else
		{
			// Default to string
			OutValue->set_type(PROPERTY_TYPE_STRING);
			OutValue->set_string_value(TCHAR_TO_UTF8(*JsonStr));
		}
	}

	// Convert proto PropertyValue to JSON string for CommandExecutor
	FString ProtoPropertyValueToJson(const PropertyValue& Value)
	{
		switch (Value.type())
		{
		case PROPERTY_TYPE_BOOL:
			return Value.bool_value() ? TEXT("true") : TEXT("false");
		case PROPERTY_TYPE_INT:
			return FString::Printf(TEXT("%lld"), Value.int_value());
		case PROPERTY_TYPE_FLOAT:
			return FString::Printf(TEXT("%f"), Value.float_value());
		case PROPERTY_TYPE_STRING:
		case PROPERTY_TYPE_NAME:
			return FString::Printf(TEXT("\"%s\""), UTF8_TO_TCHAR(Value.string_value().c_str()));
		case PROPERTY_TYPE_VECTOR:
			return FString::Printf(TEXT("{\"X\":%f,\"Y\":%f,\"Z\":%f}"),
				Value.vector_value().x(), Value.vector_value().y(), Value.vector_value().z());
		case PROPERTY_TYPE_ROTATOR:
			return FString::Printf(TEXT("{\"Pitch\":%f,\"Yaw\":%f,\"Roll\":%f}"),
				Value.rotation_value().p(), Value.rotation_value().y(), Value.rotation_value().r());
		case PROPERTY_TYPE_COLOR:
			return FString::Printf(TEXT("{\"r\":%f,\"g\":%f,\"b\":%f,\"a\":%f}"),
				Value.color_value().r() / 255.0, Value.color_value().g() / 255.0,
				Value.color_value().b() / 255.0, Value.color_value().a() / 255.0);
		default:
			return UTF8_TO_TCHAR(Value.string_value().c_str());
		}
	}

	// Fill ComponentDescriptor from component info map entry
	void FillComponentDescriptor(ComponentDescriptor* Desc, const FString& Name, const FString& ClassName)
	{
		Desc->set_name(TCHAR_TO_UTF8(*Name));
		Desc->set_class_name(TCHAR_TO_UTF8(*ClassName));
		Desc->set_is_scene_component(ClassName.Contains(TEXT("SceneComponent")));
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

		// Fill properties (already as JSON strings from CommandExecutor)
		for (const auto& Pair : CmdResponse.Actor.Properties)
		{
			PropertyKeyValue* KV = Details->add_properties();
			KV->set_key(TCHAR_TO_UTF8(*Pair.Key));
			JsonToProtoPropertyValue(Pair.Value, TEXT(""), KV->mutable_value());
		}

		// Fill components
		for (const auto& Pair : CmdResponse.Actor.Components)
		{
			FillComponentDescriptor(Details->add_components(), Pair.Key, Pair.Value);
		}

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

	// Convert PropertyValue proto to JSON strings
	for (const auto& Prop : Request.properties())
	{
		FString Key = UTF8_TO_TCHAR(Prop.key().c_str());
		FString JsonValue = ProtoPropertyValueToJson(Prop.value());
		Cmd.Properties.Add(Key, JsonValue);
	}

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
		// Convert JSON value to typed PropertyValue using type hint
		JsonToProtoPropertyValue(CmdResponse.Value, CmdResponse.TypeName, Response.mutable_value());
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

	// Convert PropertyValue proto to JSON string for CommandExecutor
	if (Request.has_value())
	{
		Cmd.Value = ProtoPropertyValueToJson(Request.value());
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

	// Convert parameters from proto to JSON strings
	for (const auto& Param : Request.parameters())
	{
		FString Key = UTF8_TO_TCHAR(Param.key().c_str());
		FString JsonValue = ProtoPropertyValueToJson(Param.value());
		Cmd.Parameters.Add(Key, JsonValue);
	}

	FFunctionCallResponse CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	CallFunctionResponse Response;

	if (CmdResponse.bSuccess)
	{
		// Convert return value (JSON string to proto PropertyValue)
		JsonToProtoPropertyValue(CmdResponse.ReturnValue, TEXT(""), Response.mutable_return_value());

		// Convert out parameters
		for (const auto& Pair : CmdResponse.OutParameters)
		{
			PropertyKeyValue* KV = Response.add_out_parameters();
			KV->set_key(TCHAR_TO_UTF8(*Pair.Key));
			JsonToProtoPropertyValue(Pair.Value, TEXT(""), KV->mutable_value());
		}

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
		// Note: Full class info requires FFindClassResponse with FClassInfo
		// Currently CommandExecutor only returns success/failure
		// Extended class info available via ListClasses RPC instead
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
		// Note: Full schema requires FGetClassSchemaResponse with properties/functions
		// Currently CommandExecutor only returns success/failure
		// Use console command AgentBridge.DumpClass for detailed schema
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
