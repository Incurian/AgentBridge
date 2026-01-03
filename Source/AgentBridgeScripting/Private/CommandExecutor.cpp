#include "CommandExecutor.h"
#include "ActorOperations.h"
#include "WorldContextManager.h"
#include "AgentPropertyPath.h"
#include "FunctionInvoker.h"
#include "TypeDiscovery.h"
#include "PropertyAccessor.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "UObject/UObjectIterator.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Blueprint.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

// Package saving
#include "UObject/SavePackage.h"

// Capture-related includes
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "ImageUtils.h"
#include "Misc/Base64.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"

// Material-related includes
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/MeshComponent.h"
#include "Engine/Texture.h"
#include "EngineUtils.h"

#if WITH_EDITOR
#include "LevelEditorViewport.h"
#include "Editor/EditorEngine.h"
#include "UnrealEdGlobals.h"
#include "HighResScreenshot.h"
#include "Slate/SceneViewport.h"

// Blueprint node manipulation includes
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_ExecutionSequence.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

// Forward declaration - implementation in JSON Serialization section
static FString PropertyTypeToString(EAgentPropertyType Type);

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
// Object Resolution (Actor or Asset)
// Following the "tools should just work" philosophy - automatically determine
// whether the ID refers to an actor in the world or an asset on disk.
//~==============================================================================

UObject* FCommandExecutor::ResolveObject(const FString& ObjectId, FString* OutError)
{
	if (ObjectId.IsEmpty())
	{
		if (OutError) *OutError = TEXT("ObjectId is empty");
		return nullptr;
	}

	// First, try to resolve as an actor (most common case)
	// This handles: actor names, labels, GUIDs, paths like "PersistentLevel.MyActor"
	UWorld* World = FWorldContextManager::Get().GetTargetWorld();
	if (World)
	{
		AActor* Actor = FActorOperations::FindActorByName(ObjectId, World);
		if (Actor)
		{
			return Actor;
		}
	}

	// If it looks like an asset path (contains /Game/ or starts with /), try loading it
	if (ObjectId.Contains(TEXT("/Game/")) || ObjectId.Contains(TEXT("/Script/")) ||
		ObjectId.StartsWith(TEXT("/")))
	{
		FSoftObjectPath SoftPath(ObjectId);

		// Try to resolve without loading first
		UObject* Object = SoftPath.ResolveObject();
		if (Object)
		{
			return Object;
		}

		// Try to load the asset
		Object = SoftPath.TryLoad();
		if (Object)
		{
			return Object;
		}

		// For DataAssets, the path might be missing the class suffix
		// e.g., "/Game/MyAsset" instead of "/Game/MyAsset.MyAsset"
		if (!ObjectId.Contains(TEXT(".")))
		{
			// Try appending the asset name as the class name
			FString AssetName = FPaths::GetBaseFilename(ObjectId);
			FString FullPath = ObjectId + TEXT(".") + AssetName;
			FSoftObjectPath FullSoftPath(FullPath);
			Object = FullSoftPath.TryLoad();
			if (Object)
			{
				return Object;
			}
		}
	}

	// If we get here, we couldn't resolve it
	if (OutError)
	{
		if (World)
		{
			*OutError = FString::Printf(
				TEXT("Could not resolve '%s' as actor or asset. "
					 "For assets, use full path like '/Game/MyAsset.MyAsset'"),
				*ObjectId);
		}
		else
		{
			*OutError = FString::Printf(
				TEXT("No world available and could not resolve '%s' as asset"),
				*ObjectId);
		}
	}

	return nullptr;
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

void FCommandExecutor::Execute(const FGetCapabilitiesCommand& Command, FGetCapabilitiesResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	FWorldContextCapabilities Caps = FWorldContextManager::Get().GetCapabilities();

	// Context identification
	Response.WorldType = Caps.WorldType;
	Response.WorldName = Caps.WorldName;
	Response.bIsGameplayActive = Caps.bIsGameplayActive;
	Response.PIEInstance = Caps.PIEInstance;

	// Core reflection (always available)
	Response.bCanIterateProperties = Caps.bCanIterateProperties;
	Response.bCanInvokeFunctions = Caps.bCanInvokeFunctions;
	Response.bCanSpawnActors = Caps.bCanSpawnActors;
	Response.bCanDestroyActors = Caps.bCanDestroyActors;
	Response.bCanModifyTransforms = Caps.bCanModifyTransforms;
	Response.bCanModifyProperties = Caps.bCanModifyProperties;

	// Editor-only features
	Response.bCanSetActorLabel = Caps.bCanSetActorLabel;
	Response.bCanSetActorFolder = Caps.bCanSetActorFolder;
	Response.bCanUseTransactions = Caps.bCanUseTransactions;
	Response.bHasPropertyMetadata = Caps.bHasPropertyMetadata;
	Response.bCanAccessEditorWorld = Caps.bCanAccessEditorWorld;

	// Explanations for unavailable features
	Response.LabelUnavailableReason = Caps.LabelUnavailableReason;
	Response.FolderUnavailableReason = Caps.FolderUnavailableReason;
	Response.TransactionUnavailableReason = Caps.TransactionUnavailableReason;
	Response.MetadataUnavailableReason = Caps.MetadataUnavailableReason;

	Response.bSuccess = true;
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
	Params.LabelPattern = Command.LabelPattern;
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

void FCommandExecutor::Execute(const FDuplicateActorCommand& Command, FSpawnActorResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	FString Error;
	AActor* SourceActor = ResolveActor(Command.ActorId, &Error);
	if (!SourceActor)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Build transform - use source transform as base, override with provided values
	FTransform NewTransform = SourceActor->GetActorTransform();
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

	AActor* NewActor = FActorOperations::DuplicateActor(SourceActor, NewTransform, Command.NewLabel);

	if (!NewActor)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Failed to duplicate actor");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	Response.Actor = BuildActorInfo(NewActor, false, false, 0);
	Response.bSuccess = true;
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
	// ResolveObject handles both actors and assets automatically
	// Following "tools should just work" philosophy - users can pass actor names OR asset paths
	UObject* Object = ResolveObject(Command.ActorId, &Error);
	if (!Object)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	FPropertyPathResult Result = FAgentPropertyPath::GetValue(Object, Command.Path);
	if (!Result.bSuccess)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Result.ErrorMessage;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	Response.Value = PropertyValueToJson(Result.Value);
	Response.TypeName = PropertyTypeToString(Result.Value.Type);
	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FSetPropertyPathCommand& Command, FAgentResponseBase& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	FString Error;
	// ResolveObject handles both actors and assets automatically
	// Following "tools should just work" philosophy - users can pass actor names OR asset paths
	UObject* Object = ResolveObject(Command.ActorId, &Error);
	if (!Object)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	FAgentPropertyValue Value = JsonToPropertyValue(Command.Value);
	Response.bSuccess = FAgentPropertyPath::SetValue(Object, Command.Path, Value);
	if (!Response.bSuccess)
	{
		Response.ErrorMessage = FString::Printf(TEXT("Failed to set path '%s'"), *Command.Path);
	}
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

//~==============================================================================
// Function Commands
//~==============================================================================

//~==============================================================================
// Function-to-Property Fallback Mapping
// Following "tools should just work" philosophy: FunctionInvoker has issues with
// struct return values, so we automatically redirect known getters to property access.
//~==============================================================================

static TMap<FString, FString> GetFunctionToPropertyMap()
{
	static TMap<FString, FString> Map;
	static bool bInitialized = false;

	if (!bInitialized)
	{
		// Actor transform getters -> equivalent property paths
		Map.Add(TEXT("K2_GetActorLocation"), TEXT("RootComponent.RelativeLocation"));
		Map.Add(TEXT("K2_GetActorRotation"), TEXT("RootComponent.RelativeRotation"));
		Map.Add(TEXT("GetActorScale3D"), TEXT("RootComponent.RelativeScale3D"));
		Map.Add(TEXT("GetActorLocation"), TEXT("RootComponent.RelativeLocation"));
		Map.Add(TEXT("GetActorRotation"), TEXT("RootComponent.RelativeRotation"));

		// Component getters
		Map.Add(TEXT("GetRelativeLocation"), TEXT("RelativeLocation"));
		Map.Add(TEXT("GetRelativeRotation"), TEXT("RelativeRotation"));
		Map.Add(TEXT("GetRelativeScale3D"), TEXT("RelativeScale3D"));
		Map.Add(TEXT("GetComponentLocation"), TEXT("RelativeLocation"));
		Map.Add(TEXT("GetComponentRotation"), TEXT("RelativeRotation"));
		Map.Add(TEXT("GetComponentScale"), TEXT("RelativeScale3D"));

		bInitialized = true;
	}

	return Map;
}

void FCommandExecutor::Execute(const FCallFunctionCommand& Command, FFunctionCallResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	UObject* Target = nullptr;
	UClass* TargetClass = nullptr;

	// Resolve target - ResolveObject handles both actors and assets
	if (!Command.ActorId.IsEmpty())
	{
		FString Error;
		Target = ResolveObject(Command.ActorId, &Error);
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

	// Check for function-to-property fallback (workaround for FunctionInvoker struct return issue)
	// Following "tools should just work" - if we know a function returns broken data, use property access
	const TMap<FString, FString>& FunctionToProperty = GetFunctionToPropertyMap();
	if (Target && FunctionToProperty.Contains(Command.FunctionName))
	{
		const FString& PropertyPath = FunctionToProperty[Command.FunctionName];
		FPropertyPathResult PathResult = FAgentPropertyPath::GetValue(Target, PropertyPath);

		if (PathResult.bSuccess)
		{
			Response.bSuccess = true;
			Response.ReturnValue = PropertyValueToJson(PathResult.Value);
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}
		// If property access fails, fall through to normal function invocation
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

void FCommandExecutor::Execute(const FCallAssetFunctionCommand& Command, FCallAssetFunctionResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	// Load the asset
	UObject* Asset = LoadObject<UObject>(nullptr, *Command.AssetPath);
	if (!Asset)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Asset '%s' not found"), *Command.AssetPath);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Navigate to subobject if specified
	UObject* Target = Asset;
	if (!Command.SubobjectPath.IsEmpty())
	{
		// Try to resolve the subobject path
		// For now, support simple property paths like "Nodes[0]"
		FPropertyPathResult PathResult = FAgentPropertyPath::GetValue(Asset, Command.SubobjectPath);
		if (!PathResult.bSuccess)
		{
			Response.bSuccess = false;
			Response.ErrorMessage = FString::Printf(TEXT("Subobject path '%s' not found: %s"),
				*Command.SubobjectPath, *PathResult.ErrorMessage);
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}

		// The path should resolve to a UObject
		if (PathResult.Value.Type != EAgentPropertyType::Object)
		{
			Response.bSuccess = false;
			Response.ErrorMessage = FString::Printf(TEXT("Subobject path '%s' did not resolve to a UObject (type: %d)"),
				*Command.SubobjectPath, (int32)PathResult.Value.Type);
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}

		// Extract the UObject pointer from the property value
		// Note: ObjectValue is stored as a path string, we need to load it
		if (!PathResult.Value.StringValue.IsEmpty())
		{
			Target = LoadObject<UObject>(nullptr, *PathResult.Value.StringValue);
			if (!Target)
			{
				Response.bSuccess = false;
				Response.ErrorMessage = FString::Printf(TEXT("Failed to load subobject at '%s'"),
					*PathResult.Value.StringValue);
				Response.ExecutionTimeMs = EndTiming(StartTime);
				return;
			}
		}
		else
		{
			Response.bSuccess = false;
			Response.ErrorMessage = FString::Printf(TEXT("Subobject path '%s' resolved to null object"),
				*Command.SubobjectPath);
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}
	}

	// Find function on target
	UFunction* Function = FFunctionInvoker::FindFunction(Target->GetClass(), Command.FunctionName);
	if (!Function)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Function '%s' not found on '%s' (class: %s)"),
			*Command.FunctionName, *Target->GetName(), *Target->GetClass()->GetName());
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Convert parameters
	TMap<FString, FAgentPropertyValue> Params;
	for (const auto& Pair : Command.Parameters)
	{
		Params.Add(Pair.Key, JsonToPropertyValue(Pair.Value));
	}

	// Invoke the function
	FAgentFunctionResult Result = FFunctionInvoker::InvokeFunction(Target, Function, Params);

	Response.bSuccess = Result.bSuccess;
	Response.ErrorMessage = Result.ErrorMessage;

	if (Result.bSuccess)
	{
		if (Result.ReturnValue.Type != EAgentPropertyType::None)
		{
			Response.ReturnValue = PropertyValueToJson(Result.ReturnValue);
			Response.ReturnTypeName = PropertyTypeToString(Result.ReturnValue.Type);
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

	// This command is deprecated - use GetClassSchema with include_functions=true instead.
	// GetClassSchema provides full function signatures with parameter types, return values, etc.
	// This stub remains for backwards compatibility with any HTTP JSON clients.

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

	// Function exists - return success (signature details available via GetClassSchema)
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

// Helper to find UScriptStruct by name (tries multiple name variants)
static UScriptStruct* FindStructByName(const FString& Name)
{
	// Try exact name
	if (UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *Name, true))
	{
		return Struct;
	}

	// Try with F prefix (UE convention for structs)
	if (!Name.StartsWith(TEXT("F")))
	{
		FString WithF = TEXT("F") + Name;
		if (UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *WithF, true))
		{
			return Struct;
		}
	}

	// Try without F prefix
	if (Name.StartsWith(TEXT("F")))
	{
		FString WithoutF = Name.Mid(1);
		if (UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *WithoutF, true))
		{
			return Struct;
		}
	}

	// Search through all packages (slower but more thorough)
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		UScriptStruct* Struct = *It;
		if (Struct->GetName() == Name ||
		    Struct->GetName() == (TEXT("F") + Name) ||
		    (Name.StartsWith(TEXT("F")) && Struct->GetName() == Name.Mid(1)))
		{
			return Struct;
		}
	}

	return nullptr;
}

void FCommandExecutor::Execute(const FGetClassSchemaCommand& Command, FGetClassSchemaResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	// First try to find as UClass
	UClass* Class = FTypeDiscovery::FindClassByName(Command.ClassName);

	// If not found as class, try as UScriptStruct
	// This allows get_class_schema to work on struct types like FBiomeAsset
	if (!Class)
	{
		UScriptStruct* Struct = FindStructByName(Command.ClassName);
		if (Struct)
		{
			// Build schema for struct
			FClassInfo& Schema = Response.Schema;
			Schema.ClassName = Struct->GetName();
			Schema.DisplayName = Struct->GetDisplayNameText().ToString();
			Schema.ClassPath = Struct->GetPathName();
			Schema.ParentClassName = Struct->GetSuperStruct() ? Struct->GetSuperStruct()->GetName() : TEXT("");
			Schema.bIsBlueprint = false;
			Schema.bIsAbstract = false;

			// Struct properties
			for (TFieldIterator<FProperty> PropIt(Struct); PropIt; ++PropIt)
			{
				FProperty* Prop = *PropIt;
				if (Prop->HasAnyPropertyFlags(CPF_Deprecated))
				{
					continue;
				}

				FAgentPropertyInfo Info;
				Info.PropertyName = Prop->GetName();
				Info.DisplayName = Prop->GetDisplayNameText().ToString();
				Info.TypeName = Prop->GetCPPType();
				Info.bIsReadOnly = Prop->HasAnyPropertyFlags(CPF_BlueprintReadOnly);
				Info.Category = Prop->GetMetaData(TEXT("Category"));
				Info.Description = Prop->GetMetaData(TEXT("ToolTip"));

				// Extract element type for container properties
				if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
				{
					Info.ElementType = ArrayProp->Inner->GetCPPType();
				}
				else if (FSetProperty* SetProp = CastField<FSetProperty>(Prop))
				{
					Info.ElementType = SetProp->ElementProp->GetCPPType();
				}
				else if (FMapProperty* MapProp = CastField<FMapProperty>(Prop))
				{
					Info.KeyType = MapProp->KeyProp->GetCPPType();
					Info.ElementType = MapProp->ValueProp->GetCPPType();
				}

				Schema.Properties.Add(MoveTemp(Info));
			}

			// Structs don't have functions
			Response.bSuccess = true;
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}

		// Neither class nor struct found
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Class or struct '%s' not found"), *Command.ClassName);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Class info
	FClassInfo& Schema = Response.Schema;
	Schema.ClassName = Class->GetName();
	Schema.DisplayName = Class->GetDisplayNameText().ToString();
	Schema.ClassPath = Class->GetPathName();
	Schema.ParentClassName = Class->GetSuperClass() ? Class->GetSuperClass()->GetName() : TEXT("");
	Schema.bIsBlueprint = Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
	Schema.bIsAbstract = Class->HasAnyClassFlags(CLASS_Abstract);

	// Properties - iterate using TFieldIterator
	EFieldIteratorFlags::SuperClassFlags SuperFlag = Command.bIncludeInherited
		? EFieldIteratorFlags::IncludeSuper
		: EFieldIteratorFlags::ExcludeSuper;

	for (TFieldIterator<FProperty> PropIt(Class, SuperFlag); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;

		// Skip deprecated and hidden properties
		if (Prop->HasAnyPropertyFlags(CPF_Deprecated))
		{
			continue;
		}

		FAgentPropertyInfo Info;
		Info.PropertyName = Prop->GetName();
		Info.DisplayName = Prop->GetDisplayNameText().ToString();
		Info.TypeName = Prop->GetCPPType();
		Info.bIsReadOnly = Prop->HasAnyPropertyFlags(CPF_BlueprintReadOnly);
		Info.Category = Prop->GetMetaData(TEXT("Category"));
		Info.Description = Prop->GetMetaData(TEXT("ToolTip"));

		// Extract element type for container properties
		if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
		{
			Info.ElementType = ArrayProp->Inner->GetCPPType();
		}
		else if (FSetProperty* SetProp = CastField<FSetProperty>(Prop))
		{
			Info.ElementType = SetProp->ElementProp->GetCPPType();
		}
		else if (FMapProperty* MapProp = CastField<FMapProperty>(Prop))
		{
			Info.KeyType = MapProp->KeyProp->GetCPPType();
			Info.ElementType = MapProp->ValueProp->GetCPPType();
		}

		Schema.Properties.Add(MoveTemp(Info));
	}

	// Functions (if requested)
	if (Command.bIncludeFunctions)
	{
		for (TFieldIterator<UFunction> FuncIt(Class, SuperFlag); FuncIt; ++FuncIt)
		{
			UFunction* Func = *FuncIt;

			// Only include Blueprint-callable functions
			if (!Func->HasAnyFunctionFlags(FUNC_BlueprintCallable))
			{
				continue;
			}

			FAgentFunctionSignature FuncInfo;
			FuncInfo.FunctionName = Func->GetName();
			FuncInfo.Description = Func->GetMetaData(TEXT("ToolTip"));
			FuncInfo.bIsStatic = Func->HasAnyFunctionFlags(FUNC_Static);
			FuncInfo.bIsBlueprintCallable = true;
			FuncInfo.bNeedsWorldContext = Func->HasMetaData(TEXT("WorldContext"));

			// Extract parameters and return value
			for (TFieldIterator<FProperty> ParamIt(Func); ParamIt; ++ParamIt)
			{
				FProperty* Param = *ParamIt;

				FAgentPropertyInfo ParamInfo;
				ParamInfo.PropertyName = Param->GetName();
				ParamInfo.DisplayName = Param->GetDisplayNameText().ToString();
				ParamInfo.TypeName = Param->GetCPPType();
				ParamInfo.bIsReadOnly = Param->HasAnyPropertyFlags(CPF_ConstParm);

				if (Param->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					FuncInfo.ReturnValue = MoveTemp(ParamInfo);
				}
				else if (!Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm))
				{
					// Input parameter (or in/out reference)
					FuncInfo.Parameters.Add(MoveTemp(ParamInfo));
				}
			}

			Schema.Functions.Add(MoveTemp(FuncInfo));
		}
	}

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

	// Track class paths we've already added to avoid duplicates
	TSet<FString> AddedClassPaths;
	int32 Count = 0;

	// Phase 1: Iterate loaded classes (fast, no asset loading)
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

		if (!Command.NamePattern.IsEmpty() && !Class->GetName().MatchesWildcard(Command.NamePattern, ESearchCase::IgnoreCase))
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

		AddedClassPaths.Add(Info.ClassPath);
		Response.Classes.Add(Info);
		Count++;
	}

	// Phase 2: If requesting Blueprints and we have room, check AssetRegistry for unloaded BP classes
	// This finds engine plugin BPs that aren't loaded into memory yet (e.g., PCGBiome classes)
	if (Command.bIncludeBlueprint && Count < Command.Limit && !Command.NamePattern.IsEmpty())
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		// Query for Blueprint assets
		TArray<FAssetData> BlueprintAssets;
		AssetRegistry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), BlueprintAssets, true);

		for (const FAssetData& AssetData : BlueprintAssets)
		{
			if (Count >= Command.Limit)
			{
				break;
			}

			// Get the generated class name (without loading the asset)
			FString GeneratedClassName;
			FAssetDataTagMapSharedView::FFindTagResult GeneratedClassResult =
				AssetData.TagsAndValues.FindTag(FBlueprintTags::GeneratedClassPath);
			if (GeneratedClassResult.IsSet())
			{
				GeneratedClassName = GeneratedClassResult.GetValue();
			}

			// Check if we already have this class from the loaded iteration
			if (AddedClassPaths.Contains(GeneratedClassName))
			{
				continue;
			}

			// Extract the simple class name from the path
			FString SimpleClassName = FPackageName::GetShortName(GeneratedClassName);
			SimpleClassName.RemoveFromEnd(TEXT("_C"));

			// Apply name pattern filter (supports wildcards like *Light*, BP_*)
			if (!SimpleClassName.MatchesWildcard(Command.NamePattern, ESearchCase::IgnoreCase))
			{
				continue;
			}

			// Load the class to verify inheritance
			UClass* GeneratedClass = LoadClass<UObject>(nullptr, *GeneratedClassName);
			if (!GeneratedClass || !GeneratedClass->IsChildOf(BaseClass))
			{
				continue;
			}

			FClassInfo Info;
			Info.ClassName = SimpleClassName;
			Info.DisplayName = SimpleClassName;
			Info.ClassPath = GeneratedClassName;
			Info.bIsBlueprint = true;
			Info.bIsAbstract = GeneratedClass->HasAnyClassFlags(CLASS_Abstract);

			if (GeneratedClass->GetSuperClass())
			{
				Info.ParentClassName = GeneratedClass->GetSuperClass()->GetName();
			}

			AddedClassPaths.Add(Info.ClassPath);
			Response.Classes.Add(Info);
			Count++;
		}
	}

	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

//~==============================================================================
// DataAsset Commands
//~==============================================================================

void FCommandExecutor::Execute(const FListDataAssetsCommand& Command, FListDataAssetsResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	// Get asset registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Determine base class to search for
	UClass* BaseClass = UDataAsset::StaticClass();
	if (!Command.BaseClassName.IsEmpty())
	{
		if (Command.BaseClassName.Equals(TEXT("DataTable"), ESearchCase::IgnoreCase))
		{
			BaseClass = UDataTable::StaticClass();
		}
		else
		{
			UClass* FoundClass = FindFirstObject<UClass>(*Command.BaseClassName, EFindFirstObjectOptions::None);
			if (FoundClass && FoundClass->IsChildOf(UDataAsset::StaticClass()))
			{
				BaseClass = FoundClass;
			}
		}
	}

	// Query assets
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByClass(BaseClass->GetClassPathName(), AssetDataList, true);

	int32 Count = 0;
	for (const FAssetData& AssetData : AssetDataList)
	{
		if (Count >= Command.Limit)
		{
			break;
		}

		// Filter by path if specified
		if (!Command.PathFilter.IsEmpty())
		{
			FString PackagePath = AssetData.PackagePath.ToString();
			if (!PackagePath.MatchesWildcard(Command.PathFilter))
			{
				continue;
			}
		}

		FDataAssetInfo Info;
		Info.AssetPath = AssetData.GetObjectPathString();
		Info.AssetName = AssetData.AssetName.ToString();
		Info.ClassName = AssetData.AssetClassPath.GetAssetName().ToString();
		Info.bIsDataTable = AssetData.AssetClassPath.GetAssetName() == UDataTable::StaticClass()->GetFName();

		// Load asset to get more details (optional - could be slow for large sets)
		if (UObject* Asset = AssetData.GetAsset())
		{
			if (UDataTable* DataTable = Cast<UDataTable>(Asset))
			{
				Info.bIsDataTable = true;
				Info.RowCount = DataTable->GetRowMap().Num();
			}
			if (Asset->GetClass()->IsChildOf(UPrimaryDataAsset::StaticClass()))
			{
				Info.bIsPrimaryDataAsset = true;
			}
		}

		Response.Assets.Add(Info);
		Count++;
	}

	Response.TotalCount = AssetDataList.Num();
	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FGetDataAssetCommand& Command, FGetDataAssetResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	// Load the asset
	UObject* Asset = LoadObject<UObject>(nullptr, *Command.AssetPath);
	if (!Asset)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("DataAsset not found: %s"), *Command.AssetPath);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Verify it's a DataAsset
	if (!Asset->IsA(UDataAsset::StaticClass()) && !Asset->IsA(UDataTable::StaticClass()))
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Asset is not a DataAsset: %s"), *Command.AssetPath);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	Response.Asset.AssetPath = Asset->GetPathName();
	Response.Asset.AssetName = Asset->GetName();
	Response.Asset.ClassName = Asset->GetClass()->GetName();
	Response.Asset.bIsDataTable = Asset->IsA(UDataTable::StaticClass());
	Response.Asset.bIsPrimaryDataAsset = Asset->GetClass()->IsChildOf(UPrimaryDataAsset::StaticClass());

	if (UDataTable* DataTable = Cast<UDataTable>(Asset))
	{
		Response.Asset.RowCount = DataTable->GetRowMap().Num();
	}

	// Read properties using PropertyAccessor
	for (TFieldIterator<FProperty> PropIt(Asset->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;

		// Skip transient and config properties
		if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_Config))
		{
			continue;
		}

		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Asset);
		FAgentPropertyValue Value = FPropertyAccessor::ReadProperty(ValuePtr, Property, Command.PropertyDepth);
		Response.Asset.Properties.Add(Property->GetName(), PropertyValueToJson(Value));
	}

	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FGetDataTableRowCommand& Command, FGetDataTableRowResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	// Load the DataTable
	UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *Command.TablePath);
	if (!DataTable)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("DataTable not found: %s"), *Command.TablePath);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	if (!RowStruct)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("DataTable has no row struct");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	Response.RowStructName = RowStruct->GetName();
	Response.TotalRowCount = DataTable->GetRowMap().Num();

	const TMap<FName, uint8*>& RowMap = DataTable->GetRowMap();

	if (!Command.RowName.IsEmpty())
	{
		// Get specific row
		FName RowFName(*Command.RowName);
		uint8* const* RowPtr = RowMap.Find(RowFName);
		if (!RowPtr)
		{
			Response.bSuccess = false;
			Response.ErrorMessage = FString::Printf(TEXT("Row not found: %s"), *Command.RowName);
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}

		FDataTableRowInfo RowInfo;
		RowInfo.RowName = Command.RowName;

		// Read row properties
		for (TFieldIterator<FProperty> PropIt(RowStruct); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(*RowPtr);
			FAgentPropertyValue Value = FPropertyAccessor::ReadProperty(ValuePtr, Property, 3);
			RowInfo.Data.Add(Property->GetName(), PropertyValueToJson(Value));
		}

		Response.Rows.Add(RowInfo);
	}
	else
	{
		// Get all rows up to limit
		int32 Count = 0;
		for (const auto& Pair : RowMap)
		{
			if (Count >= Command.Limit)
			{
				break;
			}

			FDataTableRowInfo RowInfo;
			RowInfo.RowName = Pair.Key.ToString();

			// Read row properties
			for (TFieldIterator<FProperty> PropIt(RowStruct); PropIt; ++PropIt)
			{
				FProperty* Property = *PropIt;
				const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Pair.Value);
				FAgentPropertyValue Value = FPropertyAccessor::ReadProperty(ValuePtr, Property, 3);
				RowInfo.Data.Add(Property->GetName(), PropertyValueToJson(Value));
			}

			Response.Rows.Add(RowInfo);
			Count++;
		}
	}

	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

