#include "AgentBridgeServiceSubsystem.h"
#include "CommandExecutor.h"
#include "AgentCommands.h"
#include "WorldContextManager.h"
#include "ActorOperations.h"
#include "WorldPartitionOps.h"
#include "TempoScriptingServer.h"
#include "HAL/IConsoleManager.h"

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
			&UAgentBridgeServiceSubsystem::ListClasses),

		// World Partition & Streaming
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestIsWorldPartitioned,
			&UAgentBridgeServiceSubsystem::IsWorldPartitioned),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestQueryAllActors,
			&UAgentBridgeServiceSubsystem::QueryAllActors),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestGetStreamingState,
			&UAgentBridgeServiceSubsystem::GetStreamingState),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestQueryLandscape,
			&UAgentBridgeServiceSubsystem::QueryLandscape),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestGetLandscapeBounds,
			&UAgentBridgeServiceSubsystem::GetLandscapeBounds),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestGetDataLayers,
			&UAgentBridgeServiceSubsystem::GetDataLayers),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestGetActorsInDataLayer,
			&UAgentBridgeServiceSubsystem::GetActorsInDataLayer),

		// Console Commands
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestExecuteConsoleCommand,
			&UAgentBridgeServiceSubsystem::ExecuteConsoleCommand),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestSearchConsoleCommands,
			&UAgentBridgeServiceSubsystem::SearchConsoleCommands),

		// Asset Operations (P0)
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestCreateAsset,
			&UAgentBridgeServiceSubsystem::CreateAsset),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestSaveAsset,
			&UAgentBridgeServiceSubsystem::SaveAsset),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestSaveActorAsBlueprint,
			&UAgentBridgeServiceSubsystem::SaveActorAsBlueprint),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestDuplicateAsset,
			&UAgentBridgeServiceSubsystem::DuplicateAsset),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestGetAssetThumbnail,
			&UAgentBridgeServiceSubsystem::GetAssetThumbnail),

		// Component Operations (P1)
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestGetComponentTransform,
			&UAgentBridgeServiceSubsystem::GetComponentTransform),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestSetComponentTransform,
			&UAgentBridgeServiceSubsystem::SetComponentTransform),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestAttachComponent,
			&UAgentBridgeServiceSubsystem::AttachComponent),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestAttachActor,
			&UAgentBridgeServiceSubsystem::AttachActor),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestDetachComponent,
			&UAgentBridgeServiceSubsystem::DetachComponent),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestDetachActor,
			&UAgentBridgeServiceSubsystem::DetachActor),

		// File Operations (P1)
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestReadProjectFile,
			&UAgentBridgeServiceSubsystem::ReadProjectFile),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestWriteProjectFile,
			&UAgentBridgeServiceSubsystem::WriteProjectFile),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestListProjectDirectory,
			&UAgentBridgeServiceSubsystem::ListProjectDirectory),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestCopyProjectFile,
			&UAgentBridgeServiceSubsystem::CopyProjectFile),
		SimpleRequestHandler(&AgentBridgeAsyncService::RequestDeleteProjectFile,
			&UAgentBridgeServiceSubsystem::DeleteProjectFile)
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
	Cmd.LabelPattern = UTF8_TO_TCHAR(Request.label_pattern().c_str());
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

	FGetClassSchemaResponse CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	GetClassSchemaResponse Response;

	if (CmdResponse.bSuccess)
	{
		ClassSchema* Schema = Response.mutable_schema();
		const FClassInfo& SrcSchema = CmdResponse.Schema;

		// Class info
		ClassInfo* ClassInfoProto = Schema->mutable_class_info();
		ClassInfoProto->set_class_name(TCHAR_TO_UTF8(*SrcSchema.ClassName));
		ClassInfoProto->set_display_name(TCHAR_TO_UTF8(*SrcSchema.DisplayName));
		ClassInfoProto->set_class_path(TCHAR_TO_UTF8(*SrcSchema.ClassPath));
		ClassInfoProto->set_parent_class_name(TCHAR_TO_UTF8(*SrcSchema.ParentClassName));
		ClassInfoProto->set_is_blueprint(SrcSchema.bIsBlueprint);
		ClassInfoProto->set_is_abstract(SrcSchema.bIsAbstract);

		// Properties
		for (const FAgentPropertyInfo& Prop : SrcSchema.Properties)
		{
			PropertyInfo* PropProto = Schema->add_properties();
			PropProto->set_name(TCHAR_TO_UTF8(*Prop.PropertyName));
			PropProto->set_display_name(TCHAR_TO_UTF8(*Prop.DisplayName));
			PropProto->set_type_name(TCHAR_TO_UTF8(*Prop.TypeName));
			PropProto->set_is_read_only(Prop.bIsReadOnly);
			PropProto->set_category(TCHAR_TO_UTF8(*Prop.Category));
			PropProto->set_description(TCHAR_TO_UTF8(*Prop.Description));
			// Container element types (TArray, TSet, TMap)
			if (!Prop.ElementType.IsEmpty())
			{
				PropProto->set_element_type(TCHAR_TO_UTF8(*Prop.ElementType));
			}
			if (!Prop.KeyType.IsEmpty())
			{
				PropProto->set_key_type(TCHAR_TO_UTF8(*Prop.KeyType));
			}
		}

		// Functions
		for (const FAgentFunctionSignature& Func : SrcSchema.Functions)
		{
			FunctionSignature* FuncProto = Schema->add_functions();
			FuncProto->set_function_name(TCHAR_TO_UTF8(*Func.FunctionName));
			FuncProto->set_description(TCHAR_TO_UTF8(*Func.Description));
			FuncProto->set_is_static(Func.bIsStatic);
			FuncProto->set_is_blueprint_callable(Func.bIsBlueprintCallable);
			FuncProto->set_needs_world_context(Func.bNeedsWorldContext);

			// Parameters
			for (const FAgentPropertyInfo& Param : Func.Parameters)
			{
				PropertyInfo* ParamProto = FuncProto->add_parameters();
				ParamProto->set_name(TCHAR_TO_UTF8(*Param.PropertyName));
				ParamProto->set_display_name(TCHAR_TO_UTF8(*Param.DisplayName));
				ParamProto->set_type_name(TCHAR_TO_UTF8(*Param.TypeName));
				ParamProto->set_is_read_only(Param.bIsReadOnly);
			}

			// Return value (check if populated by looking at PropertyName)
			if (!Func.ReturnValue.PropertyName.IsEmpty())
			{
				PropertyInfo* RetProto = FuncProto->mutable_return_value();
				RetProto->set_name(TCHAR_TO_UTF8(*Func.ReturnValue.PropertyName));
				RetProto->set_type_name(TCHAR_TO_UTF8(*Func.ReturnValue.TypeName));
			}
		}

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

//~==============================================================================
// World Partition & Streaming
//~==============================================================================

namespace
{
	// Helper to fill StreamingActorInfo from FStreamingActorReference
	void FillStreamingActorInfo(StreamingActorInfo* Info, const FStreamingActorReference& Ref)
	{
		// Fill basic actor info
		ActorDescriptor* ActorInfo = Info->mutable_actor_info();
		ActorInfo->set_guid(TCHAR_TO_UTF8(*Ref.Guid));
		ActorInfo->set_path(TCHAR_TO_UTF8(*Ref.Path));
		ActorInfo->set_name(TCHAR_TO_UTF8(*Ref.Name));
		ActorInfo->set_label(TCHAR_TO_UTF8(*Ref.Label));
		ActorInfo->set_class_name(TCHAR_TO_UTF8(*Ref.ClassName));

		// Transform
		ActorTransform* Transform = ActorInfo->mutable_transform();
		SetProtoVector(Transform->mutable_location(), Ref.Transform.GetLocation());
		SetProtoRotation(Transform->mutable_rotation(), Ref.Transform.Rotator());
		SetProtoScale(Transform->mutable_scale(), Ref.Transform.GetScale3D());

		// Streaming-specific fields
		switch (Ref.StreamingState)
		{
		case EActorStreamingState::NotApplicable:
			Info->set_streaming_state(STREAMING_STATE_NOT_APPLICABLE);
			break;
		case EActorStreamingState::Loaded:
			Info->set_streaming_state(STREAMING_STATE_LOADED);
			break;
		case EActorStreamingState::Unloaded:
			Info->set_streaming_state(STREAMING_STATE_UNLOADED);
			break;
		case EActorStreamingState::Invalid:
			Info->set_streaming_state(STREAMING_STATE_INVALID);
			break;
		}

		Info->set_streaming_cell(TCHAR_TO_UTF8(*Ref.StreamingCellName));
		Info->set_is_spatially_loaded(Ref.bIsSpatiallyLoaded);

		// Bounds
		if (Ref.EditorBounds.IsValid)
		{
			BoundingBox* Bounds = Info->mutable_bounds();
			SetProtoVector(Bounds->mutable_min(), Ref.EditorBounds.Min);
			SetProtoVector(Bounds->mutable_max(), Ref.EditorBounds.Max);
		}

		// Data layers
		for (const FName& Layer : Ref.DataLayers)
		{
			Info->add_data_layers(TCHAR_TO_UTF8(*Layer.ToString()));
		}
	}

	StreamingState ToProtoStreamingState(EActorStreamingState State)
	{
		switch (State)
		{
		case EActorStreamingState::Loaded: return STREAMING_STATE_LOADED;
		case EActorStreamingState::Unloaded: return STREAMING_STATE_UNLOADED;
		case EActorStreamingState::Invalid: return STREAMING_STATE_INVALID;
		default: return STREAMING_STATE_NOT_APPLICABLE;
		}
	}
}

void UAgentBridgeServiceSubsystem::IsWorldPartitioned(
	const IsWorldPartitionedRequest& Request,
	const TResponseDelegate<IsWorldPartitionedResponse>& ResponseContinuation)
{
	UWorld* World = GetWorld();
	IsWorldPartitionedResponse Response;

	Response.set_is_partitioned(FWorldPartitionOps::IsWorldPartitioned(World));
	Response.set_world_name(TCHAR_TO_UTF8(*World->GetName()));

	ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
}

void UAgentBridgeServiceSubsystem::QueryAllActors(
	const QueryAllActorsRequest& Request,
	const TResponseDelegate<QueryAllActorsResponse>& ResponseContinuation)
{
	UWorld* World = GetWorld();

	FWorldPartitionQueryParams Params;
	Params.bIncludeLoaded = Request.include_loaded() || (!Request.include_loaded() && !Request.include_unloaded());
	Params.bIncludeUnloaded = Request.include_unloaded() || (!Request.include_loaded() && !Request.include_unloaded());
	Params.NamePattern = UTF8_TO_TCHAR(Request.name_pattern().c_str());
	Params.Limit = Request.limit() > 0 ? Request.limit() : 1000;

	if (!Request.data_layer().empty())
	{
		Params.DataLayerFilter = FName(UTF8_TO_TCHAR(Request.data_layer().c_str()));
	}

	if (Request.has_bounds_filter())
	{
		FBox Box;
		Box.Min = FromProtoVector(Request.bounds_filter().min());
		Box.Max = FromProtoVector(Request.bounds_filter().max());
		Params.BoundsFilter = Box;
	}

	// Get class filter if specified
	FString ClassName = UTF8_TO_TCHAR(Request.class_name().c_str());
	if (!ClassName.IsEmpty())
	{
		// Try to find the class
		UClass* ClassFilter = FindObject<UClass>(nullptr, *ClassName);
		if (!ClassFilter)
		{
			ClassFilter = LoadClass<AActor>(nullptr, *ClassName);
		}
		if (ClassFilter)
		{
			Params.ClassFilter = ClassFilter;
		}
	}

	TArray<FStreamingActorReference> Actors = FWorldPartitionOps::QueryAllActors(Params, World);

	QueryAllActorsResponse Response;
	int32 LoadedCount = 0;
	int32 UnloadedCount = 0;

	for (const FStreamingActorReference& Ref : Actors)
	{
		FillStreamingActorInfo(Response.add_actors(), Ref);

		if (Ref.StreamingState == EActorStreamingState::Loaded)
		{
			LoadedCount++;
		}
		else if (Ref.StreamingState == EActorStreamingState::Unloaded)
		{
			UnloadedCount++;
		}
	}

	Response.set_total_loaded(LoadedCount);
	Response.set_total_unloaded(UnloadedCount);

	ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
}

void UAgentBridgeServiceSubsystem::GetStreamingState(
	const GetStreamingStateRequest& Request,
	const TResponseDelegate<GetStreamingStateResponse>& ResponseContinuation)
{
	UWorld* World = GetWorld();
	FString GuidStr = UTF8_TO_TCHAR(Request.actor_guid().c_str());

	FGuid ActorGuid;
	if (!FGuid::Parse(GuidStr, ActorGuid))
	{
		GetStreamingStateResponse Response;
		Response.set_state(STREAMING_STATE_INVALID);
		ResponseContinuation.ExecuteIfBound(Response,
			grpc::Status(grpc::INVALID_ARGUMENT, "Invalid GUID format"));
		return;
	}

	EActorStreamingState State = FWorldPartitionOps::GetActorStreamingState(ActorGuid, World);
	FStreamingActorReference Ref = FWorldPartitionOps::FindActorByGuidEx(ActorGuid, World);

	GetStreamingStateResponse Response;
	Response.set_state(ToProtoStreamingState(State));

	if (State != EActorStreamingState::Invalid)
	{
		FillStreamingActorInfo(Response.mutable_actor(), Ref);
	}

	ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
}

void UAgentBridgeServiceSubsystem::QueryLandscape(
	const QueryLandscapeRequest& Request,
	const TResponseDelegate<QueryLandscapeResponse>& ResponseContinuation)
{
	UWorld* World = GetWorld();

	TArray<FStreamingActorReference> Proxies = FWorldPartitionOps::QueryLandscapeProxies(
		World, Request.include_unloaded());

	QueryLandscapeResponse Response;
	Response.set_total_count(Proxies.Num());

	for (const FStreamingActorReference& Ref : Proxies)
	{
		FillStreamingActorInfo(Response.add_landscape_proxies(), Ref);
	}

	ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
}

void UAgentBridgeServiceSubsystem::GetLandscapeBounds(
	const GetLandscapeBoundsRequest& Request,
	const TResponseDelegate<GetLandscapeBoundsResponse>& ResponseContinuation)
{
	UWorld* World = GetWorld();
	FLandscapeBounds Bounds = FWorldPartitionOps::GetLandscapeBounds(World);

	GetLandscapeBoundsResponse Response;
	Response.set_valid(Bounds.bValid);

	if (Bounds.bValid)
	{
		auto* Min = Response.mutable_min();
		Min->set_x(Bounds.Min.X);
		Min->set_y(Bounds.Min.Y);
		Min->set_z(Bounds.Min.Z);

		auto* Max = Response.mutable_max();
		Max->set_x(Bounds.Max.X);
		Max->set_y(Bounds.Max.Y);
		Max->set_z(Bounds.Max.Z);

		auto* Center = Response.mutable_center();
		Center->set_x(Bounds.Center.X);
		Center->set_y(Bounds.Center.Y);
		Center->set_z(Bounds.Center.Z);

		auto* Extent = Response.mutable_extent();
		Extent->set_x(Bounds.Extent.X);
		Extent->set_y(Bounds.Extent.Y);
		Extent->set_z(Bounds.Extent.Z);

		Response.set_proxy_count(Bounds.ProxyCount);
		Response.set_landscape_name(TCHAR_TO_UTF8(*Bounds.LandscapeName));
	}

	ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
}

void UAgentBridgeServiceSubsystem::GetDataLayers(
	const GetDataLayersRequest& Request,
	const TResponseDelegate<GetDataLayersResponse>& ResponseContinuation)
{
	UWorld* World = GetWorld();

	TArray<FName> Layers = FWorldPartitionOps::GetDataLayers(World);

	GetDataLayersResponse Response;
	for (const FName& Layer : Layers)
	{
		Response.add_data_layers(TCHAR_TO_UTF8(*Layer.ToString()));
	}

	ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
}

void UAgentBridgeServiceSubsystem::GetActorsInDataLayer(
	const GetActorsInDataLayerRequest& Request,
	const TResponseDelegate<GetActorsInDataLayerResponse>& ResponseContinuation)
{
	UWorld* World = GetWorld();
	FName LayerName = FName(UTF8_TO_TCHAR(Request.data_layer().c_str()));

	TArray<FStreamingActorReference> Actors = FWorldPartitionOps::GetActorsInDataLayer(
		LayerName, Request.include_unloaded(), World);

	// Apply limit
	int32 Limit = Request.limit() > 0 ? Request.limit() : 1000;
	if (Actors.Num() > Limit)
	{
		Actors.SetNum(Limit);
	}

	GetActorsInDataLayerResponse Response;
	Response.set_total_count(Actors.Num());

	for (const FStreamingActorReference& Ref : Actors)
	{
		FillStreamingActorInfo(Response.add_actors(), Ref);
	}

	ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
}

//~==============================================================================
// Console Commands
//~==============================================================================

namespace
{
	/**
	 * Custom output device that captures log messages to a string.
	 * Used to capture UE_LOG output from console commands.
	 */
	class FLogCaptureOutputDevice : public FOutputDevice
	{
	public:
		FString CapturedOutput;

		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			// Format: [Category] Message
			if (!CapturedOutput.IsEmpty())
			{
				CapturedOutput += TEXT("\n");
			}
			CapturedOutput += FString::Printf(TEXT("[%s] %s"), *Category.ToString(), V);
		}

		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category, double Time) override
		{
			Serialize(V, Verbosity, Category);
		}
	};
}

