# AgentBridgeRuntime

World context management, actor operations, and property path resolution for AgentBridge.

---

## What Is This Plugin?

AgentBridgeRuntime is one of four Unreal Engine plugins that make up the **AgentBridge** system.
AgentBridge lets external AI agents (like Claude) read and modify Unreal Engine levels by
sending commands over a network connection (gRPC). Think of it as a remote control for
Unreal Engine.

AgentBridgeRuntime provides the **world interaction layer** - it is the part of AgentBridge
that actually talks to Unreal Engine's world: finding actors, spawning new ones, reading and
writing their properties, and handling large streaming worlds. If AgentBridgeCore is the
"dictionary" that knows how to read C++ types, then AgentBridgeRuntime is the "librarian"
that knows which shelf to look on and how to find the right book.

---

## Key Concepts for Beginners

### What Is a "World Context"?

Unreal Engine can have **multiple "worlds" running at the same time**. Each world is a
separate 3D space containing actors (objects in the scene):

| World Type | When It Exists | What It Is |
|------------|----------------|------------|
| **Editor** | Always (when the editor is open) | The level you are editing. No gameplay is running. |
| **PIE** (Play In Editor) | When you click "Play" in the editor | A temporary copy of the level where gameplay runs. There can be multiple PIE worlds for networked testing. |
| **Game** | When running a packaged/standalone game | The actual shipping game. No editor features available. |
| **EditorPreview** | When previewing assets | Tiny worlds used for asset thumbnail previews. Not relevant for agents. |

**Why does this matter?** When an AI agent says "move that light to position X", the system
needs to know WHICH world to operate on. `FWorldContextManager` handles this automatically:
- If PIE is active, it targets the PIE world (because that is where the action is).
- Otherwise, it targets the Editor world.
- You can also explicitly switch worlds if needed.

The gotcha: `GIsEditor` (a global Unreal variable) stays TRUE even during PIE. So you
cannot use `GIsEditor` to tell Editor from PIE apart. Instead, check `World->WorldType`.

### What Are "Actor Operations"?

In Unreal Engine, everything you see in a level is an **Actor** - lights, meshes, cameras,
trigger volumes, player characters, etc. "Actor operations" means CRUD operations on these:

| Operation | What It Does | Example |
|-----------|-------------|---------|
| **Query** | Find actors by class, name, tag, or label | "Find all PointLight actors" |
| **Spawn** | Create a new actor from a class | "Spawn a PointLight at position (100, 200, 300)" |
| **Modify** | Change an actor's properties, transform, or label | "Set this light's intensity to 5000" |
| **Delete** | Remove an actor from the world | "Delete the light named MyLight" |
| **Duplicate** | Copy an existing actor | "Duplicate this light 3 meters to the right" |
| **Attach** | Parent one actor to another | "Attach this mesh to that vehicle" |

Actors have several identifiers:
- **Name** (`GetName()`) - Internal unique name like `PointLight_0`. Not human-friendly.
- **Label** (`GetActorLabel()`) - Editor display name like `My Kitchen Light`. Human-friendly but may not be unique. Editor-only.
- **GUID** - A globally unique identifier. The most stable way to reference an actor.
- **Path** - Full object path like `/Game/Maps/MyLevel.MyLevel:PersistentLevel.PointLight_0`.

### What Are "Property Paths"?

Actors have **properties** - data values that control their behavior and appearance. Properties
can be nested inside components and structs, forming a tree structure. A "property path" is
dot-notation syntax to reach any value in that tree.

For example, a `PointLight` actor has:
- A `LightComponent0` component (the actual light source)
  - Which has an `Intensity` property (how bright the light is)
  - Which has a `LightColor` property (what color the light is)