//~==============================================================================
// Capture Commands
//~==============================================================================

namespace
{
	/**
	 * Encodes pixel data to PNG and returns base64 string.
	 */
	FString EncodePixelsToPngBase64(const TArray<FColor>& Pixels, int32 Width, int32 Height)
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

		if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
		{
			TArray64<uint8> CompressedData = ImageWrapper->GetCompressed(100);
			if (CompressedData.Num() > 0)
			{
				return FBase64::Encode(CompressedData.GetData(), CompressedData.Num());
			}
		}
		return FString();
	}

	/**
	 * Saves pixel data to file as PNG.
	 */
	bool SavePixelsToPng(const TArray<FColor>& Pixels, int32 Width, int32 Height, const FString& FilePath)
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

		if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
		{
			TArray64<uint8> CompressedData = ImageWrapper->GetCompressed(100);
			if (CompressedData.Num() > 0)
			{
				return FFileHelper::SaveArrayToFile(CompressedData, *FilePath);
			}
		}
		return false;
	}
}

void FCommandExecutor::Execute(const FCaptureViewportCommand& Command, FCaptureViewportResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

#if WITH_EDITOR
	// Get the active viewport
	FViewport* Viewport = GEditor ? GEditor->GetActiveViewport() : nullptr;
	if (!Viewport)
	{
		// Try PIE viewport
		if (GEngine && GEngine->GameViewport)
		{
			Viewport = GEngine->GameViewport->Viewport;
		}
	}

	if (!Viewport)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("No viewport available. Viewport capture requires Editor or PIE context.");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Get viewport size
	FIntPoint ViewportSize = Viewport->GetSizeXY();
	int32 Width = Command.Width > 0 ? Command.Width : ViewportSize.X;
	int32 Height = Command.Height > 0 ? Command.Height : ViewportSize.Y;

	// Read pixels from viewport
	TArray<FColor> Pixels;
	if (!Viewport->ReadPixels(Pixels))
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Failed to read pixels from viewport");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	Response.Width = ViewportSize.X;
	Response.Height = ViewportSize.Y;
	Response.Format = TEXT("PNG");
	Response.SizeBytes = Pixels.Num() * sizeof(FColor);

	// Either save to file or return base64
	if (!Command.OutputPath.IsEmpty())
	{
		if (SavePixelsToPng(Pixels, ViewportSize.X, ViewportSize.Y, Command.OutputPath))
		{
			Response.FilePath = Command.OutputPath;
			Response.bSuccess = true;
		}
		else
		{
			Response.bSuccess = false;
			Response.ErrorMessage = FString::Printf(TEXT("Failed to save image to: %s"), *Command.OutputPath);
		}
	}
	else
	{
		Response.ImageData = EncodePixelsToPngBase64(Pixels, ViewportSize.X, ViewportSize.Y);
		Response.bSuccess = !Response.ImageData.IsEmpty();
		if (!Response.bSuccess)
		{
			Response.ErrorMessage = TEXT("Failed to encode image to base64");
		}
	}
#else
	Response.bSuccess = false;
	Response.ErrorMessage = TEXT("Viewport capture requires WITH_EDITOR build");
#endif

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FCaptureSceneCommand& Command, FCaptureSceneResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	UWorld* World = FWorldContextManager::Get().GetTargetWorld();
	if (!World)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("No target world available");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	USceneCaptureComponent2D* CaptureComponent = nullptr;
	ASceneCapture2D* TempCapture = nullptr;
	FVector CameraLocation = Command.Location;
	FRotator CameraRotation = Command.Rotation;

	// Try to find existing capture component
	if (!Command.ActorId.IsEmpty())
	{
		AActor* Actor = FActorOperations::FindActorByName(Command.ActorId, World);
		if (!Actor)
		{
			Response.bSuccess = false;
			Response.ErrorMessage = FString::Printf(TEXT("Actor not found: %s"), *Command.ActorId);
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}

		// Find SceneCaptureComponent2D
		if (!Command.ComponentName.IsEmpty())
		{
			CaptureComponent = Cast<USceneCaptureComponent2D>(Actor->GetComponentByClass(USceneCaptureComponent2D::StaticClass()));
		}
		else
		{
			TArray<USceneCaptureComponent2D*> Components;
			Actor->GetComponents<USceneCaptureComponent2D>(Components);
			if (Components.Num() > 0)
			{
				CaptureComponent = Components[0];
			}
		}

		if (!CaptureComponent)
		{
			Response.bSuccess = false;
			Response.ErrorMessage = FString::Printf(TEXT("No SceneCaptureComponent2D found on actor: %s"), *Command.ActorId);
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}

		CameraLocation = CaptureComponent->GetComponentLocation();
		CameraRotation = CaptureComponent->GetComponentRotation();
	}
	else
	{
		// Create temporary scene capture
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = MakeUniqueObjectName(World, ASceneCapture2D::StaticClass(), TEXT("AgentBridge_TempCapture"));
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		TempCapture = World->SpawnActor<ASceneCapture2D>(SpawnParams);
		if (!TempCapture)
		{
			Response.bSuccess = false;
			Response.ErrorMessage = TEXT("Failed to spawn temporary SceneCapture2D");
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}

		CaptureComponent = TempCapture->GetCaptureComponent2D();
		TempCapture->SetActorLocation(Command.Location);
		TempCapture->SetActorRotation(Command.Rotation);
	}

	// Create render target
	int32 Width = Command.Width > 0 ? Command.Width : 1280;
	int32 Height = Command.Height > 0 ? Command.Height : 720;

	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>();
	RenderTarget->InitAutoFormat(Width, Height);
	RenderTarget->ClearColor = FLinearColor::Black;
	RenderTarget->UpdateResourceImmediate(true);

	// Configure capture
	CaptureComponent->TextureTarget = RenderTarget;
	if (Command.FOV > 0)
	{
		CaptureComponent->FOVAngle = Command.FOV;
	}

	// Capture the scene
	CaptureComponent->CaptureScene();

	// Wait for render to complete
	FlushRenderingCommands();

	// Read pixels from render target
	TArray<FColor> Pixels;
	FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
	if (Resource)
	{
		Pixels.AddZeroed(Width * Height);
		FReadSurfaceDataFlags ReadFlags(RCM_UNorm);
		Resource->ReadPixels(Pixels, ReadFlags);
	}

	// Cleanup temporary actor
	if (TempCapture)
	{
		TempCapture->Destroy();
	}

	// Cleanup render target reference
	CaptureComponent->TextureTarget = nullptr;

	if (Pixels.Num() == 0)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Failed to read pixels from render target");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	Response.Width = Width;
	Response.Height = Height;
	Response.Format = TEXT("PNG");
	Response.SizeBytes = Pixels.Num() * sizeof(FColor);
	Response.CameraLocation = CameraLocation;
	Response.CameraRotation = CameraRotation;

	// Either save to file or return base64
	if (!Command.OutputPath.IsEmpty())
	{
		if (SavePixelsToPng(Pixels, Width, Height, Command.OutputPath))
		{
			Response.FilePath = Command.OutputPath;
			Response.bSuccess = true;
		}
		else
		{
			Response.bSuccess = false;
			Response.ErrorMessage = FString::Printf(TEXT("Failed to save image to: %s"), *Command.OutputPath);
		}
	}
	else
	{
		Response.ImageData = EncodePixelsToPngBase64(Pixels, Width, Height);
		Response.bSuccess = !Response.ImageData.IsEmpty();
		if (!Response.bSuccess)
		{
			Response.ErrorMessage = TEXT("Failed to encode image to base64");
		}
	}

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

//~==============================================================================
// Audio Commands
//~==============================================================================

void FCommandExecutor::Execute(const FGetAudioAnalysisCommand& Command, FAudioAnalysisResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	UWorld* World = FWorldContextManager::Get().GetTargetWorld();
	if (!World)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("No target world available");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Initialize frequency bands with zeros
	Response.FrequencyBands.SetNum(Command.FrequencyBands);
	for (int32 i = 0; i < Command.FrequencyBands; i++)
	{
		Response.FrequencyBands[i] = 0.0f;
	}

	// Get audio volume from FAudioDevice if available
	if (FAudioDeviceHandle AudioDevice = World->GetAudioDevice())
	{
		// Note: Full audio analysis would require setting up a Submix listener
		// or using AudioAnalysis plugin. For now, return basic info.
		Response.AverageVolume = 0.0f; // Would need submix capture
		Response.PeakVolume = 0.0f;
		Response.bBeatDetected = false;
		Response.CurrentTime = World->GetTimeSeconds();

		Response.bSuccess = true;
	}
	else
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("No audio device available");
	}

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FStartAudioCaptureCommand& Command, FStartAudioCaptureResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	// Audio capture requires AudioCapture or AudioMixer modules
	// and proper platform-specific setup. This is a placeholder
	// that indicates the feature structure.

	if (Command.Source == EAudioCaptureSource::PlayerMic)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Player microphone capture requires Voice module setup. "
			"Use IVoiceCapture interface or AudioCapture plugin.");
	}
	else if (Command.Source == EAudioCaptureSource::WorldAudio)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("World audio capture requires Audio Mixer Submix recording. "
			"Use UAudioMixerBlueprintLibrary::StartRecordingOutput() in game code.");
	}
	else
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Actor-specific audio capture not yet implemented. "
			"Consider using a SceneComponent with audio analysis capabilities.");
	}

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FStopAudioCaptureCommand& Command, FStopAudioCaptureResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	// Placeholder - would stop an active capture and return audio data
	Response.bSuccess = false;
	Response.ErrorMessage = TEXT("No active audio capture to stop. Audio capture not yet fully implemented.");

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

//~==============================================================================
// Material Commands
//~==============================================================================

void FCommandExecutor::Execute(const FListMaterialsCommand& Command, FListMaterialsResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Build filter
	FARFilter Filter;
	if (Command.bInstancesOnly)
	{
		Filter.ClassPaths.Add(UMaterialInstance::StaticClass()->GetClassPathName());
	}
	else
	{
		Filter.ClassPaths.Add(UMaterialInterface::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;
	}
	Filter.bRecursivePaths = true;

	if (!Command.PathFilter.IsEmpty())
	{
		// Extract path before wildcard
		FString PathPrefix = Command.PathFilter;
		int32 WildcardIndex;
		if (PathPrefix.FindChar('*', WildcardIndex))
		{
			PathPrefix = PathPrefix.Left(WildcardIndex);
		}
		Filter.PackagePaths.Add(FName(*PathPrefix));
	}

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);

	int32 Count = 0;
	for (const FAssetData& Asset : Assets)
	{
		if (Count >= Command.Limit)
		{
			break;
		}

		// Apply path filter wildcard matching
		if (!Command.PathFilter.IsEmpty())
		{
			FString AssetPath = Asset.GetSoftObjectPath().ToString();
			if (!AssetPath.MatchesWildcard(Command.PathFilter))
			{
				continue;
			}
		}

		FMaterialInfo Info;
		Info.AssetPath = Asset.GetSoftObjectPath().ToString();
		Info.Name = Asset.AssetName.ToString();
		Info.bIsMaterialInstance = Asset.AssetClassPath == UMaterialInstance::StaticClass()->GetClassPathName();

		Response.Materials.Add(Info);
		Count++;
	}

	Response.TotalCount = Assets.Num();
	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FGetMaterialInfoCommand& Command, FGetMaterialInfoResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	// Try to load the material
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *Command.MaterialPath);
	if (!Material)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Material not found: %s"), *Command.MaterialPath);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	Response.Material.AssetPath = Command.MaterialPath;
	Response.Material.Name = Material->GetName();
	Response.Material.bIsMaterialInstance = Material->IsA<UMaterialInstance>();

	if (UMaterialInstance* MatInst = Cast<UMaterialInstance>(Material))
	{
		if (MatInst->Parent)
		{
			Response.Material.ParentPath = MatInst->Parent->GetPathName();
		}
	}

	// Get material parameters if requested
	if (Command.bIncludeParameters)
	{
		TArray<FMaterialParameterInfo> ParamInfos;
		TArray<FGuid> Guids;

		// Scalar parameters
		Material->GetAllScalarParameterInfo(ParamInfos, Guids);
		for (const FMaterialParameterInfo& ParamInfo : ParamInfos)
		{
			FAgentMaterialParamInfo Info;
			Info.Name = ParamInfo.Name.ToString();
			Info.Type = TEXT("Scalar");

			float Value;
			if (Material->GetScalarParameterValue(ParamInfo, Value))
			{
				Info.Value = FString::Printf(TEXT("%f"), Value);
			}
			Response.Parameters.Add(Info);
		}

		ParamInfos.Empty();
		Guids.Empty();

		// Vector parameters
		Material->GetAllVectorParameterInfo(ParamInfos, Guids);
		for (const FMaterialParameterInfo& ParamInfo : ParamInfos)
		{
			FAgentMaterialParamInfo Info;
			Info.Name = ParamInfo.Name.ToString();
			Info.Type = TEXT("Vector");

			FLinearColor Value;
			if (Material->GetVectorParameterValue(ParamInfo, Value))
			{
				Info.Value = FString::Printf(TEXT("{\"r\":%f,\"g\":%f,\"b\":%f,\"a\":%f}"), Value.R, Value.G, Value.B, Value.A);
			}
			Response.Parameters.Add(Info);
		}

		ParamInfos.Empty();
		Guids.Empty();

		// Texture parameters
		Material->GetAllTextureParameterInfo(ParamInfos, Guids);
		for (const FMaterialParameterInfo& ParamInfo : ParamInfos)
		{
			FAgentMaterialParamInfo Info;
			Info.Name = ParamInfo.Name.ToString();
			Info.Type = TEXT("Texture");

			UTexture* Value;
			if (Material->GetTextureParameterValue(ParamInfo, Value) && Value)
			{
				Info.Value = Value->GetPathName();
			}
			Response.Parameters.Add(Info);
		}
	}

	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FCreateMaterialInstanceCommand& Command, FCreateMaterialInstanceResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	// Load parent material
	UMaterialInterface* ParentMaterial = LoadObject<UMaterialInterface>(nullptr, *Command.ParentMaterialPath);
	if (!ParentMaterial)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Parent material not found: %s"), *Command.ParentMaterialPath);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Find owner actor
	AActor* OwnerActor = nullptr;
	if (!Command.OwnerActorId.IsEmpty())
	{
		OwnerActor = ResolveActor(Command.OwnerActorId, &Response.ErrorMessage);
		if (!OwnerActor)
		{
			Response.bSuccess = false;
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}
	}

	// Create dynamic material instance
	UObject* Outer = OwnerActor ? static_cast<UObject*>(OwnerActor) : GetTransientPackage();
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ParentMaterial, Outer);
	if (!MID)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Failed to create material instance");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Set name for later lookup
	if (!Command.InstanceName.IsEmpty())
	{
		MID->Rename(*Command.InstanceName);
	}

	// Apply initial scalar parameters
	for (const auto& Param : Command.ScalarParameters)
	{
		MID->SetScalarParameterValue(FName(*Param.Key), Param.Value);
	}

	// Apply initial vector parameters (parse JSON)
	for (const auto& Param : Command.VectorParameters)
	{
		TSharedPtr<FJsonObject> JsonObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Param.Value);
		if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
		{
			FLinearColor Color;
			Color.R = JsonObj->GetNumberField(TEXT("r"));
			Color.G = JsonObj->GetNumberField(TEXT("g"));
			Color.B = JsonObj->GetNumberField(TEXT("b"));
			Color.A = JsonObj->HasField(TEXT("a")) ? JsonObj->GetNumberField(TEXT("a")) : 1.0f;
			MID->SetVectorParameterValue(FName(*Param.Key), Color);
		}
	}

	Response.InstanceName = Command.InstanceName.IsEmpty() ? MID->GetName() : Command.InstanceName;
	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FSetMaterialParameterCommand& Command, FAgentResponseBase& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	// Find target actor
	AActor* Actor = ResolveActor(Command.TargetId, &Response.ErrorMessage);
	if (!Actor)
	{
		Response.bSuccess = false;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Find mesh component
	UMeshComponent* MeshComp = nullptr;
	if (Command.ComponentName.IsEmpty())
	{
		MeshComp = Actor->FindComponentByClass<UMeshComponent>();
	}
	else
	{
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (Comp->GetName() == Command.ComponentName)
			{
				MeshComp = Cast<UMeshComponent>(Comp);
				break;
			}
		}
	}

	if (!MeshComp)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("No mesh component found on actor");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Get or create dynamic material instance
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MeshComp->GetMaterial(Command.SlotIndex));
	if (!MID)
	{
		UMaterialInterface* BaseMat = MeshComp->GetMaterial(Command.SlotIndex);
		if (!BaseMat)
		{
			Response.bSuccess = false;
			Response.ErrorMessage = FString::Printf(TEXT("No material at slot %d"), Command.SlotIndex);
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}
		MID = UMaterialInstanceDynamic::Create(BaseMat, Actor);
		MeshComp->SetMaterial(Command.SlotIndex, MID);
	}

	// Set parameter based on type
	switch (Command.ParameterType)
	{
	case EAgentMaterialParamType::Scalar:
		{
			float Value = FCString::Atof(*Command.Value);
			MID->SetScalarParameterValue(FName(*Command.ParameterName), Value);
		}
		break;

	case EAgentMaterialParamType::Vector:
		{
			TSharedPtr<FJsonObject> JsonObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Command.Value);
			if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
			{
				FLinearColor Color;
				Color.R = JsonObj->GetNumberField(TEXT("r"));
				Color.G = JsonObj->GetNumberField(TEXT("g"));
				Color.B = JsonObj->GetNumberField(TEXT("b"));
				Color.A = JsonObj->HasField(TEXT("a")) ? JsonObj->GetNumberField(TEXT("a")) : 1.0f;
				MID->SetVectorParameterValue(FName(*Command.ParameterName), Color);
			}
		}
		break;

	case EAgentMaterialParamType::Texture:
		{
			UTexture* Texture = LoadObject<UTexture>(nullptr, *Command.Value);
			if (Texture)
			{
				MID->SetTextureParameterValue(FName(*Command.ParameterName), Texture);
			}
		}
		break;

	default:
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Unsupported parameter type");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FApplyMaterialToActorCommand& Command, FAgentResponseBase& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	// Find target actor
	AActor* Actor = ResolveActor(Command.ActorId, &Response.ErrorMessage);
	if (!Actor)
	{
		Response.bSuccess = false;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Load material
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *Command.MaterialPath);
	if (!Material)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Material not found: %s"), *Command.MaterialPath);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Find mesh component
	UMeshComponent* MeshComp = nullptr;
	if (Command.ComponentName.IsEmpty())
	{
		MeshComp = Actor->FindComponentByClass<UMeshComponent>();
	}
	else
	{
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (Comp->GetName() == Command.ComponentName)
			{
				MeshComp = Cast<UMeshComponent>(Comp);
				break;
			}
		}
	}

	if (!MeshComp)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("No mesh component found on actor");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Apply material
	if (Command.SlotIndex < 0)
	{
		// Apply to all slots
		int32 NumMaterials = MeshComp->GetNumMaterials();
		for (int32 i = 0; i < NumMaterials; i++)
		{
			MeshComp->SetMaterial(i, Material);
		}
	}
	else
	{
		MeshComp->SetMaterial(Command.SlotIndex, Material);
	}

	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