void UAgentBridgeServiceSubsystem::ExecuteConsoleCommand(
	const ExecuteConsoleCommandRequest& Request,
	const TResponseDelegate<ExecuteConsoleCommandResponse>& ResponseContinuation)
{
	FString Command = UTF8_TO_TCHAR(Request.command().c_str());

	ExecuteConsoleCommandResponse Response;

	if (Command.IsEmpty())
	{
		Response.set_success(false);
		Response.set_output("Empty command");
		ResponseContinuation.ExecuteIfBound(Response,
			grpc::Status(grpc::INVALID_ARGUMENT, "Empty command"));
		return;
	}

	// Create a log capture device to intercept UE_LOG output
	FLogCaptureOutputDevice LogCapture;

	// Add our capture device to GLog (output still goes to normal log too)
	GLog->AddOutputDevice(&LogCapture);

	// Also capture direct output from Exec
	FStringOutputDevice DirectOutput;

	// Execute the command using GEngine->Exec for broadest command support
	// This handles console variables, registered console commands, and more
	bool bSuccess = false;
	UWorld* World = GetWorld();

	if (GEngine)
	{
		// GEngine->Exec handles most console commands
		bSuccess = GEngine->Exec(World, *Command, DirectOutput);
	}

	// If GEngine didn't handle it, try the world
	if (!bSuccess && World)
	{
		bSuccess = World->Exec(World, *Command, DirectOutput);
	}

	// Remove our capture device
	GLog->RemoveOutputDevice(&LogCapture);

	// Combine outputs: direct output first, then captured log
	FString CombinedOutput;
	if (!DirectOutput.IsEmpty())
	{
		CombinedOutput = DirectOutput;
	}
	if (!LogCapture.CapturedOutput.IsEmpty())
	{
		if (!CombinedOutput.IsEmpty())
		{
			CombinedOutput += TEXT("\n");
		}
		CombinedOutput += LogCapture.CapturedOutput;
	}

	// Commands were executed - consider success even if Exec returns false
	// (many commands don't return true even when they work)
	Response.set_success(true);
	Response.set_output(TCHAR_TO_UTF8(*CombinedOutput));

	ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
}

