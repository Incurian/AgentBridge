# AgentBridgeRuntime Plugin

> Standalone UE plugin providing world context management, actor operations, target
> resolution, and World Partition support for AgentBridge.

AgentBridgeRuntime is a standalone Unreal Engine plugin with its own `.uplugin` file.
It depends on AgentBridgeCore and is depended upon by AgentBridgeScripting. There is
no wrapper plugin - UBT discovers all four AgentBridge plugins independently.

---

## Plugin Structure

```
AgentBridgeRuntime/
|-- AgentBridgeRuntime.uplugin    (depends on AgentBridgeCore)
|-- CLAUDE.md                     (this file)
|-- README.md                     (beginner-friendly docs)
|-- Source/AgentBridgeRuntime/
    |-- AgentBridgeRuntime.Build.cs
    |-- Public/
    |   |-- ActorOperations.h         Actor CRUD, property get/set
    |   |-- AgentBridgeDebug.h        Console commands
    |   |-- AgentBridgeRuntime.h      Module interface
    |   |-- TargetResolution.h        "Actor->Component" string resolution
    |   |-- WorldContextManager.h     Multi-world targeting
    |   |-- WorldPartitionOps.h       Streaming queries, landscape bounds
    |-- Private/
        |-- (matching .cpp files)
```

---

## Purpose

This plugin provides the abstraction layer between raw reflection (Core) and high-level
commands (Scripting):

| Class | Responsibility |
|-------|---------------|
| `FWorldContextManager` | Multi-world support (Editor, PIE, Game) |
| `FActorOperations` | Query, spawn, delete, transform, property access on actors |
| `FWorldPartitionOps` | World Partition streaming support, landscape bounds |
| `AgentBridge::TargetResolution` | Resolve "Actor->Component" strings to UE objects |
| `FAgentBridgeDebug` | Console commands for debugging and testing |

**Note:** `FAgentPropertyPath` (property path resolution) is defined in AgentBridgeCore.
Runtime uses it heavily via `FActorOperations::GetActorProperties` / `SetActorProperties`.

---

## Key Files

| File | Purpose |
|------|---------|
| `WorldContextManager.h/.cpp` | Target world selection, PIE handling, capabilities |
| `ActorOperations.h/.cpp` | Actor CRUD operations, property batch access |
| `WorldPartitionOps.h/.cpp` | Streaming actor queries, landscape bounds, data layers |
| `TargetResolution.h/.cpp` | "Actor->Component" target string parsing and resolution |
| `AgentBridgeDebug.h/.cpp` | Console commands for testing (30+ commands) |
| `AgentBridgeRuntime.h/.cpp` | Module startup/shutdown (registers debug commands) |

---

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

### World Resolution Order

`FWorldContextManager::GetTargetWorld()` resolves in this order:
1. Explicit override (if set via `SetTargetWorldOverride`)
2. Primary PIE world (if any PIE world exists)
3. Editor world (fallback)

### Capabilities System

Query what operations are available in current context:

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
| Modify transforms | Yes | Yes | Yes |
| Modify properties | Yes | Yes | Yes |
| SetActorLabel | Yes | Yes | No |
| SetActorFolder | Yes | Yes | No |
| Transactions (Undo) | Yes | No | No |
| Property metadata | Yes | Yes | No |
| WP metadata queries | Yes | Limited | No |

Capabilities struct also includes `*UnavailableReason` strings explaining why a feature
is disabled, useful for returning informative error messages to agents.

---

## Actor Operations

### FActorReference

Stable reference to an actor with multiple identifiers:
- `Guid` - Most stable, survives rename/relevel
- `Path` - Full object path, unique but changes on rename
- `Name` - `GetName()`, unique within level but internal
- `Label` - Editor display name, human-readable but may not be unique
- `ClassName` - Type information

Resolution tries identifiers in order: GUID, Path, Name, Label.

### FActorQueryParams

| Field | Type | Description |
|-------|------|-------------|
| `ClassFilter` | `UClass*` | Filter by class (nullptr = all actors) |
| `NamePattern` | `FString` | Substring match on `GetName()` |
| `LabelPattern` | `FString` | Substring match on `GetActorLabel()` |
| `Tag` | `FString` | Filter by actor tag |
| `BoundsFilter` | `TOptional<FBox>` | Spatial filter |
| `Limit` | `int32` | Max results (default 1000) |
| `bIncludeHidden` | `bool` | Include hidden actors (default false) |

