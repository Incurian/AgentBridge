# AgentBridgeScripting

> Standalone UE plugin providing the command layer with JSON serialization and dispatch
> to runtime operations. Depends on AgentBridgeCore and AgentBridgeRuntime.

## Plugin Structure

```
AgentBridgeScripting/
+-- AgentBridgeScripting.uplugin    (depends on AgentBridgeCore, AgentBridgeRuntime)
+-- CLAUDE.md                       (this file)
+-- README.md                       (beginner-friendly documentation)
+-- Source/AgentBridgeScripting/
    +-- AgentBridgeScripting.Build.cs
    +-- Public/
    |   +-- AgentCommands.h           (60+ command/response structs, ~2400 lines)
    |   +-- CommandExecutor.h         (central dispatch interface, ~330 lines)
    |   +-- AgentBridgeScripting.h    (module definition)
    +-- Private/
        +-- CommandExecutor.cpp       (ALL business logic, ~7400 lines, ~228KB)
        +-- AgentBridgeScripting.cpp  (module startup/shutdown)
```

## Purpose

This plugin provides the command/response abstraction used by both HTTP and gRPC servers:
- `AgentCommands.h` - All command and response structs
- `CommandExecutor.cpp` - Central dispatch, JSON handling, all business logic

## Key Files

| File | Purpose |
|------|---------|
| `AgentCommands.h` | Command/response structs (60+ types), enums, actor/property info structs |
| `CommandExecutor.h/.cpp` | JSON dispatch, implements all operations, serialization helpers |

## Architecture

```
gRPC Request -> Convert to Command Struct -> CommandExecutor::Execute() -> Response Struct -> gRPC Response
HTTP Request -> Parse JSON -> CommandExecutor::Execute() -> Response Struct -> JSON Response
```

## Critical Pattern: Where to Put New Functionality

**ALL business logic goes in CommandExecutor.cpp**, NOT in AgentBridgeServer.

Why: AgentBridgeServer has gRPC headers that conflict with certain UE headers
(AssetRegistry, Editor, ImageWrapper, BlueprintGraph, etc.). The Server module contains
only thin handlers that convert between proto messages and command structs. Any logic
requiring UE editor or engine headers must live here.

### Adding New Features

1. Add command/response structs to `AgentCommands.h`:
```cpp
struct FMyNewCommand : FAgentCommandBase
{
    FMyNewCommand() { Type = EAgentCommandType::MyNewCommand; }
    FString SomeParam;
};

struct FMyNewResponse : FAgentResponseBase
{
    FString Result;
};
```

2. Add enum entry in `EAgentCommandType` (same file).

3. Add Execute() declaration in `CommandExecutor.h`:
```cpp
static void Execute(const FMyNewCommand& Command, FMyNewResponse& Response);
```

4. Implement in `CommandExecutor.cpp`:
```cpp
void FCommandExecutor::Execute(const FMyNewCommand& Cmd, FMyNewResponse& Response)
{
    double StartTime = StartTiming();

    // All logic here - can include ANY UE header
    Response.Result = DoTheThing(Cmd.SomeParam);
    Response.bSuccess = true;

    Response.ExecutionTimeMs = EndTiming(StartTime);
}
```

5. Add gRPC RPC in `AgentBridge.proto` (in AgentBridgeServer).

6. Add thin handler in `AgentBridgeServiceSubsystem.cpp`:
```cpp
void UAgentBridgeServiceSubsystem::MyNewRpc(const MyNewRequest& Request,
    const TResponseDelegate<MyNewResponse>& Continuation)
{
    FMyNewCommand Cmd;
    Cmd.SomeParam = UTF8_TO_TCHAR(Request.some_param().c_str());

    FMyNewResponse CmdResponse;
    FCommandExecutor::Execute(Cmd, CmdResponse);

    // Convert to proto and continue
}
```

7. **Register in `RegisterScriptingServices()`** - easy to forget!

8. Add Python client method in `agentbridge.py`.

9. Add MCP tool wrapper and register in `MODULES` dict in `__init__.py`.