void UAgentBridgeServiceSubsystem::SearchConsoleCommands(
	const SearchConsoleCommandsRequest& Request,
	const TResponseDelegate<SearchConsoleCommandsResponse>& ResponseContinuation)
{
	FString Keyword = UTF8_TO_TCHAR(Request.keyword().c_str());
	int32 Limit = Request.limit() > 0 ? Request.limit() : 50;
	int32 Offset = Request.offset() > 0 ? Request.offset() : 0;
	bool bSearchHelp = Request.search_help();

	SearchConsoleCommandsResponse Response;

	if (Keyword.IsEmpty())
	{
		Response.set_total_scanned(0);
		Response.set_total_matches(0);
		ResponseContinuation.ExecuteIfBound(Response,
			grpc::Status(grpc::INVALID_ARGUMENT, "Keyword is required"));
		return;
	}

	int32 TotalScanned = 0;
	int32 TotalMatches = 0;
	int32 AddedCount = 0;

	// Iterate all console objects - we need to scan all to get total_matches
	IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(
		FConsoleObjectVisitor::CreateLambda([&](const TCHAR* Name, IConsoleObject* ConsoleObj)
		{
			TotalScanned++;

			FString NameStr(Name);
			const TCHAR* HelpText = ConsoleObj->GetHelp();
			FString HelpStr = HelpText ? FString(HelpText) : TEXT("");

			// Check if keyword matches name or (optionally) help text
			bool bNameMatch = NameStr.Contains(Keyword, ESearchCase::IgnoreCase);
			bool bHelpMatch = bSearchHelp && HelpStr.Contains(Keyword, ESearchCase::IgnoreCase);

			if (!bNameMatch && !bHelpMatch)
			{
				return;
			}

			// This is a match
			TotalMatches++;

			// Skip if before offset
			if (TotalMatches <= Offset)
			{
				return;
			}

			// Skip if we've already added enough
			if (AddedCount >= Limit)
			{
				return;
			}

			ConsoleCommandInfo* Info = Response.add_commands();
			Info->set_name(TCHAR_TO_UTF8(*NameStr));

			// Truncate help text for transport
			if (HelpStr.Len() > 500)
			{
				HelpStr = HelpStr.Left(500) + TEXT("...");
			}
			HelpStr.ReplaceInline(TEXT("\n"), TEXT(" "));
			HelpStr.ReplaceInline(TEXT("\r"), TEXT(""));
			Info->set_help(TCHAR_TO_UTF8(*HelpStr));

			// Check if it's a variable or command
			IConsoleVariable* CVar = ConsoleObj->AsVariable();
			if (CVar)
			{
				Info->set_is_variable(true);

				FString ValueType;
				FString CurrentValue;

				if (CVar->IsVariableInt())
				{
					ValueType = TEXT("Int");
					CurrentValue = FString::Printf(TEXT("%d"), CVar->GetInt());
				}
				else if (CVar->IsVariableFloat())
				{
					ValueType = TEXT("Float");
					CurrentValue = FString::Printf(TEXT("%.4f"), CVar->GetFloat());
				}
				else if (CVar->IsVariableBool())
				{
					ValueType = TEXT("Bool");
					CurrentValue = CVar->GetBool() ? TEXT("true") : TEXT("false");
				}
				else
				{
					ValueType = TEXT("String");
					CurrentValue = CVar->GetString();
					if (CurrentValue.Len() > 100)
					{
						CurrentValue = CurrentValue.Left(100) + TEXT("...");
					}
				}

				Info->set_value_type(TCHAR_TO_UTF8(*ValueType));
				Info->set_current_value(TCHAR_TO_UTF8(*CurrentValue));
			}
			else
			{
				Info->set_is_variable(false);
			}

			AddedCount++;
		}),
		TEXT("") // Start with all console objects
	);

	Response.set_total_scanned(TotalScanned);
	Response.set_total_matches(TotalMatches);
	ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
}

