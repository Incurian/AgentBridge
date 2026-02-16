// Copyright 2025 AgentBridge. All Rights Reserved.

#include "WorldPartitionOps.h"
#include "WorldContextManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "LandscapeStreamingProxy.h"
#include "Landscape.h"
#include "LandscapeHeightfieldCollisionComponent.h"

#if WITH_EDITOR
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/WorldPartitionEditorHash.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "Editor.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogAgentBridgeWP, Log, All);

//~==============================================================================
// FStreamingActorReference
//~==============================================================================

FStreamingActorReference FStreamingActorReference::FromLoadedActor(AActor* Actor)
{
	FStreamingActorReference Ref;

	if (!Actor)
	{
		Ref.StreamingState = EActorStreamingState::Invalid;
		return Ref;
	}

	// Copy base FActorReference fields
	Ref.Guid = Actor->GetActorGuid().ToString();
	Ref.Path = Actor->GetPathName();
	Ref.Name = Actor->GetName();
	Ref.Label = Actor->GetActorLabel();
	Ref.ClassName = Actor->GetClass()->GetName();

	// Set streaming state
	Ref.StreamingState = EActorStreamingState::Loaded;

	// Get transform from live actor
	Ref.Transform = Actor->GetActorTransform();

	// Get bounds
	Ref.EditorBounds = Actor->GetComponentsBoundingBox(true, true);

#if WITH_EDITOR
	// Get data layers
	if (const UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(Actor->GetWorld()))
	{
		TArray<const UDataLayerInstance*> DataLayerInstances = DataLayerManager->GetDataLayerInstances(Actor->GetDataLayerInstanceNames());
		for (const UDataLayerInstance* Instance : DataLayerInstances)
		{
			if (Instance)
			{
				Ref.DataLayers.Add(Instance->GetDataLayerFName());
			}
		}
	}

	// Check if spatially loaded
	Ref.bIsSpatiallyLoaded = Actor->GetIsSpatiallyLoaded();
#endif

	return Ref;
}

#if WITH_EDITOR
FStreamingActorReference FStreamingActorReference::FromActorDescInstance(const FWorldPartitionActorDescInstance* ActorDescInstance)
{
	FStreamingActorReference Ref;

	if (!ActorDescInstance)
	{
		Ref.StreamingState = EActorStreamingState::Invalid;
		return Ref;
	}

	const FWorldPartitionActorDesc* ActorDesc = ActorDescInstance->GetActorDesc();
	if (!ActorDesc)
	{
		Ref.StreamingState = EActorStreamingState::Invalid;
		return Ref;
	}

	// Check if actor is loaded
	AActor* LoadedActor = ActorDescInstance->GetActor();

	if (LoadedActor)
	{
		// Actor is loaded - use the live data
		Ref = FromLoadedActor(LoadedActor);
		Ref.StreamingState = EActorStreamingState::Loaded;
	}
	else
	{
		// Actor is unloaded - use descriptor metadata
		Ref.Guid = ActorDesc->GetGuid().ToString();
		Ref.Path = ActorDesc->GetActorSoftPath().ToString();
		Ref.Name = ActorDesc->GetActorName().ToString();
		Ref.Label = ActorDesc->GetActorLabel().ToString();
		Ref.ClassName = ActorDesc->GetActorNativeClass()->GetName();

		Ref.StreamingState = EActorStreamingState::Unloaded;
		Ref.EditorBounds = ActorDesc->GetEditorBounds();
		Ref.bIsSpatiallyLoaded = ActorDesc->GetIsSpatiallyLoaded();

		// Estimate transform from bounds center (best we can do when unloaded)
		if (Ref.EditorBounds.IsValid)
		{
			Ref.Transform.SetLocation(Ref.EditorBounds.GetCenter());
		}

		// Get data layers from descriptor instance (not ActorDesc - that's deprecated)
		for (const FName& DataLayerName : ActorDescInstance->GetDataLayerInstanceNames())
		{
			Ref.DataLayers.Add(DataLayerName);
		}
	}

	return Ref;
}
#endif