## Implemented Commands

### World Operations
- `FListWorldsCommand` / `FListWorldsResponse`
- `FSetTargetWorldCommand`
- `FGetCapabilitiesCommand` / `FGetCapabilitiesResponse`

### Actor Operations
- `FQueryActorsCommand` / `FQueryActorsResponse`
- `FGetActorCommand` / `FGetActorResponse`
- `FGetActorPropertiesCommand` / `FPropertyValueResponse`
- `FSpawnActorCommand` / `FSpawnActorResponse`
- `FDeleteActorCommand`
- `FDuplicateActorCommand` / `FSpawnActorResponse`
- `FSetActorPropertiesCommand`
- `FSetActorTransformCommand`

### Property Operations
- `FGetPropertyPathCommand` / `FPropertyValueResponse`
- `FSetPropertyPathCommand`

### Function Operations
- `FCallFunctionCommand` / `FFunctionCallResponse`
- `FCallAssetFunctionCommand` / `FCallAssetFunctionResponse`
- `FGetFunctionSignatureCommand`

### Class Discovery
- `FFindClassCommand`
- `FGetClassSchemaCommand` / `FGetClassSchemaResponse`
- `FListClassesCommand` / `FListClassesResponse`

### DataAsset Operations
- `FListDataAssetsCommand` / `FListDataAssetsResponse`
- `FGetDataAssetCommand` / `FGetDataAssetResponse`
- `FGetDataTableRowCommand` / `FGetDataTableRowResponse`

### Capture Operations
- `FCaptureViewportCommand` / `FCaptureViewportResponse`
- `FCaptureSceneCommand` / `FCaptureSceneResponse`

### Audio Operations
- `FGetAudioAnalysisCommand` / `FAudioAnalysisResponse`
- `FStartAudioCaptureCommand` / `FStartAudioCaptureResponse`
- `FStopAudioCaptureCommand` / `FStopAudioCaptureResponse`

### Material Operations
- `FListMaterialsCommand` / `FListMaterialsResponse`
- `FGetMaterialInfoCommand` / `FGetMaterialInfoResponse`
- `FCreateMaterialInstanceCommand` / `FCreateMaterialInstanceResponse`
- `FSetMaterialParameterCommand`
- `FApplyMaterialToActorCommand`

### PCG Operations
- `FListPCGActorsCommand` / `FListPCGActorsResponse`
- `FRegeneratePCGCommand` / `FRegeneratePCGResponse`
- `FSetPCGParameterCommand`

### Asset Operations
- `FCreateAssetCommand` / `FCreateAssetResponse`
- `FSaveAssetCommand` / `FSaveAssetResponse`
- `FSaveActorAsBlueprintCommand` / `FSaveActorAsBlueprintResponse`
- `FDuplicateAssetCommand` / `FDuplicateAssetResponse`
- `FGetAssetThumbnailCommand` / `FGetAssetThumbnailResponse`

### Component Operations (Legacy)
- `FGetComponentTransformCommand` / `FGetComponentTransformResponse`
- `FSetComponentTransformCommand`
- `FAttachComponentCommand`
- `FAttachActorCommand`
- `FDetachComponentCommand`
- `FDetachActorCommand`

### Unified Transform/Attachment Operations (Preferred)
- `FSetTransformCommand` - Supports `Actor->Component` syntax
- `FGetTransformCommand` / `FGetTransformResponse`
- `FAttachCommand` - Supports `Actor->Component` syntax
- `FDetachCommand`

### File Operations
- `FReadProjectFileCommand` / `FReadProjectFileResponse`
- `FWriteProjectFileCommand` / `FWriteProjectFileResponse`
- `FListProjectDirectoryCommand` / `FListProjectDirectoryResponse`
- `FCopyProjectFileCommand` / `FCopyProjectFileResponse`
- `FDeleteProjectFileCommand`