//~==============================================================================
// Asset Operations (P0)
//~==============================================================================

void UAgentBridgeServiceSubsystem::CreateAsset(
	const CreateAssetRequest& Request,
	const TResponseDelegate<CreateAssetResponse>& ResponseContinuation)
{
	FCreateAssetCommand Cmd;
	Cmd.AssetClass = UTF8_TO_TCHAR(Request.asset_class().c_str());
	Cmd.PackagePath = UTF8_TO_TCHAR(Request.package_path().c_str());
	Cmd.AssetName = UTF8_TO_TCHAR(Request.asset_name().c_str());
	Cmd.ParentAssetPath = UTF8_TO_TCHAR(Request.parent_asset_path().c_str());

	// Parse properties from proto - convert PropertyValue to JSON string
	for (const auto& Prop : Request.properties())
	{
		// For now, just use string_value if set
		FString ValueStr;
		if (Prop.value().type() == PROPERTY_TYPE_STRING)
		{
			ValueStr = UTF8_TO_TCHAR(Prop.value().string_value().c_str());
		}
		else if (Prop.value().type() == PROPERTY_TYPE_INT)
		{
			ValueStr = FString::Printf(TEXT("%lld"), Prop.value().int_value());
		}
		else if (Prop.value().type() == PROPERTY_TYPE_FLOAT)
		{
			ValueStr = FString::Printf(TEXT("%f"), Prop.value().float_value());
		}
		else if (Prop.value().type() == PROPERTY_TYPE_BOOL)
		{
			ValueStr = Prop.value().bool_value() ? TEXT("true") : TEXT("false");
		}
		else
		{
			ValueStr = UTF8_TO_TCHAR(Prop.value().string_value().c_str());
		}
		Cmd.Properties.Add(UTF8_TO_TCHAR(Prop.key().c_str()), ValueStr);
	}

	FCreateAssetResponse CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	CreateAssetResponse Response;
	Response.set_success(CmdResponse.bSuccess);
	Response.set_error_message(TCHAR_TO_UTF8(*CmdResponse.ErrorMessage));
	Response.set_asset_path(TCHAR_TO_UTF8(*CmdResponse.AssetPath));
	Response.set_asset_class(TCHAR_TO_UTF8(*CmdResponse.AssetClass));
	ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
}