//~==============================================================================
// World Partition State
//~==============================================================================

bool FWorldPartitionOps::IsWorldPartitioned(UWorld* World)
{
	return GetWorldPartition(World) != nullptr;
}

UWorldPartition* FWorldPartitionOps::GetWorldPartition(UWorld* World)
{
	if (!World)
	{
		World = FWorldContextManager::Get().GetTargetWorld();
	}

	if (!World)
	{
		return nullptr;
	}

#if WITH_EDITOR
	return World->GetWorldPartition();
#else
	return nullptr;
#endif
}

//~==============================================================================
// Streaming-Aware Actor Queries
//~==============================================================================

TArray<FStreamingActorReference> FWorldPartitionOps::QueryAllActors(
	const FWorldPartitionQueryParams& Params,
	UWorld* World)
{
	TArray<FStreamingActorReference> Results;

	if (!World)
	{
		World = FWorldContextManager::Get().GetTargetWorld();
	}

	if (!World)
	{
		return Results;
	}

	int32 Count = 0;

#if WITH_EDITOR
	UWorldPartition* WorldPartition = World->GetWorldPartition();

	if (WorldPartition && Params.bIncludeUnloaded)
	{
		// Use World Partition to enumerate ALL actors (loaded + unloaded)
		ForEachActorDescriptor(WorldPartition, Params.ClassFilter, [&](const FWorldPartitionActorDescInstance* ActorDescInstance)
		{
			if (Count >= Params.Limit)
			{
				return false; // Stop iteration
			}

			const FWorldPartitionActorDesc* ActorDesc = ActorDescInstance->GetActorDesc();
			if (!ActorDesc)
			{
				return true; // Continue
			}

			// Check if loaded/unloaded filter matches
			AActor* LoadedActor = ActorDescInstance->GetActor();
			if (LoadedActor && !Params.bIncludeLoaded)
			{
				return true; // Skip loaded actors
			}
			if (!LoadedActor && !Params.bIncludeUnloaded)
			{
				return true; // Skip unloaded actors
			}

			// Name pattern filter
			if (!Params.NamePattern.IsEmpty())
			{
				FString ActorName = LoadedActor ? LoadedActor->GetName() : ActorDesc->GetActorName().ToString();
				FString ActorLabel = LoadedActor ? LoadedActor->GetActorLabel() : ActorDesc->GetActorLabel().ToString();

				bool bMatches = ActorName.Contains(Params.NamePattern) || ActorLabel.Contains(Params.NamePattern);
				if (!bMatches)
				{
					return true; // Continue
				}
			}

			// Bounds filter
			if (Params.BoundsFilter.IsSet())
			{
				FBox ActorBounds = LoadedActor
					? LoadedActor->GetComponentsBoundingBox(true, true)
					: ActorDesc->GetEditorBounds();

				if (!Params.BoundsFilter.GetValue().Intersect(ActorBounds))
				{
					return true; // Continue
				}
			}

			// Data layer filter
			if (!Params.DataLayerFilter.IsNone())
			{
				bool bInDataLayer = false;
				if (LoadedActor)
				{
					bInDataLayer = LoadedActor->GetDataLayerInstanceNames().Contains(Params.DataLayerFilter);
				}
				else
				{
					bInDataLayer = ActorDescInstance->GetDataLayerInstanceNames().Contains(Params.DataLayerFilter);
				}

				if (!bInDataLayer)
				{
					return true; // Continue
				}
			}

			Results.Add(FStreamingActorReference::FromActorDescInstance(ActorDescInstance));
			Count++;

			return true; // Continue
		});
	}
	else
#endif
	{
		// Fall back to TActorIterator (loaded actors only)
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

			// Name pattern filter
			if (!Params.NamePattern.IsEmpty())
			{
				bool bMatches = Actor->GetName().Contains(Params.NamePattern) ||
					Actor->GetActorLabel().Contains(Params.NamePattern);
				if (!bMatches)
				{
					continue;
				}
			}

			// Bounds filter
			if (Params.BoundsFilter.IsSet())
			{
				FBox ActorBounds = Actor->GetComponentsBoundingBox(true, true);
				if (!Params.BoundsFilter.GetValue().Intersect(ActorBounds))
				{
					continue;
				}
			}

			Results.Add(FStreamingActorReference::FromLoadedActor(Actor));
			Count++;
		}
	}

	return Results;
}

