#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"

/**
 * TargetResolution - Unified target string parsing and resolution.
 *
 * Supports the "Actor->Component" syntax for unified actor/component targeting:
 * - "MyLight"                  -> Actor only
 * - "MyLight->LightComponent0" -> Actor's component
 * - "BP_Door_5->DoorMesh"      -> Blueprint actor's component
 *
 * This module centralizes resolution logic that was previously duplicated
 * across transform and attachment command handlers.
 *
 * @see FActorOperations::FindActorByName for actor resolution
 */
namespace AgentBridge
{

/** Separator for Actor->Component syntax. Easy to change if needed. */
constexpr TCHAR TARGET_SEPARATOR[] = TEXT("->");

/**
 * FTargetInfo - Parsed target string parts.
 */
struct AGENTBRIDGERUNTIME_API FTargetInfo
{
	/** Actor identifier (name, label, GUID, or path). */
	FString ActorPart;

	/** Component name. Empty if actor-only target. */
	FString ComponentPart;

	/** Returns true if this target includes a component. */
	bool IsComponent() const { return !ComponentPart.IsEmpty(); }

	/** Returns true if this is an actor-only target. */
	bool IsActor() const { return ComponentPart.IsEmpty(); }

	/** Returns true if the target string was valid (has actor part). */
	bool IsValid() const { return !ActorPart.IsEmpty(); }
};

/**
 * FResolvedTarget - Resolved actor and optional component pointers.
 */
struct AGENTBRIDGERUNTIME_API FResolvedTarget
{
	/** Resolved actor. Null if resolution failed. */
	AActor* Actor = nullptr;

	/** Resolved component. Null if actor-only target or component not found. */
	USceneComponent* Component = nullptr;

	/** Error message if resolution failed. */
	FString Error;

	/** Returns true if at least the actor was resolved. */
	bool IsValid() const { return Actor != nullptr; }

	/** Returns true if a component was requested AND resolved. */
	bool IsComponent() const { return Component != nullptr; }

	/** Returns true if resolution failed completely. */
	bool HasError() const { return !Error.IsEmpty(); }

	/**
	 * Get the scene component to operate on.
	 * Returns Component if present, otherwise Actor's RootComponent.
	 */
	USceneComponent* GetSceneComponent() const
	{
		if (Component)
		{
			return Component;
		}
		return Actor ? Actor->GetRootComponent() : nullptr;
	}
};

/**
 * TargetResolution namespace - Static functions for parsing and resolving targets.
 */
namespace TargetResolution
{
	/**
	 * Parses a target string into actor and component parts.
	 *
	 * @param Target    Target string (e.g., "MyActor" or "MyActor->Component")
	 * @return          Parsed target info
	 */
	AGENTBRIDGERUNTIME_API FTargetInfo Parse(const FString& Target);

	/**
	 * Finds a component by name on an actor.
	 *
	 * Supports:
	 * - Exact name match
	 * - Case-insensitive match
	 * - Partial match for BP components (e.g., "LightComponent" matches "LightComponent0")
	 *
	 * @param Actor         Actor to search components on
	 * @param ComponentName Name to search for
	 * @return              Found component or nullptr
	 */
	AGENTBRIDGERUNTIME_API USceneComponent* FindComponent(AActor* Actor, const FString& ComponentName);

	/**
	 * Finds any actor component by name (not just scene components).
	 *
	 * @param Actor         Actor to search components on
	 * @param ComponentName Name to search for
	 * @return              Found component or nullptr
	 */
	AGENTBRIDGERUNTIME_API UActorComponent* FindAnyComponent(AActor* Actor, const FString& ComponentName);

	/**
	 * Resolves a target string to actor and optional component.
	 *
	 * Combines Parse() with actor and component resolution.
	 * Uses FActorOperations::FindActorByName() for actor lookup.
	 *
	 * @param World     World to search in. Uses WorldContextManager if null.
	 * @param Target    Target string (e.g., "MyActor" or "MyActor->Component")
	 * @param OutError  Optional error message output
	 * @return          Resolved target (check IsValid() for success)
	 */
	AGENTBRIDGERUNTIME_API FResolvedTarget Resolve(
		UWorld* World,
		const FString& Target,
		FString* OutError = nullptr
	);

	/**
	 * Resolves two targets for attachment operations.
	 *
	 * @param World         World to search in
	 * @param ChildTarget   Child target string
	 * @param ParentTarget  Parent target string
	 * @param OutChild      Resolved child
	 * @param OutParent     Resolved parent
	 * @param OutError      Error message if resolution fails
	 * @return              True if both targets resolved successfully
	 */
	AGENTBRIDGERUNTIME_API bool ResolveAttachmentTargets(
		UWorld* World,
		const FString& ChildTarget,
		const FString& ParentTarget,
		FResolvedTarget& OutChild,
		FResolvedTarget& OutParent,
		FString* OutError = nullptr
	);

} // namespace TargetResolution

} // namespace AgentBridge