void UAgentBridgeServiceSubsystem::SaveAsset(
	const SaveAssetRequest& Request,
	const TResponseDelegate<SaveAssetResponse>& ResponseContinuation)
{
	FSaveAssetCommand Cmd;
	Cmd.AssetPath = UTF8_TO_TCHAR(Request.asset_path().c_str());
	Cmd.bPromptForCheckout = Request.prompt_for_checkout();

	FSaveAssetResponse CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	SaveAssetResponse Response;
	Response.set_success(CmdResponse.bSuccess);
	Response.set_error_message(TCHAR_TO_UTF8(*CmdResponse.ErrorMessage));
	Response.set_file_path(TCHAR_TO_UTF8(*CmdResponse.AssetPath));
	ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
}

void UAgentBridgeServiceSubsystem::SaveActorAsBlueprint(
	const SaveActorAsBlueprintRequest& Request,
	const TResponseDelegate<SaveActorAsBlueprintResponse>& ResponseContinuation)
{
	FSaveActorAsBlueprintCommand Cmd;
	Cmd.ActorId = UTF8_TO_TCHAR(Request.actor_id().c_str());
	Cmd.PackagePath = UTF8_TO_TCHAR(Request.package_path().c_str());
	Cmd.BlueprintName = UTF8_TO_TCHAR(Request.blueprint_name().c_str());
	Cmd.bReplaceExisting = Request.replace_existing();

	FSaveActorAsBlueprintResponse CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	SaveActorAsBlueprintResponse Response;
	Response.set_success(CmdResponse.bSuccess);
	Response.set_error_message(TCHAR_TO_UTF8(*CmdResponse.ErrorMessage));
	Response.set_blueprint_path(TCHAR_TO_UTF8(*CmdResponse.BlueprintPath));
	ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
}

