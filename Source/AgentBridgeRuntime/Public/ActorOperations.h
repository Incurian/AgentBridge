#pragma once

#include "CoreMinimal.h"
#include "AgentBridgeTypes.h"
#include "GameFramework/Actor.h"

/**
 * FActorReference - Stable reference to an actor across operations.
 *
 * Actors can be referenced in multiple ways:
 * - GUID: Most stable, survives rename/relevel
 * - Path: Full object path, unique but changes on rename
 * - Name: GetName(), unique within level but internal
 * - Label: Editor display name, human-readable but may not be unique
 *
 * This struct stores all identifiers for flexible resolution.
 */
struct AGENTBRIDGERUNTIME_API FActorReference
{
	/** Actor GUID - most stable identifier. */
	FString Guid;

	/** Full object path from GetPathName(). */
	FString Path;

	/** Internal name from GetName(). */
	FString Name;

	/** Editor label from GetActorLabel(). Human-readable. */
	FString Label;

	/** Class name for type information. */
	FString ClassName;

	/**
	 * Attempts to resolve this reference to an actor.
	 *
	 * Tries resolution in order: GUID, Path, Name, Label.
	 *
	 * @param World		The world to search in. Uses WorldContextManager if null.
	 * @return			Resolved actor or nullptr.
	 */
	AActor* Resolve(UWorld* World = nullptr) const;

	/**
	 * Creates a reference from an actor.
	 *
	 * @param Actor		The actor to reference.
	 * @return			Populated reference structure.
	 */
	static FActorReference FromActor(AActor* Actor);

	/**
	 * Checks if this reference has valid identifiers.
	 */
	bool IsValid() const;
};

/**
 * FActorSpawnParams - Parameters for spawning a new actor.
 */
struct AGENTBRIDGERUNTIME_API FActorSpawnParams
{
	/** Class path or name to spawn. */
	FString ClassPath;

	/** Spawn transform. */
	FTransform Transform = FTransform::Identity;

	/** Editor display label. */
	FString ActorLabel;

	/** Folder path in World Outliner (e.g., "/MyFolder/SubFolder"). */
	FString FolderPath;

	/** Initial property values to set after spawn. */
	TMap<FString, FAgentPropertyValue> InitialProperties;

	/** Collision handling method. */
	ESpawnActorCollisionHandlingMethod CollisionHandling = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
};

/**
 * FActorQueryParams - Parameters for querying actors.
 */
struct AGENTBRIDGERUNTIME_API FActorQueryParams
{
	/** Filter by class (nullptr = all actors). */
	UClass* ClassFilter = nullptr;

	/** Filter by internal name pattern (substring match on GetName()). */
	FString NamePattern;

	/** Filter by label pattern (substring match on GetActorLabel() - display names). */
	FString LabelPattern;

	/** Filter by actor tag. */
	FString Tag;

	/** Filter by bounding box (nullptr = no bounds filter). */
	TOptional<FBox> BoundsFilter;

	/** Maximum results to return. */
	int32 Limit = 1000;

	/** Include hidden actors. */
	bool bIncludeHidden = false;
};

/**
 * FActorOperations - High-level actor manipulation operations.
 *
 * This class provides the primary interface for agents to interact with actors:
 * - Querying: Find actors by class, name, tag, or bounds
 * - Spawning: Create new actors from class
 * - Modifying: Set properties, transform, label
 * - Managing: Delete, duplicate, reparent
 *
 * All operations use FWorldContextManager for world resolution unless
 * an explicit world is provided.
 *
 * Editor Integration:
 * - Actor labels and folders are editor-only features
 * - Transactions can be used for undo support (see TransactionWrapper)
 *
 * Thread Safety:
 * - All operations must be called from the Game Thread
 *
 * @see FWorldContextManager for world resolution
 * @see FPropertyAccessor for property manipulation
 */
class AGENTBRIDGERUNTIME_API FActorOperations
{
public:
	//~==============================================================================
	// Actor Queries
	//~==============================================================================

	/**
	 * Queries actors matching specified criteria.
	 *
	 * @param Params	Query parameters (class, name pattern, etc.).
	 * @param World		World to search. Uses WorldContextManager if null.
	 * @return			Array of matching actor references.
	 */
	static TArray<FActorReference> QueryActors(
		const FActorQueryParams& Params,
		UWorld* World = nullptr
	);