### FActorSpawnParams

| Field | Type | Description |
|-------|------|-------------|
| `ClassPath` | `FString` | Class path or name to spawn |
| `Transform` | `FTransform` | Spawn location/rotation/scale |
| `ActorLabel` | `FString` | Editor display label |
| `FolderPath` | `FString` | World Outliner folder path |
| `InitialProperties` | `TMap<FString, FAgentPropertyValue>` | Properties to set after spawn |
| `CollisionHandling` | `ESpawnActorCollisionHandlingMethod` | How to handle spawn collisions |

---

## Target Resolution

The `AgentBridge::TargetResolution` namespace provides unified target string parsing.

### Syntax

```
"MyActor"                   -> Actor only
"MyActor->LightComponent0" -> Actor + specific component
"BP_Door_5->DoorMesh"       -> Blueprint actor + component
```

Separator is `->` (defined as `TARGET_SEPARATOR`).

### Resolution Flow

1. `Parse(Target)` splits string into `FTargetInfo` (ActorPart + ComponentPart)
2. `Resolve(World, Target)` calls Parse, then:
   - Resolves actor via `FActorOperations::FindActorByName()`
   - If component specified, resolves via `FindComponent()` (exact, case-insensitive, partial)
3. Returns `FResolvedTarget` with actor, component, and error info

### Component Resolution

`FindComponent()` searches in order:
1. Exact name match
2. Case-insensitive match
3. Partial match for BP components (e.g., "LightComponent" matches "LightComponent0")

`FindAnyComponent()` variant finds non-scene components too (e.g., `UActorComponent`
subclasses that are not `USceneComponent`).

---

## Property Paths

Property paths are resolved by `FAgentPropertyPath` in AgentBridgeCore. Runtime exposes
them through `FActorOperations`:

```cpp
// Read
TMap<FString, FAgentPropertyValue> Props =
    FActorOperations::GetActorProperties(Actor, {"LightComponent0.Intensity"});

// Write
TMap<FString, FAgentPropertyValue> NewProps;
NewProps.Add("LightComponent0.Intensity", FAgentPropertyValue::FromFloat(5000.0f));
FActorOperations::SetActorProperties(Actor, NewProps);
```

### Supported Syntax

| Syntax | Example | Description |
|--------|---------|-------------|
| `Property` | `Mobility` | Direct property on actor |
| `Component.Property` | `LightComponent0.Intensity` | Property on a component |
| `Struct.Field` | `RelativeLocation.X` | Nested struct field |
| `Array[N]` | `Items[0]` | Array element by index |
| `Map["key"]` | `Tags["env"]` | Map element by key |
| Combined | `Mesh.Materials[0].Color.R` | Deep nesting |

---

## World Partition Support

### Checking WP Status

```cpp
bool bPartitioned = FWorldPartitionOps::IsWorldPartitioned(World);
```

### Streaming-Aware Queries

```cpp
FWorldPartitionQueryParams Params;
Params.bIncludeUnloaded = true;
Params.ClassFilter = AStaticMeshActor::StaticClass();
Params.NamePattern = "Tree";

TArray<FStreamingActorReference> Actors =
    FWorldPartitionOps::QueryAllActors(Params, World);
```

`FStreamingActorReference` extends `FActorReference` with:
- `StreamingState` - Loaded, Unloaded, or Invalid
- `StreamingCellName` - Which streaming cell the actor is in
- `EditorBounds` - Bounding box (available even for unloaded actors)
- `DataLayers` - Data layer membership
- `bIsSpatiallyLoaded` - Whether actor uses spatial streaming (vs always loaded)
- `Transform` - From loaded actor or estimated from bounds center

### Landscape Bounds

```cpp
FLandscapeBounds Bounds = FWorldPartitionOps::GetLandscapeBounds(World);
if (Bounds.bValid)
{
    // Bounds.Min, Bounds.Max - world-space corners
    // Bounds.Center - center point
    // Bounds.Extent - half-extents
    // Bounds.BiomeVolumeScale - scale factor for 100-unit BoxComponent
    // Bounds.ProxyCount - number of proxies sampled
}
```

Uses `GetComponentsBoundingBox()` on landscape proxies for accurate Z bounds that account
for terrain elevation.

### Data Layers