### Blueprint Node Operations
- `FCreateBlueprintNodeCommand` / `FCreateBlueprintNodeResponse`
- `FConnectBlueprintPinsCommand`
- `FDisconnectBlueprintPinsCommand`
- `FDeleteBlueprintNodeCommand`
- `FListBlueprintNodesCommand` / `FListBlueprintNodesResponse`
- `FListBlueprintPinsCommand` / `FListBlueprintPinsResponse`

### Batch Operations
- `FBatchExecuteCommand` / `FBatchExecuteResponse`

## JSON Value Conversion

The module handles bidirectional JSON <-> C++ conversion:

```cpp
// In CommandExecutor
FAgentPropertyValue JsonToPropertyValue(const FString& JsonString);
FString PropertyValueToJson(const FAgentPropertyValue& Value);
```

Supports: Bool, Int, Float, String, Vector, Rotator, Transform, Color, Arrays, Structs.

Also accepts UE format strings like `(X=1.0,Y=2.0,Z=3.0)` and `(R=1,G=0,B=0,A=1)`.

## Blueprint Node Commands

Full implementation complete across all layers (protos, gRPC handlers, MCP tools).

### Commands

| Command | Response | Notes |
|---------|----------|-------|
| `FCreateBlueprintNodeCommand` | `FCreateBlueprintNodeResponse` | Complete |
| `FConnectBlueprintPinsCommand` | `FAgentResponseBase` | Complete |
| `FDisconnectBlueprintPinsCommand` | `FAgentResponseBase` | Complete |
| `FDeleteBlueprintNodeCommand` | `FAgentResponseBase` | Complete |
| `FListBlueprintNodesCommand` | `FListBlueprintNodesResponse` | Complete |
| `FListBlueprintPinsCommand` | `FListBlueprintPinsResponse` | Complete |

### MCP Tools

- `bp_create_node` - Create nodes (CallFunction, Event, Variable, Branch, Sequence, Comment)
- `bp_connect_pins` - Connect two Blueprint pins
- `bp_disconnect_pins` - Disconnect Blueprint pins
- `bp_delete_node` - Delete a node from a graph
- `bp_list_nodes` - List all nodes in a graph
- `bp_list_pins` - List all pins on a node

### Supported Node Types

- `CallFunction` - Requires `FunctionReference` like `KismetSystemLibrary.PrintString`
- `Event` - Requires `EventName` like `ReceiveBeginPlay`
- `VariableGet` / `VariableSet` - Requires `VariableName`
- `Branch` - If/Then/Else node
- `Sequence` - Execution sequence
- `Comment` - Comment box

### Key Implementation Details

```cpp
// Node creation pattern (in CommandExecutor.cpp)
UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
Graph->AddNode(CallNode, false, false);
CallNode->SetFromFunction(Function);
CallNode->AllocateDefaultPins();
CallNode->NodePosX = PosX;
CallNode->NodePosY = PosY;
FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

// Pin connection pattern
const UEdGraphSchema* Schema = Graph->GetSchema();
Schema->TryCreateConnection(SourcePin, TargetPin);
```

### Implementation Layers

1. **gRPC protos** in `AgentBridge.proto` - 6 messages + 6 RPCs
2. **gRPC handlers** in `AgentBridgeServiceSubsystem.cpp` - Proto <-> Command conversion
3. **Python MCP tools** in `agentbridge.py` - 6 tools with response handling
4. **End-to-end tested**: Blueprint node creation, connection, listing works via MCP

### Editor-Only Dependencies for Blueprint Nodes

```csharp
// In AgentBridgeScripting.Build.cs (editor-only)
if (Target.bBuildEditor)
{
    PrivateDependencyModuleNames.AddRange(new string[] {
        "BlueprintGraph",    // K2Node classes
        "KismetCompiler",    // Blueprint compilation
    });
}
```

## Resolved Issues

### Resolved: TArray Property Setting

Setting array properties now works correctly. Two issues were fixed:

1. **Python MCP Layer:** `_normalize_property_value()` used `str()` on lists, producing single
   quotes. Changed to `json.dumps()` for proper JSON with double quotes.