//~==============================================================================
// PCG Commands
//~==============================================================================

void FCommandExecutor::Execute(const FListPCGActorsCommand& Command, FListPCGActorsResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	// PCG requires the PCG plugin. This is a stub that searches for actors
	// with "PCG" in their class name as a heuristic.
	UWorld* World = FWorldContextManager::Get().GetTargetWorld();
	if (!World)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("No target world available");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	int32 Count = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (Count >= Command.Limit)
		{
			break;
		}

		AActor* Actor = *It;
		FString ClassName = Actor->GetClass()->GetName();

		// Check if this is a PCG actor by class name
		if (!ClassName.Contains(TEXT("PCG")))
		{
			continue;
		}

		// Apply name filter
		if (!Command.NamePattern.IsEmpty())
		{
			FString ActorLabel = Actor->GetActorLabel();
			FString ActorName = Actor->GetName();
			if (!ActorLabel.MatchesWildcard(Command.NamePattern) && !ActorName.MatchesWildcard(Command.NamePattern))
			{
				continue;
			}
		}

		FPCGActorInfo Info;
		Info.Guid = Actor->GetActorGuid().ToString();
		Info.Name = Actor->GetName();
		Info.Label = Actor->GetActorLabel();
		Info.Status = TEXT("Unknown"); // Would need PCG module to determine

		Response.Actors.Add(Info);
		Count++;
	}

	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FRegeneratePCGCommand& Command, FRegeneratePCGResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	// PCG regeneration requires the PCG plugin module
	Response.bSuccess = false;
	Response.ErrorMessage = TEXT("PCG regeneration requires the PCG plugin. "
		"To use this feature, enable the PCG plugin and rebuild with PCG module dependency.");

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FSetPCGParameterCommand& Command, FAgentResponseBase& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	// PCG parameter modification requires the PCG plugin module
	Response.bSuccess = false;
	Response.ErrorMessage = TEXT("PCG parameter modification requires the PCG plugin. "
		"To use this feature, enable the PCG plugin and rebuild with PCG module dependency.");

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

//~==============================================================================
// Asset Commands (P0)
//~==============================================================================

