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

FString FWorldContextManager::GetWorldTypeString() const
{
	UWorld* World = GetTargetWorld();
	if (!World)
	{
		return TEXT("None");
	}

	switch (World->WorldType)
	{
	case EWorldType::Editor:
		return TEXT("Editor");
	case EWorldType::PIE:
		return TEXT("PIE");
	case EWorldType::Game:
		return TEXT("Game");
	case EWorldType::EditorPreview:
		return TEXT("EditorPreview");
	case EWorldType::GamePreview:
		return TEXT("GamePreview");
	case EWorldType::GameRPC:
		return TEXT("GameRPC");
	case EWorldType::Inactive:
		return TEXT("Inactive");
	default:
		return TEXT("Unknown");
	}
}

FWorldContextCapabilities FWorldContextManager::GetCapabilities() const
{
	FWorldContextCapabilities Caps;

	UWorld* World = GetTargetWorld();
	if (!World)
	{
		Caps.WorldType = TEXT("None");
		Caps.WorldName = TEXT("No world available");
		return Caps;
	}

	// Basic world info
	Caps.WorldType = GetWorldTypeString();
	Caps.WorldName = World->GetMapName();
	Caps.bIsGameplayActive = IsGameplayActive();

	// PIE instance info
	if (World->WorldType == EWorldType::PIE && GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World() == World)
			{
				Caps.PIEInstance = Context.PIEInstance;
				break;
			}
		}
	}

	// Core reflection - always available
	Caps.bCanIterateProperties = true;
	Caps.bCanInvokeFunctions = true;
	Caps.bCanSpawnActors = true;
	Caps.bCanDestroyActors = true;
	Caps.bCanModifyTransforms = true;
	Caps.bCanModifyProperties = true;

	// Editor-only features are determined by compile-time and runtime context
#if WITH_EDITOR
	// In editor builds, labels/folders are available
	Caps.bCanSetActorLabel = true;
	Caps.bCanSetActorFolder = true;
	Caps.LabelUnavailableReason.Empty();
	Caps.FolderUnavailableReason.Empty();

	// Transactions only work in the editor world, not PIE
	Caps.bCanUseTransactions = (World->WorldType == EWorldType::Editor);
	if (!Caps.bCanUseTransactions)
	{
		Caps.TransactionUnavailableReason = TEXT("Undo/redo only available when editing (not in PIE or Game)");
	}
	else
	{
		Caps.TransactionUnavailableReason.Empty();
	}

	// Editor world accessible check
	Caps.bCanAccessEditorWorld = (GetEditorWorld() != nullptr);
#else
	// Packaged builds - editor features not available
	Caps.bCanSetActorLabel = false;
	Caps.bCanSetActorFolder = false;
	Caps.bCanUseTransactions = false;
	Caps.bCanAccessEditorWorld = false;
	Caps.LabelUnavailableReason = TEXT("Actor labels are editor-only and not available in packaged builds");
	Caps.FolderUnavailableReason = TEXT("Actor folders are editor-only and not available in packaged builds");
	Caps.TransactionUnavailableReason = TEXT("Undo/redo is editor-only and not available in packaged builds");
#endif

	// Property metadata - available in editor builds, stripped in shipping
#if WITH_EDITORONLY_DATA
	Caps.bHasPropertyMetadata = true;
	Caps.MetadataUnavailableReason.Empty();
#else
	Caps.bHasPropertyMetadata = false;
	Caps.MetadataUnavailableReason = TEXT("Property metadata (Category, Description) is stripped in shipping builds");
#endif

	return Caps;
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
