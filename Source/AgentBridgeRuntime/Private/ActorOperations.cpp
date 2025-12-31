#include "ActorOperations.h"
#include "WorldContextManager.h"
#include "PropertyAccessor.h"
#include "TypeDiscovery.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

//~==============================================================================
// FActorReference Implementation
//~==============================================================================

AActor* FActorReference::Resolve(UWorld* World) const
{
	if (!World)
	{
		World = FWorldContextManager::Get().GetTargetWorld();
	}

	if (!World)
	{
		return nullptr;
	}

	// Try GUID first (most stable)
	if (!Guid.IsEmpty())
	{
		FGuid ParsedGuid;
		if (FGuid::Parse(Guid, ParsedGuid))
		{
			AActor* Found = FActorOperations::FindActorByGuid(ParsedGuid, World);
			if (Found)
			{
				return Found;
			}
		}
	}

	// Try path
	if (!Path.IsEmpty())
	{
		UObject* Found = FindObject<AActor>(nullptr, *Path);
		if (AActor* Actor = Cast<AActor>(Found))
		{
			return Actor;
		}
	}

	// Try name/label
	if (!Name.IsEmpty())
	{
		AActor* Found = FActorOperations::FindActorByName(Name, World);
		if (Found)
		{
			return Found;
		}
	}

	if (!Label.IsEmpty())
	{
		AActor* Found = FActorOperations::FindActorByName(Label, World);
		if (Found)
		{
			return Found;
		}
	}

	return nullptr;
}

FActorReference FActorReference::FromActor(AActor* Actor)
{
	FActorReference Ref;

	if (!Actor)
	{
		return Ref;
	}

	Ref.Guid = Actor->GetActorGuid().ToString();
	Ref.Path = Actor->GetPathName();
	Ref.Name = Actor->GetName();
	Ref.Label = Actor->GetActorLabel();
	Ref.ClassName = Actor->GetClass()->GetName();

	return Ref;
}

bool FActorReference::IsValid() const
{
	return !Guid.IsEmpty() || !Path.IsEmpty() || !Name.IsEmpty();
}

//~==============================================================================
// Actor Queries
//~==============================================================================

TArray<FActorReference> FActorOperations::QueryActors(
	const FActorQueryParams& Params,
	UWorld* World)
{
	TArray<FActorReference> Results;

	if (!World)
	{
		World = FWorldContextManager::Get().GetTargetWorld();
	}

	if (!World)
	{
		return Results;
	}

	int32 Count = 0;

	for (TActorIterator<AActor> It(World); It && Count < Params.Limit; ++It)
	{
		AActor* Actor = *It;

		if (!Actor)
		{
			continue;
		}

		// Class filter
		if (Params.ClassFilter && !Actor->IsA(Params.ClassFilter))
		{
			continue;
		}

		// Hidden filter
		if (!Params.bIncludeHidden && Actor->IsHidden())
		{
			continue;
		}

		// Name pattern filter
		if (!Params.NamePattern.IsEmpty())
		{
			bool bMatchesName = Actor->GetName().Contains(Params.NamePattern) ||
				Actor->GetActorLabel().Contains(Params.NamePattern);
			if (!bMatchesName)
			{
				continue;
			}
		}

		// Tag filter
		if (!Params.Tag.IsEmpty())
		{
			if (!Actor->ActorHasTag(*Params.Tag))
			{
				continue;
			}
		}

		// Bounds filter
		if (Params.BoundsFilter.IsSet())
		{
			FVector ActorLocation = Actor->GetActorLocation();
			if (!Params.BoundsFilter.GetValue().IsInside(ActorLocation))
			{
				continue;
			}
		}

		Results.Add(FActorReference::FromActor(Actor));
		Count++;
	}

	return Results;
}

TArray<FActorReference> FActorOperations::GetAllActors(UWorld* World, int32 Limit)
{
	FActorQueryParams Params;
	Params.Limit = Limit;
	Params.bIncludeHidden = true;
	return QueryActors(Params, World);
}

