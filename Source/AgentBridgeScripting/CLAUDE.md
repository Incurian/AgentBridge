# AgentBridgeScripting Module

> Command layer with JSON serialization and dispatch to runtime operations.

## Purpose

This module provides the command/response abstraction used by both HTTP and gRPC servers:
- `AgentCommands.h` - All command and response structs
- `CommandExecutor.cpp` - Central dispatch, JSON handling, all business logic

## Key Files

| File | Purpose |
|------|---------|
| `AgentCommands.h` | Command/response structs (50+ types) |
| `CommandExecutor.h/.cpp` | JSON dispatch, implements all operations |

## Architecture

```
gRPC Request → Convert to Command Struct → CommandExecutor::Execute() → Response Struct → gRPC Response
HTTP Request → Parse JSON → CommandExecutor::Execute() → Response Struct → JSON Response
```

## Critical Pattern: Where to Put New Functionality

**ALL business logic goes in CommandExecutor.cpp**, NOT in AgentBridgeServer.

Why: AgentBridgeServer has gRPC headers that conflict with certain UE headers.

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

2. Implement in `CommandExecutor.cpp`:
```cpp
void FCommandExecutor::Execute(const FMyNewCommand& Cmd, FMyNewResponse& Response)
{
    // All logic here - can include ANY UE header
    Response.Result = DoTheThing(Cmd.SomeParam);
    Response.bSuccess = true;
}
```

3. Add gRPC RPC in `AgentBridge.proto`

4. Add thin handler in `AgentBridgeServiceSubsystem.cpp`:
```cpp
void UAgentBridgeServiceSubsystem::MyNewRpc(const MyNewRequest& Request,
    const TResponseDelegate<MyNewResponse>& Continuation)
{
    FMyNewCommand Cmd;
    Cmd.SomeParam = UTF8_TO_TCHAR(Request.some_param().c_str());

    FMyNewResponse CmdResponse;
    FCommandExecutor::Get().Execute(Cmd, CmdResponse);

    // Convert to proto and continue
}
```

## Implemented Commands

### World Operations
- `FListWorldsCommand` / `FListWorldsResponse`
- `FSetTargetWorldCommand`
- `FGetCapabilitiesCommand` / `FGetCapabilitiesResponse`

### Actor Operations
- `FQueryActorsCommand` / `FQueryActorsResponse`
- `FGetActorCommand` / `FGetActorResponse`
- `FSpawnActorCommand` / `FSpawnActorResponse`
- `FDeleteActorCommand`
- `FSetActorTransformCommand`

### Property Operations
- `FGetPropertyPathCommand` / `FGetPropertyPathResponse`
- `FSetPropertyPathCommand`

### Class Discovery
- `FListClassesCommand` / `FListClassesResponse`
- `FGetClassSchemaCommand` / `FGetClassSchemaResponse`
- `FCallStaticFunctionCommand`

### Asset Operations
- `FCreateAssetCommand`
- `FSaveAssetCommand`
- `FDuplicateAssetCommand`
- `FSaveActorAsBlueprintCommand`

### File Operations
- `FReadProjectFileCommand`
- `FWriteProjectFileCommand`
- `FListProjectDirectoryCommand`

## JSON Value Conversion

The module handles bidirectional JSON <-> C++ conversion:

```cpp
// In CommandExecutor
FAgentPropertyValue JsonToPropertyValue(const FString& JsonString);
FString PropertyValueToJson(const FAgentPropertyValue& Value);
```

Supports: Bool, Int, Float, String, Vector, Rotator, Transform, Color, Arrays, Structs

## Known Issues

### Nested Struct Property Writes (TOP PRIORITY)

Writing to nested struct properties (e.g., `DefaultDefinition.BiomeColor`) returns success but value is unchanged.

**Symptom:**
```python
set_property(actor="X", path="DefaultDefinition.BiomeColor", value="(R=0,G=1,B=0,A=1)")
# Returns success=true, but value remains unchanged
```

**Investigation Notes:**
- Reading nested struct properties works fine
- Writing to top-level struct properties works
- Issue appears specific to nested paths in Blueprint-generated structs
- May need to mark outer struct dirty or use different write approach

**Likely Fix Location:** `CommandExecutor.cpp` - `SetPropertyPath()` function

**Workaround:** Use pre-configured DataAssets with correct values.

**Status:** TOP PRIORITY - blocks PCG Biome custom configuration.

### TSoftObjectPtr Assignment

Cannot assign `TSoftObjectPtr<>` properties directly - use `TObjectPtr<>` where possible.

**Status:** Complex UE limitation, workaround is to use TObjectPtr properties.

## Todos

- [x] Asset creation (`CreateAsset`, `SaveAsset`, `DuplicateAsset`)
- [x] Component operations (`AttachComponent`, `DetachActor`, etc.)
- [x] File operations with sandboxing
- [ ] **TOP PRIORITY: Fix nested struct property writes** - see Known Issues above
- [ ] UObject property access (DataAssets, Materials - not just actors)

## Stretch Goals

| Feature | Effort | Notes |
|---------|--------|-------|
| INI/Config automation | Medium | Read/write DefaultEngine.ini, DefaultGame.ini |
| Graph editing | Very High | Blueprint nodes, Material nodes, PCG nodes |
| Function parameters | Medium | `tempo_call_function(params={...})` support |

## Dependencies

```csharp
// AgentBridgeScripting.Build.cs
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "AgentBridgeCore",
    "AgentBridgeRuntime",
    "Json",
    "JsonUtilities",
});

// Editor-only features
if (Target.bBuildEditor)
{
    PrivateDependencyModuleNames.Add("UnrealEd");
    PrivateDependencyModuleNames.Add("AssetRegistry");
}
```
