# AgentBridgeScripting - Command Dispatch

**Plugin:** `AgentBridgeScripting/`
**Dependencies:** AgentBridgeCore, AgentBridgeRuntime, Core, CoreUObject, Engine, Json, AssetRegistry, UnrealEd (editor-only)
**Depended on by:** Server

AgentBridgeScripting contains the central command dispatcher (`FCommandExecutor`) and all
command/response structs. It's the thickest layer (~7400 lines in CommandExecutor.cpp) -
all business logic lives here to avoid gRPC header conflicts in the Server module.

## Class Diagram

```mermaid
classDiagram
    direction TB

    class FCommandExecutor {
        <<static, 7400 lines>>
        +ExecuteJson(json)$ FString
        +ExecuteBatchJson(json)$ FString
        +Execute(FListWorldsCommand)$
        +Execute(FQueryActorsCommand)$
        +Execute(FGetActorCommand)$
        +Execute(FSpawnActorCommand)$
        +Execute(FDeleteActorCommand)$
        +Execute(FDuplicateActorCommand)$
        +Execute(FGetPropertyPathCommand)$
        +Execute(FSetPropertyPathCommand)$
        +Execute(FCallFunctionCommand)$
        +Execute(FCallAssetFunctionCommand)$
        +Execute(FFindClassCommand)$
        +Execute(FGetClassSchemaCommand)$
        +Execute(FListClassesCommand)$
        +Execute(FCreateAssetCommand)$
        +Execute(FSaveAssetCommand)$
        +Execute(FDuplicateAssetCommand)$
        +Execute(FSetTransformCommand)$
        +Execute(FGetTransformCommand)$
        +Execute(FAttachCommand)$
        +Execute(FDetachCommand)$
        +Execute(FCreateBlueprintNodeCommand)$
        +Execute(FConnectBlueprintPinsCommand)$
        +Execute(FListBlueprintNodesCommand)$
        +Execute(FListBlueprintPinsCommand)$
        + 25 more Execute overloads
        -StartTiming()$
        -EndTiming()$
        -ResolveActor(name)$ AActor*
        -ResolveObject(name)$ UObject*
        -BuildActorInfo(Actor)$ FActorInfo
        -IsPathAllowed(path)$ bool
        -ToAbsoluteProjectPath(rel)$ FString
    }

    class FAgentCommandBase {
        <<abstract base>>
        +CommandId: FString
        +Type: EAgentCommandType
    }

    class FAgentResponseBase {
        <<abstract base>>
        +CommandId: FString
        +bSuccess: bool
        +ErrorMessage: FString
        +ExecutionTimeMs: double
    }

    class EAgentCommandType {
        <<enum, 116+ values>>
        None
        ListWorlds SetTargetWorld GetCapabilities
        QueryActors GetActor SpawnActor DeleteActor
        DuplicateActor SetActorProperties SetActorTransform
        GetPropertyPath SetPropertyPath
        CallFunction CallAssetFunction GetFunctionSignature
        FindClass GetClassSchema ListClasses
        CreateAsset SaveAsset DuplicateAsset
        SetTransform GetTransform Attach Detach
        CreateBlueprintNode ConnectBlueprintPins
        DisconnectBlueprintPins DeleteBlueprintNode
        ListBlueprintNodes ListBlueprintPins
        ReadProjectFile WriteProjectFile
        ListProjectDirectory CopyProjectFile
        ExecuteConsoleCommand SearchConsoleCommands
        IsWorldPartitioned QueryAllActors
        GetStreamingState QueryLandscape
        GetLandscapeBounds GetDataLayers
        ...
    }

    class FActorInfo {
        <<response struct>>
        +Guid: FString
        +Path: FString
        +Name: FString
        +Label: FString
        +ClassName: FString
        +Location: FVector
        +Rotation: FRotator
        +Scale: FVector
        +bHidden: bool
        +ParentActorId: FString
        +Properties: TMap~FString,FString~
        +Components: TMap~FString,FString~
    }

    class FWorldInfo {
        <<response struct>>
        +WorldType: FString
        +WorldName: FString
        +PIEInstance: int32
        +bHasBegunPlay: bool
        +ActorCount: int32
    }

    class FBlueprintNodeInfo {
        <<response struct>>
        +Guid: FString
        +ClassName: FString
        +Title: FString
        +PosX: int32
        +PosY: int32
        +Comment: FString
        +FunctionReference: FString
        +EventName: FString
        +VariableName: FString
        +Pins: TArray~FBlueprintPinInfo~
    }

    class FBlueprintPinInfo {
        <<response struct>>
        +Name: FString
        +Direction: FString
        +Type: FString
        +TypeDisplayName: FString
        +bIsConnected: bool
        +DefaultValue: FString
        +ConnectedTo: TArray~FString~
    }

    class ExampleCommands["Example Command Structs"] {
        FQueryActorsCommand: ClassName, NamePattern, LabelPattern, Tag, Limit
        FSpawnActorCommand: ClassName, Location, Rotation, Scale, Label
        FSetPropertyPathCommand: ActorId, Path, Value, ValueType
        FSetTransformCommand: Target, Location, Rotation, Scale, bWorldSpace, bOffset
        FCreateBlueprintNodeCommand: BlueprintPath, GraphName, NodeType, FunctionReference
        FCallFunctionCommand: Call, Parameters
    }

    %% Relationships
    FCommandExecutor ..> FAgentCommandBase : executes
    FCommandExecutor ..> FAgentResponseBase : returns
    FCommandExecutor ..> FActorInfo : builds
    FCommandExecutor ..> FWorldInfo : builds
    FCommandExecutor ..> FBlueprintNodeInfo : builds
    FAgentCommandBase --> EAgentCommandType : typed by
    ExampleCommands --|> FAgentCommandBase : extend
    FBlueprintNodeInfo o-- FBlueprintPinInfo : contains
```