void FCommandExecutor::Execute(const FCreateAssetCommand& Command, FCreateAssetResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

#if WITH_EDITOR
	// Validate inputs
	if (Command.AssetClass.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("AssetClass is required");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	if (Command.PackagePath.IsEmpty() || Command.AssetName.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("PackagePath and AssetName are required");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Validate path is within /Game/
	if (!Command.PackagePath.StartsWith(TEXT("/Game/")))
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("PackagePath must start with /Game/");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Find the asset class
	UClass* AssetClass = FTypeDiscovery::FindClassByName(Command.AssetClass);
	if (!AssetClass)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Asset class '%s' not found"), *Command.AssetClass);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Create the package
	FString PackageName = Command.PackagePath / Command.AssetName;
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Failed to create package '%s'"), *PackageName);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Create the asset object
	UObject* NewAsset = NewObject<UObject>(Package, AssetClass, *Command.AssetName, RF_Public | RF_Standalone);
	if (!NewAsset)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Failed to create asset object");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Set initial properties if provided
	for (const auto& Pair : Command.Properties)
	{
		// Find the property by name
		FProperty* Property = AssetClass->FindPropertyByName(*Pair.Key);
		if (Property)
		{
			FAgentPropertyValue PropValue = JsonToPropertyValue(Pair.Value);
			FPropertyAccessor::WriteProperty(NewAsset, Property, PropValue);
		}
	}

	// Mark the package dirty
	Package->MarkPackageDirty();

	// Notify asset registry
	FAssetRegistryModule::AssetCreated(NewAsset);

	Response.AssetPath = NewAsset->GetPathName();
	Response.AssetClass = AssetClass->GetName();
	Response.bSaved = false;
	Response.bSuccess = true;
#else
	Response.bSuccess = false;
	Response.ErrorMessage = TEXT("CreateAsset is only available in Editor builds");
#endif

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FSaveAssetCommand& Command, FSaveAssetResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

#if WITH_EDITOR
	if (Command.AssetPath.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("AssetPath is required");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Load the asset
	UObject* Asset = LoadObject<UObject>(nullptr, *Command.AssetPath);
	if (!Asset)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Asset '%s' not found"), *Command.AssetPath);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	UPackage* Package = Asset->GetOutermost();
	FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());

	// Save the package
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	bool bSaved = UPackage::SavePackage(Package, Asset, *PackageFileName, SaveArgs);

	if (bSaved)
	{
		Response.AssetPath = Command.AssetPath;

		// Get file size
		IFileManager& FileManager = IFileManager::Get();
		Response.FileSizeBytes = FileManager.FileSize(*PackageFileName);

		Response.bSuccess = true;
	}
	else
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Failed to save package");
	}
#else
	Response.bSuccess = false;
	Response.ErrorMessage = TEXT("SaveAsset is only available in Editor builds");
#endif

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FSaveActorAsBlueprintCommand& Command, FSaveActorAsBlueprintResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

#if WITH_EDITOR
	// Find the actor
	FString Error;
	AActor* Actor = ResolveActor(Command.ActorId, &Error);
	if (!Actor)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	if (Command.PackagePath.IsEmpty() || Command.BlueprintName.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("PackagePath and BlueprintName are required");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Validate path
	if (!Command.PackagePath.StartsWith(TEXT("/Game/")))
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("PackagePath must start with /Game/");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	FString PackageName = Command.PackagePath / Command.BlueprintName;

	// Check if asset already exists
	UObject* ExistingAsset = LoadObject<UObject>(nullptr, *PackageName);
	if (ExistingAsset && !Command.bReplaceExisting)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Asset '%s' already exists. Set bReplaceExisting to true to overwrite."), *PackageName);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Use FKismetEditorUtilities if available
	// For now, provide a stub implementation
	Response.bSuccess = false;
	Response.ErrorMessage = TEXT("SaveActorAsBlueprint requires FKismetEditorUtilities. Implementation pending.");
#else
	Response.bSuccess = false;
	Response.ErrorMessage = TEXT("SaveActorAsBlueprint is only available in Editor builds");
#endif

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FDuplicateAssetCommand& Command, FDuplicateAssetResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

#if WITH_EDITOR
	if (Command.SourcePath.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("SourcePath is required");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	if (Command.DestPackagePath.IsEmpty() || Command.DestAssetName.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("DestPackagePath and DestAssetName are required");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Validate destination path
	if (!Command.DestPackagePath.StartsWith(TEXT("/Game/")))
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("DestPackagePath must start with /Game/");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Load source asset
	UObject* SourceAsset = LoadObject<UObject>(nullptr, *Command.SourcePath);
	if (!SourceAsset)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Source asset '%s' not found"), *Command.SourcePath);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Create destination package
	FString DestPackageName = Command.DestPackagePath / Command.DestAssetName;
	UPackage* DestPackage = CreatePackage(*DestPackageName);
	if (!DestPackage)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Failed to create destination package");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Duplicate the object
	UObject* DuplicatedAsset = StaticDuplicateObject(SourceAsset, DestPackage, *Command.DestAssetName);
	if (DuplicatedAsset)
	{
		DuplicatedAsset->SetFlags(RF_Public | RF_Standalone);
		DuplicatedAsset->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(DuplicatedAsset);

		Response.NewAssetPath = DuplicatedAsset->GetPathName();
		Response.bSuccess = true;
	}
	else
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Failed to duplicate asset");
	}
#else
	Response.bSuccess = false;
	Response.ErrorMessage = TEXT("DuplicateAsset is only available in Editor builds");
#endif

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FGetAssetThumbnailCommand& Command, FGetAssetThumbnailResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

#if WITH_EDITOR
	if (Command.AssetPath.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("AssetPath is required");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Load the asset
	UObject* Asset = LoadObject<UObject>(nullptr, *Command.AssetPath);
	if (!Asset)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Asset '%s' not found"), *Command.AssetPath);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Thumbnail rendering requires more setup - stub for now
	Response.bSuccess = false;
	Response.ErrorMessage = TEXT("GetAssetThumbnail requires UThumbnailManager integration. Implementation pending.");
	Response.AssetType = Asset->GetClass()->GetName();
#else
	Response.bSuccess = false;
	Response.ErrorMessage = TEXT("GetAssetThumbnail is only available in Editor builds");
#endif

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

//~==============================================================================
// Component Commands (P1)
//~==============================================================================

void FCommandExecutor::Execute(const FGetComponentTransformCommand& Command, FGetComponentTransformResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	// Find the actor
	FString Error;
	AActor* Actor = ResolveActor(Command.ActorId, &Error);
	if (!Actor)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Find the component
	USceneComponent* Component = nullptr;
	if (Command.ComponentName.IsEmpty())
	{
		Component = Actor->GetRootComponent();
	}
	else
	{
		TArray<UActorComponent*> Components;
		Actor->GetComponents(Components);
		for (UActorComponent* Comp : Components)
		{
			if (Comp && Comp->GetName() == Command.ComponentName)
			{
				Component = Cast<USceneComponent>(Comp);
				break;
			}
		}
	}

	if (!Component)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Command.ComponentName.IsEmpty()
			? TEXT("Actor has no root component")
			: FString::Printf(TEXT("Component '%s' not found or is not a SceneComponent"), *Command.ComponentName);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	Response.bWorldSpace = Command.bWorldSpace;
	if (Command.bWorldSpace)
	{
		Response.Location = Component->GetComponentLocation();
		Response.Rotation = Component->GetComponentRotation();
		Response.Scale = Component->GetComponentScale();
	}
	else
	{
		Response.Location = Component->GetRelativeLocation();
		Response.Rotation = Component->GetRelativeRotation();
		Response.Scale = Component->GetRelativeScale3D();
	}

	// Parent info
	if (USceneComponent* Parent = Component->GetAttachParent())
	{
		Response.ParentComponentName = Parent->GetName();
	}

	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FSetComponentTransformCommand& Command, FAgentResponseBase& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	// Find the actor
	FString Error;
	AActor* Actor = ResolveActor(Command.ActorId, &Error);
	if (!Actor)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Find the component
	USceneComponent* Component = nullptr;
	if (Command.ComponentName.IsEmpty())
	{
		Component = Actor->GetRootComponent();
	}
	else
	{
		TArray<UActorComponent*> Components;
		Actor->GetComponents(Components);
		for (UActorComponent* Comp : Components)
		{
			if (Comp && Comp->GetName() == Command.ComponentName)
			{
				Component = Cast<USceneComponent>(Comp);
				break;
			}
		}
	}

	if (!Component)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Command.ComponentName.IsEmpty()
			? TEXT("Actor has no root component")
			: FString::Printf(TEXT("Component '%s' not found or is not a SceneComponent"), *Command.ComponentName);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Apply transform changes
	if (Command.bWorldSpace)
	{
		if (Command.Location.IsSet())
		{
			Component->SetWorldLocation(Command.Location.GetValue(), Command.bSweep);
		}
		if (Command.Rotation.IsSet())
		{
			Component->SetWorldRotation(Command.Rotation.GetValue(), Command.bSweep);
		}
		if (Command.Scale.IsSet())
		{
			Component->SetWorldScale3D(Command.Scale.GetValue());
		}
	}
	else
	{
		if (Command.Location.IsSet())
		{
			Component->SetRelativeLocation(Command.Location.GetValue(), Command.bSweep);
		}
		if (Command.Rotation.IsSet())
		{
			Component->SetRelativeRotation(Command.Rotation.GetValue(), Command.bSweep);
		}
		if (Command.Scale.IsSet())
		{
			Component->SetRelativeScale3D(Command.Scale.GetValue());
		}
	}

	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

// Helper to convert EAttachmentRuleType to EAttachmentRule
static EAttachmentRule ToAttachmentRule(EAttachmentRuleType RuleType)
{
	switch (RuleType)
	{
	case EAttachmentRuleType::KeepRelative: return EAttachmentRule::KeepRelative;
	case EAttachmentRuleType::KeepWorld: return EAttachmentRule::KeepWorld;
	case EAttachmentRuleType::SnapToTarget: return EAttachmentRule::SnapToTarget;
	default: return EAttachmentRule::KeepRelative;
	}
}

void FCommandExecutor::Execute(const FAttachComponentCommand& Command, FAgentResponseBase& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	// Find the actor
	FString Error;
	AActor* Actor = ResolveActor(Command.ActorId, &Error);
	if (!Actor)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Find the component to attach
	USceneComponent* Component = nullptr;
	TArray<UActorComponent*> Components;
	Actor->GetComponents(Components);
	for (UActorComponent* Comp : Components)
	{
		if (Comp && Comp->GetName() == Command.ComponentName)
		{
			Component = Cast<USceneComponent>(Comp);
			break;
		}
	}

	if (!Component)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Component '%s' not found"), *Command.ComponentName);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Find parent component
	USceneComponent* ParentComponent = nullptr;
	if (Command.ParentComponentName.IsEmpty())
	{
		ParentComponent = Actor->GetRootComponent();
	}
	else
	{
		for (UActorComponent* Comp : Components)
		{
			if (Comp && Comp->GetName() == Command.ParentComponentName)
			{
				ParentComponent = Cast<USceneComponent>(Comp);
				break;
			}
		}
	}

	if (!ParentComponent)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Command.ParentComponentName.IsEmpty()
			? TEXT("Actor has no root component")
			: FString::Printf(TEXT("Parent component '%s' not found"), *Command.ParentComponentName);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Perform attachment
	FAttachmentTransformRules Rules(
		ToAttachmentRule(Command.LocationRule),
		ToAttachmentRule(Command.RotationRule),
		ToAttachmentRule(Command.ScaleRule),
		false // bWeldSimulatedBodies
	);

	FName SocketName = Command.SocketName.IsEmpty() ? NAME_None : FName(*Command.SocketName);
	Component->AttachToComponent(ParentComponent, Rules, SocketName);

	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FAttachActorCommand& Command, FAgentResponseBase& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	// Find child actor
	FString Error;
	AActor* ChildActor = ResolveActor(Command.ChildActorId, &Error);
	if (!ChildActor)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Child actor: %s"), *Error);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Find parent actor
	AActor* ParentActor = ResolveActor(Command.ParentActorId, &Error);
	if (!ParentActor)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Parent actor: %s"), *Error);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Find parent component
	USceneComponent* ParentComponent = nullptr;
	if (Command.ParentComponentName.IsEmpty())
	{
		ParentComponent = ParentActor->GetRootComponent();
	}
	else
	{
		TArray<UActorComponent*> Components;
		ParentActor->GetComponents(Components);
		for (UActorComponent* Comp : Components)
		{
			if (Comp && Comp->GetName() == Command.ParentComponentName)
			{
				ParentComponent = Cast<USceneComponent>(Comp);
				break;
			}
		}
	}

	if (!ParentComponent)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Command.ParentComponentName.IsEmpty()
			? TEXT("Parent actor has no root component")
			: FString::Printf(TEXT("Parent component '%s' not found"), *Command.ParentComponentName);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Perform attachment
	FAttachmentTransformRules Rules(
		ToAttachmentRule(Command.LocationRule),
		ToAttachmentRule(Command.RotationRule),
		ToAttachmentRule(Command.ScaleRule),
		false // bWeldSimulatedBodies
	);

	FName SocketName = Command.SocketName.IsEmpty() ? NAME_None : FName(*Command.SocketName);
	ChildActor->AttachToComponent(ParentComponent, Rules, SocketName);

	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FDetachComponentCommand& Command, FAgentResponseBase& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	// Find the actor
	FString Error;
	AActor* Actor = ResolveActor(Command.ActorId, &Error);
	if (!Actor)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Find the component
	USceneComponent* Component = nullptr;
	TArray<UActorComponent*> Components;
	Actor->GetComponents(Components);
	for (UActorComponent* Comp : Components)
	{
		if (Comp && Comp->GetName() == Command.ComponentName)
		{
			Component = Cast<USceneComponent>(Comp);
			break;
		}
	}

	if (!Component)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Component '%s' not found"), *Command.ComponentName);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Perform detachment
	FDetachmentTransformRules Rules(
		Command.bMaintainWorldPosition ? EDetachmentRule::KeepWorld : EDetachmentRule::KeepRelative,
		Command.bMaintainWorldPosition ? EDetachmentRule::KeepWorld : EDetachmentRule::KeepRelative,
		Command.bMaintainWorldPosition ? EDetachmentRule::KeepWorld : EDetachmentRule::KeepRelative,
		true // bCallModify
	);

	Component->DetachFromComponent(Rules);

	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FDetachActorCommand& Command, FAgentResponseBase& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	// Find the actor
	FString Error;
	AActor* Actor = ResolveActor(Command.ActorId, &Error);
	if (!Actor)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Perform detachment
	FDetachmentTransformRules Rules(
		Command.bMaintainWorldPosition ? EDetachmentRule::KeepWorld : EDetachmentRule::KeepRelative,
		Command.bMaintainWorldPosition ? EDetachmentRule::KeepWorld : EDetachmentRule::KeepRelative,
		Command.bMaintainWorldPosition ? EDetachmentRule::KeepWorld : EDetachmentRule::KeepRelative,
		true // bCallModify
	);

	Actor->DetachFromActor(Rules);

	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

//~==============================================================================
// File Commands (P1) - Constrained to Project Directory
//~==============================================================================

bool FCommandExecutor::IsPathAllowed(const FString& RelativePath, FString* OutError)
{
	// Check for path traversal
	if (RelativePath.Contains(TEXT("..")) || RelativePath.Contains(TEXT("/..")) || RelativePath.Contains(TEXT("\\..")))
	{
		if (OutError) *OutError = TEXT("Path traversal not allowed");
		return false;
	}

	// Normalize path separators
	FString NormalizedPath = RelativePath;
	NormalizedPath.ReplaceInline(TEXT("\\"), TEXT("/"));

	// Block certain directories
	static const TArray<FString> BlockedPrefixes = {
		TEXT("Binaries/"),
		TEXT("Intermediate/"),
		TEXT("Saved/Crashes/"),
		TEXT("Saved/Logs/"),
		TEXT(".git/"),
		TEXT(".vs/")
	};

	for (const FString& Blocked : BlockedPrefixes)
	{
		if (NormalizedPath.StartsWith(Blocked, ESearchCase::IgnoreCase))
		{
			if (OutError) *OutError = FString::Printf(TEXT("Access to '%s' directory is not allowed"), *Blocked);
			return false;
		}
	}

	// Block certain extensions
	static const TArray<FString> BlockedExtensions = {
		TEXT(".exe"),
		TEXT(".dll"),
		TEXT(".pdb"),
		TEXT(".lib"),
		TEXT(".so"),
		TEXT(".dylib")
	};

	for (const FString& Ext : BlockedExtensions)
	{
		if (NormalizedPath.EndsWith(Ext, ESearchCase::IgnoreCase))
		{
			if (OutError) *OutError = FString::Printf(TEXT("Files with '%s' extension are not allowed"), *Ext);
			return false;
		}
	}

	return true;
}

FString FCommandExecutor::ToAbsoluteProjectPath(const FString& RelativePath, FString* OutError)
{
	if (!IsPathAllowed(RelativePath, OutError))
	{
		return FString();
	}

	FString ProjectDir = FPaths::ProjectDir();
	FString AbsolutePath = FPaths::Combine(ProjectDir, RelativePath);
	FPaths::NormalizeFilename(AbsolutePath);

	// Verify the resulting path is still within project directory
	FString NormalizedProjectDir = ProjectDir;
	FPaths::NormalizeDirectoryName(NormalizedProjectDir);

	if (!AbsolutePath.StartsWith(NormalizedProjectDir))
	{
		if (OutError) *OutError = TEXT("Resulting path is outside project directory");
		return FString();
	}

	return AbsolutePath;
}

void FCommandExecutor::Execute(const FReadProjectFileCommand& Command, FReadProjectFileResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	if (Command.RelativePath.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("RelativePath is required");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	FString Error;
	FString AbsolutePath = ToAbsoluteProjectPath(Command.RelativePath, &Error);
	if (AbsolutePath.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	IFileManager& FileManager = IFileManager::Get();

	// Check if file exists
	if (!FileManager.FileExists(*AbsolutePath))
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("File '%s' not found"), *Command.RelativePath);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Get file info
	Response.FileSizeBytes = FileManager.FileSize(*AbsolutePath);
	FDateTime ModTime = FileManager.GetTimeStamp(*AbsolutePath);
	Response.ModificationTime = ModTime.ToIso8601();

	// Check size limit
	int64 BytesToRead = Response.FileSizeBytes;
	if (Command.MaxBytes > 0 && BytesToRead > Command.MaxBytes)
	{
		BytesToRead = Command.MaxBytes;
	}

	// Read file
	if (Command.bAsBase64)
	{
		TArray<uint8> FileData;
		if (FFileHelper::LoadFileToArray(FileData, *AbsolutePath))
		{
			if (Command.MaxBytes > 0 && FileData.Num() > Command.MaxBytes)
			{
				FileData.SetNum(Command.MaxBytes);
			}
			Response.Content = FBase64::Encode(FileData);
			Response.bIsBase64 = true;
			Response.bSuccess = true;
		}
		else
		{
			Response.bSuccess = false;
			Response.ErrorMessage = TEXT("Failed to read file");
		}
	}
	else
	{
		FString FileContent;
		if (FFileHelper::LoadFileToString(FileContent, *AbsolutePath))
		{
			if (Command.MaxBytes > 0 && FileContent.Len() > Command.MaxBytes)
			{
				FileContent = FileContent.Left(Command.MaxBytes);
			}
			Response.Content = FileContent;
			Response.bIsBase64 = false;
			Response.bSuccess = true;
		}
		else
		{
			Response.bSuccess = false;
			Response.ErrorMessage = TEXT("Failed to read file as text");
		}
	}

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FWriteProjectFileCommand& Command, FWriteProjectFileResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	if (Command.RelativePath.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("RelativePath is required");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	FString Error;
	FString AbsolutePath = ToAbsoluteProjectPath(Command.RelativePath, &Error);
	if (AbsolutePath.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Create directories if requested
	if (Command.bCreateDirectories)
	{
		FString Directory = FPaths::GetPath(AbsolutePath);
		IFileManager::Get().MakeDirectory(*Directory, true);
	}

	bool bWriteSuccess = false;
	int64 BytesWritten = 0;

	if (Command.bIsBase64)
	{
		TArray<uint8> FileData;
		if (FBase64::Decode(Command.Content, FileData))
		{
			if (Command.bAppend)
			{
				// Load existing, append, and save
				TArray<uint8> ExistingData;
				FFileHelper::LoadFileToArray(ExistingData, *AbsolutePath);
				ExistingData.Append(FileData);
				bWriteSuccess = FFileHelper::SaveArrayToFile(ExistingData, *AbsolutePath);
				BytesWritten = FileData.Num();
			}
			else
			{
				bWriteSuccess = FFileHelper::SaveArrayToFile(FileData, *AbsolutePath);
				BytesWritten = FileData.Num();
			}
		}
		else
		{
			Response.bSuccess = false;
			Response.ErrorMessage = TEXT("Failed to decode base64 content");
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}
	}
	else
	{
		uint32 WriteFlags = Command.bAppend ? (FILEWRITE_Append | FILEWRITE_AllowRead) : FILEWRITE_None;
		bWriteSuccess = FFileHelper::SaveStringToFile(Command.Content, *AbsolutePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), WriteFlags);
		BytesWritten = Command.Content.Len();
	}

	if (bWriteSuccess)
	{
		Response.AbsolutePath = AbsolutePath;
		Response.BytesWritten = BytesWritten;
		Response.bSuccess = true;
	}
	else
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Failed to write file");
	}

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FListProjectDirectoryCommand& Command, FListProjectDirectoryResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	FString RelativePath = Command.RelativePath.IsEmpty() ? TEXT(".") : Command.RelativePath;

	FString Error;
	FString AbsolutePath = ToAbsoluteProjectPath(RelativePath, &Error);
	if (AbsolutePath.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	IFileManager& FileManager = IFileManager::Get();

	// Check if directory exists
	if (!FileManager.DirectoryExists(*AbsolutePath))
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Directory '%s' not found"), *RelativePath);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	Response.AbsolutePath = AbsolutePath;

	// List files
	FString SearchPattern = Command.Pattern.IsEmpty() ? TEXT("*") : Command.Pattern;
	FString SearchPath = FPaths::Combine(AbsolutePath, SearchPattern);

	TArray<FString> FoundFiles;
	if (Command.bRecursive)
	{
		FileManager.FindFilesRecursive(FoundFiles, *AbsolutePath, *SearchPattern, true, true, false);
	}
	else
	{
		FileManager.FindFiles(FoundFiles, *SearchPath, true, true);
		// Prepend the base path
		for (FString& File : FoundFiles)
		{
			File = FPaths::Combine(AbsolutePath, File);
		}
	}

	Response.TotalCount = FoundFiles.Num();
	FString ProjectDir = FPaths::ProjectDir();

	int32 Count = 0;
	for (const FString& FullPath : FoundFiles)
	{
		if (Count >= Command.Limit)
		{
			break;
		}

		FFileInfo Info;
		Info.RelativePath = FullPath;
		FPaths::MakePathRelativeTo(Info.RelativePath, *ProjectDir);
		Info.Name = FPaths::GetCleanFilename(FullPath);
		Info.bIsDirectory = FileManager.DirectoryExists(*FullPath);
		Info.Extension = FPaths::GetExtension(FullPath, true);

		if (!Info.bIsDirectory)
		{
			Info.SizeBytes = FileManager.FileSize(*FullPath);
			FDateTime ModTime = FileManager.GetTimeStamp(*FullPath);
			Info.ModificationTime = ModTime.ToIso8601();
		}

		Response.Files.Add(Info);
		Count++;
	}

	Response.bSuccess = true;
	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FCopyProjectFileCommand& Command, FCopyProjectFileResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	if (Command.SourcePath.IsEmpty() || Command.DestPath.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("SourcePath and DestPath are required");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	FString Error;
	FString AbsoluteSource = ToAbsoluteProjectPath(Command.SourcePath, &Error);
	if (AbsoluteSource.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Source: %s"), *Error);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	FString AbsoluteDest = ToAbsoluteProjectPath(Command.DestPath, &Error);
	if (AbsoluteDest.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Destination: %s"), *Error);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	IFileManager& FileManager = IFileManager::Get();

	// Check source exists
	if (!FileManager.FileExists(*AbsoluteSource))
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Source file '%s' not found"), *Command.SourcePath);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Check if destination exists
	if (FileManager.FileExists(*AbsoluteDest) && !Command.bOverwrite)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Destination file already exists. Set bOverwrite to true to overwrite.");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Create destination directory
	FString DestDir = FPaths::GetPath(AbsoluteDest);
	FileManager.MakeDirectory(*DestDir, true);

	// Copy file
	uint32 CopyResult = FileManager.Copy(*AbsoluteDest, *AbsoluteSource, true);
	if (CopyResult == COPY_OK)
	{
		Response.DestAbsolutePath = AbsoluteDest;
		Response.BytesCopied = FileManager.FileSize(*AbsoluteDest);
		Response.bSuccess = true;
	}
	else
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Failed to copy file");
	}

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FDeleteProjectFileCommand& Command, FAgentResponseBase& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

	if (Command.RelativePath.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("RelativePath is required");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	FString Error;
	FString AbsolutePath = ToAbsoluteProjectPath(Command.RelativePath, &Error);
	if (AbsolutePath.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	IFileManager& FileManager = IFileManager::Get();

	// Check if it's a directory
	bool bIsDirectory = FileManager.DirectoryExists(*AbsolutePath);
	if (bIsDirectory && !Command.bAllowDirectoryDelete)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Cannot delete directories unless bAllowDirectoryDelete is true");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	bool bDeleted = false;
	if (bIsDirectory)
	{
		bDeleted = FileManager.DeleteDirectory(*AbsolutePath, false, true);
	}
	else if (FileManager.FileExists(*AbsolutePath))
	{
		bDeleted = FileManager.Delete(*AbsolutePath);
	}
	else
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("File or directory not found");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	if (bDeleted)
	{
		Response.bSuccess = true;
	}
	else
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Failed to delete file or directory");
	}

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

//~==============================================================================
// Blueprint Node Commands (P2 - Visual Scripting)
//~==============================================================================

#if WITH_EDITOR

// Helper: Build pin info from UEdGraphPin
static FBlueprintPinInfo BuildPinInfo(UEdGraphPin* Pin)
{
	FBlueprintPinInfo Info;
	if (!Pin)
	{
		return Info;
	}

	Info.Name = Pin->GetFName().ToString();
	Info.Direction = (Pin->Direction == EGPD_Input) ? TEXT("Input") : TEXT("Output");

	// Get type from category
	Info.Type = Pin->PinType.PinCategory.ToString();
	if (Pin->PinType.PinSubCategory != NAME_None)
	{
		Info.Type += TEXT(":") + Pin->PinType.PinSubCategory.ToString();
	}

	// Friendly display name
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	if (K2Schema)
	{
		Info.TypeDisplayName = K2Schema->TypeToText(Pin->PinType).ToString();
	}

	Info.bIsConnected = Pin->LinkedTo.Num() > 0;
	Info.DefaultValue = Pin->DefaultValue;

	// Collect connected pins
	for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
	{
		if (LinkedPin && LinkedPin->GetOwningNode())
		{
			FString ConnectedId = FString::Printf(TEXT("%s.%s"),
				*LinkedPin->GetOwningNode()->NodeGuid.ToString(),
				*LinkedPin->GetFName().ToString());
			Info.ConnectedTo.Add(ConnectedId);
		}
	}

	return Info;
}

// Helper: Build node info from UEdGraphNode
static FBlueprintNodeInfo BuildNodeInfo(UEdGraphNode* Node)
{
	FBlueprintNodeInfo Info;
	if (!Node)
	{
		return Info;
	}

	Info.Guid = Node->NodeGuid.ToString();
	Info.ClassName = Node->GetClass()->GetName();
	Info.Title = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	Info.PosX = Node->NodePosX;
	Info.PosY = Node->NodePosY;
	Info.Comment = Node->NodeComment;

	// Extract type-specific data
	if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
	{
		UFunction* Func = CallNode->GetTargetFunction();
		if (Func && Func->GetOwnerClass())
		{
			Info.FunctionReference = FString::Printf(TEXT("%s.%s"),
				*Func->GetOwnerClass()->GetName(), *Func->GetName());
		}
	}
	else if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
	{
		UFunction* EventFunc = EventNode->FindEventSignatureFunction();
		if (EventFunc)
		{
			Info.EventName = EventFunc->GetName();
		}
		else if (!EventNode->CustomFunctionName.IsNone())
		{
			Info.EventName = EventNode->CustomFunctionName.ToString();
		}
	}
	else if (UK2Node_VariableGet* VarGetNode = Cast<UK2Node_VariableGet>(Node))
	{
		Info.VariableName = VarGetNode->GetVarName().ToString();
	}
	else if (UK2Node_VariableSet* VarSetNode = Cast<UK2Node_VariableSet>(Node))
	{
		Info.VariableName = VarSetNode->GetVarName().ToString();
	}

	// Collect pins
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && !Pin->bHidden)
		{
			Info.Pins.Add(BuildPinInfo(Pin));
		}
	}

	return Info;
}

// Helper: Find node by GUID or name in a Blueprint
static UEdGraphNode* FindBlueprintNode(UBlueprint* Blueprint, const FString& NodeId, FString* OutError = nullptr)
{
	if (!Blueprint)
	{
		if (OutError) *OutError = TEXT("Blueprint is null");
		return nullptr;
	}

	// Try parsing as GUID first
	FGuid NodeGuid;
	if (FGuid::Parse(NodeId, NodeGuid))
	{
		// Search all graphs
		TArray<UEdGraph*> AllGraphs;
		Blueprint->GetAllGraphs(AllGraphs);

		for (UEdGraph* Graph : AllGraphs)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node && Node->NodeGuid == NodeGuid)
				{
					return Node;
				}
			}
		}
	}

	// Try finding by path/name
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && (Node->GetPathName().Contains(NodeId) ||
				Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Contains(NodeId)))
			{
				return Node;
			}
		}
	}

	if (OutError) *OutError = FString::Printf(TEXT("Node '%s' not found"), *NodeId);
	return nullptr;
}

// Helper: Find graph by name in Blueprint
static UEdGraph* FindBlueprintGraph(UBlueprint* Blueprint, const FString& GraphName)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	// Empty or "EventGraph" means the main event graph
	if (GraphName.IsEmpty() || GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
	{
		return FBlueprintEditorUtils::FindEventGraph(Blueprint);
	}

	// Search in all graphs
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
		{
			return Graph;
		}
	}

	return nullptr;
}

// Helper: Parse function reference like "KismetSystemLibrary.PrintString" or "/Script/Engine.Actor:K2_DestroyActor"
static bool ParseFunctionReference(const FString& FunctionRef, UClass*& OutClass, UFunction*& OutFunction, FString& OutError)
{
	OutClass = nullptr;
	OutFunction = nullptr;

	// Handle format: Class.Function or Class:Function
	FString ClassName, FunctionName;
	if (FunctionRef.Contains(TEXT(":")))
	{
		// Path format: /Script/Module.Class:Function
		int32 ColonIdx = FunctionRef.Find(TEXT(":"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		ClassName = FunctionRef.Left(ColonIdx);
		FunctionName = FunctionRef.Mid(ColonIdx + 1);
	}
	else if (FunctionRef.Contains(TEXT(".")))
	{
		// Simple format: Class.Function
		int32 DotIdx = FunctionRef.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		ClassName = FunctionRef.Left(DotIdx);
		FunctionName = FunctionRef.Mid(DotIdx + 1);
	}
	else
	{
		OutError = FString::Printf(TEXT("Invalid function reference: %s (expected Class.Function)"), *FunctionRef);
		return false;
	}

	// Find the class
	OutClass = FindObject<UClass>(nullptr, *ClassName);
	if (!OutClass)
	{
		// Try common library prefixes
		TArray<FString> Prefixes = {
			TEXT("/Script/Engine."),
			TEXT("/Script/CoreUObject."),
			TEXT("/Script/UMG."),
			TEXT("")
		};

		for (const FString& Prefix : Prefixes)
		{
			OutClass = FindObject<UClass>(nullptr, *(Prefix + ClassName));
			if (OutClass)
			{
				break;
			}
		}
	}

	if (!OutClass)
	{
		OutError = FString::Printf(TEXT("Class not found: %s"), *ClassName);
		return false;
	}

	// Find the function
	OutFunction = OutClass->FindFunctionByName(*FunctionName);
	if (!OutFunction)
	{
		OutError = FString::Printf(TEXT("Function '%s' not found in class '%s'"), *FunctionName, *ClassName);
		return false;
	}

	return true;
}

#endif // WITH_EDITOR

void FCommandExecutor::Execute(const FCreateBlueprintNodeCommand& Command, FCreateBlueprintNodeResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

#if WITH_EDITOR
	if (Command.BlueprintPath.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("BlueprintPath is required");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Load the Blueprint
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Command.BlueprintPath);
	if (!Blueprint)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Blueprint not found: %s"), *Command.BlueprintPath);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Find the graph
	UEdGraph* Graph = FindBlueprintGraph(Blueprint, Command.GraphName);
	if (!Graph)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Graph not found: %s"), *Command.GraphName);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	UEdGraphNode* NewNode = nullptr;
	FString Error;

	// Create node based on type
	if (Command.NodeType.Equals(TEXT("CallFunction"), ESearchCase::IgnoreCase))
	{
		if (Command.FunctionReference.IsEmpty())
		{
			Response.bSuccess = false;
			Response.ErrorMessage = TEXT("FunctionReference is required for CallFunction nodes");
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}

		UClass* FuncClass = nullptr;
		UFunction* Function = nullptr;
		if (!ParseFunctionReference(Command.FunctionReference, FuncClass, Function, Error))
		{
			Response.bSuccess = false;
			Response.ErrorMessage = Error;
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}

		UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
		Graph->AddNode(CallNode, false, false);
		CallNode->SetFromFunction(Function);
		CallNode->AllocateDefaultPins();
		NewNode = CallNode;
	}
	else if (Command.NodeType.Equals(TEXT("Event"), ESearchCase::IgnoreCase))
	{
		if (Command.EventName.IsEmpty())
		{
			Response.bSuccess = false;
			Response.ErrorMessage = TEXT("EventName is required for Event nodes");
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}

		// Check if event already exists
		UClass* ParentClass = Blueprint->ParentClass;
		UFunction* EventFunc = nullptr;

		// Try to find the event in parent class hierarchy
		if (ParentClass)
		{
			EventFunc = ParentClass->FindFunctionByName(*Command.EventName);
		}

		// Also try in AActor if not found (common events like ReceiveBeginPlay)
		if (!EventFunc)
		{
			EventFunc = AActor::StaticClass()->FindFunctionByName(*Command.EventName);
		}

		UK2Node_Event* EventNode = NewObject<UK2Node_Event>(Graph);
		Graph->AddNode(EventNode, false, false);

		if (EventFunc)
		{
			// Override existing event
			EventNode->EventReference.SetFromField<UFunction>(EventFunc, false);
			EventNode->bOverrideFunction = true;
		}
		else
		{
			// Custom event
			EventNode->CustomFunctionName = *Command.EventName;
			EventNode->bOverrideFunction = false;
		}

		EventNode->AllocateDefaultPins();
		NewNode = EventNode;
	}
	else if (Command.NodeType.Equals(TEXT("VariableGet"), ESearchCase::IgnoreCase))
	{
		if (Command.VariableName.IsEmpty())
		{
			Response.bSuccess = false;
			Response.ErrorMessage = TEXT("VariableName is required for VariableGet nodes");
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}

		UK2Node_VariableGet* VarNode = NewObject<UK2Node_VariableGet>(Graph);
		Graph->AddNode(VarNode, false, false);

		// Find the variable property
		FProperty* VarProp = Blueprint->GeneratedClass ?
			Blueprint->GeneratedClass->FindPropertyByName(*Command.VariableName) : nullptr;

		if (VarProp)
		{
			VarNode->VariableReference.SetFromField<FProperty>(VarProp, Blueprint->GeneratedClass);
		}
		else
		{
			// Set variable by name
			VarNode->VariableReference.SetSelfMember(*Command.VariableName);
		}

		VarNode->AllocateDefaultPins();
		NewNode = VarNode;
	}
	else if (Command.NodeType.Equals(TEXT("VariableSet"), ESearchCase::IgnoreCase))
	{
		if (Command.VariableName.IsEmpty())
		{
			Response.bSuccess = false;
			Response.ErrorMessage = TEXT("VariableName is required for VariableSet nodes");
			Response.ExecutionTimeMs = EndTiming(StartTime);
			return;
		}

		UK2Node_VariableSet* VarNode = NewObject<UK2Node_VariableSet>(Graph);
		Graph->AddNode(VarNode, false, false);

		FProperty* VarProp = Blueprint->GeneratedClass ?
			Blueprint->GeneratedClass->FindPropertyByName(*Command.VariableName) : nullptr;

		if (VarProp)
		{
			VarNode->VariableReference.SetFromField<FProperty>(VarProp, Blueprint->GeneratedClass);
		}
		else
		{
			VarNode->VariableReference.SetSelfMember(*Command.VariableName);
		}

		VarNode->AllocateDefaultPins();
		NewNode = VarNode;
	}
	else if (Command.NodeType.Equals(TEXT("Branch"), ESearchCase::IgnoreCase))
	{
		UK2Node_IfThenElse* BranchNode = NewObject<UK2Node_IfThenElse>(Graph);
		Graph->AddNode(BranchNode, false, false);
		BranchNode->AllocateDefaultPins();
		NewNode = BranchNode;
	}
	else if (Command.NodeType.Equals(TEXT("Sequence"), ESearchCase::IgnoreCase))
	{
		UK2Node_ExecutionSequence* SeqNode = NewObject<UK2Node_ExecutionSequence>(Graph);
		Graph->AddNode(SeqNode, false, false);
		SeqNode->AllocateDefaultPins();
		NewNode = SeqNode;
	}
	else if (Command.NodeType.Equals(TEXT("Comment"), ESearchCase::IgnoreCase))
	{
		UEdGraphNode_Comment* CommentNode = NewObject<UEdGraphNode_Comment>(Graph);
		Graph->AddNode(CommentNode, false, false);
		CommentNode->NodeComment = Command.Comment;
		// Comments don't need AllocateDefaultPins
		NewNode = CommentNode;
	}
	else
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Unknown node type: %s. Supported: CallFunction, Event, VariableGet, VariableSet, Branch, Sequence, Comment"), *Command.NodeType);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	if (!NewNode)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Failed to create node");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Ensure node has a valid GUID (required for reliable identification)
	if (!NewNode->NodeGuid.IsValid())
	{
		NewNode->CreateNewGuid();
	}

	// Set position
	NewNode->NodePosX = Command.PosX;
	NewNode->NodePosY = Command.PosY;

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	// Build response
	Response.bSuccess = true;
	Response.Node = BuildNodeInfo(NewNode);

#else
	Response.bSuccess = false;
	Response.ErrorMessage = TEXT("Blueprint node creation is only available in Editor builds");
#endif

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FConnectBlueprintPinsCommand& Command, FAgentResponseBase& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

#if WITH_EDITOR
	if (Command.BlueprintPath.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("BlueprintPath is required");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Command.BlueprintPath);
	if (!Blueprint)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Blueprint not found: %s"), *Command.BlueprintPath);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	FString Error;
	UEdGraphNode* SourceNode = FindBlueprintNode(Blueprint, Command.SourceNode, &Error);
	if (!SourceNode)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Source node not found: %s"), *Error);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	UEdGraphNode* TargetNode = FindBlueprintNode(Blueprint, Command.TargetNode, &Error);
	if (!TargetNode)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Target node not found: %s"), *Error);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Find pins
	UEdGraphPin* SourcePin = SourceNode->FindPin(*Command.SourcePin);
	if (!SourcePin)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Source pin '%s' not found on node"), *Command.SourcePin);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	UEdGraphPin* TargetPin = TargetNode->FindPin(*Command.TargetPin);
	if (!TargetPin)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Target pin '%s' not found on node"), *Command.TargetPin);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Get schema for validated connection
	UEdGraph* Graph = SourceNode->GetGraph();
	const UEdGraphSchema* Schema = Graph->GetSchema();

	// Try to create connection
	if (!Schema->TryCreateConnection(SourcePin, TargetPin))
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Failed to connect pins - types may be incompatible");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	Response.bSuccess = true;

#else
	Response.bSuccess = false;
	Response.ErrorMessage = TEXT("Blueprint pin connection is only available in Editor builds");
#endif

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FDisconnectBlueprintPinsCommand& Command, FAgentResponseBase& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

#if WITH_EDITOR
	if (Command.BlueprintPath.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("BlueprintPath is required");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Command.BlueprintPath);
	if (!Blueprint)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Blueprint not found: %s"), *Command.BlueprintPath);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	FString Error;
	UEdGraphNode* SourceNode = FindBlueprintNode(Blueprint, Command.SourceNode, &Error);
	if (!SourceNode)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	UEdGraphNode* TargetNode = FindBlueprintNode(Blueprint, Command.TargetNode, &Error);
	if (!TargetNode)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	UEdGraphPin* SourcePin = SourceNode->FindPin(*Command.SourcePin);
	if (!SourcePin)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Source pin '%s' not found"), *Command.SourcePin);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	UEdGraphPin* TargetPin = TargetNode->FindPin(*Command.TargetPin);
	if (!TargetPin)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Target pin '%s' not found"), *Command.TargetPin);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Break the link
	SourcePin->BreakLinkTo(TargetPin);

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	Response.bSuccess = true;

#else
	Response.bSuccess = false;
	Response.ErrorMessage = TEXT("Blueprint pin disconnection is only available in Editor builds");
#endif

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FDeleteBlueprintNodeCommand& Command, FAgentResponseBase& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

#if WITH_EDITOR
	if (Command.BlueprintPath.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("BlueprintPath is required");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Command.BlueprintPath);
	if (!Blueprint)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Blueprint not found: %s"), *Command.BlueprintPath);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	FString Error;
	UEdGraphNode* Node = FindBlueprintNode(Blueprint, Command.NodeId, &Error);
	if (!Node)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	UEdGraph* Graph = Node->GetGraph();
	if (!Graph)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Node is not in a graph");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Use FBlueprintEditorUtils to properly remove the node
	FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);

	Response.bSuccess = true;

#else
	Response.bSuccess = false;
	Response.ErrorMessage = TEXT("Blueprint node deletion is only available in Editor builds");
#endif

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FListBlueprintNodesCommand& Command, FListBlueprintNodesResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

#if WITH_EDITOR
	if (Command.BlueprintPath.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("BlueprintPath is required");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Command.BlueprintPath);
	if (!Blueprint)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Blueprint not found: %s"), *Command.BlueprintPath);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	UEdGraph* Graph = FindBlueprintGraph(Blueprint, Command.GraphName);
	if (!Graph)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Graph not found: %s"), *Command.GraphName);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	Response.GraphName = Graph->GetName();

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}

		// Apply class filter if specified
		if (!Command.NodeClassFilter.IsEmpty())
		{
			if (!Node->GetClass()->GetName().Contains(Command.NodeClassFilter))
			{
				continue;
			}
		}

		Response.Nodes.Add(BuildNodeInfo(Node));
	}

	Response.bSuccess = true;

#else
	Response.bSuccess = false;
	Response.ErrorMessage = TEXT("Blueprint node listing is only available in Editor builds");
#endif

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

void FCommandExecutor::Execute(const FListBlueprintPinsCommand& Command, FListBlueprintPinsResponse& Response)
{
	double StartTime = StartTiming();
	Response.CommandId = Command.CommandId;

#if WITH_EDITOR
	if (Command.BlueprintPath.IsEmpty())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("BlueprintPath is required");
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Command.BlueprintPath);
	if (!Blueprint)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Blueprint not found: %s"), *Command.BlueprintPath);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	FString Error;
	UEdGraphNode* Node = FindBlueprintNode(Blueprint, Command.NodeId, &Error);
	if (!Node)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = Error;
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	Response.NodeGuid = Node->NodeGuid.ToString();
	Response.NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && !Pin->bHidden)
		{
			Response.Pins.Add(BuildPinInfo(Pin));
		}
	}

	Response.bSuccess = true;

#else
	Response.bSuccess = false;
	Response.ErrorMessage = TEXT("Blueprint pin listing is only available in Editor builds");
#endif

	Response.ExecutionTimeMs = EndTiming(StartTime);
}

