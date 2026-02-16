# AgentBridgeRuntime Module

> World context management, actor operations, and property path resolution.

## Purpose

This module provides the abstraction layer between raw reflection and high-level commands:
- `FWorldContextManager` - Multi-world support (Editor, PIE, Game)
- `FActorOperations` - Query, spawn, delete, transform actors
- `FAgentPropertyPath` - Nested property resolution ("Mesh.Materials[0].Color")
- `FWorldPartitionOps` - World Partition streaming support

## Key Files

| File | Purpose |
|------|---------|
| `WorldContextManager.h/.cpp` | Target world selection, PIE handling |
| `ActorOperations.h/.cpp` | Actor CRUD operations |
| `AgentPropertyPath.h/.cpp` | Property path parsing and resolution |
| `WorldPartitionOps.h/.cpp` | Streaming actor queries, landscape bounds |
| `DebugCommands.cpp` | Console commands for testing |

## World Context

### Context Detection

**CRITICAL:** `GIsEditor` remains TRUE during PIE. Use `World->WorldType`:

```cpp
switch (World->WorldType)
{
    case EWorldType::Editor:    // Level editing
    case EWorldType::PIE:       // Play In Editor
    case EWorldType::Game:      // Standalone/packaged
}
```

### Capabilities System

Query what's available in current context:

```cpp
FWorldContextCapabilities Caps = FWorldContextManager::Get().GetCapabilities();

if (Caps.bCanUseTransactions)
{
    // Safe to use undo/redo (Editor only)
}
```

| Feature | Editor | PIE | Packaged |
|---------|--------|-----|----------|
| Property iteration | Yes | Yes | Yes |
| Spawn/Destroy | Yes | Yes | Yes |
| SetActorLabel | Yes | Yes | No |
| Transactions (Undo) | Yes | No | No |
| Property metadata | Yes | Yes | No |

## Property Paths

Resolves paths like `"Mesh.RelativeLocation.X"` or `"Items[3].Name"`:

```cpp
FAgentPropertyValue Value;
FAgentPropertyPath::ReadPropertyPath(Actor, "LightComponent.Intensity", Value);

FAgentPropertyPath::WritePropertyPath(Actor, "LightComponent.Intensity", NewValue);
```

### Supported Syntax

- `Property` - Direct property
- `Component.Property` - Component property
- `Array[0]` - Array index
- `Map["key"]` - Map key
- `Struct.Field` - Nested struct

## World Partition Support

For streaming-aware queries on large worlds:

```cpp
// Check if world uses WP
bool bPartitioned = FWorldPartitionOps::IsWorldPartitioned(World);

// Query including unloaded actors
TArray<FStreamingActorReference> Actors;
FWorldPartitionOps::QueryAllActors(World, Actors, ClassFilter, NamePattern);

// Get complete landscape bounds
FLandscapeBoundsInfo Bounds;
FWorldPartitionOps::GetLandscapeBounds(World, Bounds);
```

## Console Commands

| Command | Description |
|---------|-------------|
| `AgentBridge.ListWorlds` | List all world contexts |
| `AgentBridge.Capabilities` | Show current context capabilities |
| `AgentBridge.QueryActors <class> [limit]` | Query actors |
| `AgentBridge.IsPartitioned` | Check if world uses WP |
| `AgentBridge.QueryAllActors [pattern] [limit]` | Query including unloaded |
| `AgentBridge.QueryLandscape` | List landscape proxies |
| `AgentBridge.GetLandscapeBounds` | Get full landscape bounds |

## Thread Safety

Most UObject operations require game thread:

```cpp
TWeakObjectPtr<UObject> WeakRef(MyObject);

AsyncTask(ENamedThreads::AnyBackgroundThread, [WeakRef]()
{
    // Do non-UObject work...

    // Bounce to game thread for UObject access
    AsyncTask(ENamedThreads::GameThread, [WeakRef]()
    {
        if (UObject* Obj = WeakRef.Get())
        {
            // Safe to access UObject here
        }
    });
});
```

## Known Issues

### Landscape Bounds Calculation

Fixed in Session 18: Now uses `GetComponentsBoundingBox()` instead of `CachedLocalBox` for accurate landscape extent calculation.

## Todos

- [x] `get_landscape_bounds` - Returns world-space min/max, center, half-extents
- [x] World Partition streaming-aware queries
- [x] Data layer support

## Stretch Goals

| Feature | Effort | Notes |
|---------|--------|-------|
| Landscape sculpting | High | Import/export heightmaps, brush strokes |
| Force-load streaming regions | Medium | `LoadActor()` / `LoadRegion()` for WP |

## Dependencies

```csharp
// AgentBridgeRuntime.Build.cs
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "AgentBridgeCore",
});

PrivateDependencyModuleNames.AddRange(new string[] {
    "UnrealEd",  // For editor operations (WITH_EDITOR)
});
```
