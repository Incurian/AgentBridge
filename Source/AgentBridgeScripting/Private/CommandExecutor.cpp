#include "CommandExecutor.h"
#include "ActorOperations.h"
#include "WorldContextManager.h"
#include "AgentPropertyPath.h"
#include "FunctionInvoker.h"
#include "TypeDiscovery.h"
#include "PropertyAccessor.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

//~==============================================================================
// Timing Helpers
//~==============================================================================

double FCommandExecutor::StartTiming()
{
	return FPlatformTime::Seconds();
}

double FCommandExecutor::EndTiming(double StartTime)
{
	return (FPlatformTime::Seconds() - StartTime) * 1000.0;
}

//~==============================================================================
// Actor Resolution
//~==============================================================================

AActor* FCommandExecutor::ResolveActor(const FString& ActorId, FString* OutError)
{
	if (ActorId.IsEmpty())
	{
		if (OutError) *OutError = TEXT("ActorId is empty");
		return nullptr;
	}

	UWorld* World = FWorldContextManager::Get().GetTargetWorld();
	if (!World)
	{
		if (OutError) *OutError = TEXT("No target world available");
		return nullptr;
	}

	// Try to resolve via ActorOperations
	AActor* Actor = FActorOperations::FindActorByName(ActorId, World);
	if (!Actor)
	{
		if (OutError) *OutError = FString::Printf(TEXT("Actor '%s' not found"), *ActorId);
	}

	return Actor;
}

//~==============================================================================
// Actor Info Building
//~==============================================================================

FActorInfo FCommandExecutor::BuildActorInfo(AActor* Actor, bool bIncludeProperties, bool bIncludeComponents, int32 PropertyDepth)
{
	FActorInfo Info;

	if (!Actor)
	{
		return Info;
	}

	// Basic info
	Info.Guid = Actor->GetActorGuid().ToString();
	Info.Path = Actor->GetPathName();
	Info.Name = Actor->GetName();
	Info.Label = Actor->GetActorLabel();
	Info.ClassName = Actor->GetClass()->GetName();

	// Transform
	Info.Location = Actor->GetActorLocation();
	Info.Rotation = Actor->GetActorRotation();
	Info.Scale = Actor->GetActorScale3D();

	// State
	Info.bHidden = Actor->IsHidden();

	// Parent
	if (AActor* Parent = Actor->GetAttachParentActor())
	{
		Info.ParentActorId = Parent->GetActorGuid().ToString();
	}

	// Properties
	if (bIncludeProperties)
	{
		TMap<FString, FAgentPropertyValue> Props = FActorOperations::GetActorProperties(Actor);
		for (const auto& Pair : Props)
		{
			Info.Properties.Add(Pair.Key, PropertyValueToJson(Pair.Value));
		}
	}

	// Components
	if (bIncludeComponents)
	{
		TArray<UActorComponent*> Components;
		Actor->GetComponents(Components);
		for (UActorComponent* Comp : Components)
		{
			if (Comp)
			{
				Info.Components.Add(Comp->GetName(), Comp->GetClass()->GetName());
			}
		}
	}

	return Info;
}

//~==============================================================================
// World Commands
//~==============================================================================