//~==============================================================================
// JSON Serialization
//~==============================================================================

// Convert EAgentPropertyType to string name for gRPC type hints
// JsonToProtoPropertyValue uses Contains() to match these strings
static FString PropertyTypeToString(EAgentPropertyType Type)
{
	switch (Type)
	{
	case EAgentPropertyType::Bool:       return TEXT("Bool");
	case EAgentPropertyType::Int8:
	case EAgentPropertyType::Int16:
	case EAgentPropertyType::Int32:
	case EAgentPropertyType::Int64:      return TEXT("Int");
	case EAgentPropertyType::UInt8:
	case EAgentPropertyType::UInt16:
	case EAgentPropertyType::UInt32:
	case EAgentPropertyType::UInt64:     return TEXT("Int");  // Also matched by Int
	case EAgentPropertyType::Float:      return TEXT("Float");
	case EAgentPropertyType::Double:     return TEXT("Double");
	case EAgentPropertyType::String:
	case EAgentPropertyType::Name:
	case EAgentPropertyType::Text:       return TEXT("String");
	case EAgentPropertyType::Vector:     return TEXT("Vector");
	case EAgentPropertyType::Rotator:    return TEXT("Rotator");
	case EAgentPropertyType::Transform:  return TEXT("Transform");
	case EAgentPropertyType::Color:      return TEXT("Color");
	case EAgentPropertyType::Object:
	case EAgentPropertyType::SoftObject:
	case EAgentPropertyType::WeakObject:
	case EAgentPropertyType::Class:      return TEXT("Object");
	case EAgentPropertyType::Struct:     return TEXT("Struct");
	case EAgentPropertyType::Enum:       return TEXT("Enum");
	case EAgentPropertyType::Array:      return TEXT("Array");
	case EAgentPropertyType::Map:        return TEXT("Map");
	case EAgentPropertyType::Set:        return TEXT("Set");
	default:                             return TEXT("Unknown");
	}
}

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
	case EAgentPropertyType::Color:
		// Color is stored as "(R=0.2,G=0.8,B=0.2,A=1.0)" in StringValue by PropertyAccessor
		// Return as-is - this format is what FLinearColor::ToString() produces
		return Value.StringValue;

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

	// Array - parse JSON array into ArrayValue
	// Following "tools should just work" philosophy: parse arrays so WriteArrayProperty works
	if (Json.StartsWith(TEXT("[")) && Json.EndsWith(TEXT("]")))
	{
		TSharedPtr<FJsonValue> ParsedJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (FJsonSerializer::Deserialize(Reader, ParsedJson) && ParsedJson.IsValid())
		{
			if (ParsedJson->Type == EJson::Array)
			{
				Value.Type = EAgentPropertyType::Array;
				const TArray<TSharedPtr<FJsonValue>>& JsonArray = ParsedJson->AsArray();
				for (const TSharedPtr<FJsonValue>& JsonElement : JsonArray)
				{
					FAgentPropertyValue ElementValue;
					if (JsonElement.IsValid())
					{
						switch (JsonElement->Type)
						{
						case EJson::String:
							ElementValue = FAgentPropertyValue(JsonElement->AsString());
							break;
						case EJson::Number:
							ElementValue = FAgentPropertyValue(JsonElement->AsNumber());
							break;
						case EJson::Boolean:
							ElementValue = FAgentPropertyValue(JsonElement->AsBool());
							break;
						case EJson::Object:
							// Nested object - parse into StructValue for WriteStructProperty
							{
								ElementValue.Type = EAgentPropertyType::Struct;
								const TSharedPtr<FJsonObject>& NestedObj = JsonElement->AsObject();
								for (const auto& NestedPair : NestedObj->Values)
								{
									FAgentPropertyValue NestedValue;
									if (NestedPair.Value.IsValid())
									{
										switch (NestedPair.Value->Type)
										{
										case EJson::String:
											NestedValue = FAgentPropertyValue(NestedPair.Value->AsString());
											break;
										case EJson::Number:
											NestedValue = FAgentPropertyValue(NestedPair.Value->AsNumber());
											break;
										case EJson::Boolean:
											NestedValue = FAgentPropertyValue(NestedPair.Value->AsBool());
											break;
										default:
											// Deeper nesting - serialize to string as fallback
											{
												FString NestedJson;
												TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&NestedJson);
												if (NestedPair.Value->Type == EJson::Object)
												{
													FJsonSerializer::Serialize(NestedPair.Value->AsObject().ToSharedRef(), Writer);
												}
												else if (NestedPair.Value->Type == EJson::Array)
												{
													FJsonSerializer::Serialize(NestedPair.Value->AsArray(), *Writer);
												}
												NestedValue.Type = EAgentPropertyType::String;
												NestedValue.StringValue = NestedJson;
											}
											break;
										}
									}
									ElementValue.StructValue.Add(NestedPair.Key, MakeShared<FAgentPropertyValue>(MoveTemp(NestedValue)));
								}
							}
							break;
						case EJson::Array:
							// Nested array - recursively parse (for TArray<TArray<...>>)
							{
								FString NestedArrayJson;
								TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&NestedArrayJson);
								FJsonSerializer::Serialize(JsonElement->AsArray(), *Writer);
								// Re-parse through JsonToPropertyValue for proper handling
								ElementValue = JsonToPropertyValue(NestedArrayJson);
							}
							break;
						default:
							ElementValue.Type = EAgentPropertyType::Unknown;
							break;
						}
					}
					Value.ArrayValue.Add(MakeShared<FAgentPropertyValue>(MoveTemp(ElementValue)));
				}
				return Value;
			}
		}
		// If JSON parse failed, fall through to string handling
	}

	// Object - parse JSON object into StructValue (for map/struct properties)
	if (Json.StartsWith(TEXT("{")) && Json.EndsWith(TEXT("}")))
	{
		TSharedPtr<FJsonValue> ParsedJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (FJsonSerializer::Deserialize(Reader, ParsedJson) && ParsedJson.IsValid())
		{
			if (ParsedJson->Type == EJson::Object)
			{
				Value.Type = EAgentPropertyType::Struct;
				const TSharedPtr<FJsonObject>& JsonObj = ParsedJson->AsObject();
				for (const auto& Pair : JsonObj->Values)
				{
					FAgentPropertyValue MemberValue;
					if (Pair.Value.IsValid())
					{
						switch (Pair.Value->Type)
						{
						case EJson::String:
							MemberValue = FAgentPropertyValue(Pair.Value->AsString());
							break;
						case EJson::Number:
							MemberValue = FAgentPropertyValue(Pair.Value->AsNumber());
							break;
						case EJson::Boolean:
							MemberValue = FAgentPropertyValue(Pair.Value->AsBool());
							break;
						case EJson::Object:
							// Nested object - serialize to string
							{
								FString MemberJson;
								TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MemberJson);
								FJsonSerializer::Serialize(Pair.Value->AsObject().ToSharedRef(), Writer);
								MemberValue.Type = EAgentPropertyType::String;
								MemberValue.StringValue = MemberJson;
							}
							break;
						case EJson::Array:
							// Nested array - serialize to string
							{
								FString MemberJson;
								TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MemberJson);
								FJsonSerializer::Serialize(Pair.Value->AsArray(), *Writer);
								MemberValue.Type = EAgentPropertyType::String;
								MemberValue.StringValue = MemberJson;
							}
							break;
						default:
							// Unknown type - leave as unknown
							MemberValue.Type = EAgentPropertyType::Unknown;
							break;
						}
					}
					Value.StructValue.Add(Pair.Key, MakeShared<FAgentPropertyValue>(MoveTemp(MemberValue)));
				}
				return Value;
			}
		}
		// If JSON parse failed, fall through to string handling
	}

	// For unrecognized formats, store as string and let ImportText handle it
	// This covers UE-style literals like "(X=1,Y=2,Z=3)" for vectors
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
// Response Serialization Helpers
//~==============================================================================