void UAgentBridgeServiceSubsystem::DuplicateAsset(
	const DuplicateAssetRequest& Request,
	const TResponseDelegate<DuplicateAssetResponse>& ResponseContinuation)
{
	FDuplicateAssetCommand Cmd;
	Cmd.SourcePath = UTF8_TO_TCHAR(Request.source_path().c_str());
	Cmd.DestPackagePath = UTF8_TO_TCHAR(Request.dest_package_path().c_str());
	Cmd.DestAssetName = UTF8_TO_TCHAR(Request.dest_asset_name().c_str());

	FDuplicateAssetResponse CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	DuplicateAssetResponse Response;
	Response.set_success(CmdResponse.bSuccess);
	Response.set_error_message(TCHAR_TO_UTF8(*CmdResponse.ErrorMessage));
	Response.set_new_asset_path(TCHAR_TO_UTF8(*CmdResponse.NewAssetPath));
	ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
}

void UAgentBridgeServiceSubsystem::GetAssetThumbnail(
	const GetAssetThumbnailRequest& Request,
	const TResponseDelegate<GetAssetThumbnailResponse>& ResponseContinuation)
{
	FGetAssetThumbnailCommand Cmd;
	Cmd.AssetPath = UTF8_TO_TCHAR(Request.asset_path().c_str());
	Cmd.Width = Request.width() > 0 ? Request.width() : 256;
	Cmd.Height = Request.height() > 0 ? Request.height() : 256;

	FGetAssetThumbnailResponse CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	GetAssetThumbnailResponse Response;
	Response.set_success(CmdResponse.bSuccess);
	Response.set_error_message(TCHAR_TO_UTF8(*CmdResponse.ErrorMessage));
	// ImageData is base64-encoded string, set as bytes
	Response.set_image_data(TCHAR_TO_UTF8(*CmdResponse.ImageData));
	Response.set_width(CmdResponse.Width);
	Response.set_height(CmdResponse.Height);
	ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
}

//~==============================================================================
// Component Operations (P1)
//~==============================================================================

void UAgentBridgeServiceSubsystem::GetComponentTransform(
	const GetComponentTransformRequest& Request,
	const TResponseDelegate<GetComponentTransformResponse>& ResponseContinuation)
{
	FGetComponentTransformCommand Cmd;
	Cmd.ActorId = UTF8_TO_TCHAR(Request.actor_id().c_str());
	Cmd.ComponentName = UTF8_TO_TCHAR(Request.component_name().c_str());
	Cmd.bWorldSpace = Request.world_space();

	FGetComponentTransformResponse CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	GetComponentTransformResponse Response;
	Response.set_success(CmdResponse.bSuccess);
	Response.set_error_message(TCHAR_TO_UTF8(*CmdResponse.ErrorMessage));

	if (CmdResponse.bSuccess)
	{
		auto* Transform = Response.mutable_transform();
		SetProtoVector(Transform->mutable_location(), CmdResponse.Location);
		SetProtoRotation(Transform->mutable_rotation(), CmdResponse.Rotation);
		Transform->mutable_scale()->set_x(CmdResponse.Scale.X);
		Transform->mutable_scale()->set_y(CmdResponse.Scale.Y);
		Transform->mutable_scale()->set_z(CmdResponse.Scale.Z);
	}
	ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
}