AActor* FActorOperations::FindActorByName(const FString& SearchString, UWorld* World)
{
	if (SearchString.IsEmpty())
	{
		return nullptr;
	}

	if (!World)
	{
		World = FWorldContextManager::Get().GetTargetWorld();
	}

	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;

		// Check name
		if (Actor->GetName().Equals(SearchString, ESearchCase::IgnoreCase))
		{
			return Actor;
		}

		// Check label
		if (Actor->GetActorLabel().Equals(SearchString, ESearchCase::IgnoreCase))
		{
			return Actor;
		}

		// Check contains
		if (Actor->GetName().Contains(SearchString) || Actor->GetActorLabel().Contains(SearchString))
		{
			return Actor;
		}
	}

	return nullptr;
}

AActor* FActorOperations::FindActorByGuid(const FGuid& Guid, UWorld* World)
{
	if (!Guid.IsValid())
	{
		return nullptr;
	}

	if (!World)
	{
		World = FWorldContextManager::Get().GetTargetWorld();
	}

	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->GetActorGuid() == Guid)
		{
			return Actor;
		}
	}

	return nullptr;
}

AActor* FActorOperations::ResolveActorReference(const FActorReference& Ref, UWorld* World)
{
	return Ref.Resolve(World);
}

//~==============================================================================
// Actor Creation
//~==============================================================================

AActor* FActorOperations::SpawnActor(
	const FActorSpawnParams& Params,
	UWorld* World,
	FString* OutError)
{
	if (!World)
	{
		World = FWorldContextManager::Get().GetTargetWorld();
	}

	if (!World)
	{
		if (OutError) *OutError = TEXT("No world available");
		return nullptr;
	}

	// Find the class
	UClass* ActorClass = FTypeDiscovery::FindClassByName(Params.ClassPath);
	if (!ActorClass)
	{
		if (OutError) *OutError = FString::Printf(TEXT("Class not found: %s"), *Params.ClassPath);
		return nullptr;
	}

	// Verify it's an actor class
	if (!ActorClass->IsChildOf(AActor::StaticClass()))
	{
		if (OutError) *OutError = FString::Printf(TEXT("Class is not an Actor: %s"), *Params.ClassPath);
		return nullptr;
	}

	// Setup spawn parameters
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = Params.CollisionHandling;

	// Spawn the actor
	AActor* NewActor = World->SpawnActor<AActor>(ActorClass, Params.Transform, SpawnParams);

	if (!NewActor)
	{
		if (OutError) *OutError = TEXT("SpawnActor returned null");
		return nullptr;
	}

	// Set label if provided
	if (!Params.ActorLabel.IsEmpty())
	{
		SetActorLabel(NewActor, Params.ActorLabel);
	}

	// Set folder if provided
	if (!Params.FolderPath.IsEmpty())
	{
		SetActorFolder(NewActor, Params.FolderPath);
	}

	// Set initial properties
	if (Params.InitialProperties.Num() > 0)
	{
		SetActorProperties(NewActor, Params.InitialProperties);
	}

	return NewActor;
}

AActor* FActorOperations::DuplicateActor(
	AActor* Source,
	const FTransform& NewTransform,
	const FString& NewLabel)
{
	if (!Source)
	{
		return nullptr;
	}

	UWorld* World = Source->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Use spawn with same class and transform
	FActorSpawnParams Params;
	Params.ClassPath = Source->GetClass()->GetPathName();
	Params.Transform = NewTransform;
	Params.ActorLabel = NewLabel.IsEmpty() ? Source->GetActorLabel() + TEXT("_Copy") : NewLabel;

	AActor* NewActor = SpawnActor(Params, World);

	// TODO: Copy properties from source to duplicate
	// This is complex and may require special handling

	return NewActor;
}

//~==============================================================================
// Actor Destruction
//~==============================================================================

bool FActorOperations::DestroyActor(AActor* Actor)
{
	if (!Actor)
	{
		return false;
	}

	UWorld* World = Actor->GetWorld();
	if (!World)
	{
		return false;
	}

	return World->DestroyActor(Actor);
}