FString FCommandExecutor::SerializeBaseResponse(const FAgentResponseBase& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeListWorldsResponse(const FListWorldsResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);
	Obj->SetNumberField(TEXT("currentWorldIndex"), Response.CurrentWorldIndex);

	TArray<TSharedPtr<FJsonValue>> WorldsArray;
	for (const FWorldInfo& World : Response.Worlds)
	{
		TSharedPtr<FJsonObject> WorldObj = MakeShared<FJsonObject>();
		WorldObj->SetStringField(TEXT("worldType"), World.WorldType);
		WorldObj->SetStringField(TEXT("worldName"), World.WorldName);
		WorldObj->SetNumberField(TEXT("pieInstance"), World.PIEInstance);
		WorldObj->SetBoolField(TEXT("hasBegunPlay"), World.bHasBegunPlay);
		WorldObj->SetNumberField(TEXT("actorCount"), World.ActorCount);
		WorldsArray.Add(MakeShared<FJsonValueObject>(WorldObj));
	}
	Obj->SetArrayField(TEXT("worlds"), WorldsArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeQueryActorsResponse(const FQueryActorsResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);
	Obj->SetNumberField(TEXT("totalCount"), Response.TotalCount);

	TArray<TSharedPtr<FJsonValue>> ActorsArray;
	for (const FActorInfo& Actor : Response.Actors)
	{
		TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
		ActorObj->SetStringField(TEXT("guid"), Actor.Guid);
		ActorObj->SetStringField(TEXT("name"), Actor.Name);
		ActorObj->SetStringField(TEXT("label"), Actor.Label);
		ActorObj->SetStringField(TEXT("className"), Actor.ClassName);
		ActorObj->SetBoolField(TEXT("hidden"), Actor.bHidden);

		TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
		LocObj->SetNumberField(TEXT("x"), Actor.Location.X);
		LocObj->SetNumberField(TEXT("y"), Actor.Location.Y);
		LocObj->SetNumberField(TEXT("z"), Actor.Location.Z);
		ActorObj->SetObjectField(TEXT("location"), LocObj);

		ActorsArray.Add(MakeShared<FJsonValueObject>(ActorObj));
	}
	Obj->SetArrayField(TEXT("actors"), ActorsArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeGetActorResponse(const FGetActorResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	// Actor details
	TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
	ActorObj->SetStringField(TEXT("guid"), Response.Actor.Guid);
	ActorObj->SetStringField(TEXT("path"), Response.Actor.Path);
	ActorObj->SetStringField(TEXT("name"), Response.Actor.Name);
	ActorObj->SetStringField(TEXT("label"), Response.Actor.Label);
	ActorObj->SetStringField(TEXT("className"), Response.Actor.ClassName);
	ActorObj->SetBoolField(TEXT("hidden"), Response.Actor.bHidden);
	ActorObj->SetStringField(TEXT("parentActorId"), Response.Actor.ParentActorId);

	TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
	LocObj->SetNumberField(TEXT("x"), Response.Actor.Location.X);
	LocObj->SetNumberField(TEXT("y"), Response.Actor.Location.Y);
	LocObj->SetNumberField(TEXT("z"), Response.Actor.Location.Z);
	ActorObj->SetObjectField(TEXT("location"), LocObj);

	TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
	RotObj->SetNumberField(TEXT("pitch"), Response.Actor.Rotation.Pitch);
	RotObj->SetNumberField(TEXT("yaw"), Response.Actor.Rotation.Yaw);
	RotObj->SetNumberField(TEXT("roll"), Response.Actor.Rotation.Roll);
	ActorObj->SetObjectField(TEXT("rotation"), RotObj);

	TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
	ScaleObj->SetNumberField(TEXT("x"), Response.Actor.Scale.X);
	ScaleObj->SetNumberField(TEXT("y"), Response.Actor.Scale.Y);
	ScaleObj->SetNumberField(TEXT("z"), Response.Actor.Scale.Z);
	ActorObj->SetObjectField(TEXT("scale"), ScaleObj);

	// Components
	TSharedPtr<FJsonObject> ComponentsObj = MakeShared<FJsonObject>();
	for (const auto& Pair : Response.Actor.Components)
	{
		ComponentsObj->SetStringField(Pair.Key, Pair.Value);
	}
	ActorObj->SetObjectField(TEXT("components"), ComponentsObj);

	// Properties
	TSharedPtr<FJsonObject> PropsObj = MakeShared<FJsonObject>();
	for (const auto& Pair : Response.Actor.Properties)
	{
		PropsObj->SetStringField(Pair.Key, Pair.Value);
	}
	ActorObj->SetObjectField(TEXT("properties"), PropsObj);

	Obj->SetObjectField(TEXT("actor"), ActorObj);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeSpawnActorResponse(const FSpawnActorResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
		ActorObj->SetStringField(TEXT("guid"), Response.Actor.Guid);
		ActorObj->SetStringField(TEXT("name"), Response.Actor.Name);
		ActorObj->SetStringField(TEXT("label"), Response.Actor.Label);
		ActorObj->SetStringField(TEXT("className"), Response.Actor.ClassName);

		TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
		LocObj->SetNumberField(TEXT("x"), Response.Actor.Location.X);
		LocObj->SetNumberField(TEXT("y"), Response.Actor.Location.Y);
		LocObj->SetNumberField(TEXT("z"), Response.Actor.Location.Z);
		ActorObj->SetObjectField(TEXT("location"), LocObj);

		Obj->SetObjectField(TEXT("actor"), ActorObj);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializePropertyValueResponse(const FPropertyValueResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);
	Obj->SetStringField(TEXT("value"), Response.Value);
	Obj->SetStringField(TEXT("typeName"), Response.TypeName);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeFunctionCallResponse(const FFunctionCallResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);
	Obj->SetStringField(TEXT("returnValue"), Response.ReturnValue);

	TSharedPtr<FJsonObject> OutParamsObj = MakeShared<FJsonObject>();
	for (const auto& Pair : Response.OutParameters)
	{
		OutParamsObj->SetStringField(Pair.Key, Pair.Value);
	}
	Obj->SetObjectField(TEXT("outParameters"), OutParamsObj);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeListClassesResponse(const FListClassesResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	TArray<TSharedPtr<FJsonValue>> ClassesArray;
	for (const FClassInfo& Class : Response.Classes)
	{
		TSharedPtr<FJsonObject> ClassObj = MakeShared<FJsonObject>();
		ClassObj->SetStringField(TEXT("className"), Class.ClassName);
		ClassObj->SetStringField(TEXT("displayName"), Class.DisplayName);
		ClassObj->SetStringField(TEXT("classPath"), Class.ClassPath);
		ClassObj->SetStringField(TEXT("parentClassName"), Class.ParentClassName);
		ClassObj->SetBoolField(TEXT("isBlueprint"), Class.bIsBlueprint);
		ClassObj->SetBoolField(TEXT("isAbstract"), Class.bIsAbstract);
		ClassesArray.Add(MakeShared<FJsonValueObject>(ClassObj));
	}
	Obj->SetArrayField(TEXT("classes"), ClassesArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeGetCapabilitiesResponse(const FGetCapabilitiesResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	// Context identification
	Obj->SetStringField(TEXT("worldType"), Response.WorldType);
	Obj->SetStringField(TEXT("worldName"), Response.WorldName);
	Obj->SetBoolField(TEXT("isGameplayActive"), Response.bIsGameplayActive);
	Obj->SetNumberField(TEXT("pieInstance"), Response.PIEInstance);

	// Core reflection capabilities (always available)
	TSharedPtr<FJsonObject> CoreCaps = MakeShared<FJsonObject>();
	CoreCaps->SetBoolField(TEXT("canIterateProperties"), Response.bCanIterateProperties);
	CoreCaps->SetBoolField(TEXT("canInvokeFunctions"), Response.bCanInvokeFunctions);
	CoreCaps->SetBoolField(TEXT("canSpawnActors"), Response.bCanSpawnActors);
	CoreCaps->SetBoolField(TEXT("canDestroyActors"), Response.bCanDestroyActors);
	CoreCaps->SetBoolField(TEXT("canModifyTransforms"), Response.bCanModifyTransforms);
	CoreCaps->SetBoolField(TEXT("canModifyProperties"), Response.bCanModifyProperties);
	Obj->SetObjectField(TEXT("coreCapabilities"), CoreCaps);

	// Editor capabilities
	TSharedPtr<FJsonObject> EditorCaps = MakeShared<FJsonObject>();
	EditorCaps->SetBoolField(TEXT("canSetActorLabel"), Response.bCanSetActorLabel);
	EditorCaps->SetBoolField(TEXT("canSetActorFolder"), Response.bCanSetActorFolder);
	EditorCaps->SetBoolField(TEXT("canUseTransactions"), Response.bCanUseTransactions);
	EditorCaps->SetBoolField(TEXT("hasPropertyMetadata"), Response.bHasPropertyMetadata);
	EditorCaps->SetBoolField(TEXT("canAccessEditorWorld"), Response.bCanAccessEditorWorld);
	Obj->SetObjectField(TEXT("editorCapabilities"), EditorCaps);

	// Unavailable feature explanations (only include non-empty reasons)
	TSharedPtr<FJsonObject> Reasons = MakeShared<FJsonObject>();
	if (!Response.LabelUnavailableReason.IsEmpty())
	{
		Reasons->SetStringField(TEXT("label"), Response.LabelUnavailableReason);
	}
	if (!Response.FolderUnavailableReason.IsEmpty())
	{
		Reasons->SetStringField(TEXT("folder"), Response.FolderUnavailableReason);
	}
	if (!Response.TransactionUnavailableReason.IsEmpty())
	{
		Reasons->SetStringField(TEXT("transaction"), Response.TransactionUnavailableReason);
	}
	if (!Response.MetadataUnavailableReason.IsEmpty())
	{
		Reasons->SetStringField(TEXT("metadata"), Response.MetadataUnavailableReason);
	}
	if (Reasons->Values.Num() > 0)
	{
		Obj->SetObjectField(TEXT("unavailableReasons"), Reasons);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeListDataAssetsResponse(const FListDataAssetsResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);
	Obj->SetNumberField(TEXT("totalCount"), Response.TotalCount);

	TArray<TSharedPtr<FJsonValue>> AssetsArray;
	for (const FDataAssetInfo& Asset : Response.Assets)
	{
		TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
		AssetObj->SetStringField(TEXT("assetPath"), Asset.AssetPath);
		AssetObj->SetStringField(TEXT("assetName"), Asset.AssetName);
		AssetObj->SetStringField(TEXT("className"), Asset.ClassName);
		AssetObj->SetBoolField(TEXT("isDataTable"), Asset.bIsDataTable);
		AssetObj->SetBoolField(TEXT("isPrimaryDataAsset"), Asset.bIsPrimaryDataAsset);
		AssetObj->SetNumberField(TEXT("rowCount"), Asset.RowCount);
		AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
	}
	Obj->SetArrayField(TEXT("assets"), AssetsArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeGetDataAssetResponse(const FGetDataAssetResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	// Asset info
	TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
	AssetObj->SetStringField(TEXT("assetPath"), Response.Asset.AssetPath);
	AssetObj->SetStringField(TEXT("assetName"), Response.Asset.AssetName);
	AssetObj->SetStringField(TEXT("className"), Response.Asset.ClassName);
	AssetObj->SetBoolField(TEXT("isDataTable"), Response.Asset.bIsDataTable);
	AssetObj->SetBoolField(TEXT("isPrimaryDataAsset"), Response.Asset.bIsPrimaryDataAsset);
	AssetObj->SetNumberField(TEXT("rowCount"), Response.Asset.RowCount);

	// Properties (already JSON strings, but need to embed as proper JSON)
	TSharedPtr<FJsonObject> PropsObj = MakeShared<FJsonObject>();
	for (const auto& Pair : Response.Asset.Properties)
	{
		// The property values are already JSON-encoded strings
		PropsObj->SetStringField(Pair.Key, Pair.Value);
	}
	AssetObj->SetObjectField(TEXT("properties"), PropsObj);

	Obj->SetObjectField(TEXT("asset"), AssetObj);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeGetDataTableRowResponse(const FGetDataTableRowResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);
	Obj->SetStringField(TEXT("rowStructName"), Response.RowStructName);
	Obj->SetNumberField(TEXT("totalRowCount"), Response.TotalRowCount);

	TArray<TSharedPtr<FJsonValue>> RowsArray;
	for (const FDataTableRowInfo& Row : Response.Rows)
	{
		TSharedPtr<FJsonObject> RowObj = MakeShared<FJsonObject>();
		RowObj->SetStringField(TEXT("rowName"), Row.RowName);

		// Row data (already JSON-encoded strings)
		TSharedPtr<FJsonObject> DataObj = MakeShared<FJsonObject>();
		for (const auto& Pair : Row.Data)
		{
			DataObj->SetStringField(Pair.Key, Pair.Value);
		}
		RowObj->SetObjectField(TEXT("data"), DataObj);

		RowsArray.Add(MakeShared<FJsonValueObject>(RowObj));
	}
	Obj->SetArrayField(TEXT("rows"), RowsArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeCaptureViewportResponse(const FCaptureViewportResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		Obj->SetStringField(TEXT("filePath"), Response.FilePath);
		Obj->SetStringField(TEXT("imageData"), Response.ImageData);
		Obj->SetStringField(TEXT("format"), Response.Format);
		Obj->SetNumberField(TEXT("width"), Response.Width);
		Obj->SetNumberField(TEXT("height"), Response.Height);
		Obj->SetNumberField(TEXT("sizeBytes"), Response.SizeBytes);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeCaptureSceneResponse(const FCaptureSceneResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		Obj->SetStringField(TEXT("filePath"), Response.FilePath);
		Obj->SetStringField(TEXT("imageData"), Response.ImageData);
		Obj->SetStringField(TEXT("format"), Response.Format);
		Obj->SetNumberField(TEXT("width"), Response.Width);
		Obj->SetNumberField(TEXT("height"), Response.Height);
		Obj->SetNumberField(TEXT("sizeBytes"), Response.SizeBytes);

		TSharedPtr<FJsonObject> CameraLocObj = MakeShared<FJsonObject>();
		CameraLocObj->SetNumberField(TEXT("x"), Response.CameraLocation.X);
		CameraLocObj->SetNumberField(TEXT("y"), Response.CameraLocation.Y);
		CameraLocObj->SetNumberField(TEXT("z"), Response.CameraLocation.Z);
		Obj->SetObjectField(TEXT("cameraLocation"), CameraLocObj);

		TSharedPtr<FJsonObject> CameraRotObj = MakeShared<FJsonObject>();
		CameraRotObj->SetNumberField(TEXT("pitch"), Response.CameraRotation.Pitch);
		CameraRotObj->SetNumberField(TEXT("yaw"), Response.CameraRotation.Yaw);
		CameraRotObj->SetNumberField(TEXT("roll"), Response.CameraRotation.Roll);
		Obj->SetObjectField(TEXT("cameraRotation"), CameraRotObj);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeAudioAnalysisResponse(const FAudioAnalysisResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		TArray<TSharedPtr<FJsonValue>> BandsArray;
		for (float Band : Response.FrequencyBands)
		{
			BandsArray.Add(MakeShared<FJsonValueNumber>(Band));
		}
		Obj->SetArrayField(TEXT("frequencyBands"), BandsArray);
		Obj->SetNumberField(TEXT("averageVolume"), Response.AverageVolume);
		Obj->SetNumberField(TEXT("peakVolume"), Response.PeakVolume);
		Obj->SetBoolField(TEXT("beatDetected"), Response.bBeatDetected);
		Obj->SetNumberField(TEXT("currentTime"), Response.CurrentTime);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeStartAudioCaptureResponse(const FStartAudioCaptureResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		Obj->SetStringField(TEXT("captureId"), Response.CaptureId);
		Obj->SetNumberField(TEXT("sampleRate"), Response.SampleRate);
		Obj->SetNumberField(TEXT("channels"), Response.Channels);
		Obj->SetNumberField(TEXT("maxDuration"), Response.MaxDuration);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeStopAudioCaptureResponse(const FStopAudioCaptureResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		Obj->SetStringField(TEXT("filePath"), Response.FilePath);
		Obj->SetStringField(TEXT("audioData"), Response.AudioData);
		Obj->SetStringField(TEXT("format"), Response.Format);
		Obj->SetNumberField(TEXT("duration"), Response.Duration);
		Obj->SetNumberField(TEXT("sampleRate"), Response.SampleRate);
		Obj->SetNumberField(TEXT("channels"), Response.Channels);
		Obj->SetNumberField(TEXT("sizeBytes"), Response.SizeBytes);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeListMaterialsResponse(const FListMaterialsResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		TArray<TSharedPtr<FJsonValue>> MaterialsArr;
		for (const FMaterialInfo& Mat : Response.Materials)
		{
			TSharedPtr<FJsonObject> MatObj = MakeShared<FJsonObject>();
			MatObj->SetStringField(TEXT("assetPath"), Mat.AssetPath);
			MatObj->SetStringField(TEXT("name"), Mat.Name);
			MatObj->SetBoolField(TEXT("isMaterialInstance"), Mat.bIsMaterialInstance);
			MatObj->SetStringField(TEXT("parentPath"), Mat.ParentPath);
			MatObj->SetBoolField(TEXT("twoSided"), Mat.bTwoSided);
			MatObj->SetStringField(TEXT("blendMode"), Mat.BlendMode);
			MaterialsArr.Add(MakeShared<FJsonValueObject>(MatObj));
		}
		Obj->SetArrayField(TEXT("materials"), MaterialsArr);
		Obj->SetNumberField(TEXT("totalCount"), Response.TotalCount);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeGetMaterialInfoResponse(const FGetMaterialInfoResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		TSharedPtr<FJsonObject> MatObj = MakeShared<FJsonObject>();
		MatObj->SetStringField(TEXT("assetPath"), Response.Material.AssetPath);
		MatObj->SetStringField(TEXT("name"), Response.Material.Name);
		MatObj->SetBoolField(TEXT("isMaterialInstance"), Response.Material.bIsMaterialInstance);
		MatObj->SetStringField(TEXT("parentPath"), Response.Material.ParentPath);
		MatObj->SetBoolField(TEXT("twoSided"), Response.Material.bTwoSided);
		MatObj->SetStringField(TEXT("blendMode"), Response.Material.BlendMode);
		Obj->SetObjectField(TEXT("material"), MatObj);

		TArray<TSharedPtr<FJsonValue>> ParamsArr;
		for (const FAgentMaterialParamInfo& Param : Response.Parameters)
		{
			TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
			ParamObj->SetStringField(TEXT("name"), Param.Name);
			ParamObj->SetStringField(TEXT("type"), Param.Type);
			ParamObj->SetStringField(TEXT("value"), Param.Value);
			ParamObj->SetStringField(TEXT("group"), Param.Group);
			ParamsArr.Add(MakeShared<FJsonValueObject>(ParamObj));
		}
		Obj->SetArrayField(TEXT("parameters"), ParamsArr);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeCreateMaterialInstanceResponse(const FCreateMaterialInstanceResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		Obj->SetStringField(TEXT("instanceName"), Response.InstanceName);
		Obj->SetBoolField(TEXT("appliedToOwner"), Response.bAppliedToOwner);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeListPCGActorsResponse(const FListPCGActorsResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		TArray<TSharedPtr<FJsonValue>> ActorsArr;
		for (const FPCGActorInfo& Actor : Response.Actors)
		{
			TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
			ActorObj->SetStringField(TEXT("guid"), Actor.Guid);
			ActorObj->SetStringField(TEXT("name"), Actor.Name);
			ActorObj->SetStringField(TEXT("label"), Actor.Label);
			ActorObj->SetStringField(TEXT("graphName"), Actor.GraphName);
			ActorObj->SetBoolField(TEXT("isGenerated"), Actor.bIsGenerated);
			ActorObj->SetStringField(TEXT("status"), Actor.Status);
			ActorsArr.Add(MakeShared<FJsonValueObject>(ActorObj));
		}
		Obj->SetArrayField(TEXT("actors"), ActorsArr);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeRegeneratePCGResponse(const FRegeneratePCGResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		Obj->SetNumberField(TEXT("generatedCount"), Response.GeneratedCount);
		Obj->SetNumberField(TEXT("generationTimeMs"), Response.GenerationTimeMs);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

//~==============================================================================
// Asset Response Serialization (P0)
//~==============================================================================

FString FCommandExecutor::SerializeCreateAssetResponse(const FCreateAssetResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		Obj->SetStringField(TEXT("assetPath"), Response.AssetPath);
		Obj->SetStringField(TEXT("assetClass"), Response.AssetClass);
		Obj->SetBoolField(TEXT("saved"), Response.bSaved);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeSaveAssetResponse(const FSaveAssetResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		Obj->SetStringField(TEXT("assetPath"), Response.AssetPath);
		Obj->SetNumberField(TEXT("fileSizeBytes"), Response.FileSizeBytes);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeSaveActorAsBlueprintResponse(const FSaveActorAsBlueprintResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		Obj->SetStringField(TEXT("blueprintPath"), Response.BlueprintPath);
		Obj->SetStringField(TEXT("generatedClassPath"), Response.GeneratedClassPath);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeDuplicateAssetResponse(const FDuplicateAssetResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		Obj->SetStringField(TEXT("newAssetPath"), Response.NewAssetPath);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeGetAssetThumbnailResponse(const FGetAssetThumbnailResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		Obj->SetStringField(TEXT("imageData"), Response.ImageData);
		Obj->SetNumberField(TEXT("width"), Response.Width);
		Obj->SetNumberField(TEXT("height"), Response.Height);
		Obj->SetStringField(TEXT("assetType"), Response.AssetType);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

//~==============================================================================
// Component Response Serialization (P1)
//~==============================================================================

FString FCommandExecutor::SerializeGetComponentTransformResponse(const FGetComponentTransformResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
		LocObj->SetNumberField(TEXT("x"), Response.Location.X);
		LocObj->SetNumberField(TEXT("y"), Response.Location.Y);
		LocObj->SetNumberField(TEXT("z"), Response.Location.Z);
		Obj->SetObjectField(TEXT("location"), LocObj);

		TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
		RotObj->SetNumberField(TEXT("pitch"), Response.Rotation.Pitch);
		RotObj->SetNumberField(TEXT("yaw"), Response.Rotation.Yaw);
		RotObj->SetNumberField(TEXT("roll"), Response.Rotation.Roll);
		Obj->SetObjectField(TEXT("rotation"), RotObj);

		TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
		ScaleObj->SetNumberField(TEXT("x"), Response.Scale.X);
		ScaleObj->SetNumberField(TEXT("y"), Response.Scale.Y);
		ScaleObj->SetNumberField(TEXT("z"), Response.Scale.Z);
		Obj->SetObjectField(TEXT("scale"), ScaleObj);

		Obj->SetBoolField(TEXT("worldSpace"), Response.bWorldSpace);
		if (!Response.ParentComponentName.IsEmpty())
		{
			Obj->SetStringField(TEXT("parentComponentName"), Response.ParentComponentName);
		}
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

//~==============================================================================
// File Response Serialization (P1)
//~==============================================================================

FString FCommandExecutor::SerializeReadProjectFileResponse(const FReadProjectFileResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		Obj->SetStringField(TEXT("content"), Response.Content);
		Obj->SetBoolField(TEXT("isBase64"), Response.bIsBase64);
		Obj->SetNumberField(TEXT("fileSizeBytes"), Response.FileSizeBytes);
		Obj->SetStringField(TEXT("modificationTime"), Response.ModificationTime);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeWriteProjectFileResponse(const FWriteProjectFileResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		Obj->SetStringField(TEXT("absolutePath"), Response.AbsolutePath);
		Obj->SetNumberField(TEXT("bytesWritten"), Response.BytesWritten);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeListProjectDirectoryResponse(const FListProjectDirectoryResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		Obj->SetStringField(TEXT("absolutePath"), Response.AbsolutePath);
		Obj->SetNumberField(TEXT("totalCount"), Response.TotalCount);

		TArray<TSharedPtr<FJsonValue>> FilesArr;
		for (const FFileInfo& File : Response.Files)
		{
			TSharedPtr<FJsonObject> FileObj = MakeShared<FJsonObject>();
			FileObj->SetStringField(TEXT("relativePath"), File.RelativePath);
			FileObj->SetStringField(TEXT("name"), File.Name);
			FileObj->SetBoolField(TEXT("isDirectory"), File.bIsDirectory);
			FileObj->SetNumberField(TEXT("sizeBytes"), File.SizeBytes);
			FileObj->SetStringField(TEXT("modificationTime"), File.ModificationTime);
			FileObj->SetStringField(TEXT("extension"), File.Extension);
			FilesArr.Add(MakeShared<FJsonValueObject>(FileObj));
		}
		Obj->SetArrayField(TEXT("files"), FilesArr);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeCopyProjectFileResponse(const FCopyProjectFileResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		Obj->SetStringField(TEXT("destAbsolutePath"), Response.DestAbsolutePath);
		Obj->SetNumberField(TEXT("bytesCopied"), Response.BytesCopied);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

// Helper: Serialize FBlueprintPinInfo to JSON object
static TSharedPtr<FJsonObject> BlueprintPinInfoToJson(const FBlueprintPinInfo& Pin)
{
	TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
	PinObj->SetStringField(TEXT("name"), Pin.Name);
	PinObj->SetStringField(TEXT("direction"), Pin.Direction);
	PinObj->SetStringField(TEXT("type"), Pin.Type);
	PinObj->SetStringField(TEXT("typeDisplayName"), Pin.TypeDisplayName);
	PinObj->SetBoolField(TEXT("isConnected"), Pin.bIsConnected);
	PinObj->SetStringField(TEXT("defaultValue"), Pin.DefaultValue);

	TArray<TSharedPtr<FJsonValue>> ConnectedArr;
	for (const FString& Conn : Pin.ConnectedTo)
	{
		ConnectedArr.Add(MakeShared<FJsonValueString>(Conn));
	}
	PinObj->SetArrayField(TEXT("connectedTo"), ConnectedArr);

	return PinObj;
}

// Helper: Serialize FBlueprintNodeInfo to JSON object
static TSharedPtr<FJsonObject> BlueprintNodeInfoToJson(const FBlueprintNodeInfo& Node)
{
	TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
	NodeObj->SetStringField(TEXT("guid"), Node.Guid);
	NodeObj->SetStringField(TEXT("className"), Node.ClassName);
	NodeObj->SetStringField(TEXT("title"), Node.Title);
	NodeObj->SetNumberField(TEXT("posX"), Node.PosX);
	NodeObj->SetNumberField(TEXT("posY"), Node.PosY);
	NodeObj->SetStringField(TEXT("comment"), Node.Comment);
	NodeObj->SetStringField(TEXT("functionReference"), Node.FunctionReference);
	NodeObj->SetStringField(TEXT("eventName"), Node.EventName);
	NodeObj->SetStringField(TEXT("variableName"), Node.VariableName);

	TArray<TSharedPtr<FJsonValue>> PinsArr;
	for (const FBlueprintPinInfo& Pin : Node.Pins)
	{
		PinsArr.Add(MakeShared<FJsonValueObject>(BlueprintPinInfoToJson(Pin)));
	}
	NodeObj->SetArrayField(TEXT("pins"), PinsArr);

	return NodeObj;
}

FString FCommandExecutor::SerializeCreateBlueprintNodeResponse(const FCreateBlueprintNodeResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		Obj->SetObjectField(TEXT("node"), BlueprintNodeInfoToJson(Response.Node));
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeListBlueprintNodesResponse(const FListBlueprintNodesResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		Obj->SetStringField(TEXT("graphName"), Response.GraphName);

		TArray<TSharedPtr<FJsonValue>> NodesArr;
		for (const FBlueprintNodeInfo& Node : Response.Nodes)
		{
			NodesArr.Add(MakeShared<FJsonValueObject>(BlueprintNodeInfoToJson(Node)));
		}
		Obj->SetArrayField(TEXT("nodes"), NodesArr);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return OutputString;
}

FString FCommandExecutor::SerializeListBlueprintPinsResponse(const FListBlueprintPinsResponse& Response)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), Response.bSuccess);
	if (!Response.bSuccess)
	{
		Obj->SetStringField(TEXT("error"), Response.ErrorMessage);
	}
	Obj->SetStringField(TEXT("commandId"), Response.CommandId);
	Obj->SetNumberField(TEXT("executionTimeMs"), Response.ExecutionTimeMs);

	if (Response.bSuccess)
	{
		Obj->SetStringField(TEXT("nodeGuid"), Response.NodeGuid);
		Obj->SetStringField(TEXT("nodeTitle"), Response.NodeTitle);

		TArray<TSharedPtr<FJsonValue>> PinsArr;
		for (const FBlueprintPinInfo& Pin : Response.Pins)
		{
			PinsArr.Add(MakeShared<FJsonValueObject>(BlueprintPinInfoToJson(Pin)));
		}
		Obj->SetArrayField(TEXT("pins"), PinsArr);
	}

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
		return TEXT("{\"success\":false,\"error\":\"Failed to parse command JSON\"}");
	}

	// Get command type
	FString TypeStr;
	if (!JsonObj->TryGetStringField(TEXT("type"), TypeStr))
	{
		return TEXT("{\"success\":false,\"error\":\"Missing command type\"}");
	}

	// Get optional command ID
	FString CommandId;
	JsonObj->TryGetStringField(TEXT("commandId"), CommandId);

	// Dispatch based on type
	if (TypeStr == TEXT("ListWorlds"))
	{
		FListWorldsCommand Cmd;
		Cmd.CommandId = CommandId;
		FListWorldsResponse Response;
		Execute(Cmd, Response);
		return SerializeListWorldsResponse(Response);
	}
	else if (TypeStr == TEXT("SetTargetWorld"))
	{
		FSetTargetWorldCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("worldIdentifier"), Cmd.WorldIdentifier);
		FAgentResponseBase Response;
		Execute(Cmd, Response);
		return SerializeBaseResponse(Response);
	}
	else if (TypeStr == TEXT("GetCapabilities"))
	{
		FGetCapabilitiesCommand Cmd;
		Cmd.CommandId = CommandId;
		FGetCapabilitiesResponse Response;
		Execute(Cmd, Response);
		return SerializeGetCapabilitiesResponse(Response);
	}
	else if (TypeStr == TEXT("QueryActors"))
	{
		FQueryActorsCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("className"), Cmd.ClassName);
		JsonObj->TryGetStringField(TEXT("namePattern"), Cmd.NamePattern);
		JsonObj->TryGetStringField(TEXT("tag"), Cmd.Tag);
		JsonObj->TryGetNumberField(TEXT("limit"), Cmd.Limit);
		JsonObj->TryGetBoolField(TEXT("includeHidden"), Cmd.bIncludeHidden);
		FQueryActorsResponse Response;
		Execute(Cmd, Response);
		return SerializeQueryActorsResponse(Response);
	}
	else if (TypeStr == TEXT("GetActor"))
	{
		FGetActorCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("actorId"), Cmd.ActorId);
		JsonObj->TryGetBoolField(TEXT("includeProperties"), Cmd.bIncludeProperties);
		JsonObj->TryGetBoolField(TEXT("includeComponents"), Cmd.bIncludeComponents);
		JsonObj->TryGetNumberField(TEXT("propertyDepth"), Cmd.PropertyDepth);
		FGetActorResponse Response;
		Execute(Cmd, Response);
		return SerializeGetActorResponse(Response);
	}
	else if (TypeStr == TEXT("SpawnActor"))
	{
		FSpawnActorCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("className"), Cmd.ClassName);
		JsonObj->TryGetStringField(TEXT("label"), Cmd.Label);
		JsonObj->TryGetStringField(TEXT("folderPath"), Cmd.FolderPath);

		// Parse location
		const TSharedPtr<FJsonObject>* LocObj;
		if (JsonObj->TryGetObjectField(TEXT("location"), LocObj))
		{
			(*LocObj)->TryGetNumberField(TEXT("x"), Cmd.Location.X);
			(*LocObj)->TryGetNumberField(TEXT("y"), Cmd.Location.Y);
			(*LocObj)->TryGetNumberField(TEXT("z"), Cmd.Location.Z);
		}

		// Parse rotation
		const TSharedPtr<FJsonObject>* RotObj;
		if (JsonObj->TryGetObjectField(TEXT("rotation"), RotObj))
		{
			(*RotObj)->TryGetNumberField(TEXT("pitch"), Cmd.Rotation.Pitch);
			(*RotObj)->TryGetNumberField(TEXT("yaw"), Cmd.Rotation.Yaw);
			(*RotObj)->TryGetNumberField(TEXT("roll"), Cmd.Rotation.Roll);
		}

		// Parse scale
		const TSharedPtr<FJsonObject>* ScaleObj;
		if (JsonObj->TryGetObjectField(TEXT("scale"), ScaleObj))
		{
			(*ScaleObj)->TryGetNumberField(TEXT("x"), Cmd.Scale.X);
			(*ScaleObj)->TryGetNumberField(TEXT("y"), Cmd.Scale.Y);
			(*ScaleObj)->TryGetNumberField(TEXT("z"), Cmd.Scale.Z);
		}

		FSpawnActorResponse Response;
		Execute(Cmd, Response);
		return SerializeSpawnActorResponse(Response);
	}
	else if (TypeStr == TEXT("DeleteActor"))
	{
		FDeleteActorCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("actorId"), Cmd.ActorId);
		FAgentResponseBase Response;
		Execute(Cmd, Response);
		return SerializeBaseResponse(Response);
	}
	else if (TypeStr == TEXT("SetActorTransform"))
	{
		FSetActorTransformCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("actorId"), Cmd.ActorId);
		JsonObj->TryGetBoolField(TEXT("sweep"), Cmd.bSweep);

		// Parse optional location
		const TSharedPtr<FJsonObject>* LocObj;
		if (JsonObj->TryGetObjectField(TEXT("location"), LocObj))
		{
			FVector Loc;
			(*LocObj)->TryGetNumberField(TEXT("x"), Loc.X);
			(*LocObj)->TryGetNumberField(TEXT("y"), Loc.Y);
			(*LocObj)->TryGetNumberField(TEXT("z"), Loc.Z);
			Cmd.Location = Loc;
		}

		// Parse optional rotation
		const TSharedPtr<FJsonObject>* RotObj;
		if (JsonObj->TryGetObjectField(TEXT("rotation"), RotObj))
		{
			FRotator Rot;
			(*RotObj)->TryGetNumberField(TEXT("pitch"), Rot.Pitch);
			(*RotObj)->TryGetNumberField(TEXT("yaw"), Rot.Yaw);
			(*RotObj)->TryGetNumberField(TEXT("roll"), Rot.Roll);
			Cmd.Rotation = Rot;
		}

		// Parse optional scale
		const TSharedPtr<FJsonObject>* ScaleObj;
		if (JsonObj->TryGetObjectField(TEXT("scale"), ScaleObj))
		{
			FVector Scale;
			(*ScaleObj)->TryGetNumberField(TEXT("x"), Scale.X);
			(*ScaleObj)->TryGetNumberField(TEXT("y"), Scale.Y);
			(*ScaleObj)->TryGetNumberField(TEXT("z"), Scale.Z);
			Cmd.Scale = Scale;
		}

		FAgentResponseBase Response;
		Execute(Cmd, Response);
		return SerializeBaseResponse(Response);
	}
	else if (TypeStr == TEXT("GetPropertyPath"))
	{
		FGetPropertyPathCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("actorId"), Cmd.ActorId);
		JsonObj->TryGetStringField(TEXT("path"), Cmd.Path);
		FPropertyValueResponse Response;
		Execute(Cmd, Response);
		return SerializePropertyValueResponse(Response);
	}
	else if (TypeStr == TEXT("SetPropertyPath"))
	{
		FSetPropertyPathCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("actorId"), Cmd.ActorId);
		JsonObj->TryGetStringField(TEXT("path"), Cmd.Path);
		JsonObj->TryGetStringField(TEXT("value"), Cmd.Value);
		FAgentResponseBase Response;
		Execute(Cmd, Response);
		return SerializeBaseResponse(Response);
	}
	else if (TypeStr == TEXT("CallFunction"))
	{
		FCallFunctionCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("actorId"), Cmd.ActorId);
		JsonObj->TryGetStringField(TEXT("className"), Cmd.ClassName);
		JsonObj->TryGetStringField(TEXT("functionName"), Cmd.FunctionName);

		// Parse parameters
		const TSharedPtr<FJsonObject>* ParamsObj;
		if (JsonObj->TryGetObjectField(TEXT("parameters"), ParamsObj))
		{
			for (const auto& Pair : (*ParamsObj)->Values)
			{
				FString ValueStr;
				TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ValueStr);
				FJsonSerializer::Serialize(Pair.Value, Pair.Key, Writer);
				Cmd.Parameters.Add(Pair.Key, ValueStr);
			}
		}

		FFunctionCallResponse Response;
		Execute(Cmd, Response);
		return SerializeFunctionCallResponse(Response);
	}
	else if (TypeStr == TEXT("ListClasses"))
	{
		FListClassesCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("baseClassName"), Cmd.BaseClassName);
		JsonObj->TryGetStringField(TEXT("namePattern"), Cmd.NamePattern);
		JsonObj->TryGetBoolField(TEXT("includeBlueprint"), Cmd.bIncludeBlueprint);
		JsonObj->TryGetBoolField(TEXT("includeAbstract"), Cmd.bIncludeAbstract);
		JsonObj->TryGetNumberField(TEXT("limit"), Cmd.Limit);
		FListClassesResponse Response;
		Execute(Cmd, Response);
		return SerializeListClassesResponse(Response);
	}
	else if (TypeStr == TEXT("ListDataAssets"))
	{
		FListDataAssetsCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("baseClassName"), Cmd.BaseClassName);
		JsonObj->TryGetStringField(TEXT("pathFilter"), Cmd.PathFilter);
		JsonObj->TryGetNumberField(TEXT("limit"), Cmd.Limit);
		FListDataAssetsResponse Response;
		Execute(Cmd, Response);
		return SerializeListDataAssetsResponse(Response);
	}
	else if (TypeStr == TEXT("GetDataAsset"))
	{
		FGetDataAssetCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("assetPath"), Cmd.AssetPath);
		JsonObj->TryGetNumberField(TEXT("propertyDepth"), Cmd.PropertyDepth);
		FGetDataAssetResponse Response;
		Execute(Cmd, Response);
		return SerializeGetDataAssetResponse(Response);
	}
	else if (TypeStr == TEXT("GetDataTableRow"))
	{
		FGetDataTableRowCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("tablePath"), Cmd.TablePath);
		JsonObj->TryGetStringField(TEXT("rowName"), Cmd.RowName);
		JsonObj->TryGetNumberField(TEXT("limit"), Cmd.Limit);
		FGetDataTableRowResponse Response;
		Execute(Cmd, Response);
		return SerializeGetDataTableRowResponse(Response);
	}
	else if (TypeStr == TEXT("CaptureViewport"))
	{
		FCaptureViewportCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("outputPath"), Cmd.OutputPath);
		JsonObj->TryGetNumberField(TEXT("width"), Cmd.Width);
		JsonObj->TryGetNumberField(TEXT("height"), Cmd.Height);
		JsonObj->TryGetBoolField(TEXT("showUI"), Cmd.bShowUI);
		JsonObj->TryGetStringField(TEXT("format"), Cmd.Format);
		FCaptureViewportResponse Response;
		Execute(Cmd, Response);
		return SerializeCaptureViewportResponse(Response);
	}
	else if (TypeStr == TEXT("CaptureScene"))
	{
		FCaptureSceneCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("actorId"), Cmd.ActorId);
		JsonObj->TryGetStringField(TEXT("componentName"), Cmd.ComponentName);
		JsonObj->TryGetStringField(TEXT("outputPath"), Cmd.OutputPath);
		JsonObj->TryGetStringField(TEXT("format"), Cmd.Format);

		// Parse location
		const TSharedPtr<FJsonObject>* LocObj;
		if (JsonObj->TryGetObjectField(TEXT("location"), LocObj))
		{
			(*LocObj)->TryGetNumberField(TEXT("x"), Cmd.Location.X);
			(*LocObj)->TryGetNumberField(TEXT("y"), Cmd.Location.Y);
			(*LocObj)->TryGetNumberField(TEXT("z"), Cmd.Location.Z);
		}

		// Parse rotation
		const TSharedPtr<FJsonObject>* RotObj;
		if (JsonObj->TryGetObjectField(TEXT("rotation"), RotObj))
		{
			(*RotObj)->TryGetNumberField(TEXT("pitch"), Cmd.Rotation.Pitch);
			(*RotObj)->TryGetNumberField(TEXT("yaw"), Cmd.Rotation.Yaw);
			(*RotObj)->TryGetNumberField(TEXT("roll"), Cmd.Rotation.Roll);
		}

		double TempFOV;
		if (JsonObj->TryGetNumberField(TEXT("fov"), TempFOV))
		{
			Cmd.FOV = static_cast<float>(TempFOV);
		}
		JsonObj->TryGetNumberField(TEXT("width"), Cmd.Width);
		JsonObj->TryGetNumberField(TEXT("height"), Cmd.Height);

		FCaptureSceneResponse Response;
		Execute(Cmd, Response);
		return SerializeCaptureSceneResponse(Response);
	}
	else if (TypeStr == TEXT("GetAudioAnalysis"))
	{
		FGetAudioAnalysisCommand Cmd;
		Cmd.CommandId = CommandId;

		// Parse source enum
		FString SourceStr;
		if (JsonObj->TryGetStringField(TEXT("source"), SourceStr))
		{
			if (SourceStr.Equals(TEXT("PlayerMic"), ESearchCase::IgnoreCase))
			{
				Cmd.Source = EAudioCaptureSource::PlayerMic;
			}
			else if (SourceStr.Equals(TEXT("Actor"), ESearchCase::IgnoreCase))
			{
				Cmd.Source = EAudioCaptureSource::Actor;
			}
			// Default is WorldAudio
		}

		JsonObj->TryGetStringField(TEXT("actorId"), Cmd.ActorId);
		JsonObj->TryGetNumberField(TEXT("frequencyBands"), Cmd.FrequencyBands);

		FAudioAnalysisResponse Response;
		Execute(Cmd, Response);
		return SerializeAudioAnalysisResponse(Response);
	}
	else if (TypeStr == TEXT("StartAudioCapture"))
	{
		FStartAudioCaptureCommand Cmd;
		Cmd.CommandId = CommandId;

		// Parse source enum
		FString SourceStr;
		if (JsonObj->TryGetStringField(TEXT("source"), SourceStr))
		{
			if (SourceStr.Equals(TEXT("PlayerMic"), ESearchCase::IgnoreCase))
			{
				Cmd.Source = EAudioCaptureSource::PlayerMic;
			}
			else if (SourceStr.Equals(TEXT("Actor"), ESearchCase::IgnoreCase))
			{
				Cmd.Source = EAudioCaptureSource::Actor;
			}
		}

		JsonObj->TryGetStringField(TEXT("actorId"), Cmd.ActorId);

		double TempMaxDuration;
		if (JsonObj->TryGetNumberField(TEXT("maxDuration"), TempMaxDuration))
		{
			Cmd.MaxDuration = static_cast<float>(TempMaxDuration);
		}
		JsonObj->TryGetNumberField(TEXT("sampleRate"), Cmd.SampleRate);
		JsonObj->TryGetNumberField(TEXT("channels"), Cmd.Channels);

		FStartAudioCaptureResponse Response;
		Execute(Cmd, Response);
		return SerializeStartAudioCaptureResponse(Response);
	}
	else if (TypeStr == TEXT("StopAudioCapture"))
	{
		FStopAudioCaptureCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("captureId"), Cmd.CaptureId);
		JsonObj->TryGetStringField(TEXT("outputPath"), Cmd.OutputPath);

		FStopAudioCaptureResponse Response;
		Execute(Cmd, Response);
		return SerializeStopAudioCaptureResponse(Response);
	}
	else if (TypeStr == TEXT("ListMaterials"))
	{
		FListMaterialsCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("pathFilter"), Cmd.PathFilter);
		JsonObj->TryGetBoolField(TEXT("instancesOnly"), Cmd.bInstancesOnly);
		JsonObj->TryGetNumberField(TEXT("limit"), Cmd.Limit);

		FListMaterialsResponse Response;
		Execute(Cmd, Response);
		return SerializeListMaterialsResponse(Response);
	}
	else if (TypeStr == TEXT("GetMaterialInfo"))
	{
		FGetMaterialInfoCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("materialPath"), Cmd.MaterialPath);
		JsonObj->TryGetBoolField(TEXT("includeParameters"), Cmd.bIncludeParameters);

		FGetMaterialInfoResponse Response;
		Execute(Cmd, Response);
		return SerializeGetMaterialInfoResponse(Response);
	}
	else if (TypeStr == TEXT("CreateMaterialInstance"))
	{
		FCreateMaterialInstanceCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("parentMaterialPath"), Cmd.ParentMaterialPath);
		JsonObj->TryGetStringField(TEXT("instanceName"), Cmd.InstanceName);
		JsonObj->TryGetStringField(TEXT("ownerActorId"), Cmd.OwnerActorId);

		// Parse scalar parameters
		const TSharedPtr<FJsonObject>* ScalarParamsObj;
		if (JsonObj->TryGetObjectField(TEXT("scalarParameters"), ScalarParamsObj))
		{
			for (const auto& Pair : (*ScalarParamsObj)->Values)
			{
				Cmd.ScalarParameters.Add(Pair.Key, Pair.Value->AsNumber());
			}
		}

		// Parse vector parameters (as string values containing JSON)
		const TSharedPtr<FJsonObject>* VectorParamsObj;
		if (JsonObj->TryGetObjectField(TEXT("vectorParameters"), VectorParamsObj))
		{
			for (const auto& Pair : (*VectorParamsObj)->Values)
			{
				FString ValueStr;
				TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ValueStr);
				FJsonSerializer::Serialize(Pair.Value->AsObject().ToSharedRef(), Writer);
				Cmd.VectorParameters.Add(Pair.Key, ValueStr);
			}
		}

		FCreateMaterialInstanceResponse Response;
		Execute(Cmd, Response);
		return SerializeCreateMaterialInstanceResponse(Response);
	}
	else if (TypeStr == TEXT("SetMaterialParameter"))
	{
		FSetMaterialParameterCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("targetId"), Cmd.TargetId);
		JsonObj->TryGetStringField(TEXT("componentName"), Cmd.ComponentName);
		JsonObj->TryGetNumberField(TEXT("slotIndex"), Cmd.SlotIndex);
		JsonObj->TryGetStringField(TEXT("parameterName"), Cmd.ParameterName);
		JsonObj->TryGetStringField(TEXT("value"), Cmd.Value);

		// Parse parameter type
		FString TypeString;
		if (JsonObj->TryGetStringField(TEXT("parameterType"), TypeString))
		{
			if (TypeString.Equals(TEXT("Vector"), ESearchCase::IgnoreCase))
			{
				Cmd.ParameterType = EAgentMaterialParamType::Vector;
			}
			else if (TypeString.Equals(TEXT("Texture"), ESearchCase::IgnoreCase))
			{
				Cmd.ParameterType = EAgentMaterialParamType::Texture;
			}
			else if (TypeString.Equals(TEXT("StaticSwitch"), ESearchCase::IgnoreCase))
			{
				Cmd.ParameterType = EAgentMaterialParamType::StaticSwitch;
			}
			// Default is Scalar
		}

		FAgentResponseBase Response;
		Execute(Cmd, Response);
		return SerializeBaseResponse(Response);
	}
	else if (TypeStr == TEXT("ApplyMaterialToActor"))
	{
		FApplyMaterialToActorCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("actorId"), Cmd.ActorId);
		JsonObj->TryGetStringField(TEXT("componentName"), Cmd.ComponentName);
		JsonObj->TryGetStringField(TEXT("materialPath"), Cmd.MaterialPath);
		JsonObj->TryGetNumberField(TEXT("slotIndex"), Cmd.SlotIndex);

		FAgentResponseBase Response;
		Execute(Cmd, Response);
		return SerializeBaseResponse(Response);
	}
	else if (TypeStr == TEXT("ListPCGActors"))
	{
		FListPCGActorsCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("namePattern"), Cmd.NamePattern);
		JsonObj->TryGetBoolField(TEXT("includeGraphInfo"), Cmd.bIncludeGraphInfo);
		JsonObj->TryGetNumberField(TEXT("limit"), Cmd.Limit);

		FListPCGActorsResponse Response;
		Execute(Cmd, Response);
		return SerializeListPCGActorsResponse(Response);
	}
	else if (TypeStr == TEXT("RegeneratePCG"))
	{
		FRegeneratePCGCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("actorId"), Cmd.ActorId);
		JsonObj->TryGetStringField(TEXT("componentName"), Cmd.ComponentName);
		JsonObj->TryGetBoolField(TEXT("forceRefresh"), Cmd.bForceRefresh);

		FRegeneratePCGResponse Response;
		Execute(Cmd, Response);
		return SerializeRegeneratePCGResponse(Response);
	}
	else if (TypeStr == TEXT("SetPCGParameter"))
	{
		FSetPCGParameterCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("actorId"), Cmd.ActorId);
		JsonObj->TryGetStringField(TEXT("parameterName"), Cmd.ParameterName);
		JsonObj->TryGetStringField(TEXT("value"), Cmd.Value);
		JsonObj->TryGetBoolField(TEXT("autoRegenerate"), Cmd.bAutoRegenerate);

		FAgentResponseBase Response;
		Execute(Cmd, Response);
		return SerializeBaseResponse(Response);
	}
	//~==============================================================================
	// Asset Commands (P0)
	//~==============================================================================
	else if (TypeStr == TEXT("CreateAsset"))
	{
		FCreateAssetCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("assetClass"), Cmd.AssetClass);
		JsonObj->TryGetStringField(TEXT("packagePath"), Cmd.PackagePath);
		JsonObj->TryGetStringField(TEXT("assetName"), Cmd.AssetName);
		JsonObj->TryGetStringField(TEXT("parentAssetPath"), Cmd.ParentAssetPath);

		// Parse properties map
		const TSharedPtr<FJsonObject>* PropsObj;
		if (JsonObj->TryGetObjectField(TEXT("properties"), PropsObj))
		{
			for (const auto& Pair : (*PropsObj)->Values)
			{
				FString ValueStr;
				TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ValueStr);
				FJsonSerializer::Serialize(Pair.Value, TEXT(""), Writer);
				Cmd.Properties.Add(Pair.Key, ValueStr);
			}
		}

		FCreateAssetResponse Response;
		Execute(Cmd, Response);
		return SerializeCreateAssetResponse(Response);
	}
	else if (TypeStr == TEXT("SaveAsset"))
	{
		FSaveAssetCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("assetPath"), Cmd.AssetPath);
		JsonObj->TryGetBoolField(TEXT("promptForCheckout"), Cmd.bPromptForCheckout);

		FSaveAssetResponse Response;
		Execute(Cmd, Response);
		return SerializeSaveAssetResponse(Response);
	}
	else if (TypeStr == TEXT("SaveActorAsBlueprint"))
	{
		FSaveActorAsBlueprintCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("actorId"), Cmd.ActorId);
		JsonObj->TryGetStringField(TEXT("packagePath"), Cmd.PackagePath);
		JsonObj->TryGetStringField(TEXT("blueprintName"), Cmd.BlueprintName);
		JsonObj->TryGetBoolField(TEXT("replaceExisting"), Cmd.bReplaceExisting);

		FSaveActorAsBlueprintResponse Response;
		Execute(Cmd, Response);
		return SerializeSaveActorAsBlueprintResponse(Response);
	}
	else if (TypeStr == TEXT("DuplicateAsset"))
	{
		FDuplicateAssetCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("sourcePath"), Cmd.SourcePath);
		JsonObj->TryGetStringField(TEXT("destPackagePath"), Cmd.DestPackagePath);
		JsonObj->TryGetStringField(TEXT("destAssetName"), Cmd.DestAssetName);

		FDuplicateAssetResponse Response;
		Execute(Cmd, Response);
		return SerializeDuplicateAssetResponse(Response);
	}
	else if (TypeStr == TEXT("GetAssetThumbnail"))
	{
		FGetAssetThumbnailCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("assetPath"), Cmd.AssetPath);
		JsonObj->TryGetNumberField(TEXT("width"), Cmd.Width);
		JsonObj->TryGetNumberField(TEXT("height"), Cmd.Height);

		FGetAssetThumbnailResponse Response;
		Execute(Cmd, Response);
		return SerializeGetAssetThumbnailResponse(Response);
	}
	//~==============================================================================
	// Component Commands (P1)
	//~==============================================================================
	else if (TypeStr == TEXT("GetComponentTransform"))
	{
		FGetComponentTransformCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("actorId"), Cmd.ActorId);
		JsonObj->TryGetStringField(TEXT("componentName"), Cmd.ComponentName);
		JsonObj->TryGetBoolField(TEXT("worldSpace"), Cmd.bWorldSpace);

		FGetComponentTransformResponse Response;
		Execute(Cmd, Response);
		return SerializeGetComponentTransformResponse(Response);
	}
	else if (TypeStr == TEXT("SetComponentTransform"))
	{
		FSetComponentTransformCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("actorId"), Cmd.ActorId);
		JsonObj->TryGetStringField(TEXT("componentName"), Cmd.ComponentName);
		JsonObj->TryGetBoolField(TEXT("worldSpace"), Cmd.bWorldSpace);
		JsonObj->TryGetBoolField(TEXT("sweep"), Cmd.bSweep);

		// Parse location
		const TSharedPtr<FJsonObject>* LocObj;
		if (JsonObj->TryGetObjectField(TEXT("location"), LocObj))
		{
			FVector Loc;
			(*LocObj)->TryGetNumberField(TEXT("x"), Loc.X);
			(*LocObj)->TryGetNumberField(TEXT("y"), Loc.Y);
			(*LocObj)->TryGetNumberField(TEXT("z"), Loc.Z);
			Cmd.Location = Loc;
		}

		// Parse rotation
		const TSharedPtr<FJsonObject>* RotObj;
		if (JsonObj->TryGetObjectField(TEXT("rotation"), RotObj))
		{
			FRotator Rot;
			(*RotObj)->TryGetNumberField(TEXT("pitch"), Rot.Pitch);
			(*RotObj)->TryGetNumberField(TEXT("yaw"), Rot.Yaw);
			(*RotObj)->TryGetNumberField(TEXT("roll"), Rot.Roll);
			Cmd.Rotation = Rot;
		}

		// Parse scale
		const TSharedPtr<FJsonObject>* ScaleObj;
		if (JsonObj->TryGetObjectField(TEXT("scale"), ScaleObj))
		{
			FVector Scale;
			(*ScaleObj)->TryGetNumberField(TEXT("x"), Scale.X);
			(*ScaleObj)->TryGetNumberField(TEXT("y"), Scale.Y);
			(*ScaleObj)->TryGetNumberField(TEXT("z"), Scale.Z);
			Cmd.Scale = Scale;
		}

		FAgentResponseBase Response;
		Execute(Cmd, Response);
		return SerializeBaseResponse(Response);
	}
	else if (TypeStr == TEXT("AttachComponent"))
	{
		FAttachComponentCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("actorId"), Cmd.ActorId);
		JsonObj->TryGetStringField(TEXT("componentName"), Cmd.ComponentName);
		JsonObj->TryGetStringField(TEXT("parentComponentName"), Cmd.ParentComponentName);
		JsonObj->TryGetStringField(TEXT("socketName"), Cmd.SocketName);

		// Parse attachment rules
		FString RuleStr;
		if (JsonObj->TryGetStringField(TEXT("locationRule"), RuleStr))
		{
			if (RuleStr.Equals(TEXT("KeepWorld"), ESearchCase::IgnoreCase)) Cmd.LocationRule = EAttachmentRuleType::KeepWorld;
			else if (RuleStr.Equals(TEXT("SnapToTarget"), ESearchCase::IgnoreCase)) Cmd.LocationRule = EAttachmentRuleType::SnapToTarget;
		}
		if (JsonObj->TryGetStringField(TEXT("rotationRule"), RuleStr))
		{
			if (RuleStr.Equals(TEXT("KeepWorld"), ESearchCase::IgnoreCase)) Cmd.RotationRule = EAttachmentRuleType::KeepWorld;
			else if (RuleStr.Equals(TEXT("SnapToTarget"), ESearchCase::IgnoreCase)) Cmd.RotationRule = EAttachmentRuleType::SnapToTarget;
		}
		if (JsonObj->TryGetStringField(TEXT("scaleRule"), RuleStr))
		{
			if (RuleStr.Equals(TEXT("KeepWorld"), ESearchCase::IgnoreCase)) Cmd.ScaleRule = EAttachmentRuleType::KeepWorld;
			else if (RuleStr.Equals(TEXT("SnapToTarget"), ESearchCase::IgnoreCase)) Cmd.ScaleRule = EAttachmentRuleType::SnapToTarget;
		}

		FAgentResponseBase Response;
		Execute(Cmd, Response);
		return SerializeBaseResponse(Response);
	}
	else if (TypeStr == TEXT("AttachActor"))
	{
		FAttachActorCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("childActorId"), Cmd.ChildActorId);
		JsonObj->TryGetStringField(TEXT("parentActorId"), Cmd.ParentActorId);
		JsonObj->TryGetStringField(TEXT("parentComponentName"), Cmd.ParentComponentName);
		JsonObj->TryGetStringField(TEXT("socketName"), Cmd.SocketName);

		// Parse attachment rules
		FString RuleStr;
		if (JsonObj->TryGetStringField(TEXT("locationRule"), RuleStr))
		{
			if (RuleStr.Equals(TEXT("KeepRelative"), ESearchCase::IgnoreCase)) Cmd.LocationRule = EAttachmentRuleType::KeepRelative;
			else if (RuleStr.Equals(TEXT("SnapToTarget"), ESearchCase::IgnoreCase)) Cmd.LocationRule = EAttachmentRuleType::SnapToTarget;
		}
		if (JsonObj->TryGetStringField(TEXT("rotationRule"), RuleStr))
		{
			if (RuleStr.Equals(TEXT("KeepRelative"), ESearchCase::IgnoreCase)) Cmd.RotationRule = EAttachmentRuleType::KeepRelative;
			else if (RuleStr.Equals(TEXT("SnapToTarget"), ESearchCase::IgnoreCase)) Cmd.RotationRule = EAttachmentRuleType::SnapToTarget;
		}
		if (JsonObj->TryGetStringField(TEXT("scaleRule"), RuleStr))
		{
			if (RuleStr.Equals(TEXT("KeepRelative"), ESearchCase::IgnoreCase)) Cmd.ScaleRule = EAttachmentRuleType::KeepRelative;
			else if (RuleStr.Equals(TEXT("SnapToTarget"), ESearchCase::IgnoreCase)) Cmd.ScaleRule = EAttachmentRuleType::SnapToTarget;
		}

		FAgentResponseBase Response;
		Execute(Cmd, Response);
		return SerializeBaseResponse(Response);
	}
	else if (TypeStr == TEXT("DetachComponent"))
	{
		FDetachComponentCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("actorId"), Cmd.ActorId);
		JsonObj->TryGetStringField(TEXT("componentName"), Cmd.ComponentName);
		JsonObj->TryGetBoolField(TEXT("maintainWorldPosition"), Cmd.bMaintainWorldPosition);

		FAgentResponseBase Response;
		Execute(Cmd, Response);
		return SerializeBaseResponse(Response);
	}
	else if (TypeStr == TEXT("DetachActor"))
	{
		FDetachActorCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("actorId"), Cmd.ActorId);
		JsonObj->TryGetBoolField(TEXT("maintainWorldPosition"), Cmd.bMaintainWorldPosition);

		FAgentResponseBase Response;
		Execute(Cmd, Response);
		return SerializeBaseResponse(Response);
	}
	//~==============================================================================
	// File Commands (P1)
	//~==============================================================================
	else if (TypeStr == TEXT("ReadProjectFile"))
	{
		FReadProjectFileCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("relativePath"), Cmd.RelativePath);
		JsonObj->TryGetBoolField(TEXT("asBase64"), Cmd.bAsBase64);
		JsonObj->TryGetNumberField(TEXT("maxBytes"), Cmd.MaxBytes);

		FReadProjectFileResponse Response;
		Execute(Cmd, Response);
		return SerializeReadProjectFileResponse(Response);
	}
	else if (TypeStr == TEXT("WriteProjectFile"))
	{
		FWriteProjectFileCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("relativePath"), Cmd.RelativePath);
		JsonObj->TryGetStringField(TEXT("content"), Cmd.Content);
		JsonObj->TryGetBoolField(TEXT("isBase64"), Cmd.bIsBase64);
		JsonObj->TryGetBoolField(TEXT("createDirectories"), Cmd.bCreateDirectories);
		JsonObj->TryGetBoolField(TEXT("append"), Cmd.bAppend);

		FWriteProjectFileResponse Response;
		Execute(Cmd, Response);
		return SerializeWriteProjectFileResponse(Response);
	}
	else if (TypeStr == TEXT("ListProjectDirectory"))
	{
		FListProjectDirectoryCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("relativePath"), Cmd.RelativePath);
		JsonObj->TryGetStringField(TEXT("pattern"), Cmd.Pattern);
		JsonObj->TryGetBoolField(TEXT("recursive"), Cmd.bRecursive);
		JsonObj->TryGetNumberField(TEXT("limit"), Cmd.Limit);

		FListProjectDirectoryResponse Response;
		Execute(Cmd, Response);
		return SerializeListProjectDirectoryResponse(Response);
	}
	else if (TypeStr == TEXT("CopyProjectFile"))
	{
		FCopyProjectFileCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("sourcePath"), Cmd.SourcePath);
		JsonObj->TryGetStringField(TEXT("destPath"), Cmd.DestPath);
		JsonObj->TryGetBoolField(TEXT("overwrite"), Cmd.bOverwrite);

		FCopyProjectFileResponse Response;
		Execute(Cmd, Response);
		return SerializeCopyProjectFileResponse(Response);
	}
	else if (TypeStr == TEXT("DeleteProjectFile"))
	{
		FDeleteProjectFileCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("relativePath"), Cmd.RelativePath);
		JsonObj->TryGetBoolField(TEXT("allowDirectoryDelete"), Cmd.bAllowDirectoryDelete);

		FAgentResponseBase Response;
		Execute(Cmd, Response);
		return SerializeBaseResponse(Response);
	}
	// Blueprint Node Commands
	else if (TypeStr == TEXT("CreateBlueprintNode"))
	{
		FCreateBlueprintNodeCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("blueprintPath"), Cmd.BlueprintPath);
		JsonObj->TryGetStringField(TEXT("graphName"), Cmd.GraphName);
		JsonObj->TryGetStringField(TEXT("nodeType"), Cmd.NodeType);
		JsonObj->TryGetStringField(TEXT("functionReference"), Cmd.FunctionReference);
		JsonObj->TryGetStringField(TEXT("eventName"), Cmd.EventName);
		JsonObj->TryGetStringField(TEXT("variableName"), Cmd.VariableName);
		JsonObj->TryGetStringField(TEXT("comment"), Cmd.Comment);
		JsonObj->TryGetNumberField(TEXT("posX"), Cmd.PosX);
		JsonObj->TryGetNumberField(TEXT("posY"), Cmd.PosY);

		FCreateBlueprintNodeResponse Response;
		Execute(Cmd, Response);
		return SerializeCreateBlueprintNodeResponse(Response);
	}
	else if (TypeStr == TEXT("ConnectBlueprintPins"))
	{
		FConnectBlueprintPinsCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("blueprintPath"), Cmd.BlueprintPath);
		JsonObj->TryGetStringField(TEXT("sourceNode"), Cmd.SourceNode);
		JsonObj->TryGetStringField(TEXT("sourcePin"), Cmd.SourcePin);
		JsonObj->TryGetStringField(TEXT("targetNode"), Cmd.TargetNode);
		JsonObj->TryGetStringField(TEXT("targetPin"), Cmd.TargetPin);

		FAgentResponseBase Response;
		Execute(Cmd, Response);
		return SerializeBaseResponse(Response);
	}
	else if (TypeStr == TEXT("DisconnectBlueprintPins"))
	{
		FDisconnectBlueprintPinsCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("blueprintPath"), Cmd.BlueprintPath);
		JsonObj->TryGetStringField(TEXT("sourceNode"), Cmd.SourceNode);
		JsonObj->TryGetStringField(TEXT("sourcePin"), Cmd.SourcePin);
		JsonObj->TryGetStringField(TEXT("targetNode"), Cmd.TargetNode);
		JsonObj->TryGetStringField(TEXT("targetPin"), Cmd.TargetPin);

		FAgentResponseBase Response;
		Execute(Cmd, Response);
		return SerializeBaseResponse(Response);
	}
	else if (TypeStr == TEXT("DeleteBlueprintNode"))
	{
		FDeleteBlueprintNodeCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("blueprintPath"), Cmd.BlueprintPath);
		JsonObj->TryGetStringField(TEXT("nodeId"), Cmd.NodeId);

		FAgentResponseBase Response;
		Execute(Cmd, Response);
		return SerializeBaseResponse(Response);
	}
	else if (TypeStr == TEXT("ListBlueprintNodes"))
	{
		FListBlueprintNodesCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("blueprintPath"), Cmd.BlueprintPath);
		JsonObj->TryGetStringField(TEXT("graphName"), Cmd.GraphName);
		JsonObj->TryGetStringField(TEXT("nodeClassFilter"), Cmd.NodeClassFilter);

		FListBlueprintNodesResponse Response;
		Execute(Cmd, Response);
		return SerializeListBlueprintNodesResponse(Response);
	}
	else if (TypeStr == TEXT("ListBlueprintPins"))
	{
		FListBlueprintPinsCommand Cmd;
		Cmd.CommandId = CommandId;
		JsonObj->TryGetStringField(TEXT("blueprintPath"), Cmd.BlueprintPath);
		JsonObj->TryGetStringField(TEXT("nodeId"), Cmd.NodeId);

		FListBlueprintPinsResponse Response;
		Execute(Cmd, Response);
		return SerializeListBlueprintPinsResponse(Response);
	}

	return TEXT("{\"success\":false,\"error\":\"Unknown command type\"}");
}