EActorStreamingState FWorldPartitionOps::GetActorStreamingState(const FGuid& ActorGuid, UWorld* World)
{
	if (!ActorGuid.IsValid())
	{
		return EActorStreamingState::Invalid;
	}

	if (!World)
	{
		World = FWorldContextManager::Get().GetTargetWorld();
	}

	if (!World)
	{
		return EActorStreamingState::NotApplicable;
	}

#if WITH_EDITOR
	UWorldPartition* WorldPartition = World->GetWorldPartition();
	if (WorldPartition)
	{
		const FWorldPartitionActorDescInstance* ActorDescInstance = WorldPartition->GetActorDescInstance(ActorGuid);
		if (!ActorDescInstance)
		{
			return EActorStreamingState::Invalid;
		}

		if (ActorDescInstance->GetActor())
		{
			return EActorStreamingState::Loaded;
		}
		else
		{
			return EActorStreamingState::Unloaded;
		}
	}
#endif

	// Non-WP world or runtime: check if actor exists in world
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorGuid() == ActorGuid)
		{
			return EActorStreamingState::Loaded;
		}
	}

	return EActorStreamingState::NotApplicable;
}

FStreamingActorReference FWorldPartitionOps::FindActorByGuidEx(const FGuid& Guid, UWorld* World)
{
	FStreamingActorReference Ref;

	if (!Guid.IsValid())
	{
		Ref.StreamingState = EActorStreamingState::Invalid;
		return Ref;
	}

	if (!World)
	{
		World = FWorldContextManager::Get().GetTargetWorld();
	}

	if (!World)
	{
		Ref.StreamingState = EActorStreamingState::NotApplicable;
		return Ref;
	}

#if WITH_EDITOR
	UWorldPartition* WorldPartition = World->GetWorldPartition();
	if (WorldPartition)
	{
		const FWorldPartitionActorDescInstance* ActorDescInstance = WorldPartition->GetActorDescInstance(Guid);
		if (ActorDescInstance)
		{
			return FStreamingActorReference::FromActorDescInstance(ActorDescInstance);
		}
	}
#endif

	// Fall back: look for loaded actor
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorGuid() == Guid)
		{
			return FStreamingActorReference::FromLoadedActor(*It);
		}
	}

	Ref.StreamingState = EActorStreamingState::Invalid;
	return Ref;
}

//~==============================================================================
// Landscape Streaming
//~==============================================================================

TArray<FStreamingActorReference> FWorldPartitionOps::QueryLandscapeProxies(UWorld* World, bool bIncludeUnloaded)
{
	FWorldPartitionQueryParams Params;
	Params.ClassFilter = ALandscapeProxy::StaticClass();
	Params.bIncludeLoaded = true;
	Params.bIncludeUnloaded = bIncludeUnloaded;
	Params.Limit = 10000; // Landscapes can have many streaming proxies

	return QueryAllActors(Params, World);
}

ALandscapeProxy* FWorldPartitionOps::GetMainLandscape(UWorld* World)
{
	if (!World)
	{
		World = FWorldContextManager::Get().GetTargetWorld();
	}

	if (!World)
	{
		return nullptr;
	}

	// Look for the main ALandscape actor (not streaming proxies)
	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		return *It;
	}

	// Fall back to any loaded landscape proxy
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		return *It;
	}

	return nullptr;
}