void UAgentBridgeServiceSubsystem::SetComponentTransform(
	const SetComponentTransformRequest& Request,
	const TResponseDelegate<TempoScripting::Empty>& ResponseContinuation)
{
	FSetComponentTransformCommand Cmd;
	Cmd.ActorId = UTF8_TO_TCHAR(Request.actor_id().c_str());
	Cmd.ComponentName = UTF8_TO_TCHAR(Request.component_name().c_str());
	Cmd.bWorldSpace = Request.world_space();
	Cmd.bSweep = Request.sweep();

	if (Request.has_transform())
	{
		const auto& T = Request.transform();
		if (T.has_location())
		{
			Cmd.Location = FVector(T.location().x(), T.location().y(), T.location().z());
		}
		if (T.has_rotation())
		{
			Cmd.Rotation = FRotator(T.rotation().p(), T.rotation().y(), T.rotation().r());
		}
		if (T.has_scale())
		{
			Cmd.Scale = FVector(T.scale().x(), T.scale().y(), T.scale().z());
		}
	}

	FAgentResponseBase CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	TempoScripting::Empty Response;
	if (!CmdResponse.bSuccess)
	{
		ResponseContinuation.ExecuteIfBound(Response,
			grpc::Status(grpc::INTERNAL, TCHAR_TO_UTF8(*CmdResponse.ErrorMessage)));
	}
	else
	{
		ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
	}
}

namespace
{
	EAttachmentRuleType ProtoToAttachmentRule(AgentBridgeServer::AttachmentRule Rule)
	{
		switch (Rule)
		{
		case AgentBridgeServer::ATTACHMENT_RULE_KEEP_WORLD:
			return EAttachmentRuleType::KeepWorld;
		case AgentBridgeServer::ATTACHMENT_RULE_SNAP_TO_TARGET:
			return EAttachmentRuleType::SnapToTarget;
		default:
			return EAttachmentRuleType::KeepRelative;
		}
	}
}

void UAgentBridgeServiceSubsystem::AttachComponent(
	const AttachComponentRequest& Request,
	const TResponseDelegate<TempoScripting::Empty>& ResponseContinuation)
{
	FAttachComponentCommand Cmd;
	Cmd.ActorId = UTF8_TO_TCHAR(Request.actor_id().c_str());
	Cmd.ComponentName = UTF8_TO_TCHAR(Request.component_name().c_str());
	Cmd.ParentComponentName = UTF8_TO_TCHAR(Request.parent_component_name().c_str());
	Cmd.SocketName = UTF8_TO_TCHAR(Request.socket_name().c_str());
	Cmd.LocationRule = ProtoToAttachmentRule(Request.location_rule());
	Cmd.RotationRule = ProtoToAttachmentRule(Request.rotation_rule());
	Cmd.ScaleRule = ProtoToAttachmentRule(Request.scale_rule());

	FAgentResponseBase CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	TempoScripting::Empty Response;
	if (!CmdResponse.bSuccess)
	{
		ResponseContinuation.ExecuteIfBound(Response,
			grpc::Status(grpc::INTERNAL, TCHAR_TO_UTF8(*CmdResponse.ErrorMessage)));
	}
	else
	{
		ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
	}
}

void UAgentBridgeServiceSubsystem::AttachActor(
	const AttachActorRequest& Request,
	const TResponseDelegate<TempoScripting::Empty>& ResponseContinuation)
{
	FAttachActorCommand Cmd;
	Cmd.ChildActorId = UTF8_TO_TCHAR(Request.child_actor_id().c_str());
	Cmd.ParentActorId = UTF8_TO_TCHAR(Request.parent_actor_id().c_str());
	Cmd.ParentComponentName = UTF8_TO_TCHAR(Request.parent_component_name().c_str());
	Cmd.SocketName = UTF8_TO_TCHAR(Request.socket_name().c_str());
	Cmd.LocationRule = ProtoToAttachmentRule(Request.location_rule());
	Cmd.RotationRule = ProtoToAttachmentRule(Request.rotation_rule());
	Cmd.ScaleRule = ProtoToAttachmentRule(Request.scale_rule());

	FAgentResponseBase CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	TempoScripting::Empty Response;
	if (!CmdResponse.bSuccess)
	{
		ResponseContinuation.ExecuteIfBound(Response,
			grpc::Status(grpc::INTERNAL, TCHAR_TO_UTF8(*CmdResponse.ErrorMessage)));
	}
	else
	{
		ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
	}
}

void UAgentBridgeServiceSubsystem::DetachComponent(
	const DetachComponentRequest& Request,
	const TResponseDelegate<TempoScripting::Empty>& ResponseContinuation)
{
	FDetachComponentCommand Cmd;
	Cmd.ActorId = UTF8_TO_TCHAR(Request.actor_id().c_str());
	Cmd.ComponentName = UTF8_TO_TCHAR(Request.component_name().c_str());
	Cmd.bMaintainWorldPosition = Request.maintain_world_position();

	FAgentResponseBase CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	TempoScripting::Empty Response;
	if (!CmdResponse.bSuccess)
	{
		ResponseContinuation.ExecuteIfBound(Response,
			grpc::Status(grpc::INTERNAL, TCHAR_TO_UTF8(*CmdResponse.ErrorMessage)));
	}
	else
	{
		ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
	}
}

void UAgentBridgeServiceSubsystem::DetachActor(
	const DetachActorRequest& Request,
	const TResponseDelegate<TempoScripting::Empty>& ResponseContinuation)
{
	FDetachActorCommand Cmd;
	Cmd.ActorId = UTF8_TO_TCHAR(Request.actor_id().c_str());
	Cmd.bMaintainWorldPosition = Request.maintain_world_position();

	FAgentResponseBase CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	TempoScripting::Empty Response;
	if (!CmdResponse.bSuccess)
	{
		ResponseContinuation.ExecuteIfBound(Response,
			grpc::Status(grpc::INTERNAL, TCHAR_TO_UTF8(*CmdResponse.ErrorMessage)));
	}
	else
	{
		ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
	}
}