2. **C++ Scripting Layer:** `JsonToPropertyValue()` now parses JSON arrays into
   `FAgentPropertyValue` with `Type = Array` and populated `ArrayValue`. Previously it stored
   arrays as raw strings.

```python
# This now works:
set_property("ArrayTestCube", "Tags", '["TestTag1", "TestTag2"]')
# Result: Tags = [TestTag1, TestTag2]
```

### Resolved: GET Property Returns Empty

Reading properties now returns actual typed values instead of empty strings. Two issues fixed:

1. **C++ Scripting Layer:** `Response.TypeName` was set to numeric enum value like `"3"` instead
   of `"Float"`. Added `PropertyTypeToString()` helper to convert enums to string names that
   `JsonToProtoPropertyValue()` can match with `Contains()`.

2. **Python MCP Layer:** Handler only extracted `result.value.string_value`. Added
   `_extract_property_value()` to read typed fields (`float_value`, `vector_value`, etc.).

```python
# All now return proper typed values:
get_property("Light", "LightComponent0.Intensity")      # -> 5000.0
get_property("Cube", "RootComponent.RelativeLocation")  # -> {"x": 0, "y": 0, "z": 100}
get_property("Actor", "bHidden")                        # -> False
```

### Resolved: Nested Struct Property Writes

Writing to nested struct properties now works correctly. The fix was in AgentBridgeCore:
- Added `WritePropertyDirect()` for pre-resolved value pointers
- Path resolution returns direct pointers, so we skip `ContainerPtrToValuePtr()` offset

### Resolved: UObject Property Access

Property operations (`get_property`, `set_property`) now work on **both actors AND assets**:
- Pass actor name/label/GUID - works as before
- Pass asset path like `/Game/Data/MyAsset.MyAsset` - loads and accesses the asset
- The tool automatically figures out which resolution method to use via `ResolveObject()`

```python
# These all work now:
get_property(actor_id="MyActor", path="Health")           # Actor by name
get_property(actor_id="/Game/Data/BiomeDef", path="BiomeColor")  # DataAsset
set_property(actor_id="/Game/Data/BiomeDef", path="BiomeColor", value="(R=1,G=0,B=0,A=1)")
```

## Known Limitations

### TSoftObjectPtr Assignment

Cannot assign `TSoftObjectPtr<>` properties directly - use `TObjectPtr<>` where possible.

**Status:** Complex UE limitation, workaround is to use TObjectPtr properties.

### call_function Argument Support

gRPC `CallFunction` only supports zero-arg void return UFunctions. Use `set_property` /
`get_property` for parameterized operations. Full argument support is a future feature.

### Plugin Content Duplication

`duplicate_asset` from engine/plugin content paths crashes the editor. Always duplicate from
`/Game/` paths. Pre-copy plugin templates to `/Game/` first.

### BoxExtent Visual Update

`set_property` on BoxExtent stores the value but the wireframe does not update.
PostEditChangeProperty approach was attempted and reverted (caused regression - zeroed
vector/struct properties). Use `set_transform` on component scale for sizing.

## Dependencies

```csharp
// AgentBridgeScripting.Build.cs
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "Json",
    "JsonUtilities",
    "AgentBridgeCore",
    "AgentBridgeRuntime",
    "AssetRegistry",
    "ImageWrapper",
    "RenderCore",
    "RHI",
});

// Editor-only features
if (Target.bBuildEditor)
{
    PrivateDependencyModuleNames.AddRange(new string[] {
        "UnrealEd",
        "BlueprintGraph",
        "KismetCompiler",
    });
}
```

## Plugin Configuration

| Setting | Value |
|---------|-------|
| Module Type | Runtime |
| Loading Phase | Default |
| Enabled By Default | true |
| Can Contain Content | false |

## Stretch Goals

| Feature | Effort | Notes |
|---------|--------|-------|
| INI/Config automation | Medium | Read/write DefaultEngine.ini, DefaultGame.ini |
| Function parameters | Medium | Full `call_function(params={...})` support |