FString FCommandExecutor::ExecuteBatchJson(const FString& CommandsJson, bool bStopOnError)
{
	// Parse batch request - expects {"commands": [...]} or just [...]
	TArray<TSharedPtr<FJsonValue>> CommandArray;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CommandsJson);

	// Try parsing as object with "commands" field first
	TSharedPtr<FJsonObject> BatchObj;
	if (FJsonSerializer::Deserialize(Reader, BatchObj) && BatchObj.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Commands = nullptr;
		if (BatchObj->TryGetArrayField(TEXT("commands"), Commands))
		{
			CommandArray = *Commands;
		}
	}

	// If that didn't work, try parsing as direct array
	if (CommandArray.Num() == 0)
	{
		TSharedRef<TJsonReader<>> ArrayReader = TJsonReaderFactory<>::Create(CommandsJson);
		FJsonSerializer::Deserialize(ArrayReader, CommandArray);
	}

	if (CommandArray.Num() == 0)
	{
		return TEXT("{\"success\":false,\"error\":\"Expected {\\\"commands\\\":[...]} or [...]\",\"responses\":[]}");
	}

	// Execute each command
	TArray<FString> Responses;
	bool bAllSucceeded = true;

	for (const TSharedPtr<FJsonValue>& CmdValue : CommandArray)
	{
		const TSharedPtr<FJsonObject>* CmdObj = nullptr;
		if (!CmdValue->TryGetObject(CmdObj) || !CmdObj || !CmdObj->IsValid())
		{
			Responses.Add(TEXT("{\"success\":false,\"error\":\"Invalid command object\"}"));
			bAllSucceeded = false;
			if (bStopOnError)
			{
				break;
			}
			continue;
		}

		// Serialize command back to JSON and execute
		FString CmdJson;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&CmdJson);
		FJsonSerializer::Serialize(CmdObj->ToSharedRef(), Writer);

		FString Response = ExecuteJson(CmdJson);
		Responses.Add(Response);

		// Check if this command failed
		TSharedPtr<FJsonObject> ResponseObj;
		TSharedRef<TJsonReader<>> ResponseReader = TJsonReaderFactory<>::Create(Response);
		if (FJsonSerializer::Deserialize(ResponseReader, ResponseObj) && ResponseObj.IsValid())
		{
			bool bSuccess = false;
			ResponseObj->TryGetBoolField(TEXT("success"), bSuccess);
			if (!bSuccess)
			{
				bAllSucceeded = false;
				if (bStopOnError)
				{
					break;
				}
			}
		}
	}

	// Build batch response
	FString Result = FString::Printf(TEXT("{\"success\":%s,\"executed\":%d,\"total\":%d,\"responses\":["),
		bAllSucceeded ? TEXT("true") : TEXT("false"),
		Responses.Num(),
		CommandArray.Num());

	for (int32 i = 0; i < Responses.Num(); i++)
	{
		if (i > 0) Result += TEXT(",");
		Result += Responses[i];
	}
	Result += TEXT("]}");

	return Result;
}