FLandscapeBounds FWorldPartitionOps::GetLandscapeBounds(UWorld* World)
{
	FLandscapeBounds Result;

	if (!World)
	{
		World = FWorldContextManager::Get().GetTargetWorld();
	}

	if (!World)
	{
		return Result;
	}

	// Get the main landscape for scale reference
	ALandscapeProxy* MainLandscape = GetMainLandscape(World);
	if (!MainLandscape)
	{
		UE_LOG(LogAgentBridgeWP, Warning, TEXT("GetLandscapeBounds: No landscape found in world"));
		return Result;
	}

	Result.LandscapeName = MainLandscape->GetName();
	FVector LandscapeScale = MainLandscape->GetActorScale3D();

	// Initialize bounds to invalid state for min/max tracking
	FVector MinBounds(MAX_FLT, MAX_FLT, MAX_FLT);
	FVector MaxBounds(-MAX_FLT, -MAX_FLT, -MAX_FLT);
	int32 ProxyCount = 0;

	// Iterate all landscape proxies (including streaming proxies)
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		ALandscapeProxy* Proxy = *It;
		if (!Proxy)
		{
			continue;
		}

		ProxyCount++;
		FVector ProxyLocation = Proxy->GetActorLocation();

		// Get the collision component which has CachedLocalBox
		ULandscapeHeightfieldCollisionComponent* CollisionComp = Proxy->CollisionComponents.Num() > 0
			? Proxy->CollisionComponents[0]
			: nullptr;

		// Use the proxy's full bounding box for accurate bounds
		// Note: CachedLocalBox on collision components doesn't account for the full
		// landscape segment extent - it can be off by half a segment (the collision
		// bounds are centered differently than the visual geometry).
		FBox ProxyBounds = Proxy->GetComponentsBoundingBox(false, true);
		if (ProxyBounds.IsValid)
		{
			MinBounds = MinBounds.ComponentMin(ProxyBounds.Min);
			MaxBounds = MaxBounds.ComponentMax(ProxyBounds.Max);
		}
		else if (CollisionComp)
		{
			// Fallback: use collision bounds if bounding box not available
			FBox LocalBox = CollisionComp->CachedLocalBox;
			FVector WorldMin = ProxyLocation + LocalBox.Min * LandscapeScale;
			FVector WorldMax = ProxyLocation + LocalBox.Max * LandscapeScale;
			MinBounds = MinBounds.ComponentMin(WorldMin);
			MaxBounds = MaxBounds.ComponentMax(WorldMax);
		}
	}

	// Check if we found valid bounds
	if (ProxyCount == 0 || MinBounds.X > MaxBounds.X)
	{
		UE_LOG(LogAgentBridgeWP, Warning, TEXT("GetLandscapeBounds: No valid landscape proxies found"));
		return Result;
	}

	Result.bValid = true;
	Result.Min = MinBounds;
	Result.Max = MaxBounds;
	Result.Center = (MinBounds + MaxBounds) * 0.5;
	Result.Extent = (MaxBounds - MinBounds) * 0.5;

	// BoxComponent default half-extent is (100, 100, 100).
	// Scale factor: extent / 100.  Z gets +10000 for elevation headroom.
	Result.BiomeVolumeScale = FVector(
		Result.Extent.X / 100.0,
		Result.Extent.Y / 100.0,
		(Result.Extent.Z + 10000.0) / 100.0);

	Result.ProxyCount = ProxyCount;

	UE_LOG(LogAgentBridgeWP, Log, TEXT("GetLandscapeBounds: %s with %d proxies, bounds [%.0f, %.0f, %.0f] to [%.0f, %.0f, %.0f]"),
		*Result.LandscapeName, ProxyCount,
		MinBounds.X, MinBounds.Y, MinBounds.Z,
		MaxBounds.X, MaxBounds.Y, MaxBounds.Z);

	return Result;
}

