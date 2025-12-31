#pragma once

#include "CoreMinimal.h"

/**
 * FWorldContextManager - Manages target world selection across editor, PIE, and game contexts.
 *
 * Unreal Engine can have multiple worlds active simultaneously:
 * - Editor world: The level being edited
 * - PIE worlds: One or more Play-In-Editor instances
 * - Game world: Standalone game instance
 * - Preview worlds: Asset previews, animation previews, etc.
 *
 * This singleton class provides a consistent way to get the "current" world for
 * AgentBridge operations, with support for explicit overrides when needed.
 *
 * Default Behavior:
 * - If PIE is active, returns the primary PIE world
 * - Otherwise returns the Editor world
 * - Explicit override takes precedence over heuristics
 *
 * World Types (EWorldType):
 * - None: Invalid
 * - Game: Standalone game
 * - Editor: Level editing (no gameplay)
 * - PIE: Play In Editor
 * - EditorPreview: Asset previews
 * - GamePreview: Game preview
 * - GameRPC: Networked game
 * - Inactive: Inactive streaming level
 *
 * Thread Safety:
 * - All methods should be called from the Game Thread
 * - World pointers should be validated before use
 *
 * @see AgentBridge_Handover.md for PIE handling details
 */
class AGENTBRIDGERUNTIME_API FWorldContextManager
{
public:
	/**
	 * Gets the singleton instance.
	 */
	static FWorldContextManager& Get();

	//~==============================================================================
	// Primary Interface
	//~==============================================================================

	/**
	 * Gets the current target world for AgentBridge operations.
	 *
	 * Resolution order:
	 * 1. Explicit override (if set)
	 * 2. Primary PIE world (if PIE active)
	 * 3. Editor world
	 *
	 * @return The resolved world, or nullptr if none available.
	 */
	UWorld* GetTargetWorld() const;

	/**
	 * Sets an explicit world override.
	 *
	 * Use this to force AgentBridge to operate on a specific world,
	 * e.g., a specific PIE instance in a multi-client setup.
	 *
	 * @param World		The world to use. Pass nullptr to clear.
	 */
	void SetTargetWorldOverride(UWorld* World);

	/**
	 * Clears any explicit world override.
	 *
	 * Returns to automatic world selection.
	 */
	void ClearTargetWorldOverride();

	/**
	 * Checks if an explicit override is active.
	 */
	bool HasWorldOverride() const;

	//~==============================================================================
	// Context Queries
	//~==============================================================================

	/**
	 * Checks if the current target is an Editor world (not playing).
	 *
	 * @return True if editing, false if playing.
	 */
	bool IsEditorWorld() const;

	/**
	 * Checks if the current target is a PIE world.
	 *
	 * @return True if Play-In-Editor is active.
	 */
	bool IsPIEWorld() const;

	/**
	 * Checks if the current target is a standalone Game world.
	 *
	 * @return True if running as standalone game.
	 */
	bool IsGameWorld() const;

	/**
	 * Checks if gameplay is currently active.
	 *
	 * This is true if the world is PIE or Game AND HasBegunPlay.
	 *
	 * @return True if gameplay is running.
	 */
	bool IsGameplayActive() const;

	//~==============================================================================
	// Multi-World Support
	//~==============================================================================

	/**
	 * Gets all active PIE worlds.
	 *
	 * In networked PIE with multiple clients, returns all instances.
	 *
	 * @return Array of PIE world pointers.
	 */
	TArray<UWorld*> GetAllPIEWorlds() const;

	/**
	 * Gets the number of active PIE instances.
	 *
	 * @return PIE instance count (0 if not in PIE).
	 */
	int32 GetPIEInstanceCount() const;

	/**
	 * Gets the Editor world (the level being edited).
	 *
	 * @return Editor world, or nullptr if not available.
	 */
	UWorld* GetEditorWorld() const;

	/**
	 * Gets all world contexts from the engine.
	 *
	 * Low-level access for advanced use cases.
	 *
	 * @return Reference to engine's world context array.
	 */
	const TIndirectArray<FWorldContext>& GetAllWorldContexts() const;

private:
	FWorldContextManager() = default;
	~FWorldContextManager() = default;

	// Non-copyable
	FWorldContextManager(const FWorldContextManager&) = delete;
	FWorldContextManager& operator=(const FWorldContextManager&) = delete;

	/**
	 * Internal world resolution without override.
	 */
	UWorld* ResolveWorld() const;

	/** Explicit world override. */
	TWeakObjectPtr<UWorld> WorldOverride;
};