int32 FActorOperations::DestroyActors(const TArray<AActor*>& Actors)
{
	int32 Destroyed = 0;

	for (AActor* Actor : Actors)
	{
		if (DestroyActor(Actor))
		{
			Destroyed++;
		}
	}

	return Destroyed;
}

//~==============================================================================
// Actor Modification
//~==============================================================================

bool FActorOperations::SetActorTransform(AActor* Actor, const FTransform& Transform, bool bSweep)
{
	if (!Actor)
	{
		return false;
	}

	return Actor->SetActorTransform(Transform, bSweep);
}

bool FActorOperations::SetActorProperties(
	AActor* Actor,
	const TMap<FString, FAgentPropertyValue>& Properties)
{
	if (!Actor)
	{
		return false;
	}

	UClass* ActorClass = Actor->GetClass();
	bool bAllSuccess = true;

	for (const auto& Pair : Properties)
	{
		// Find property by display name
		FProperty* Property = nullptr;

		for (TFieldIterator<FProperty> PropIt(ActorClass); PropIt; ++PropIt)
		{
			FString DisplayName = FTypeDiscovery::GetDisplayPropertyName(*PropIt);
			if (DisplayName.Equals(Pair.Key, ESearchCase::IgnoreCase) ||
				PropIt->GetName().Equals(Pair.Key, ESearchCase::IgnoreCase))
			{
				Property = *PropIt;
				break;
			}
		}

		if (Property)
		{
			if (!FPropertyAccessor::WriteProperty(Actor, Property, Pair.Value))
			{
				bAllSuccess = false;
			}
		}
		else
		{
			bAllSuccess = false;
		}
	}

	return bAllSuccess;
}

TMap<FString, FAgentPropertyValue> FActorOperations::GetActorProperties(
	AActor* Actor,
	const TArray<FString>& PropertyNames)
{
	TMap<FString, FAgentPropertyValue> Results;

	if (!Actor)
	{
		return Results;
	}

	UClass* ActorClass = Actor->GetClass();

	if (PropertyNames.Num() == 0)
	{
		// Get all properties
		for (TFieldIterator<FProperty> PropIt(ActorClass); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			FString DisplayName = FTypeDiscovery::GetDisplayPropertyName(Property);

			FAgentPropertyValue Value = FPropertyAccessor::ReadProperty(Actor, Property);
			Results.Add(DisplayName, Value);
		}
	}
	else
	{
		// Get specific properties
		for (const FString& PropName : PropertyNames)
		{
			for (TFieldIterator<FProperty> PropIt(ActorClass); PropIt; ++PropIt)
			{
				FProperty* Property = *PropIt;
				FString DisplayName = FTypeDiscovery::GetDisplayPropertyName(Property);

				if (DisplayName.Equals(PropName, ESearchCase::IgnoreCase) ||
					Property->GetName().Equals(PropName, ESearchCase::IgnoreCase))
				{
					FAgentPropertyValue Value = FPropertyAccessor::ReadProperty(Actor, Property);
					Results.Add(DisplayName, Value);
					break;
				}
			}
		}
	}

	return Results;
}

bool FActorOperations::SetActorLabel(AActor* Actor, const FString& NewLabel)
{
	if (!Actor)
	{
		return false;
	}

#if WITH_EDITOR
	Actor->SetActorLabel(NewLabel);
	return true;
#else
	return false;
#endif
}

bool FActorOperations::SetActorFolder(AActor* Actor, const FString& FolderPath)
{
	if (!Actor)
	{
		return false;
	}

#if WITH_EDITOR
	Actor->SetFolderPath(*FolderPath);
	return true;
#else
	return false;
#endif
}

bool FActorOperations::AttachActor(AActor* Child, AActor* Parent)
{
	if (!Child)
	{
		return false;
	}

	if (Parent)
	{
		// Attach to parent
		Child->AttachToActor(Parent, FAttachmentTransformRules::KeepWorldTransform);
	}
	else
	{
		// Detach
		Child->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}

	return true;
}
