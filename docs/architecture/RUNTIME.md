# AgentBridgeRuntime - World Operations

**Plugin:** `AgentBridgeRuntime/`
**Dependencies:** AgentBridgeCore, Core, CoreUObject, Engine, Landscape
**Depended on by:** Scripting, Server

AgentBridgeRuntime provides world context management, actor operations, target resolution,
and World Partition support. It bridges Core's reflection primitives and Scripting's
command dispatch.

## Class Diagram

```mermaid
classDiagram
    direction TB

    class FAgentBridgeRuntimeModule {
        <<IModuleInterface>>
        +StartupModule()
        +ShutdownModule()
    }

    class FWorldContextManager {
        <<singleton>>
        -WorldOverride: TWeakObjectPtr~UWorld~
        +Get()$ FWorldContextManager
        +GetTargetWorld() UWorld*
        +SetTargetWorldOverride(World)
        +ClearTargetWorldOverride()
        +HasWorldOverride() bool
        +IsEditorWorld() bool
        +IsPIEWorld() bool
        +IsGameWorld() bool
        +IsGameplayActive() bool
        +GetCapabilities() FWorldContextCapabilities
        +GetWorldTypeString() FString
        +GetAllPIEWorlds() TArray~UWorld*~
        +GetEditorWorld() UWorld*
        -ResolveWorld() UWorld*
    }

    class FWorldContextCapabilities {
        <<struct>>
        +WorldType: FString
        +WorldName: FString
        +bIsGameplayActive: bool
        +PIEInstance: int32
        +bCanIterateProperties: bool
        +bCanInvokeFunctions: bool
        +bCanSpawnActors: bool
        +bCanDestroyActors: bool
        +bCanModifyTransforms: bool
        +bCanModifyProperties: bool
        +bCanSetActorLabel: bool
        +bCanSetActorFolder: bool
        +bCanUseTransactions: bool
        +bHasPropertyMetadata: bool
        +bCanAccessEditorWorld: bool
    }

    class FActorOperations {
        <<static>>
        +QueryActors(Params, World)$ TArray~FActorReference~
        +GetAllActors(World, Limit)$ TArray~FActorReference~
        +FindActorByName(SearchString, World)$ AActor*
        +FindActorByGuid(Guid, World)$ AActor*
        +ResolveActorReference(Ref, World)$ AActor*
        +SpawnActor(Params, World, OutError)$ AActor*
        +DuplicateActor(Source, Transform, Label)$ AActor*
        +DestroyActor(Actor)$ bool
        +DestroyActors(Actors)$ int32
        +SetActorTransform(Actor, Transform, bSweep)$ bool
        +SetActorProperties(Actor, Properties)$ bool
        +GetActorProperties(Actor, Names)$ TMap
        +SetActorLabel(Actor, Label)$ bool
        +SetActorFolder(Actor, FolderPath)$ bool
        +AttachActor(Child, Parent)$ bool
    }

    class FActorReference {
        <<struct>>
        +Guid: FString
        +Path: FString
        +Name: FString
        +Label: FString
        +ClassName: FString
        +Resolve(World) AActor*
        +FromActor(Actor)$ FActorReference
        +IsValid() bool
    }

    class FActorSpawnParams {
        <<struct>>
        +ClassPath: FString
        +Transform: FTransform
        +ActorLabel: FString
        +FolderPath: FString
        +InitialProperties: TMap~FString,FAgentPropertyValue~
        +CollisionHandling: ESpawnActorCollisionHandlingMethod
    }

    class FActorQueryParams {
        <<struct>>
        +ClassFilter: UClass*
        +NamePattern: FString
        +LabelPattern: FString
        +Tag: FString
        +BoundsFilter: TOptional~FBox~
        +Limit: int32
        +bIncludeHidden: bool
    }

    class TargetResolution {
        <<namespace>>
        +Parse(Target)$ FTargetInfo
        +FindComponent(Actor, ComponentName)$ USceneComponent*
        +FindAnyComponent(Actor, ComponentName)$ UActorComponent*
        +Resolve(World, Target, OutError)$ FResolvedTarget
        +ResolveAttachmentTargets(World, Child, Parent, OutChild, OutParent, OutError)$ bool
    }

    class FTargetInfo {
        <<struct, in AgentBridge namespace>>
        +ActorPart: FString
        +ComponentPart: FString
        +IsComponent() bool
        +IsActor() bool
        +IsValid() bool
    }

    class FResolvedTarget {
        <<struct, in AgentBridge namespace>>
        +Actor: AActor*
        +Component: USceneComponent*
        +Error: FString
        +IsValid() bool
        +IsComponent() bool
        +HasError() bool
        +GetSceneComponent() USceneComponent*
    }

    class FWorldPartitionOps {
        <<static>>
        +IsWorldPartitioned(World)$ bool
        +GetWorldPartition(World)$ UWorldPartition*
        +QueryAllActors(Params, World)$ TArray~FStreamingActorReference~
        +GetActorStreamingState(Guid, World)$ EActorStreamingState
        +FindActorByGuidEx(Guid, World)$ FStreamingActorReference
        +QueryLandscapeProxies(World, bIncludeUnloaded)$ TArray~FStreamingActorReference~
        +GetMainLandscape(World)$ ALandscapeProxy*
        +GetLandscapeBounds(World)$ FLandscapeBounds
        +GetDataLayers(World)$ TArray~FName~
        +GetActorsInDataLayer(Name, bUnloaded, World)$ TArray
    }

    class FStreamingActorReference {
        <<struct>>
        +StreamingState: EActorStreamingState
        +StreamingCellName: FString
        +EditorBounds: FBox
        +DataLayers: TArray~FName~
        +bIsSpatiallyLoaded: bool
        +Transform: FTransform
        +FromLoadedActor(Actor)$ FStreamingActorReference
    }

    class EActorStreamingState {
        <<enum>>
        NotApplicable
        Loaded
        Unloaded
        Invalid
    }

    class FLandscapeBounds {
        <<struct>>
        +bValid: bool
        +Min: FVector
        +Max: FVector
        +Center: FVector
        +Extent: FVector
        +BiomeVolumeScale: FVector
        +ProxyCount: int32
        +LandscapeName: FString
    }

    class FAgentBridgeDebug {
        <<static>>
        -RegisteredCommands: TArray~IConsoleObject*~
        +RegisterCommands()$
        +UnregisterCommands()$
        +DumpObject(Object, MaxDepth)$
        +DumpClassSchema(Class)$
        +ListWorlds()$
    }

    %% Relationships
    FStreamingActorReference --|> FActorReference : extends
    FWorldContextManager ..> FWorldContextCapabilities : returns
    FActorOperations --> FWorldContextManager : resolves world
    FActorOperations ..> FActorReference : returns
    FActorOperations ..> FActorSpawnParams : input param
    FActorOperations ..> FActorQueryParams : input param
    TargetResolution ..> FTargetInfo : parses into
    TargetResolution ..> FResolvedTarget : resolves into
    TargetResolution --> FActorOperations : FindActorByName
    FWorldPartitionOps --> FWorldContextManager : resolves world
    FWorldPartitionOps ..> FStreamingActorReference : returns
    FWorldPartitionOps ..> FLandscapeBounds : returns
    FStreamingActorReference --> EActorStreamingState : typed by
    FAgentBridgeDebug --> FActorOperations : uses
    FAgentBridgeDebug --> FWorldContextManager : uses
    FAgentBridgeDebug --> FWorldPartitionOps : uses
    FAgentBridgeRuntimeModule --> FAgentBridgeDebug : registers commands
```

