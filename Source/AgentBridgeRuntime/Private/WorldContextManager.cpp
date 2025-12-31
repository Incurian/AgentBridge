#include "WorldContextManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

FWorldContextManager& FWorldContextManager::Get()
{
	static FWorldContextManager Instance;
	return Instance;
}

//~==============================================================================
// Primary Interface
//~==============================================================================

UWorld* FWorldContextManager::GetTargetWorld() const
{
	// Check override first
	if (WorldOverride.IsValid())
	{
		return WorldOverride.Get();
	}

	return ResolveWorld();
}

void FWorldContextManager::SetTargetWorldOverride(UWorld* World)
{
	WorldOverride = World;
}

void FWorldContextManager::ClearTargetWorldOverride()
{
	WorldOverride.Reset();
}

bool FWorldContextManager::HasWorldOverride() const
{
	return WorldOverride.IsValid();
}

//~==============================================================================
// Context Queries
//~==============================================================================

bool FWorldContextManager::IsEditorWorld() const
{
	UWorld* World = GetTargetWorld();
	if (!World)
	{
		return false;
	}

	return World->WorldType == EWorldType::Editor;
}

bool FWorldContextManager::IsPIEWorld() const
{
	UWorld* World = GetTargetWorld();
	if (!World)
	{
		return false;
	}

	return World->WorldType == EWorldType::PIE;
}

bool FWorldContextManager::IsGameWorld() const
{
	UWorld* World = GetTargetWorld();
	if (!World)
	{
		return false;
	}

	return World->WorldType == EWorldType::Game;
}

bool FWorldContextManager::IsGameplayActive() const
{
	UWorld* World = GetTargetWorld();
	if (!World)
	{
		return false;
	}

	// Check if playing (PIE or Game) AND gameplay has begun
	bool bIsPlaying = World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game;
	return bIsPlaying && World->HasBegunPlay();
}

//~==============================================================================
// Multi-World Support
//~==============================================================================

TArray<UWorld*> FWorldContextManager::GetAllPIEWorlds() const
{
	TArray<UWorld*> Results;

	if (!GEngine)
	{
		return Results;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE && Context.World())
		{
			Results.Add(Context.World());
		}
	}

	return Results;
}

int32 FWorldContextManager::GetPIEInstanceCount() const
{
	return GetAllPIEWorlds().Num();
}

UWorld* FWorldContextManager::GetEditorWorld() const
{
	if (!GEngine)
	{
		return nullptr;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Editor && Context.World())
		{
			return Context.World();
		}
	}

	return nullptr;
}

const TIndirectArray<FWorldContext>& FWorldContextManager::GetAllWorldContexts() const
{
	static TIndirectArray<FWorldContext> EmptyArray;

	if (!GEngine)
	{
		return EmptyArray;
	}

	return GEngine->GetWorldContexts();
}

//~==============================================================================
// Private Implementation
//~==============================================================================

UWorld* FWorldContextManager::ResolveWorld() const
{
	if (!GEngine)
	{
		return nullptr;
	}

	const TIndirectArray<FWorldContext>& Contexts = GEngine->GetWorldContexts();

	// Priority 1: PIE world (prefer server/first instance)
	UWorld* PIEWorld = nullptr;
	int32 LowestPIEInstance = INT_MAX;

	for (const FWorldContext& Context : Contexts)
	{
		if (Context.WorldType == EWorldType::PIE && Context.World())
		{
			// Prefer the lowest PIE instance (typically the server or first client)
			if (Context.PIEInstance < LowestPIEInstance)
			{
				LowestPIEInstance = Context.PIEInstance;
				PIEWorld = Context.World();
			}
		}
	}

	if (PIEWorld)
	{
		return PIEWorld;
	}

	// Priority 2: Editor world
	for (const FWorldContext& Context : Contexts)
	{
		if (Context.WorldType == EWorldType::Editor && Context.World())
		{
			return Context.World();
		}
	}

	// Priority 3: Game world (standalone)
	for (const FWorldContext& Context : Contexts)
	{
		if (Context.WorldType == EWorldType::Game && Context.World())
		{
			return Context.World();
		}
	}

	// Last resort: any valid world
	for (const FWorldContext& Context : Contexts)
	{
		if (Context.World())
		{
			return Context.World();
		}
	}

	return nullptr;
}