//~==============================================================================
// Streaming Cell Operations (Editor Only)
//~==============================================================================

#if WITH_EDITOR
AActor* FWorldPartitionOps::LoadActor(const FGuid& ActorGuid, UWorld* World)
{
	if (!World)
	{
		World = FWorldContextManager::Get().GetTargetWorld();
	}

	UWorldPartition* WorldPartition = World ? World->GetWorldPartition() : nullptr;
	if (!WorldPartition)
	{
		return nullptr;
	}

	const FWorldPartitionActorDescInstance* ActorDescInstance = WorldPartition->GetActorDescInstance(ActorGuid);
	if (!ActorDescInstance)
	{
		UE_LOG(LogAgentBridgeWP, Warning, TEXT("LoadActor: Actor GUID %s not found in World Partition"), *ActorGuid.ToString());
		return nullptr;
	}

	// If already loaded, just return it
	if (AActor* LoadedActor = ActorDescInstance->GetActor())
	{
		return LoadedActor;
	}

	// Create a handle to load the actor
	FWorldPartitionHandle ActorHandle(WorldPartition, ActorGuid);
	if (!ActorHandle.IsValid())
	{
		UE_LOG(LogAgentBridgeWP, Warning, TEXT("LoadActor: Failed to create handle for GUID %s"), *ActorGuid.ToString());
		return nullptr;
	}

	// Pin the reference to keep it loaded
	FWorldPartitionHandlePinRefScope PinRefScope(ActorHandle);

	// The actor should now be loaded
	return ActorHandle->GetActor();
}

int32 FWorldPartitionOps::LoadRegion(const FBox& Box, UWorld* World)
{
	if (!World)
	{
		World = FWorldContextManager::Get().GetTargetWorld();
	}

	UWorldPartition* WorldPartition = World ? World->GetWorldPartition() : nullptr;
	if (!WorldPartition)
	{
		return 0;
	}

	int32 LoadedCount = 0;

	FWorldPartitionHelpers::ForEachIntersectingActorDescInstance(WorldPartition, Box, AActor::StaticClass(),
		[&](const FWorldPartitionActorDescInstance* ActorDescInstance)
		{
			if (!ActorDescInstance->GetActor())
			{
				// Actor is unloaded, try to load it
				FWorldPartitionHandle ActorHandle(WorldPartition, ActorDescInstance->GetGuid());
				if (ActorHandle.IsValid())
				{
					FWorldPartitionHandlePinRefScope PinRefScope(ActorHandle);
					if (ActorHandle->GetActor())
					{
						LoadedCount++;
					}
				}
			}
			return true;
		});

	return LoadedCount;
}

void FWorldPartitionOps::UnloadRegion(const FBox& Box, UWorld* World)
{
	if (!World)
	{
		World = FWorldContextManager::Get().GetTargetWorld();
	}

	UWorldPartition* WorldPartition = World ? World->GetWorldPartition() : nullptr;
	if (!WorldPartition)
	{
		return;
	}

	// Use the same pattern as Tempo's PointCloudWorldPartitionHelpers
	if (WorldPartition->EditorHash)
	{
		WorldPartition->EditorHash->ForEachIntersectingActor(Box,
			[WorldPartition](FWorldPartitionActorDescInstance* ActorDescInstance)
			{
				// Creating and immediately destroying a handle will release the reference
				FWorldPartitionHandle ActorHandle(WorldPartition, ActorDescInstance->GetGuid());
				FWorldPartitionHandlePinRefScope ActorPinRefScope(ActorHandle);
			});
	}

	// Request garbage collection to actually unload
	CollectGarbage(RF_NoFlags, true);
}

bool FWorldPartitionOps::DeleteActorWP(const FGuid& ActorGuid, UWorld* World)
{
	TArray<FGuid> Guids;
	Guids.Add(ActorGuid);
	return DeleteActorsWP(Guids, World) > 0;
}

