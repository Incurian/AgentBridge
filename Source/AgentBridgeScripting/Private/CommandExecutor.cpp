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
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

// Capture-related includes
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "ImageUtils.h"
#include "Misc/Base64.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"

#if WITH_EDITOR
#include "LevelEditorViewport.h"
#include "Editor/EditorEngine.h"
#include "UnrealEdGlobals.h"
#include "HighResScreenshot.h"
#include "Slate/SceneViewport.h"
#endif

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
		FAgentPropertyValue Value = FPropertyAccessor::ReadProperty(Property, ValuePtr, Command.PropertyDepth);
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

	UScriptStruct* RowStruct = DataTable->GetRowStruct();
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
			FAgentPropertyValue Value = FPropertyAccessor::ReadProperty(Property, ValuePtr, 3);
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
				FAgentPropertyValue Value = FPropertyAccessor::ReadProperty(Property, ValuePtr, 3);
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
			TArray64<uint8> CompressedData;
			if (ImageWrapper->GetCompressed(CompressedData, 100))
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
			TArray64<uint8> CompressedData;
			if (ImageWrapper->GetCompressed(CompressedData, 100))
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

	return TEXT("{\"success\":false,\"error\":\"Unknown command type\"}");
}

FString FCommandExecutor::ExecuteBatchJson(const FString& CommandsJson, bool bStopOnError)
{
	// TODO: Implement batch execution
	return TEXT("{\"success\":true,\"responses\":[]}");
}