- A `RootComponent` (the actor's position in 3D space)
  - Which has a `RelativeLocation` property (a struct with X, Y, Z fields)

You can reach any of these with dot-notation:

```
LightComponent0.Intensity           -> 5000.0
LightComponent0.LightColor          -> (R=255, G=255, B=200, A=255)
RootComponent.RelativeLocation.X    -> 100.0
Items[3].Name                       -> "Sword"
```

This syntax lets agents read or write ANY property on ANY actor without needing to know the
C++ class hierarchy. The property path system (implemented in AgentBridgeCore's
`FAgentPropertyPath`) handles all the traversal automatically.

**Note:** `FAgentPropertyPath` itself is defined in AgentBridgeCore, but AgentBridgeRuntime
is where it gets used most heavily - `FActorOperations` calls it to get/set properties on
actors.

### What Is "World Partition"?

World Partition is Unreal Engine 5's system for managing **very large open worlds**. Instead
of loading the entire level into memory at once, UE5 divides the world into a grid of
"streaming cells" and only loads the cells near the player (or near the editor camera).

This creates a challenge for agents: **some actors may not be loaded into memory**. They
exist in the level data on disk, but they are not currently active in the engine. For
example, a tree 10 kilometers away might be "unloaded" - it has metadata (position, class,
GUID) but no actual `AActor` object in memory.

`FWorldPartitionOps` handles this by:
- Detecting whether the current world uses World Partition
- Querying actor metadata even for unloaded actors (editor only)
- Providing landscape bounds that account for all terrain chunks
- Supporting data layers (groups of actors that stream together)

---

## Where This Fits in the AgentBridge Stack

AgentBridge has four plugins forming a dependency chain. There is NO wrapper plugin - Unreal
Build Tool (UBT) discovers each plugin independently by scanning the `AgentBridge/` directory.

```
AgentBridgeServer   (gRPC/HTTP server, proto definitions)
        |
        | depends on
        v
AgentBridgeScripting  (command dispatch, JSON serialization)
        |
        | depends on
        v
AgentBridgeRuntime  <-- YOU ARE HERE (world ops, actor CRUD, target resolution)
        |
        | depends on
        v
AgentBridgeCore     (C++ reflection, property paths, type discovery)
```

- **AgentBridgeCore** provides the low-level tools: reading C++ property types, traversing
  struct fields, discovering class schemas.
- **AgentBridgeRuntime** uses those tools to provide world-level operations: "find all lights
  in the world", "spawn this actor", "set that property on that component".
- **AgentBridgeScripting** wraps Runtime operations into JSON-based commands that can be
  dispatched from gRPC handlers.
- **AgentBridgeServer** receives gRPC/HTTP requests from external agents and routes them
  through the Scripting layer.

---

## Plugin Structure

```
AgentBridgeRuntime/
|-- AgentBridgeRuntime.uplugin        Plugin descriptor (depends on AgentBridgeCore)
|-- CLAUDE.md                         Technical docs for AI assistants
|-- README.md                         This file (beginner-friendly docs)
|-- Source/AgentBridgeRuntime/
    |-- AgentBridgeRuntime.Build.cs   Build configuration (C# for UBT)
    |-- Public/                       Header files (.h) - the public API
    |   |-- ActorOperations.h         Query, spawn, delete, modify actors
    |   |-- AgentBridgeDebug.h        Console commands for testing
    |   |-- AgentBridgeRuntime.h      Module interface (startup/shutdown)
    |   |-- TargetResolution.h        Resolve "MyActor->Component" strings
    |   |-- WorldContextManager.h     Multi-world support (Editor/PIE/Game)
    |   |-- WorldPartitionOps.h       World Partition streaming support
    |-- Private/                      Implementation files (.cpp)
        |-- ActorOperations.cpp
        |-- AgentBridgeDebug.cpp
        |-- AgentBridgeRuntime.cpp
        |-- TargetResolution.cpp
        |-- WorldContextManager.cpp
        |-- WorldPartitionOps.cpp
```

---

## Key Classes

### FWorldContextManager

**What it does:** Decides which "world" AgentBridge should operate on.

Unreal can have multiple worlds at once (the editor world, a PIE play session, preview
worlds, etc.). When an agent says "query all lights", we need to know which world to search.
FWorldContextManager is a singleton that provides a consistent answer.

**Default behavior:**
1. If you set an explicit override, it always uses that.
2. If PIE (Play In Editor) is active, it returns the PIE world.
3. Otherwise, it returns the Editor world.

**Key methods:**
- `Get()` - Get the singleton instance
- `GetTargetWorld()` - Get the current target world
- `SetTargetWorldOverride(World)` - Force a specific world
- `GetCapabilities()` - Query what features are available in this context
- `IsEditorWorld()` / `IsPIEWorld()` / `IsGameWorld()` - Quick checks

### FActorOperations

**What it does:** Provides all the CRUD operations for actors.

This is the workhorse class that agents use (indirectly, via gRPC) to interact with actors
in the world. It handles finding actors, spawning new ones, setting their properties,
moving them, and deleting them.

**Key methods:**
- `QueryActors(Params)` - Find actors matching filters (class, name pattern, tag, label)
- `SpawnActor(Params)` - Create a new actor
- `DestroyActor(Actor)` - Remove an actor
- `DuplicateActor(Source, Transform)` - Copy an actor
- `SetActorTransform(Actor, Transform)` - Move/rotate/scale an actor
- `GetActorProperties(Actor, Names)` - Read property values
- `SetActorProperties(Actor, Properties)` - Write property values
- `AttachActor(Child, Parent)` - Create parent-child relationship

### FWorldPartitionOps

**What it does:** Extends actor operations to handle large streaming worlds.

In a World Partition world, many actors may be "unloaded" (not in memory). This class can
still query their metadata (class, position, GUID, data layer) from the World Partition
system, which stores this information even for unloaded actors.

**Key methods:**
- `IsWorldPartitioned(World)` - Check if the world uses World Partition
- `QueryAllActors(Params)` - Query actors including unloaded ones
- `GetLandscapeBounds(World)` - Get the full bounds of the landscape (all chunks)
- `QueryLandscapeProxies(World)` - List all landscape streaming chunks
- `GetDataLayers(World)` - List data layers (streaming groups)
- `GetActorStreamingState(Guid)` - Check if a specific actor is loaded or unloaded

### FTargetResolution (AgentBridge::TargetResolution)

**What it does:** Parses target strings like `"MyActor->LightComponent0"` into actual
actor and component pointers.

The MCP tools use a string syntax where you can specify either an actor alone or an actor
with a specific component, separated by `->`. This class parses that syntax and resolves
the strings to real Unreal Engine objects.

**Syntax examples:**
- `"MyLight"` - Resolves to the actor named/labeled "MyLight"
- `"MyLight->LightComponent0"` - Resolves to the LightComponent0 on MyLight
- `"BP_Door_5->DoorMesh"` - Resolves to the DoorMesh component on BP_Door_5

**Key methods:**
- `Parse(Target)` - Split a target string into actor and component parts
- `Resolve(World, Target)` - Parse and resolve to actual UE objects
- `FindComponent(Actor, Name)` - Find a scene component by name (supports fuzzy matching)

### FAgentBridgeDebug

**What it does:** Registers console commands for testing AgentBridge from the Unreal console.

These are developer-facing tools - you type them into the Unreal console (press ~ or use
the Output Log) to test that AgentBridge is working correctly.

---

## Capabilities by Context

Not all features work in all contexts. Here is what is available where:

| Feature | Editor | PIE | Packaged Game |
|---------|--------|-----|---------------|
| Iterate properties | Yes | Yes | Yes |
| Invoke functions | Yes | Yes | Yes |
| Spawn actors | Yes | Yes | Yes |
| Destroy actors | Yes | Yes | Yes |
| Modify transforms | Yes | Yes | Yes |
| Modify properties | Yes | Yes | Yes |
| Set actor label | Yes | Yes | No |
| Set actor folder | Yes | Yes | No |
| Undo/Redo (Transactions) | Yes | No | No |
| Property metadata | Yes | Yes | No |
| World Partition metadata | Yes | Limited | No |

---

## Console Commands

These commands are available in the Unreal console for debugging. All output goes to the
`LogAgentBridge` log category.

### World & Context

| Command | Description |
|---------|-------------|
| `AgentBridge.ListWorlds` | Lists all active world contexts (Editor, PIE, Preview, etc.) |
| `AgentBridge.Capabilities` | Shows what features are available in the current world context |

### Actor Inspection

| Command | Description |
|---------|-------------|
| `AgentBridge.DumpActor <Name> [Depth]` | Prints all properties of an actor (default depth 3) |
| `AgentBridge.DumpClass <Name>` | Prints the class schema (properties and functions) |
| `AgentBridge.QueryActors [Pattern] [Limit]` | Finds actors matching a name pattern |
| `AgentBridge.SpawnActor <Class> [X Y Z] [Label]` | Spawns an actor at a location |

### Property Paths

| Command | Description |
|---------|-------------|
| `AgentBridge.GetPath <Actor> <Path>` | Reads a nested property value (e.g., `LightComponent0.Intensity`) |
| `AgentBridge.SetPath <Actor> <Path> <Value>` | Writes a nested property value |

### World Partition & Streaming

| Command | Description |
|---------|-------------|
| `AgentBridge.IsPartitioned` | Checks if the current world uses World Partition |
| `AgentBridge.QueryAllActors [Pattern] [Limit]` | Queries all actors including those in unloaded streaming cells |
| `AgentBridge.StreamingState <ActorGuid>` | Gets the streaming state (Loaded/Unloaded/Invalid) of an actor |
| `AgentBridge.QueryLandscape` | Lists all landscape proxies including streaming chunks |
| `AgentBridge.GetLandscapeBounds` | Computes and prints the full landscape bounding box |
| `AgentBridge.DataLayers` | Lists all data layers in the world |

### Materials

| Command | Description |
|---------|-------------|
| `AgentBridge.ListMaterials [Filter] [Limit]` | Lists project materials matching a filter |
| `AgentBridge.GetMaterial <Path>` | Gets material info and parameter list |
| `AgentBridge.SetMaterialParam <Actor> <Param> <Value> [Type]` | Sets a material parameter on an actor |

### Console Variables

| Command | Description |
|---------|-------------|
| `AgentBridge.GetCVar <Name>` | Gets a console variable value |
| `AgentBridge.SetCVar <Name> <Value>` | Sets a console variable value |
| `AgentBridge.ListCVars [Pattern] [Limit]` | Lists console variables matching a pattern |
| `AgentBridge.SearchCommands <Keyword> [Limit]` | Searches all console commands by keyword |

### Functions

| Command | Description |
|---------|-------------|
| `AgentBridge.CallFunc <ActorName> <FunctionName>` | Calls a zero-argument void function on an actor |

### PCG

| Command | Description |
|---------|-------------|
| `AgentBridge.ListPCG [Pattern]` | Lists PCG (Procedural Content Generation) actors in the world |

---

## Property Path Syntax

Property paths use dot-notation to reach nested values. Here are examples with explanations:

```
# Simple property on the actor itself
Mobility                               -> EComponentMobility value

# Component property (component name, then dot, then property name)
LightComponent0.Intensity              -> float (light brightness)
LightComponent0.LightColor             -> FLinearColor struct

# Nested struct fields (keep adding dots to go deeper)
RootComponent.RelativeLocation         -> FVector struct
RootComponent.RelativeLocation.X       -> float (just the X coordinate)

# Array elements (use square brackets with zero-based index)
Items[0]                               -> First element of Items array
Items[3].Name                          -> Name property of 4th item

# Map entries (use square brackets with quoted key)
Tags["environment"]                    -> Value for key "environment"

# Deep nesting (combine all of the above)
MeshComponent.Materials[0].Color.R     -> Red channel of first material's color
```

**Important:** Property paths are resolved by `FAgentPropertyPath` in AgentBridgeCore.
AgentBridgeRuntime's `FActorOperations` calls into it for `GetActorProperties` and
`SetActorProperties`.

---

## Dependencies

### Plugin Dependencies (in .uplugin)

- **AgentBridgeCore** - Required. Provides reflection, property paths, type discovery.

### Module Dependencies (in .Build.cs)

```csharp
// Public dependencies (available to modules that depend on AgentBridgeRuntime)
"Core"
"CoreUObject"
"Engine"
"AgentBridgeCore"
"Landscape"           // For ALandscapeProxy, ALandscapeStreamingProxy

// Private dependencies (editor-only, not exposed)
"UnrealEd"            // Conditional: only when building for editor (Target.bBuildEditor)
```

### Module Configuration

- **Module Type:** Runtime
- **Loading Phase:** Default
- **EnabledByDefault:** true

---

## Part of AgentBridge

This plugin is one of four that make up the AgentBridge system:

| Plugin | Role | Depends On |
|--------|------|------------|
| **AgentBridgeCore** | Reflection primitives, property paths, type discovery | (none) |
| **AgentBridgeRuntime** | **World ops, actor CRUD, target resolution** | Core |
| **AgentBridgeScripting** | Command dispatch, JSON serialization | Core, Runtime |
| **AgentBridgeServer** | gRPC/HTTP server, proto definitions | Core, Runtime, Scripting |

There is no wrapper plugin. UBT discovers each sub-plugin independently by recursively
scanning the `Plugins/AgentBridge/` directory.

---

## Further Reading

- [CLAUDE.md](CLAUDE.md) - Technical implementation details, thread safety patterns, known
  issues, and architecture notes for developers and AI assistants.
- [Parent README](../README.md) - The main AgentBridge documentation with full tool reference
  and setup instructions.