int32 FWorldPartitionOps::DeleteActorsWP(const TArray<FGuid>& ActorGuids, UWorld* World)
{
	if (!World)
	{
		World = FWorldContextManager::Get().GetTargetWorld();
	}

	UWorldPartition* WorldPartition = World ? World->GetWorldPartition() : nullptr;
	int32 DeletedCount = 0;

	// Remove potential references to to-be deleted objects from the global selection sets
	if (GIsEditor && GEditor)
	{
		GEditor->ResetAllSelectionSets();
	}

	if (WorldPartition)
	{
		TSet<UPackage*> PackagesToCleanup;

		for (const FGuid& ActorGuid : ActorGuids)
		{
			const FWorldPartitionActorDescInstance* ActorDescInstance = WorldPartition->GetActorDescInstance(ActorGuid);
			if (!ActorDescInstance)
			{
				continue;
			}

			// If actor is loaded, destroy it and mark package for cleanup
			if (AActor* LoadedActor = ActorDescInstance->GetActor())
			{
				if (UPackage* ExternalPackage = LoadedActor->GetExternalPackage())
				{
					PackagesToCleanup.Add(ExternalPackage);
				}
				World->DestroyActor(LoadedActor);
				DeletedCount++;
			}
			else
			{
				// Actor is unloaded - remove from World Partition directly
				WorldPartition->RemoveActor(ActorGuid);
				DeletedCount++;
			}
		}

		// Clean up packages from deleted loaded actors
		if (PackagesToCleanup.Num() > 0)
		{
			// Note: This requires ObjectTools, similar to Tempo's implementation
			// For now, we just rely on the normal WP update mechanism
			UE_LOG(LogAgentBridgeWP, Log, TEXT("Deleted %d actors, %d packages to cleanup"), DeletedCount, PackagesToCleanup.Num());
		}
	}
	else
	{
		// Non-WP world: use standard destruction
		for (const FGuid& ActorGuid : ActorGuids)
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if ((*It)->GetActorGuid() == ActorGuid)
				{
					World->DestroyActor(*It);
					DeletedCount++;
					break;
				}
			}
		}
	}

	return DeletedCount;
}
#endif

//~==============================================================================
// Data Layer Operations
//~==============================================================================

TArray<FName> FWorldPartitionOps::GetDataLayers(UWorld* World)
{
	TArray<FName> Result;

	if (!World)
	{
		World = FWorldContextManager::Get().GetTargetWorld();
	}

#if WITH_EDITOR
	if (World)
	{
		if (const UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(World))
		{
			DataLayerManager->ForEachDataLayerInstance([&Result](UDataLayerInstance* DataLayerInstance)
			{
				if (DataLayerInstance)
				{
					Result.Add(DataLayerInstance->GetDataLayerFName());
				}
				return true;
			});
		}
	}
#endif

	return Result;
}

TArray<FStreamingActorReference> FWorldPartitionOps::GetActorsInDataLayer(
	FName DataLayerName,
	bool bIncludeUnloaded,
	UWorld* World)
{
	FWorldPartitionQueryParams Params;
	Params.DataLayerFilter = DataLayerName;
	Params.bIncludeLoaded = true;
	Params.bIncludeUnloaded = bIncludeUnloaded;

	return QueryAllActors(Params, World);
}

#if WITH_EDITOR
void FWorldPartitionOps::ForEachActorDescriptor(
	UWorldPartition* WorldPartition,
	TSubclassOf<AActor> ClassFilter,
	TFunctionRef<bool(const FWorldPartitionActorDescInstance*)> Callback)
{
	if (!WorldPartition)
	{
		return;
	}

	if (ClassFilter)
	{
		FWorldPartitionHelpers::ForEachActorDescInstance(WorldPartition, ClassFilter, Callback);
	}
	else
	{
		FWorldPartitionHelpers::ForEachActorDescInstance<AActor>(WorldPartition, Callback);
	}
}
#endif
