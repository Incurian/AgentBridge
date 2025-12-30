# AgentBridge Plugin

> UE 5.7 plugin exposing editor/runtime state to external AI agents via gRPC + MCP.
> Primary use case: "Build me a level" — agents need full read/write/discover capabilities.

## Important Paths

### Engine
- **Engine root:** `D:\UE571`
- **Engine source:** `D:\UE571\Engine\Source\Runtime` (and `/Editor`, `/Developer`)
- **Engine logs:** `D:\UE571\Engine\Saved\Logs\Unreal.log`

### Project
- **Project root:** `E:\UnrealProjects\VR_Project`
- **Project logs:** `E:\UnrealProjects\VR_Project\Saved\Logs\VR_Project.log` (most recent)
- **Crash logs:** `E:\UnrealProjects\VR_Project\Saved\Crashes\`
- **UBT config:** `E:\UnrealProjects\VR_Project\Saved\UnrealBuildTool\BuildConfiguration.xml`

### Build
- **UBT log:** `D:\UE571\Engine\Programs\UnrealBuildTool\Log.txt` (compile errors, linker errors, build config)

### When to Check Each Log
| Log | When to Check |
|-----|---------------|
| `VR_Project.log` | Runtime errors, PIE issues, Blueprint errors, plugin load failures |
| `Engine\Saved\Logs\Unreal.log` | Editor crashes, low-level engine issues |
| `Saved\Crashes\` | Hard crashes with minidumps |
| **UBT Log.txt** | **Compile errors, linker errors, missing includes** (persisted!) |

## Architecture

```
External Agents (Claude, LLMs)
         │
         ▼
MCP Server (Python) ─ Tools: spawn, modify, query
         │
         ▼
Python gRPC Client ─ agentbridge.AgentBridgeClient
         │
         ▼ (gRPC over localhost:50051)
AgentBridgeServer (UE Module) ─ gRPC service, game thread dispatch
         │
         ▼
AgentBridgeScripting (UE Module) ─ High-level commands, undo, validation
         │
         ▼
AgentBridgeRuntime (UE Module) ─ World context, actor ops, property paths
         │
         ▼
AgentBridgeCore (UE Module) ─ FProperty access, UFunction invoke, type discovery
         │
         ▼
Unreal Engine 5.7 ─ Reflection System, World, Actors
```

## Module Structure

```
Plugins/AgentBridge/
├── AgentBridge.uplugin
├── Source/
│   ├── AgentBridgeCore/        # Reflection primitives
│   ├── AgentBridgeRuntime/     # Abstraction & helpers
│   ├── AgentBridgeScripting/   # High-level operations
│   └── AgentBridgeServer/      # gRPC server
├── Protos/                     # Protobuf definitions
└── Python/                     # Python client & MCP
```

## Critical Technical Gotchas

### Blueprint vs C++ Reflection

**The `_C` suffix:** Blueprint classes have TWO objects:
- `BP_MyActor` — the `UBlueprint` asset (editor-only)
- `BP_MyActor_C` — the `UBlueprintGeneratedClass` (runtime class)

```cpp
// WRONG - references the asset
LoadObject<UClass>(nullptr, TEXT("/Game/BP_MyActor.BP_MyActor"));
// CORRECT - references the generated class
UClass* Class = LoadClass<AActor>(nullptr, TEXT("/Game/BP_MyActor.BP_MyActor_C"));
```

**Property name mangling:** BP variables have GUID suffixes like `PropertyName_23_abc123`. Use `GetAuthoredName()` for clean display names.

**BP detection:**
```cpp
bool bIsBlueprintClass = Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
bool bIsBlueprintProperty = !Property->GetOwnerClass()->IsNative();
```

**Metadata is editor-only** — stripped in shipping builds. Use property flags (`CPF_*`) for runtime functionality.

### UObject Pointer Types

| Type | Reflection Class | GC Behavior |
|------|------------------|-------------|
| `UObject*` / `TObjectPtr<>` | `FObjectProperty` | Prevents GC |
| `TSoftObjectPtr<>` | `FSoftObjectProperty` | Path-based, no GC prevention |
| `TWeakObjectPtr<>` | `FWeakObjectProperty` | Auto-nulls when target GC'd |
| `TSubclassOf<>` | `FClassProperty` | Prevents GC of UClass |

**Thread safety:** Most UObject operations require game thread. Use `TWeakObjectPtr` for cross-thread storage.

### Nested Arrays/Structs

**Critical insight:** At each nesting level, the element becomes the container for inner properties.

```cpp
// Array iteration
FScriptArrayHelper Helper(ArrayProp, ArrayPtr);
for (int32 i = 0; i < Helper.Num(); i++) {
    void* ElementPtr = Helper.GetRawPtr(i);
    // ElementPtr IS the container for Inner property
}

