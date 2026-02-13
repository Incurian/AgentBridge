// Copyright 2025 AgentBridge. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActorOperations.h"

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionHelpers.h"
#endif

class UWorld;
class UWorldPartition;
class ALandscapeProxy;
class ALandscapeStreamingProxy;

/**
 * Complete landscape bounds information
 * Includes XY extents from proxy positions and Z from height data
 */
struct AGENTBRIDGERUNTIME_API FLandscapeBounds
{
	/** Whether valid bounds were found */
	bool bValid = false;

	/** Minimum corner of the bounding box (world space) */
	FVector Min = FVector::ZeroVector;

	/** Maximum corner of the bounding box (world space) */
	FVector Max = FVector::ZeroVector;

	/** Center point of the landscape */
	FVector Center = FVector::ZeroVector;

	/** Half-extents (distance from center to edge) */
	FVector Extent = FVector::ZeroVector;

	/** Scale factor to make a 100-unit-extent BoxComponent match landscape bounds.
	 *  XY = Extent / 100. Z = (Extent.Z + 10000) / 100 for elevation headroom. */
	FVector BiomeVolumeScale = FVector::OneVector;

	/** Number of landscape proxies sampled */
	int32 ProxyCount = 0;

	/** Name of the main landscape actor (if found) */
	FString LandscapeName;
};

/**
 * Streaming state for an actor in World Partition
 */
enum class EActorStreamingState : uint8
{
	/** Not in a World Partition world, or no WP data available */
	NotApplicable,

	/** Actor is currently loaded in memory */
	Loaded,

	/** Actor exists in WP metadata but is not loaded (in unloaded streaming cell) */
	Unloaded,

	/** Actor was deleted or doesn't exist */
	Invalid
};

/**
 * Extended actor reference with World Partition streaming info
 */
struct AGENTBRIDGERUNTIME_API FStreamingActorReference : public FActorReference
{
	/** Current streaming state of this actor */
	EActorStreamingState StreamingState = EActorStreamingState::NotApplicable;

	/** The streaming cell/region this actor belongs to (if applicable) */
	FString StreamingCellName;

	/** Bounding box from actor descriptor (available even when unloaded) */
	FBox EditorBounds;

	/** Data layers this actor belongs to */
	TArray<FName> DataLayers;

	/** Whether this actor is spatially loaded (vs always loaded) */
	bool bIsSpatiallyLoaded = true;

	/** Actor transform (from loaded actor or estimated from bounds center) */
	FTransform Transform;

	/** Create from loaded actor */
	static FStreamingActorReference FromLoadedActor(AActor* Actor);

#if WITH_EDITOR
	/** Create from actor descriptor (may be unloaded) */
	static FStreamingActorReference FromActorDescInstance(const FWorldPartitionActorDescInstance* ActorDescInstance);
#endif
};

/**
 * Query parameters for World Partition-aware actor queries
 */
struct AGENTBRIDGERUNTIME_API FWorldPartitionQueryParams
{
	/** Include loaded actors */
	bool bIncludeLoaded = true;

	/** Include unloaded actors (from WP metadata) */
	bool bIncludeUnloaded = true;

	/** Filter by class (nullptr = all actors) */
	TSubclassOf<AActor> ClassFilter;

	/** Filter by name pattern (empty = no filter) */
	FString NamePattern;

	/** Filter by bounding box (only actors intersecting this box) */
	TOptional<FBox> BoundsFilter;

	/** Filter by data layer name */
	FName DataLayerFilter;

	/** Maximum results to return */
	int32 Limit = 1000;
};

/**
 * World Partition-aware actor operations
 *
 * This module extends ActorOperations to handle:
 * - Actors in unloaded streaming cells
 * - Landscape streaming proxies
 * - World Partition metadata queries
 * - Streaming cell load/unload operations
 */
class AGENTBRIDGERUNTIME_API FWorldPartitionOps
{
public:
	//~==============================================================================
	// World Partition State
	//~==============================================================================