## Key Classes

### FWorldContextManager (singleton)

Manages which UWorld is the "target" for all operations. Resolution order:
1. Explicit override (set via `SetTargetWorldOverride`)
2. PIE world (if playing)
3. Editor world (fallback)

All Runtime and Scripting operations call `FWorldContextManager::Get().GetTargetWorld()`
to determine which world to operate on.

### FActorOperations (static)

High-level actor CRUD operations. Resolution cascade for `FindActorByName()`:
GUID -> Object Path -> GetName() exact match -> Label exact match -> Name substring -> Label substring.

### TargetResolution (namespace)

Parses the `"Actor->Component"` syntax used by unified transform/attach tools.
`Parse("MyLight->LightComponent0")` returns `FTargetInfo{ActorPart="MyLight", ComponentPart="LightComponent0"}`.
Component search: exact match -> case-insensitive -> partial match.

### FWorldPartitionOps (static)

World Partition-aware operations that can query unloaded actors via actor descriptors.
Essential for large open worlds where most actors are streamed out.

### FAgentBridgeDebug (static)

Registers ~30 console commands (e.g., `AgentBridge.DumpActor`, `AgentBridge.QueryActors`)
for debugging without MCP/gRPC. Useful during development.

## Files

| File | Contents |
|------|----------|
| `Public/AgentBridgeRuntime.h` | `FAgentBridgeRuntimeModule` |
| `Public/WorldContextManager.h` | `FWorldContextManager`, `FWorldContextCapabilities` |
| `Public/ActorOperations.h` | `FActorOperations`, `FActorReference`, `FActorSpawnParams`, `FActorQueryParams` |
| `Public/TargetResolution.h` | `TargetResolution` namespace, `FTargetInfo`, `FResolvedTarget` |
| `Public/WorldPartitionOps.h` | `FWorldPartitionOps`, `FStreamingActorReference`, `EActorStreamingState`, `FLandscapeBounds` |
| `Public/AgentBridgeDebug.h` | `FAgentBridgeDebug` |