```cpp
TArray<FName> Layers = FWorldPartitionOps::GetDataLayers(World);

TArray<FStreamingActorReference> LayerActors =
    FWorldPartitionOps::GetActorsInDataLayer(FName("Foliage"), true, World);
```

### Editor-Only Streaming Operations

These require `WITH_EDITOR`:

```cpp
// Load a specific actor
AActor* Actor = FWorldPartitionOps::LoadActor(ActorGuid, World);

// Load all actors in a region
int32 Count = FWorldPartitionOps::LoadRegion(Box, World);

// Unload a region
FWorldPartitionOps::UnloadRegion(Box, World);
```

---

## Console Commands

All registered in `FAgentBridgeDebug::RegisterCommands()`, output to `LogAgentBridge`.

| Command | Args | Description |
|---------|------|-------------|
| `AgentBridge.ListWorlds` | none | List all world contexts |
| `AgentBridge.Capabilities` | none | Show current context capabilities |
| `AgentBridge.DumpActor` | `<Name> [Depth]` | Dump actor properties |
| `AgentBridge.DumpClass` | `<Name>` | Dump class schema |
| `AgentBridge.GetPath` | `<Actor> <Path>` | Read nested property |
| `AgentBridge.SetPath` | `<Actor> <Path> <Value>` | Write nested property |
| `AgentBridge.QueryActors` | `[Pattern] [Limit]` | Query actors by name |
| `AgentBridge.SpawnActor` | `<Class> [X Y Z] [Label]` | Spawn actor |
| `AgentBridge.CallFunc` | `<Actor> <Func>` | Call zero-arg void function |
| `AgentBridge.IsPartitioned` | none | Check WP status |
| `AgentBridge.QueryAllActors` | `[Pattern] [Limit]` | Query including unloaded |
| `AgentBridge.StreamingState` | `<ActorGuid>` | Get actor streaming state |
| `AgentBridge.QueryLandscape` | none | List landscape proxies |
| `AgentBridge.GetLandscapeBounds` | none | Get landscape bounds |
| `AgentBridge.DataLayers` | none | List data layers |
| `AgentBridge.ListMaterials` | `[Filter] [Limit]` | List materials |
| `AgentBridge.GetMaterial` | `<Path>` | Get material info |
| `AgentBridge.SetMaterialParam` | `<Actor> <Param> <Value> [Type]` | Set material param |
| `AgentBridge.ListPCG` | `[Pattern]` | List PCG actors |
| `AgentBridge.GetCVar` | `<Name>` | Get console variable |
| `AgentBridge.SetCVar` | `<Name> <Value>` | Set console variable |
| `AgentBridge.ListCVars` | `[Pattern] [Limit]` | List console variables |
| `AgentBridge.SearchCommands` | `<Keyword> [Limit] [SearchHelp]` | Search commands |

---

## Thread Safety

Most UObject operations require the game thread. Pattern for async contexts:

```cpp
TWeakObjectPtr<UObject> WeakRef(MyObject);

AsyncTask(ENamedThreads::AnyBackgroundThread, [WeakRef]()
{
    // Do non-UObject work here...

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

Key rules:
- Always use `TWeakObjectPtr` for stored UObject references (prevents dangling pointers)
- Never access UObjects from background threads without game thread bouncing
- `FWorldContextManager` methods should be called from the game thread
- `FActorOperations` methods must be called from the game thread

---

## Dependencies

```csharp
// AgentBridgeRuntime.Build.cs
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "AgentBridgeCore",
    "Landscape",  // For ALandscapeProxy, ALandscapeStreamingProxy
});

// Editor-only (conditional)
if (Target.bBuildEditor)
{
    PrivateDependencyModuleNames.Add("UnrealEd");
}
```

Module type: **Runtime**, Loading phase: **Default**, EnabledByDefault: **true**.

---

## Resolved Issues

### Landscape Bounds Calculation (Fixed)

Previously, landscape bounds used `CachedLocalBox` which could return inaccurate extents.
Now uses `GetComponentsBoundingBox()` for accurate landscape extent calculation including
proper Z bounds from terrain elevation data. The `BiomeVolumeScale` field was also
added to `FLandscapeBounds` and properly exposed through the Python MCP layer.

---

## Stretch Goals

| Feature | Effort | Notes |
|---------|--------|-------|
| Landscape sculpting | High | Import/export heightmaps, brush strokes |
| Force-load streaming regions | Medium | `LoadActor()` / `LoadRegion()` for WP (API exists, needs MCP exposure) |