// Map iteration - SPARSE INDICES!
for (int32 i = 0; i < Helper.GetMaxIndex(); i++) {
    if (Helper.IsValidIndex(i)) { /* use it */ }
}
```

### PIE and Runtime Contexts

**Critical:** `GIsEditor` remains TRUE during PIE. Use `World->WorldType` for accurate detection.

| Feature | Editor | PIE | Packaged |
|---------|--------|-----|----------|
| FProperty iteration | Yes | Yes | Yes |
| Property metadata | Yes | Yes | No (stripped) |
| GEditor pointer | Yes | Yes | Null |
| Transactions/Undo | Yes | No | No |

Use `#if WITH_EDITOR` for editor-only code. Use `GIsEditor` for runtime behavior branching.

## Key Patterns

### Safe Object Access
```cpp
TWeakObjectPtr<AActor> WeakActor = SomeActor;
if (AActor* Actor = WeakActor.Get()) { /* Safe */ }
```

### Game Thread Dispatch
```cpp
AsyncTask(ENamedThreads::GameThread, [WeakObj = TWeakObjectPtr<UObject>(MyObj)]() {
    if (UObject* Obj = WeakObj.Get()) { /* Safe UObject work */ }
});
```

### Transaction Wrapper (Undo Support)
```cpp
{
    FScopedAgentTransaction Trans(LOCTEXT("ModifyActor", "Agent: Modify Actor"));
    Trans.Modify(Actor);
    Actor->SetActorLocation(NewLocation);
}  // Undo point created
```

## Build Commands

```bash
# Compile plugin (from bash/terminal)
"D:/UE571/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" \
  VR_ProjectEditor Win64 Development \
  -Project="E:/UnrealProjects/VR_Project/VR_Project.uproject" -WaitMutex
```

## Testing Commands

Run console commands headlessly (takes ~60-90s for editor startup):

```bash
# Run command and check log
"D:/UE571/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "E:/UnrealProjects/VR_Project/VR_Project.uproject" \
  -ExecCmds="AgentBridge.ListWorlds,Quit" \
  -unattended -NullRHI -nosplash -nosound

# Then grep the log for output
grep "LogAgentBridge" "E:/UnrealProjects/VR_Project/Saved/Logs/VR_Project.log"
```

**Available test commands:**
- `AgentBridge.ListWorlds` - verify plugin loads, show world contexts
- `AgentBridge.DumpActor Floor` - dump actor properties (55 actors in VRTemplateMap)
- `AgentBridge.DumpClass StaticMeshActor` - dump class schema

## Common Build Errors

| Error Pattern | Cause | Fix |
|--------------|-------|-----|
| `Expected a GENERATED_BODY()` | Missing macro | Add GENERATED_BODY() after access specifier |
| `Unrecognized type 'X'` | Type not reflected | Add USTRUCT/UCLASS or #include |
| `LNK2019 unresolved external` | Missing dependency | Add to PublicDependencyModuleNames in .Build.cs |
| `.generated.h must be last` | Include order | Move .generated.h to end of includes |

## Console Commands (Debug)

```
AgentBridge.DumpActor <ActorName>   - Dump reflection data
AgentBridge.DumpClass <ClassName>   - Dump class schema
AgentBridge.TestPath <Actor> <Path> - Test property path resolution
AgentBridge.ListWorlds              - List all world contexts
```

## Reference: AgentBridge_Handover.md

For complete implementation details including:
- Full header file definitions for all modules
- Protobuf service definitions
- Python client structure
- Detailed implementation order (8-week plan)
- Risk mitigation strategies

See `AgentBridge_Handover.md` in this directory.