void FCommandExecutor::Execute(const FListWorldsCommand& Command, FListWorldsResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	const TIndirectArray<FWorldContext>& Contexts = FWorldContextManager::Get().GetAllWorldContexts();
	UWorld* CurrentTarget = FWorldContextManager::Get().GetTargetWorld();
	int32 CurrentIndex = -1;

	for (int32 i = 0; i < Contexts.Num(); i++)
	{
		const FWorldContext& Context = Contexts[i];
		UWorld* World = Context.World();

		FWorldInfo Info;

		switch (Context.WorldType)
		{
		case EWorldType::Editor: Info.WorldType = TEXT("Editor"); break;
		case EWorldType::PIE: Info.WorldType = TEXT("PIE"); break;
		case EWorldType::Game: Info.WorldType = TEXT("Game"); break;
		case EWorldType::EditorPreview: Info.WorldType = TEXT("EditorPreview"); break;
		default: Info.WorldType = TEXT("Other"); break;
		}

		Info.WorldName = World ? World->GetName() : TEXT("");
		Info.PIEInstance = Context.PIEInstance;
		Info.bHasBegunPlay = World ? World->HasBegunPlay() : false;
		Info.ActorCount = World ? World->GetActorCount() : 0;

		Response.Worlds.Add(Info);

		if (World == CurrentTarget)
		{
			CurrentIndex = i;
		}
	}

	Response.CurrentWorldIndex = CurrentIndex;
	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FSetTargetWorldCommand& Command, FAgentResponseBase& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	// Clear override if empty
	if (Command.WorldIdentifier.IsEmpty() || Command.WorldIdentifier.Equals(TEXT("auto"), ESearchCase::IgnoreCase))
	{
		FWorldContextManager::Get().ClearTargetWorldOverride();
		Response.bSuccess = true;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Try to find world by identifier
	const TIndirectArray<FWorldContext>& Contexts = FWorldContextManager::Get().GetAllWorldContexts();

	// Try as index
	if (Command.WorldIdentifier.IsNumeric())
	{
		int32 Index = FCString::Atoi(*Command.WorldIdentifier);
		if (Index >= 0 && Index < Contexts.Num())
		{
			FWorldContextManager::Get().SetTargetWorldOverride(Contexts[Index].World());
			Response.bSuccess = true;
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}
	}

	// Try as type
	if (Command.WorldIdentifier.Equals(TEXT("editor"), ESearchCase::IgnoreCase))
	{
		UWorld* EditorWorld = FWorldContextManager::Get().GetEditorWorld();
		if (EditorWorld)
		{
			FWorldContextManager::Get().SetTargetWorldOverride(EditorWorld);
			Response.bSuccess = true;
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}
	}

	if (Command.WorldIdentifier.Equals(TEXT("pie"), ESearchCase::IgnoreCase))
	{
		TArray<UWorld*> PIEWorlds = FWorldContextManager::Get().GetAllPIEWorlds();
		if (PIEWorlds.Num() > 0)
		{
			FWorldContextManager::Get().SetTargetWorldOverride(PIEWorlds[0]);
			Response.bSuccess = true;
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}
	}

	// Try as world name
	for (const FWorldContext& Context : Contexts)
	{
		if (Context.World() && Context.World()->GetName().Contains(Command.WorldIdentifier))
		{
			FWorldContextManager::Get().SetTargetWorldOverride(Context.World());
			Response.bSuccess = true;
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}
	}

	Response.bSuccess = false;
	Response.ErrorMessage = FString::Printf(TEXT("World '%s' not found"), *Command.WorldIdentifier);
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

//~==============================================================================
// Actor Query Commands
//~==============================================================================

void FCommandExecutor::Execute(const FQueryActorsCommand& Command, FQueryActorsResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	FActorQueryParams Params;
	Params.NamePattern = Command.NamePattern;
	Params.Tag = Command.Tag;
	Params.Limit = Command.Limit;
	Params.bIncludeHidden = Command.bIncludeHidden;

	// Resolve class filter
	if (!Command.ClassName.IsEmpty())
	{
		Params.ClassFilter = FTypeDiscovery::FindClassByName(Command.ClassName);
		if (!Params.ClassFilter)
		{
			Response.bSuccess = false;
			Response.ErrorMessage = FString::Printf(TEXT("Class '%s' not found"), *Command.ClassName);
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}
	}

	TArray<FActorReference> Results = FActorOperations::QueryActors(Params);

	for (const FActorReference& Ref : Results)
	{
		FActorInfo Info;
		Info.Guid = Ref.Guid;
		Info.Path = Ref.Path;
		Info.Name = Ref.Name;
		Info.Label = Ref.Label;
		Info.ClassName = Ref.ClassName;

		// Resolve to get transform
		if (AActor* Actor = Ref.Resolve())
		{
			Info.Location = Actor->GetActorLocation();
			Info.Rotation = Actor->GetActorRotation();
			Info.Scale = Actor->GetActorScale3D();
			Info.bHidden = Actor->IsHidden();
		}

		Response.Actors.Add(Info);
	}

	Response.TotalCount = Response.Actors.Num();
	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FGetActorCommand& Command, FGetActorResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	FString Error;
	AActor* Actor = ResolveActor(Command.ActorId, &Error);
	if (!Actor)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	Response.Actor = BuildActorInfo(Actor, Command.bIncludeProperties, Command.bIncludeComponents, Command.PropertyDepth);
	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FGetActorPropertiesCommand& Command, FPropertyValueResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	FString Error;
	AActor* Actor = ResolveActor(Command.ActorId, &Error);
	if (!Actor)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	TMap<FString, FAgentPropertyValue> Props = FActorOperations::GetActorProperties(Actor, Command.PropertyNames);

	// Convert to JSON object
	TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();
	for (const auto& Pair : Props)
	{
		JsonObj->SetStringField(Pair.Key, PropertyValueToJson(Pair.Value));
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);

	Response.Value = OutputString;
	Response.TypeName = TEXT("Object");
	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

//~==============================================================================
// Actor Modification Commands
//~==============================================================================

void FCommandExecutor::Execute(const FSpawnActorCommand& Command, FSpawnActorResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	FActorSpawnParams Params;
	Params.ClassPath = Command.ClassName;
	Params.Transform.SetLocation(Command.Location);
	Params.Transform.SetRotation(Command.Rotation.Quaternion());
	Params.Transform.SetScale3D(Command.Scale);
	Params.ActorLabel = Command.Label;
	Params.FolderPath = Command.FolderPath;

	// Convert string properties to FAgentPropertyValue
	for (const auto& Pair : Command.Properties)
	{
		FAgentPropertyValue Value = JsonToPropertyValue(Pair.Value);
		Params.InitialProperties.Add(Pair.Key, Value);
	}

	FString Error;
	AActor* NewActor = FActorOperations::SpawnActor(Params, nullptr, &Error);

	if (!NewActor)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	Response.Actor = BuildActorInfo(NewActor, false, false, 0);
	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FDeleteActorCommand& Command, FAgentResponseBase& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	FString Error;
	AActor* Actor = ResolveActor(Command.ActorId, &Error);
	if (!Actor)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	bool bDestroyed = FActorOperations::DestroyActor(Actor);
	Response.bSuccess = bDestroyed;
	if (!bDestroyed)
	{
		Response.ErrorMessage = TEXT("Failed to destroy actor");
	}
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FSetActorPropertiesCommand& Command, FAgentResponseBase& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	FString Error;
	AActor* Actor = ResolveActor(Command.ActorId, &Error);
	if (!Actor)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	TMap<FString, FAgentPropertyValue> Props;
	for (const auto& Pair : Command.Properties)
	{
		Props.Add(Pair.Key, JsonToPropertyValue(Pair.Value));
	}

	Response.bSuccess = FActorOperations::SetActorProperties(Actor, Props);
	if (!Response.bSuccess)
	{
		Response.ErrorMessage = TEXT("Failed to set one or more properties");
	}
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FSetActorTransformCommand& Command, FAgentResponseBase& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	FString Error;
	AActor* Actor = ResolveActor(Command.ActorId, &Error);
	if (!Actor)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	FTransform NewTransform = Actor->GetActorTransform();

	if (Command.Location.IsSet())
	{
		NewTransform.SetLocation(Command.Location.GetValue());
	}
	if (Command.Rotation.IsSet())
	{
		NewTransform.SetRotation(Command.Rotation.GetValue().Quaternion());
	}
	if (Command.Scale.IsSet())
	{
		NewTransform.SetScale3D(Command.Scale.GetValue());
	}

	Response.bSuccess = FActorOperations::SetActorTransform(Actor, NewTransform, Command.bSweep);
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

//~==============================================================================
// Property Path Commands
//~==============================================================================

void FCommandExecutor::Execute(const FGetPropertyPathCommand& Command, FPropertyValueResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	FString Error;
	AActor* Actor = ResolveActor(Command.ActorId, &Error);
	if (!Actor)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	FPropertyPathResult Result = FAgentPropertyPath::GetValue(Actor, Command.Path);
	if (!Result.bSuccess)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Result.ErrorMessage;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	Response.Value = PropertyValueToJson(Result.Value);
	Response.TypeName = FString::Printf(TEXT("%d"), static_cast<int32>(Result.Value.Type));
	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FSetPropertyPathCommand& Command, FAgentResponseBase& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	FString Error;
	AActor* Actor = ResolveActor(Command.ActorId, &Error);
	if (!Actor)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	FAgentPropertyValue Value = JsonToPropertyValue(Command.Value);
	Response.bSuccess = FAgentPropertyPath::SetValue(Actor, Command.Path, Value);
	if (!Response.bSuccess)
	{
		Response.ErrorMessage = FString::Printf(TEXT("Failed to set path '%s'"), *Command.Path);
	}
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

//~==============================================================================
// Function Commands
//~==============================================================================

void FCommandExecutor::Execute(const FCallFunctionCommand& Command, FFunctionCallResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	UObject* Target = nullptr;
	UClass* TargetClass = nullptr;

	// Resolve target
	if (!Command.ActorId.IsEmpty())
	{
		FString Error;
		Target = ResolveActor(Command.ActorId, &Error);
		if (!Target)
		{
			Response.bSuccess = false;
			Response.ErrorMessage = Error;
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}
		TargetClass = Target->GetClass();
	}
	else if (!Command.ClassName.IsEmpty())
	{
		TargetClass = FTypeDiscovery::FindClassByName(Command.ClassName);
		if (!TargetClass)
		{
			Response.bSuccess = false;
			Response.ErrorMessage = FString::Printf(TEXT("Class '%s' not found"), *Command.ClassName);
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}
	}
	else
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Must specify ActorId or ClassName");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Find function
	UFunction* Function = FFunctionInvoker::FindFunction(TargetClass, Command.FunctionName);
	if (!Function)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Function '%s' not found"), *Command.FunctionName);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Convert parameters
	TMap<FString, FAgentPropertyValue> Params;
	for (const auto& Pair : Command.Parameters)
	{
		Params.Add(Pair.Key, JsonToPropertyValue(Pair.Value));
	}

	// Invoke
	FAgentFunctionResult Result = FFunctionInvoker::InvokeFunction(Target, Function, Params);

	Response.bSuccess = Result.bSuccess;
	Response.ErrorMessage = Result.ErrorMessage;

	if (Result.bSuccess)
	{
		if (Result.ReturnValue.Type != EAgentPropertyType::None)
		{
			Response.ReturnValue = PropertyValueToJson(Result.ReturnValue);
		}

		for (const auto& Pair : Result.OutParams)
		{
			if (Pair.Value.IsValid())
			{
				Response.OutParameters.Add(Pair.Key, PropertyValueToJson(*Pair.Value));
			}
		}
	}

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FGetFunctionSignatureCommand& Command, FAgentResponseBase& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	UClass* Class = FTypeDiscovery::FindClassByName(Command.ClassName);
	if (!Class)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Class '%s' not found"), *Command.ClassName);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	UFunction* Function = FFunctionInvoker::FindFunction(Class, Command.FunctionName);
	if (!Function)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Function '%s' not found"), *Command.FunctionName);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// TODO: Return signature in response
	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

//~==============================================================================
// Type Discovery Commands
//~==============================================================================

void FCommandExecutor::Execute(const FFindClassCommand& Command, FAgentResponseBase& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	UClass* Class = FTypeDiscovery::FindClassByName(Command.ClassName);
	Response.bSuccess = (Class != nullptr);
	if (!Response.bSuccess)
	{
		Response.ErrorMessage = FString::Printf(TEXT("Class '%s' not found"), *Command.ClassName);
	}
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FGetClassSchemaCommand& Command, FAgentResponseBase& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	UClass* Class = FTypeDiscovery::FindClassByName(Command.ClassName);
	if (!Class)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Class '%s' not found"), *Command.ClassName);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// TODO: Return full schema in response
	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FListClassesCommand& Command, FListClassesResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	// Resolve base class
	UClass* BaseClass = AActor::StaticClass();
	if (!Command.BaseClassName.IsEmpty())
	{
		BaseClass = FTypeDiscovery::FindClassByName(Command.BaseClassName);
		if (!BaseClass)
		{
			Response.bSuccess = false;
			Response.ErrorMessage = FString::Printf(TEXT("Base class '%s' not found"), *Command.BaseClassName);
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}
	}

	int32 Count = 0;
	for (TObjectIterator<UClass> It; It && Count < Command.Limit; ++It)
	{
		UClass* Class = *It;

		if (!Class->IsChildOf(BaseClass))
		{
			continue;
		}

		if (!Command.bIncludeAbstract && Class->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}

		bool bIsBP = Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
		if (!Command.bIncludeBlueprint && bIsBP)
		{
			continue;
		}

		if (!Command.NamePattern.IsEmpty() && !Class->GetName().Contains(Command.NamePattern))
		{
			continue;
		}

		FClassInfo Info;
		Info.ClassName = Class->GetName();
		Info.DisplayName = Class->GetName();
		Info.ClassPath = Class->GetPathName();
		Info.bIsBlueprint = bIsBP;
		Info.bIsAbstract = Class->HasAnyClassFlags(CLASS_Abstract);

		if (Class->GetSuperClass())
		{
			Info.ParentClassName = Class->GetSuperClass()->GetName();
		}

		Response.Classes.Add(Info);
		Count++;
	}

	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

//~==============================================================================
// JSON Serialization
//~==============================================================================

FString FCommandExecutor::PropertyValueToJson(const FAgentPropertyValue& Value)
{
	switch (Value.Type)
	{
	case EAgentPropertyType::Bool:
		return Value.AsBool() ? TEXT("true") : TEXT("false");

	case EAgentPropertyType::Int8:
	case EAgentPropertyType::Int16:
	case EAgentPropertyType::Int32:
	case EAgentPropertyType::Int64:
	case EAgentPropertyType::UInt8:
	case EAgentPropertyType::UInt16:
	case EAgentPropertyType::UInt32:
	case EAgentPropertyType::UInt64:
		return FString::Printf(TEXT("%lld"), Value.AsInt());

	case EAgentPropertyType::Float:
	case EAgentPropertyType::Double:
		return FString::Printf(TEXT("%f"), Value.AsFloat());

	case EAgentPropertyType::String:
	case EAgentPropertyType::Name:
	case EAgentPropertyType::Text:
		return FString::Printf(TEXT("\"%s\""), *Value.AsString().ReplaceCharWithEscapedChar());

	case EAgentPropertyType::Vector:
	{
		FVector V = Value.AsVector();
		return FString::Printf(TEXT("{\"X\":%f,\"Y\":%f,\"Z\":%f}"), V.X, V.Y, V.Z);
	}

	case EAgentPropertyType::Rotator:
	{
		FRotator R = Value.AsRotator();
		return FString::Printf(TEXT("{\"Pitch\":%f,\"Yaw\":%f,\"Roll\":%f}"), R.Pitch, R.Yaw, R.Roll);
	}

	case EAgentPropertyType::Transform:
	{
		FTransform T = Value.AsTransform();
		FVector Loc = T.GetLocation();
		FRotator Rot = T.Rotator();
		FVector Scale = T.GetScale3D();
		return FString::Printf(
			TEXT("{\"Location\":{\"X\":%f,\"Y\":%f,\"Z\":%f},\"Rotation\":{\"Pitch\":%f,\"Yaw\":%f,\"Roll\":%f},\"Scale\":{\"X\":%f,\"Y\":%f,\"Z\":%f}}"),
			Loc.X, Loc.Y, Loc.Z,
			Rot.Pitch, Rot.Yaw, Rot.Roll,
			Scale.X, Scale.Y, Scale.Z
		);
	}

	case EAgentPropertyType::Object:
	case EAgentPropertyType::SoftObject:
	case EAgentPropertyType::WeakObject:
	case EAgentPropertyType::Class:
		return FString::Printf(TEXT("\"%s\""), *Value.AsString().ReplaceCharWithEscapedChar());

	case EAgentPropertyType::Array:
	{
		FString Result = TEXT("[");
		for (int32 i = 0; i < Value.ArrayValue.Num(); i++)
		{
			if (i > 0) Result += TEXT(",");
			if (Value.ArrayValue[i].IsValid())
			{
				Result += PropertyValueToJson(*Value.ArrayValue[i]);
			}
			else
			{
				Result += TEXT("null");
			}
		}
		Result += TEXT("]");
		return Result;
	}

	case EAgentPropertyType::Struct:
	case EAgentPropertyType::Map:
	{
		FString Result = TEXT("{");
		bool bFirst = true;
		for (const auto& Pair : Value.StructValue)
		{
			if (!bFirst) Result += TEXT(",");
			bFirst = false;
			Result += FString::Printf(TEXT("\"%s\":"), *Pair.Key.ReplaceCharWithEscapedChar());
			if (Pair.Value.IsValid())
			{
				Result += PropertyValueToJson(*Pair.Value);
			}
			else
			{
				Result += TEXT("null");
			}
		}
		Result += TEXT("}");
		return Result;
	}

	default:
		return TEXT("null");
	}
}

FAgentPropertyValue FCommandExecutor::JsonToPropertyValue(const FString& Json, EAgentPropertyType TypeHint)
{
	FAgentPropertyValue Value;

	if (Json.IsEmpty() || Json == TEXT("null"))
	{
		return Value;
	}

	// Simple type detection from JSON
	if (Json == TEXT("true"))
	{
		return FAgentPropertyValue(true);
	}
	if (Json == TEXT("false"))
	{
		return FAgentPropertyValue(false);
	}

	// String (quoted)
	if (Json.StartsWith(TEXT("\"")) && Json.EndsWith(TEXT("\"")))
	{
		FString Unquoted = Json.Mid(1, Json.Len() - 2);
		// Unescape
		Unquoted = Unquoted.ReplaceEscapedCharWithChar();
		return FAgentPropertyValue(Unquoted);
	}

	// Number
	if (Json.IsNumeric() || (Json.StartsWith(TEXT("-")) && Json.Mid(1).IsNumeric()))
	{
		if (Json.Contains(TEXT(".")))
		{
			return FAgentPropertyValue(FCString::Atod(*Json));
		}
		else
		{
			return FAgentPropertyValue(static_cast<int64>(FCString::Atoi64(*Json)));
		}
	}

	// For complex types (objects, arrays), store as string and let the caller parse
	Value.Type = EAgentPropertyType::String;
	Value.StringValue = Json;
	return Value;
}

FString FCommandExecutor::ActorInfoToJson(const FActorInfo& Info)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

	Obj->SetStringField(TEXT("guid"), Info.Guid);
	Obj->SetStringField(TEXT("path"), Info.Path);
	Obj->SetStringField(TEXT("name"), Info.Name);
	Obj->SetStringField(TEXT("label"), Info.Label);
	Obj->SetStringField(TEXT("className"), Info.ClassName);
	Obj->SetBoolField(TEXT("hidden"), Info.bHidden);

	TSharedPtr<FJsonObject> Location = MakeShared<FJsonObject>();
	Location->SetNumberField(TEXT("x"), Info.Location.X);
	Location->SetNumberField(TEXT("y"), Info.Location.Y);
	Location->SetNumberField(TEXT("z"), Info.Location.Z);
	Obj->SetObjectField(TEXT("location"), Location);

	TSharedPtr<FJsonObject> Rotation = MakeShared<FJsonObject>();
	Rotation->SetNumberField(TEXT("pitch"), Info.Rotation.Pitch);
	Rotation->SetNumberField(TEXT("yaw"), Info.Rotation.Yaw);
	Rotation->SetNumberField(TEXT("roll"), Info.Rotation.Roll);
	Obj->SetObjectField(TEXT("rotation"), Rotation);

	TSharedPtr<FJsonObject> Scale = MakeShared<FJsonObject>();
	Scale->SetNumberField(TEXT("x"), Info.Scale.X);
	Scale->SetNumberField(TEXT("y"), Info.Scale.Y);
	Scale->SetNumberField(TEXT("z"), Info.Scale.Z);
	Obj->SetObjectField(TEXT("scale"), Scale);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);

	return OutputString;
}

FString FCommandExecutor::WorldInfoToJson(const FWorldInfo& Info)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

	Obj->SetStringField(TEXT("worldType"), Info.WorldType);
	Obj->SetStringField(TEXT("worldName"), Info.WorldName);
	Obj->SetNumberField(TEXT("pieInstance"), Info.PIEInstance);
	Obj->SetBoolField(TEXT("hasBegunPlay"), Info.bHasBegunPlay);
	Obj->SetNumberField(TEXT("actorCount"), Info.ActorCount);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);

	return OutputString;
}

//~==============================================================================
// JSON Command Execution
//~==============================================================================

FString FCommandExecutor::ExecuteJson(const FString& CommandJson)
{
	// Parse the command JSON
	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CommandJson);

	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		FAgentResponseBase ErrorResponse;
		ErrorResponse.bSuccess = false;
		ErrorResponse.ErrorMessage = TEXT("Failed to parse command JSON");
		// TODO: Serialize error response
		return TEXT("{\"success\":false,\"error\":\"Failed to parse command JSON\"}");
	}

	// Get command type
	FString TypeStr;
	if (!JsonObj->TryGetStringField(TEXT("type"), TypeStr))
	{
		return TEXT("{\"success\":false,\"error\":\"Missing command type\"}");
	}

	// TODO: Dispatch based on type and return serialized response
	// For now, return a placeholder
	return TEXT("{\"success\":true,\"message\":\"Command received\"}");
}

FString FCommandExecutor::ExecuteBatchJson(const FString& CommandsJson, bool bStopOnError)
{
	// TODO: Implement batch execution
	return TEXT("{\"success\":true,\"responses\":[]}");
}