	/** Check if the world uses World Partition */
	static bool IsWorldPartitioned(UWorld* World = nullptr);

	/** Get World Partition subsystem for a world */
	static UWorldPartition* GetWorldPartition(UWorld* World = nullptr);

	//~==============================================================================
	// Streaming-Aware Actor Queries
	//~==============================================================================

	/**
	 * Query actors including those in unloaded streaming cells
	 * In editor, this uses FWorldPartitionHelpers::ForEachActorDescInstance
	 * At runtime, falls back to standard TActorIterator (only loaded actors)
	 */
	static TArray<FStreamingActorReference> QueryAllActors(
		const FWorldPartitionQueryParams& Params,
		UWorld* World = nullptr);

	/**
	 * Get streaming state for an actor by GUID
	 */
	static EActorStreamingState GetActorStreamingState(const FGuid& ActorGuid, UWorld* World = nullptr);

	/**
	 * Find actor by GUID, including unloaded actors
	 * Returns streaming reference with metadata even if actor is unloaded
	 */
	static FStreamingActorReference FindActorByGuidEx(const FGuid& Guid, UWorld* World = nullptr);

	//~==============================================================================
	// Landscape Streaming
	//~==============================================================================

	/**
	 * Query all landscape proxies (including streaming proxies)
	 * Handles ALandscapeProxy, ALandscapeStreamingProxy, ALandscape
	 */
	static TArray<FStreamingActorReference> QueryLandscapeProxies(
		UWorld* World = nullptr,
		bool bIncludeUnloaded = true);

	/**
	 * Get the main landscape actor for the level
	 */
	static ALandscapeProxy* GetMainLandscape(UWorld* World = nullptr);

	/**
	 * Get complete landscape bounds in world space
	 * Samples collision components from streaming proxies to get accurate Z bounds
	 *
	 * @param World - World to query (nullptr = current world)
	 * @return FLandscapeBounds with min/max/center/extent in world space
	 */
	static FLandscapeBounds GetLandscapeBounds(UWorld* World = nullptr);

	//~==============================================================================
	// Streaming Cell Operations (Editor Only)
	//~==============================================================================

#if WITH_EDITOR
	/**
	 * Request loading of a specific actor by GUID
	 * Returns the loaded actor, or nullptr if load failed
	 */
	static AActor* LoadActor(const FGuid& ActorGuid, UWorld* World = nullptr);

	/**
	 * Request loading of actors in a region
	 * @param Box - World-space bounding box to load
	 * @return Number of actors that were loaded
	 */
	static int32 LoadRegion(const FBox& Box, UWorld* World = nullptr);

	/**
	 * Unload a region (release references, allowing GC)
	 * @param Box - World-space bounding box to unload
	 */
	static void UnloadRegion(const FBox& Box, UWorld* World = nullptr);

	/**
	 * Delete an actor with proper World Partition cleanup
	 * Handles both loaded and unloaded actors
	 */
	static bool DeleteActorWP(const FGuid& ActorGuid, UWorld* World = nullptr);

	/**
	 * Delete multiple actors with WP cleanup
	 */
	static int32 DeleteActorsWP(const TArray<FGuid>& ActorGuids, UWorld* World = nullptr);
#endif

	//~==============================================================================
	// Data Layer Operations
	//~==============================================================================

	/** Get all data layers in the world */
	static TArray<FName> GetDataLayers(UWorld* World = nullptr);

	/** Get actors in a specific data layer */
	static TArray<FStreamingActorReference> GetActorsInDataLayer(
		FName DataLayerName,
		bool bIncludeUnloaded = true,
		UWorld* World = nullptr);

private:
#if WITH_EDITOR
	/** Internal helper to iterate all actor descriptors */
	static void ForEachActorDescriptor(
		UWorldPartition* WorldPartition,
		TSubclassOf<AActor> ClassFilter,
		TFunctionRef<bool(const FWorldPartitionActorDescInstance*)> Callback);
#endif
};