	/**
	 * Gets all actors in the world.
	 *
	 * @param World		World to enumerate. Uses WorldContextManager if null.
	 * @param Limit		Maximum results.
	 * @return			Array of actor references.
	 */
	static TArray<FActorReference> GetAllActors(UWorld* World = nullptr, int32 Limit = 10000);

	/**
	 * Finds a single actor by name or label.
	 *
	 * @param SearchString	Name or label to search for.
	 * @param World			World to search.
	 * @return				Found actor or nullptr.
	 */
	static AActor* FindActorByName(const FString& SearchString, UWorld* World = nullptr);

	/**
	 * Finds an actor by its GUID.
	 *
	 * @param Guid		The actor GUID.
	 * @param World		World to search.
	 * @return			Found actor or nullptr.
	 */
	static AActor* FindActorByGuid(const FGuid& Guid, UWorld* World = nullptr);

	/**
	 * Resolves an actor reference to an actor pointer.
	 *
	 * @param Ref		The reference to resolve.
	 * @param World		World to search.
	 * @return			Resolved actor or nullptr.
	 */
	static AActor* ResolveActorReference(const FActorReference& Ref, UWorld* World = nullptr);

	//~==============================================================================
	// Actor Creation
	//~==============================================================================

	/**
	 * Spawns a new actor from the given parameters.
	 *
	 * @param Params		Spawn parameters.
	 * @param World			World to spawn in. Uses WorldContextManager if null.
	 * @param OutError		Error message if spawn fails.
	 * @return				Spawned actor or nullptr.
	 */
	static AActor* SpawnActor(
		const FActorSpawnParams& Params,
		UWorld* World = nullptr,
		FString* OutError = nullptr
	);

	/**
	 * Duplicates an existing actor.
	 *
	 * @param Source		Actor to duplicate.
	 * @param NewTransform	Transform for the duplicate.
	 * @param NewLabel		Optional new label.
	 * @return				Duplicated actor or nullptr.
	 */
	static AActor* DuplicateActor(
		AActor* Source,
		const FTransform& NewTransform,
		const FString& NewLabel = TEXT("")
	);

	//~==============================================================================
	// Actor Destruction
	//~==============================================================================

	/**
	 * Destroys an actor.
	 *
	 * @param Actor		Actor to destroy.
	 * @return			True if destruction was initiated.
	 */
	static bool DestroyActor(AActor* Actor);

	/**
	 * Destroys multiple actors.
	 *
	 * @param Actors	Array of actors to destroy.
	 * @return			Number of actors destroyed.
	 */
	static int32 DestroyActors(const TArray<AActor*>& Actors);

	//~==============================================================================
	// Actor Modification
	//~==============================================================================

	/**
	 * Sets an actor's world transform.
	 *
	 * @param Actor		Actor to modify.
	 * @param Transform	New world transform.
	 * @param bSweep	Whether to sweep for collision.
	 * @return			True if successful.
	 */
	static bool SetActorTransform(AActor* Actor, const FTransform& Transform, bool bSweep = false);

	/**
	 * Sets multiple properties on an actor.
	 *
	 * @param Actor		Actor to modify.
	 * @param Properties	Map of property name -> value.
	 * @return			True if all properties were set successfully.
	 */
	static bool SetActorProperties(
		AActor* Actor,
		const TMap<FString, FAgentPropertyValue>& Properties
	);

	/**
	 * Gets property values from an actor.
	 *
	 * @param Actor			Actor to read from.
	 * @param PropertyNames	Names of properties to read. Empty = all properties.
	 * @return				Map of property name -> value.
	 */
	static TMap<FString, FAgentPropertyValue> GetActorProperties(
		AActor* Actor,
		const TArray<FString>& PropertyNames = TArray<FString>()
	);

	/**
	 * Sets an actor's editor label.
	 *
	 * @param Actor		Actor to modify.
	 * @param NewLabel	New label.
	 * @return			True if successful.
	 */
	static bool SetActorLabel(AActor* Actor, const FString& NewLabel);

	/**
	 * Sets an actor's folder path in the World Outliner.
	 *
	 * @param Actor		Actor to modify.
	 * @param FolderPath	New folder path.
	 * @return			True if successful.
	 */
	static bool SetActorFolder(AActor* Actor, const FString& FolderPath);

	/**
	 * Attaches an actor to a new parent.
	 *
	 * @param Child		Actor to attach.
	 * @param Parent	New parent actor. Nullptr to detach.
	 * @return			True if successful.
	 */
	static bool AttachActor(AActor* Child, AActor* Parent);
};
