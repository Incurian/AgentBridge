# AgentBridgeScripting

Command dispatch, JSON serialization, and business logic for the AgentBridge ecosystem.

## Table of Contents

- [What Is This Plugin?](#what-is-this-plugin)
- [Why Does This Plugin Exist?](#why-does-this-plugin-exist)
- [How It Fits in the Stack](#how-it-fits-in-the-stack)
- [Key Concepts](#key-concepts)
  - [The Command/Response Pattern](#the-commandresponse-pattern)
  - [CommandExecutor - The Central Dispatcher](#commandexecutor---the-central-dispatcher)
  - [JSON Value Conversion](#json-value-conversion)
- [Data Flow](#data-flow)
- [What Commands Are Supported?](#what-commands-are-supported)
- [Plugin Structure](#plugin-structure)
- [Dependencies](#dependencies)
- [How to Add a New Command](#how-to-add-a-new-command)
- [Detailed Documentation](#detailed-documentation)

---

## What Is This Plugin?

AgentBridgeScripting is the **command layer** of AgentBridge. It sits in the middle of the
stack, between the transport layer (gRPC/HTTP server) and the engine layer (world operations,
property access, reflection).

In simple terms, this plugin:

1. **Defines** all the command and response types (C++ structs) that represent what an AI agent
   can ask Unreal Engine to do
2. **Implements** all the business logic that actually carries out those commands
3. **Handles** converting between JSON values and C++ types (vectors, colors, arrays, etc.)

There are 60+ command types covering everything from spawning actors to editing Blueprint
graphs to capturing screenshots.

## Why Does This Plugin Exist?

You might wonder: why not put the business logic in the Server plugin (which handles gRPC)?

The answer is a **C++ header conflict**. The Server plugin includes gRPC headers, and some of
those headers conflict with certain Unreal Engine headers. Specifically, gRPC pulls in Windows
SDK headers that define macros (like `SendMessage`, `GetObject`) that clash with UE's own
definitions. This means the Server plugin **cannot** include headers like:

- `AssetRegistry/AssetRegistryModule.h` (for finding assets)
- `Editor.h`, `LevelEditor.h` (for editor operations)
- `IImageWrapper.h` (for image processing)
- `BlueprintGraph` headers (for Blueprint node manipulation)

So the Server plugin is intentionally kept as a thin "translation layer" that just converts
between protobuf messages and C++ command structs. ALL the actual work - finding actors,
reading properties, creating assets, editing Blueprints - happens here in AgentBridgeScripting,
where we can freely include any UE header we need.

**Rule of thumb:** If you need to add new functionality, put the logic in
`CommandExecutor.cpp`, not in the Server plugin.

## How It Fits in the Stack

AgentBridge consists of **four independent UE plugins** that form a dependency chain. There is
no wrapper plugin - Unreal Build Tool (UBT) discovers each plugin independently by scanning the
`Plugins/AgentBridge/` directory.

```
                          AI Agent (Claude, etc.)
                                |
                                v
                    MCP Server (Python, ~100 tools)
                                |
                                v
                      gRPC (port 10001)
                                |
                                v
+-----------------------------------------------------------------+
|  AgentBridgeServer    | gRPC handlers, proto definitions        |
|                       | Thin layer: proto <-> command structs    |
+-----------------------------------------------------------------+
                                |
                                v
+-----------------------------------------------------------------+
|  AgentBridgeScripting | Command structs, CommandExecutor,       |
|  (THIS PLUGIN)        | JSON serialization, ALL business logic  |
+-----------------------------------------------------------------+
                                |
                                v
+-----------------------------------------------------------------+
|  AgentBridgeRuntime   | World context, actor operations,        |
|                       | property path resolution                |
+-----------------------------------------------------------------+
                                |
                                v
+-----------------------------------------------------------------+
|  AgentBridgeCore      | Reflection primitives (FProperty,       |
|                       | UFunction, type discovery)              |
+-----------------------------------------------------------------+
```

Each arrow represents a dependency - plugins at the top depend on plugins below them. The
Server depends on Scripting, Scripting depends on Runtime and Core, and Runtime depends on Core.

## Key Concepts

### The Command/Response Pattern

Every operation in AgentBridge follows a simple pattern: you create a **command struct**
describing what you want to do, and you get back a **response struct** with the result.

For example, to query actors in the world:

```cpp
// 1. Create a command describing what you want
FQueryActorsCommand Cmd;
Cmd.ClassName = "PointLight";   // Only find PointLight actors
Cmd.Limit = 10;                 // Return at most 10

// 2. Create an empty response to fill
FQueryActorsResponse Response;

// 3. Execute the command
FCommandExecutor::Execute(Cmd, Response);

// 4. Check the result
if (Response.bSuccess)
{
    for (const FActorInfo& Actor : Response.Actors)
    {
        UE_LOG(LogTemp, Log, TEXT("Found: %s"), *Actor.Label);
    }
}
else
{
    UE_LOG(LogTemp, Error, TEXT("Failed: %s"), *Response.ErrorMessage);
}
```

All command structs inherit from `FAgentCommandBase` (which has a `Type` enum and a `CommandId`).
All response structs inherit from `FAgentResponseBase` (which has `bSuccess`, `ErrorMessage`,
and `ExecutionTimeMs`).

The command/response structs are defined in `AgentCommands.h`. There are about 60 command types
and 40+ response types.

### CommandExecutor - The Central Dispatcher

`FCommandExecutor` is a static class with overloaded `Execute()` methods - one for each
command type. It is the single entry point for all operations.

```cpp
// CommandExecutor has many Execute() overloads:
static void Execute(const FQueryActorsCommand& Cmd, FQueryActorsResponse& Response);
static void Execute(const FSpawnActorCommand& Cmd, FSpawnActorResponse& Response);
static void Execute(const FGetPropertyPathCommand& Cmd, FPropertyValueResponse& Response);
static void Execute(const FSetPropertyPathCommand& Cmd, FAgentResponseBase& Response);
// ... and many more
```

It also supports JSON-based execution for the HTTP API:

```cpp
// JSON entry point (used by HTTP handlers)
static FString ExecuteJson(const FString& CommandJson);
static FString ExecuteBatchJson(const FString& CommandsJson, bool bStopOnError = true);
```

**Important:** All `Execute()` methods must be called on the **Game Thread**. If you are
calling from an async context (like a gRPC handler), you need to bounce to the game thread
first using `AsyncTask(ENamedThreads::GameThread, ...)` or `FGraphEvent`.

### JSON Value Conversion

When agents send property values over gRPC or HTTP, those values arrive as JSON strings. This
plugin handles converting between JSON and C++ types in both directions.

**Supported types:**

| JSON Format | C++ Type | Example JSON |
|------------|----------|--------------|
| `true` / `false` | `bool` | `true` |
| `42` | `int32` | `42` |
| `3.14` | `float` / `double` | `3.14` |
| `"hello"` | `FString` | `"hello"` |
| `{"x":1,"y":2,"z":3}` | `FVector` | `{"x":0,"y":0,"z":100}` |
| `{"r":0,"p":90,"y":0}` | `FRotator` | `{"r":0,"p":90,"y":0}` |
| `{"R":1,"G":0,"B":0,"A":1}` | `FLinearColor` | `{"R":1,"G":0,"B":0,"A":1}` |
| `[1, 2, 3]` | `TArray` | `["tag1","tag2"]` |
| `{"key":"value"}` | `FStruct` | `{"Location":{"x":0,"y":0,"z":0}}` |

The conversion is handled by two key functions:

- `JsonToPropertyValue()` - Parses a JSON string into an `FAgentPropertyValue` struct
- `PropertyValueToJson()` - Converts an `FAgentPropertyValue` back to a JSON string

The system also accepts UE's own format strings like `(X=1.0,Y=2.0,Z=3.0)` for vectors
and `(R=1.0,G=0.0,B=0.0,A=1.0)` for colors.

## Data Flow

Here is the complete data flow for a typical gRPC request, showing how data transforms at
each layer:

```
AI Agent
  |
  | "Set the intensity of MyLight to 5000"
  v
MCP Server (Python)
  |
  | set_property(actor_id="MyLight", path="LightComponent0.Intensity", value=5000)
  v
gRPC Client (Python)
  |
  | SetPropertyPath proto message {actor_id: "MyLight", path: "...", value: {float_value: 5000}}
  v
AgentBridgeServer (C++ gRPC handler)
  |
  | Converts proto -> FSetPropertyPathCommand struct
  | Cmd.ActorId = "MyLight"
  | Cmd.Path = "LightComponent0.Intensity"
  | Cmd.Value = "5000"
  v
AgentBridgeScripting (CommandExecutor.cpp)
  |
  | FCommandExecutor::Execute(Cmd, Response)
  | 1. Resolves "MyLight" to AActor* via ResolveActor()
  | 2. Resolves "LightComponent0.Intensity" property path
  | 3. Converts "5000" JSON to FAgentPropertyValue
  | 4. Calls into AgentBridgeRuntime to write the property
  | 5. Sets Response.bSuccess = true
  v
AgentBridgeServer
  |
  | Converts FAgentResponseBase -> proto response
  v
gRPC -> MCP -> Agent
  |
  | "Success"
```

## What Commands Are Supported?

Commands are organized into categories:

### World Operations (3 commands)
- `ListWorlds` - List all available world contexts (Editor, PIE, Game)
- `SetTargetWorld` - Switch which world subsequent commands operate on
- `GetCapabilities` - Check what operations are available in the current context

### Actor Operations (7 commands)
- `QueryActors` - Find actors by class, name, label, or tag
- `GetActor` - Get detailed info about a specific actor
- `GetActorProperties` - Get specific properties from an actor
- `SpawnActor` - Spawn a new actor of any class
- `DeleteActor` - Remove an actor from the world
- `DuplicateActor` - Clone an existing actor
- `SetActorTransform` - Move/rotate/scale an actor

### Property Operations (2 commands)
- `GetPropertyPath` - Read any property by path (e.g., `RootComponent.RelativeLocation.X`)
- `SetPropertyPath` - Write any property by path with JSON values

### Function Operations (3 commands)
- `CallFunction` - Call a UFunction on an actor or static class
- `CallAssetFunction` - Call a function on a loaded asset (PCG graph, etc.)
- `GetFunctionSignature` - Get parameter info for a function

### Type Discovery (3 commands)
- `FindClass` - Find a UClass by name
- `GetClassSchema` - Get full property/function schema for a class
- `ListClasses` - List classes matching criteria

### DataAsset Operations (3 commands)
- `ListDataAssets` - Find DataAssets by class and path
- `GetDataAsset` - Read a DataAsset and its properties
- `GetDataTableRow` - Read rows from a DataTable

### Capture Operations (2 commands)
- `CaptureViewport` - Screenshot the editor viewport
- `CaptureScene` - Render from an arbitrary camera position

### Audio Operations (3 commands)
- `GetAudioAnalysis` - Real-time frequency/volume analysis
- `StartAudioCapture` - Begin recording audio
- `StopAudioCapture` - Stop recording and retrieve audio data

### Material Operations (5 commands)
- `ListMaterials` - Find materials in the project
- `GetMaterialInfo` - Get material details and parameters
- `CreateMaterialInstance` - Create a dynamic material instance
- `SetMaterialParameter` - Modify material parameter values
- `ApplyMaterialToActor` - Apply a material to an actor's mesh

### PCG Operations (3 commands)
- `ListPCGActors` - Find PCG (Procedural Content Generation) actors
- `RegeneratePCG` - Trigger PCG graph regeneration
- `SetPCGParameter` - Modify PCG graph parameters

### Asset Operations (5 commands)
- `CreateAsset` - Create a new asset (DataAsset, MaterialInstance, etc.)
- `SaveAsset` - Save an asset to disk
- `SaveActorAsBlueprint` - Convert an actor to a Blueprint asset
- `DuplicateAsset` - Copy an existing asset
- `GetAssetThumbnail` - Get a thumbnail image of an asset

### Transform/Attachment Operations (4 unified commands)
- `SetTransform` - Set transform on actors or components (supports `Actor->Component` syntax)
- `GetTransform` - Get transform from actors or components
- `Attach` - Attach actors or components together
- `Detach` - Detach actors or components

### Component Operations (6 legacy commands)
- `GetComponentTransform` / `SetComponentTransform` - Per-component transforms
- `AttachComponent` / `AttachActor` - Attachment operations
- `DetachComponent` / `DetachActor` - Detachment operations

(The unified transform/attachment commands above are the preferred API. These legacy commands
still work but the unified versions are simpler to use.)

### File Operations (5 commands)
- `ReadProjectFile` - Read a file from the project directory
- `WriteProjectFile` - Write a file to the project directory
- `ListProjectDirectory` - List files in a directory
- `CopyProjectFile` - Copy a file within the project
- `DeleteProjectFile` - Delete a file from the project

### Blueprint Node Operations (6 commands)
- `CreateBlueprintNode` - Create a K2Node in a Blueprint graph
- `ConnectBlueprintPins` - Connect two pins between nodes
- `DisconnectBlueprintPins` - Disconnect pins
- `DeleteBlueprintNode` - Remove a node from a graph
- `ListBlueprintNodes` - List all nodes in a graph
- `ListBlueprintPins` - List all pins on a node

### Batch Operations (1 command)
- `BatchExecute` - Execute multiple commands in sequence, optionally with transactions

## Plugin Structure

```
AgentBridgeScripting/
|-- AgentBridgeScripting.uplugin      Plugin descriptor
|-- CLAUDE.md                         AI assistant documentation
|-- README.md                         This file
|-- Source/AgentBridgeScripting/
    |-- AgentBridgeScripting.Build.cs  Build configuration (dependencies)
    |-- Public/
    |   |-- AgentCommands.h            60+ command/response structs (~2400 lines)
    |   |-- CommandExecutor.h          Central dispatch interface (~330 lines)
    |   |-- AgentBridgeScripting.h     Module definition
    |-- Private/
        |-- CommandExecutor.cpp        ALL business logic (~7400 lines, ~228KB)
        |-- AgentBridgeScripting.cpp   Module startup/shutdown
```

**Key files explained:**

- **AgentCommands.h** - This is where all command and response structs are defined. Every
  operation has a command struct (what you want to do) and a response struct (what happened).
  If you want to understand what parameters a command accepts, look here.

- **CommandExecutor.h** - The header for the central dispatcher. Lists all the `Execute()`
  overloads, JSON serialization helpers, and private utility functions.

- **CommandExecutor.cpp** - The largest file in the entire AgentBridge system. Contains the
  implementation of every single command handler. This is where you add new functionality.

## Dependencies

### Plugin Dependencies (in .uplugin)

| Plugin | Why |
|--------|-----|
| AgentBridgeCore | Reflection primitives - reading/writing FProperty, type discovery |
| AgentBridgeRuntime | World context management, actor operations, property path resolution |

### Module Dependencies (in .Build.cs)

**Always available:**

| Module | Why |
|--------|-----|
| Core, CoreUObject, Engine | Base UE modules |
| Json, JsonUtilities | JSON parsing and serialization |
| AgentBridgeCore | Reflection layer |
| AgentBridgeRuntime | World/actor operations |
| AssetRegistry | Finding assets by path and class |
| ImageWrapper | Image format conversion (for capture commands) |
| RenderCore, RHI | Rendering support (for scene capture) |

**Editor-only** (only included when building the editor, not packaged games):

| Module | Why |
|--------|-----|
| UnrealEd | Editor utilities (asset creation, Blueprint compilation) |
| BlueprintGraph | K2Node classes for Blueprint node manipulation |
| KismetCompiler | Blueprint compilation utilities |

The editor-only dependencies are wrapped in `if (Target.bBuildEditor)` in the Build.cs file.
This means Blueprint node editing commands only work in the editor, not in packaged games.

### Plugin Configuration

| Setting | Value |
|---------|-------|
| Module Type | Runtime |
| Loading Phase | Default |
| Enabled By Default | true |
| Can Contain Content | false |

## How to Add a New Command

Here is a step-by-step guide for adding a new command to AgentBridgeScripting. This example
adds a hypothetical "RenameActor" command.

### Step 1: Add the enum value

In `AgentCommands.h`, add a new entry to `EAgentCommandType`:

```cpp
enum class EAgentCommandType : uint8
{
    // ... existing entries ...

    // Actor Modification Commands
    SpawnActor,
    DeleteActor,
    SetActorProperties,
    SetActorTransform,
    DuplicateActor,
    RenameActor,     // <-- ADD THIS
```

### Step 2: Define command and response structs

Still in `AgentCommands.h`, add the structs. Follow the existing naming convention:
`F<Name>Command` and `F<Name>Response`.

```cpp
/**
 * FRenameActorCommand - Renames an actor's label.
 */
struct AGENTBRIDGESCRIPTING_API FRenameActorCommand : FAgentCommandBase
{
    FRenameActorCommand() { Type = EAgentCommandType::RenameActor; }

    /** Actor to rename (name, label, or GUID). */
    FString ActorId;

    /** New label for the actor. */
    FString NewLabel;
};

// If the response only needs success/failure, use FAgentResponseBase directly.
// Only create a custom response if you need to return extra data.
```

### Step 3: Add the Execute() declaration

In `CommandExecutor.h`, add the overload:

```cpp
// Typed Execution - Actor Modifications
static void Execute(const FRenameActorCommand& Command, FAgentResponseBase& Response);
```

### Step 4: Implement the handler

In `CommandExecutor.cpp`, add the implementation. This is where all the actual logic goes:

```cpp
void FCommandExecutor::Execute(const FRenameActorCommand& Cmd, FAgentResponseBase& Response)
{
    double StartTime = StartTiming();

    // Resolve the actor (handles name, label, GUID, path)
    FString Error;
    AActor* Actor = ResolveActor(Cmd.ActorId, &Error);
    if (!Actor)
    {
        Response.bSuccess = false;
        Response.ErrorMessage = Error;
        Response.ExecutionTimeMs = EndTiming(StartTime);
        return;
    }

    // Do the actual work
#if WITH_EDITOR
    Actor->SetActorLabel(Cmd.NewLabel);
    Response.bSuccess = true;
#else
    Response.bSuccess = false;
    Response.ErrorMessage = TEXT("SetActorLabel is only available in the editor");
#endif

    Response.ExecutionTimeMs = EndTiming(StartTime);
}
```

### Step 5: Add gRPC proto message (in AgentBridgeServer)

In `AgentBridge.proto`, add the request/response messages and RPC:

```protobuf
message RenameActorRequest {
    string actor_id = 1;
    string new_label = 2;
}

// In the service definition:
rpc RenameActor (RenameActorRequest) returns (AgentBridgeResponse);
```

### Step 6: Add the gRPC handler (in AgentBridgeServer)

In `AgentBridgeServiceSubsystem.cpp`, add a thin handler that converts between proto
and command structs:

```cpp
void UAgentBridgeServiceSubsystem::RenameActor(
    const RenameActorRequest& Request,
    const TResponseDelegate<AgentBridgeResponse>& Continuation)
{
    FRenameActorCommand Cmd;
    Cmd.ActorId = UTF8_TO_TCHAR(Request.actor_id().c_str());
    Cmd.NewLabel = UTF8_TO_TCHAR(Request.new_label().c_str());

    FAgentResponseBase CmdResponse;
    FCommandExecutor::Execute(Cmd, CmdResponse);

    AgentBridgeResponse ProtoResponse;
    ProtoResponse.set_success(CmdResponse.bSuccess);
    if (!CmdResponse.bSuccess)
    {
        ProtoResponse.set_error(TCHAR_TO_UTF8(*CmdResponse.ErrorMessage));
    }
    Continuation.ExecuteIfBound(ProtoResponse);
}
```

### Step 7: Register the RPC handler

**This is easy to forget!** In `AgentBridgeServiceSubsystem.cpp`, find
`RegisterScriptingServices()` and add your new RPC.

### Step 8: Add the Python MCP tool

In `mcp/agentbridge.py`, add a Python method that calls the gRPC endpoint, and register
it as an MCP tool.

### Step 9: Test

1. Rebuild (kill editor first, then run Build.sh)
2. Start the editor
3. Connect via MCP and test the new command
4. Verify the result visually in the editor

## Detailed Documentation

For implementation details, resolved issues, and development patterns, see
[CLAUDE.md](CLAUDE.md).

For the full AgentBridge system documentation, see the
[parent README](../README.md).