//~==============================================================================
// File Operations (P1)
//~==============================================================================

void UAgentBridgeServiceSubsystem::ReadProjectFile(
	const ReadProjectFileRequest& Request,
	const TResponseDelegate<ReadProjectFileResponse>& ResponseContinuation)
{
	FReadProjectFileCommand Cmd;
	Cmd.RelativePath = UTF8_TO_TCHAR(Request.relative_path().c_str());
	Cmd.bAsBase64 = Request.as_base64();
	Cmd.MaxBytes = Request.max_bytes();

	FReadProjectFileResponse CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	ReadProjectFileResponse Response;
	Response.set_success(CmdResponse.bSuccess);
	Response.set_error_message(TCHAR_TO_UTF8(*CmdResponse.ErrorMessage));
	Response.set_content(TCHAR_TO_UTF8(*CmdResponse.Content));
	Response.set_file_size(CmdResponse.FileSizeBytes);
	Response.set_is_binary(CmdResponse.bIsBase64);
	ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
}

void UAgentBridgeServiceSubsystem::WriteProjectFile(
	const WriteProjectFileRequest& Request,
	const TResponseDelegate<WriteProjectFileResponse>& ResponseContinuation)
{
	FWriteProjectFileCommand Cmd;
	Cmd.RelativePath = UTF8_TO_TCHAR(Request.relative_path().c_str());
	Cmd.Content = UTF8_TO_TCHAR(Request.content().c_str());
	Cmd.bIsBase64 = Request.is_base64();
	Cmd.bCreateDirectories = Request.create_directories();
	Cmd.bAppend = Request.append();

	FWriteProjectFileResponse CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	WriteProjectFileResponse Response;
	Response.set_success(CmdResponse.bSuccess);
	Response.set_error_message(TCHAR_TO_UTF8(*CmdResponse.ErrorMessage));
	Response.set_bytes_written(CmdResponse.BytesWritten);
	ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
}

void UAgentBridgeServiceSubsystem::ListProjectDirectory(
	const ListProjectDirectoryRequest& Request,
	const TResponseDelegate<ListProjectDirectoryResponse>& ResponseContinuation)
{
	FListProjectDirectoryCommand Cmd;
	Cmd.RelativePath = UTF8_TO_TCHAR(Request.relative_path().c_str());
	Cmd.Pattern = UTF8_TO_TCHAR(Request.pattern().c_str());
	Cmd.bRecursive = Request.recursive();
	Cmd.Limit = Request.limit() > 0 ? Request.limit() : 100;

	FListProjectDirectoryResponse CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	ListProjectDirectoryResponse Response;
	Response.set_success(CmdResponse.bSuccess);
	Response.set_error_message(TCHAR_TO_UTF8(*CmdResponse.ErrorMessage));
	Response.set_total_count(CmdResponse.TotalCount);

	for (const auto& File : CmdResponse.Files)
	{
		auto* ProtoFile = Response.add_files();
		ProtoFile->set_name(TCHAR_TO_UTF8(*File.Name));
		ProtoFile->set_relative_path(TCHAR_TO_UTF8(*File.RelativePath));
		ProtoFile->set_is_directory(File.bIsDirectory);
		ProtoFile->set_size(File.SizeBytes);
		ProtoFile->set_last_modified(TCHAR_TO_UTF8(*File.ModificationTime));
	}
	ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
}

void UAgentBridgeServiceSubsystem::CopyProjectFile(
	const CopyProjectFileRequest& Request,
	const TResponseDelegate<CopyProjectFileResponse>& ResponseContinuation)
{
	FCopyProjectFileCommand Cmd;
	Cmd.SourcePath = UTF8_TO_TCHAR(Request.source_path().c_str());
	Cmd.DestPath = UTF8_TO_TCHAR(Request.dest_path().c_str());
	Cmd.bOverwrite = Request.overwrite();

	FCopyProjectFileResponse CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	CopyProjectFileResponse Response;
	Response.set_success(CmdResponse.bSuccess);
	Response.set_error_message(TCHAR_TO_UTF8(*CmdResponse.ErrorMessage));
	Response.set_dest_full_path(TCHAR_TO_UTF8(*CmdResponse.DestAbsolutePath));
	ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
}

void UAgentBridgeServiceSubsystem::DeleteProjectFile(
	const DeleteProjectFileRequest& Request,
	const TResponseDelegate<TempoScripting::Empty>& ResponseContinuation)
{
	FDeleteProjectFileCommand Cmd;
	Cmd.RelativePath = UTF8_TO_TCHAR(Request.relative_path().c_str());
	Cmd.bAllowDirectoryDelete = Request.allow_directory_delete();

	FAgentResponseBase CmdResponse;
	FCommandExecutor::Execute(Cmd, CmdResponse);

	TempoScripting::Empty Response;
	if (!CmdResponse.bSuccess)
	{
		ResponseContinuation.ExecuteIfBound(Response,
			grpc::Status(grpc::INTERNAL, TCHAR_TO_UTF8(*CmdResponse.ErrorMessage)));
	}
	else
	{
		ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
	}
}
