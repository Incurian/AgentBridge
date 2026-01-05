#include "TargetResolution.h"
#include "ActorOperations.h"
#include "WorldContextManager.h"
#include "Components/ActorComponent.h"

namespace AgentBridge
{
namespace TargetResolution
{

//~==============================================================================
// Parsing
//~==============================================================================

FTargetInfo Parse(const FString& Target)
{
	FTargetInfo Info;

	if (Target.IsEmpty())
	{
		return Info;
	}

	// Find the separator
	int32 SeparatorIndex;
	if (Target.FindChar(TEXT('-'), SeparatorIndex))
	{
		// Check if it's actually "->" (not just a hyphen in a name)
		if (SeparatorIndex + 1 < Target.Len() && Target[SeparatorIndex + 1] == TEXT('>'))
		{
			// Split on "->"
			Info.ActorPart = Target.Left(SeparatorIndex).TrimStartAndEnd();
			Info.ComponentPart = Target.Mid(SeparatorIndex + 2).TrimStartAndEnd();
			return Info;
		}
	}

	// No separator found - actor only
	Info.ActorPart = Target.TrimStartAndEnd();
	return Info;
}

//~==============================================================================
// Component Resolution
//~==============================================================================

UActorComponent* FindAnyComponent(AActor* Actor, const FString& ComponentName)
{
	if (!Actor || ComponentName.IsEmpty())
	{
		return nullptr;
	}

	// Get all components
	TInlineComponentArray<UActorComponent*> Components;
	Actor->GetComponents(Components);

	// Try exact match first
	for (UActorComponent* Comp : Components)
	{
		if (Comp && Comp->GetName() == ComponentName)
		{
			return Comp;
		}
	}

	// Try case-insensitive match
	for (UActorComponent* Comp : Components)
	{
		if (Comp && Comp->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
		{
			return Comp;
		}
	}

	// Try partial match (handles "LightComponent" matching "LightComponent0")
	// This is common for Blueprint-generated component names
	for (UActorComponent* Comp : Components)
	{
		if (Comp)
		{
			FString CompName = Comp->GetName();
			// Check if ComponentName is a prefix of the actual name
			if (CompName.StartsWith(ComponentName, ESearchCase::IgnoreCase))
			{
				// Make sure the remaining part is just digits (e.g., "0", "1", "12")
				FString Suffix = CompName.Mid(ComponentName.Len());
				bool bAllDigits = true;
				for (TCHAR Char : Suffix)
				{
					if (!FChar::IsDigit(Char))
					{
						bAllDigits = false;
						break;
					}
				}
				if (Suffix.IsEmpty() || bAllDigits)
				{
					return Comp;
				}
			}
		}
	}

	return nullptr;
}

USceneComponent* FindComponent(AActor* Actor, const FString& ComponentName)
{
	UActorComponent* Found = FindAnyComponent(Actor, ComponentName);
	return Cast<USceneComponent>(Found);
}

//~==============================================================================
// Resolution
//~==============================================================================

FResolvedTarget Resolve(UWorld* World, const FString& Target, FString* OutError)
{
	FResolvedTarget Result;

	// Parse the target string
	FTargetInfo Info = Parse(Target);

	if (!Info.IsValid())
	{
		Result.Error = TEXT("Target string is empty");
		if (OutError) *OutError = Result.Error;
		return Result;
	}

	// Get world if not provided
	if (!World)
	{
		World = FWorldContextManager::Get().GetTargetWorld();
	}

	if (!World)
	{
		Result.Error = TEXT("No world available");
		if (OutError) *OutError = Result.Error;
		return Result;
	}

	// Resolve the actor
	Result.Actor = FActorOperations::FindActorByName(Info.ActorPart, World);

	if (!Result.Actor)
	{
		Result.Error = FString::Printf(TEXT("Actor '%s' not found"), *Info.ActorPart);
		if (OutError) *OutError = Result.Error;
		return Result;
	}

	// If component was requested, resolve it
	if (Info.IsComponent())
	{
		Result.Component = FindComponent(Result.Actor, Info.ComponentPart);

		if (!Result.Component)
		{
			// Try to find as any component (might not be a SceneComponent)
			UActorComponent* AnyComp = FindAnyComponent(Result.Actor, Info.ComponentPart);
			if (AnyComp)
			{
				Result.Error = FString::Printf(
					TEXT("Component '%s' found but is not a SceneComponent (type: %s)"),
					*Info.ComponentPart,
					*AnyComp->GetClass()->GetName()
				);
			}
			else
			{
				Result.Error = FString::Printf(
					TEXT("Component '%s' not found on actor '%s'"),
					*Info.ComponentPart,
					*Result.Actor->GetActorLabel()
				);
			}
			if (OutError) *OutError = Result.Error;
			// Note: We still return the actor even if component wasn't found
			// Caller can check Result.IsComponent() to see if component resolution succeeded
		}
	}

	return Result;
}

bool ResolveAttachmentTargets(
	UWorld* World,
	const FString& ChildTarget,
	const FString& ParentTarget,
	FResolvedTarget& OutChild,
	FResolvedTarget& OutParent,
	FString* OutError)
{
	// Resolve child
	OutChild = Resolve(World, ChildTarget);
	if (!OutChild.IsValid())
	{
		if (OutError) *OutError = FString::Printf(TEXT("Child: %s"), *OutChild.Error);
		return false;
	}

	// For attachment, if a component was requested but not found, that's an error
	FTargetInfo ChildInfo = Parse(ChildTarget);
	if (ChildInfo.IsComponent() && !OutChild.IsComponent())
	{
		if (OutError) *OutError = FString::Printf(TEXT("Child component not found: %s"), *OutChild.Error);
		return false;
	}

	// Resolve parent
	OutParent = Resolve(World, ParentTarget);
	if (!OutParent.IsValid())
	{
		if (OutError) *OutError = FString::Printf(TEXT("Parent: %s"), *OutParent.Error);
		return false;
	}

	// For attachment, if a component was requested but not found, that's an error
	FTargetInfo ParentInfo = Parse(ParentTarget);
	if (ParentInfo.IsComponent() && !OutParent.IsComponent())
	{
		if (OutError) *OutError = FString::Printf(TEXT("Parent component not found: %s"), *OutParent.Error);
		return false;
	}

	return true;
}

} // namespace TargetResolution
} // namespace AgentBridge