## Key Design

### FCommandExecutor - The Central Dispatcher

Every gRPC and HTTP request ultimately calls one of `FCommandExecutor`'s 50+ overloaded
`Execute()` methods. Each follows the same pattern:

```cpp
void FCommandExecutor::Execute(const FMyCommand& Cmd, FMyResponse& Response)
{
    StartTiming();

    // 1. Validate inputs
    // 2. Resolve actors/objects
    // 3. Perform operation (delegates to Runtime/Core)
    // 4. Build response

    EndTiming(Response);
}
```

### Actor Resolution

`ResolveActor(name)` tries multiple strategies: GUID, object path, exact name match,
label match, substring match. `ResolveObject(name)` additionally tries loading as an asset
path - this is used for `call_function` which can target assets (like PCG graphs), not
just actors.

### Blueprint Node Operations (WITH_EDITOR)

Blueprint editing is implemented directly in CommandExecutor using UE's `BlueprintGraph`
module APIs:
- Creates `UK2Node_CallFunction`, `UK2Node_Event`, `UK2Node_IfThenElse`, etc.
- Uses `UEdGraphSchema_K2::TryCreateConnection()` for pin connections
- Marks blueprints modified via `FBlueprintEditorUtils`

### PCG Graph Operations

PCG tools route through `FCallAssetFunctionCommand` - they call UFunctions on `UPCGGraph`
objects (e.g., `AddNodeOfType`, `AddEdge`, `RemoveNode`). This works because PCG graphs
expose their editing API as BlueprintCallable UFunctions.

### Command Struct Pattern

All 50+ command structs inherit from `FAgentCommandBase` and set their `Type` in the
constructor. Response structs inherit from `FAgentResponseBase` which provides `bSuccess`,
`ErrorMessage`, and `ExecutionTimeMs`.

## Files

| File | Contents |
|------|----------|
| `Public/AgentBridgeScripting.h` | `FAgentBridgeScriptingModule` |
| `Public/AgentCommands.h` | `EAgentCommandType`, `FAgentCommandBase`, `FAgentResponseBase`, 50+ command structs, info structs (~2400 lines) |
| `Public/CommandExecutor.h` | `FCommandExecutor` declaration (~330 lines) |
| `Private/CommandExecutor.cpp` | `FCommandExecutor` implementation (~7400 lines) |
