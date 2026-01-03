# BP Toolset Implementation Plan

**Date:** 2026-01-02
**Goal:** Enable full programmatic creation and manipulation of PCG graphs and Blueprint event graphs via MCP tools

---

## Executive Summary

Two distinct approaches are needed because PCG and Blueprint graphs have fundamentally different architectures:

| Aspect | PCG Graph | Blueprint Graph |
|--------|-----------|-----------------|
| Node storage | `Nodes[]` TArray | `UbergraphPages[].Nodes[]` |
| Node creation | `AddNodeOfType(TSubclassOf<UPCGSettings>)` | `ConstructObject<UK2Node>()` + pin setup |
| Connections | `AddEdge(From, FromPin, To, ToPin)` | Pin GUID linking in binary `Extras` |
| Settings | Separate `UPCGSettings` subobject | Properties on K2Node itself |
| Complexity | **Medium** - Clean API exists | **High** - Binary pin serialization |

**Recommended Approach:**
1. **Phase 1:** Add `call_asset_function` tool for PCG (enables immediate full manipulation)
2. **Phase 2:** Add high-level PCG helper tools (better UX)
3. **Phase 3:** Add Blueprint manipulation via Kismet Editor utilities

---

## Phase 1: Asset Function Calling (Foundation)

### Problem

Current MCP tools can call functions on:
- **Static functions** via `call_static_function` (KismetSystemLibrary, etc.)
- **World actors** via `tempo_call_function` (actors in the level)

But **not** on:
- UObject assets (PCGGraph, Blueprint, DataAsset, etc.)
- Subobjects within assets (UPCGNode, K2Node, etc.)

### Solution: `call_asset_function` Tool

**New MCP Tool:**
```python
def call_asset_function(
    asset_path: str,           # "/Game/MyAssets/MyPCGGraph"
    function_name: str,        # "AddNodeOfType"
    parameters: dict = None,   # {"SettingsClass": "PCGSurfaceSamplerSettings"}
    subobject_path: str = None # Optional: "Nodes[0]" to call on subobject
) -> dict:
    """Call a UFunction on a loaded UObject asset."""
```

**Implementation in C++ (AgentBridgeScripting):**

```cpp
// CommandExecutor.h - New command struct
struct FCallAssetFunctionCommand
{
    FString AssetPath;
    FString FunctionName;
    FString SubobjectPath;  // Optional
    TMap<FString, FString> Parameters;
};

// CommandExecutor.cpp - Handler
FAgentCommandResult UCommandExecutor::ExecuteCallAssetFunction(
    const FCallAssetFunctionCommand& Cmd)
{
    // 1. Load asset
    UObject* Asset = LoadObject<UObject>(nullptr, *Cmd.AssetPath);
    if (!Asset) return Error("Asset not found");

    // 2. Navigate to subobject if specified
    UObject* Target = Asset;
    if (!Cmd.SubobjectPath.IsEmpty())
    {
        Target = ResolveSubobjectPath(Asset, Cmd.SubobjectPath);
        if (!Target) return Error("Subobject not found");
    }

    // 3. Find and validate function
    UFunction* Func = Target->FindFunction(*Cmd.FunctionName);
    if (!Func) return Error("Function not found");

    // 4. Prepare parameters (reuse FunctionInvoker logic)
    void* Params = FMemory_Alloca(Func->ParmsSize);
    // ... parameter marshaling ...

    // 5. Call function
    Target->ProcessEvent(Func, Params);

    // 6. Extract return value if any
    return ExtractReturnValue(Func, Params);
}
```

**gRPC Proto Addition:**
```protobuf
message CallAssetFunctionRequest {
    string asset_path = 1;
    string function_name = 2;
    map<string, string> parameters = 3;
    string subobject_path = 4;
}

message CallAssetFunctionResponse {
    bool success = 1;
    string error = 2;
    string return_value = 3;
}

service AgentBridge {
    // ... existing RPCs ...
    rpc CallAssetFunction(CallAssetFunctionRequest) returns (CallAssetFunctionResponse);
}
```

### Files to Modify

| File | Changes |
|------|---------|
| `AgentBridgeScripting/Public/AgentCommands.h` | Add `FCallAssetFunctionCommand` |
| `AgentBridgeScripting/Public/CommandExecutor.h` | Add handler declaration |
| `AgentBridgeScripting/Private/CommandExecutor.cpp` | Implement handler |
| `AgentBridgeServer/Public/AgentBridge.proto` | Add RPC and messages |
| `AgentBridgeServer/Private/AgentBridgeServiceSubsystem.cpp` | Wire up gRPC handler |
| `Python/mcp/services/agentbridge.py` | Add MCP tool wrapper |

### Effort: ~4-6 hours

---

## Phase 2: PCG Graph Manipulation

### Prerequisite
Phase 1 (`call_asset_function`) must be complete.

### Available UPCGGraph Functions

From class schema analysis:

| Function | Signature | Purpose |
|----------|-----------|---------|
| `AddNodeOfType` | `UPCGNode* AddNodeOfType(TSubclassOf<UPCGSettings>)` | Create new node |
| `AddNodeCopy` | `UPCGNode* AddNodeCopy(UPCGSettings*)` | Clone from settings |
| `AddEdge` | `bool AddEdge(UPCGNode* From, FName FromPin, UPCGNode* To, FName ToPin)` | Connect nodes |
| `RemoveNode` | `void RemoveNode(UPCGNode*)` | Delete node |
| `RemoveEdge` | `bool RemoveEdge(UPCGNode* From, FName FromPin, UPCGNode* To, FName ToPin)` | Disconnect |
| `RemoveAllInboundEdges` | `void RemoveAllInboundEdges(UPCGNode*)` | Clear inputs |
| `RemoveAllOutboundEdges` | `void RemoveAllOutboundEdges(UPCGNode*)` | Clear outputs |

### PCG Settings Classes (Node Types)

```
PCGSurfaceSamplerSettings      - Surface point generation
PCGStaticMeshSpawnerSettings   - Mesh spawning
PCGFilterByTagSettings         - Tag-based filtering
PCGTransformPointsSettings     - Point transformation
PCGBranchSettings              - Control flow
PCGAddTagSettings              - Tag assignment
PCGDensityFilterSettings       - Density-based culling
PCGDistanceSettings            - Distance calculations
PCGCopyPointsSettings          - Point duplication
PCGProjectionSettings          - Surface projection
PCGDifferenceSettings          - Boolean difference
PCGIntersectionSettings        - Boolean intersection
PCGUnionSettings               - Boolean union
... (50+ more)
```

### Option A: Direct Function Calls (Minimal)

With `call_asset_function`, users can directly call PCG functions:

```python
# Create a surface sampler node
result = call_asset_function(
    asset_path="/Game/AgentBridge/MyPCG",
    function_name="AddNodeOfType",
    parameters={"SettingsClass": "PCGSurfaceSamplerSettings"}
)
node_path = result["return_value"]  # Path to new node

# Configure the node
set_property(
    actor_id=f"/Game/AgentBridge/MyPCG.MyPCG:{node_path}",
    path="SettingsInterface.PointsPerSquaredMeter",
    value="0.5"
)

# Connect to output
call_asset_function(
    asset_path="/Game/AgentBridge/MyPCG",
    function_name="AddEdge",
    parameters={
        "From": node_path,
        "FromPin": "Out",
        "To": "OutputNode",
        "ToPin": "In"
    }
)

# Save
save_asset("/Game/AgentBridge/MyPCG")
```

### Option B: High-Level PCG Tools (Better UX)

Create convenience wrappers that hide complexity:

```python
# New MCP tools
def pcg_add_node(
    graph_path: str,
    node_type: str,           # "SurfaceSampler", "StaticMeshSpawner", etc.
    position: tuple = (0, 0), # Graph position
    settings: dict = None     # Initial settings
) -> str:
    """Add a node to a PCG graph. Returns the node path."""

def pcg_connect(
    graph_path: str,
    from_node: str,
    from_pin: str,
    to_node: str,
    to_pin: str
) -> bool:
    """Connect two PCG nodes."""

def pcg_disconnect(
    graph_path: str,
    from_node: str,
    from_pin: str,
    to_node: str,
    to_pin: str
) -> bool:
    """Disconnect two PCG nodes."""

def pcg_delete_node(
    graph_path: str,
    node: str
) -> bool:
    """Delete a node from a PCG graph."""

def pcg_list_node_types() -> list:
    """List all available PCG node types."""
```

**Implementation:** These would be Python wrappers around `call_asset_function` + `set_property`.

### Effort:
- Option A (function calling only): Included in Phase 1
- Option B (high-level tools): ~2-3 hours additional

---

## Phase 3: Blueprint Graph Manipulation

### The Challenge

Blueprint nodes are fundamentally more complex than PCG nodes:

1. **Pin data is binary:** Stored in `Extras` field, not JSON-serializable properties
2. **GUID linking:** Pins are connected via GUIDs, not named references
3. **Schema reconstruction:** Pins must match the expected schema for the node type
4. **Event wiring:** Execution pins (white) vs data pins (colored) have different semantics

From `UASSET_DISSECTION_GUIDE.md`:
```
K2Node pins are stored in binary Extras field, not in JSON Data properties.
This makes programmatic node creation more complex - cloning existing nodes
(which preserves Extras) is easier than creating from scratch.
```

### Option A: Use ElgKismetEditorWidget (Recommended)

From `RESEARCH.md`, the **ElgKismetEditorWidget** plugin provides:

```cpp
// High-level Blueprint manipulation
UElgKismetLibrary::AddVariableToBlueprint(Blueprint, VarName, Type);
UElgKismetLibrary::AddFunctionToBlueprint(Blueprint, FuncName);
UElgKismetLibrary::AddNodeToGraph(Graph, NodeClass, Position);
UElgKismetLibrary::ConnectNodes(FromNode, FromPin, ToNode, ToPin);
```

**Approach:**
1. Check if ElgKismetEditorWidget is installed
2. If yes, use its functions via `call_static_function`
3. If no, fall back to limited bp_toolkit cloning

### Option B: Direct K2Node Manipulation (Complex)

If implementing from scratch:

```cpp
// CommandExecutor.cpp - Blueprint node creation
FAgentCommandResult CreateBlueprintNode(
    UBlueprint* BP,
    UEdGraph* Graph,
    const FString& NodeClassName,  // "K2Node_CallFunction"
    FVector2D Position)
{
    // 1. Find node class
    UClass* NodeClass = FindObject<UClass>(ANY_PACKAGE, *NodeClassName);
    if (!NodeClass) return Error("Node class not found");

    // 2. Create node
    UK2Node* NewNode = NewObject<UK2Node>(Graph, NodeClass);
    NewNode->NodePosX = Position.X;
    NewNode->NodePosY = Position.Y;

    // 3. Allocate default pins (CRITICAL)
    NewNode->AllocateDefaultPins();

    // 4. Add to graph
    Graph->AddNode(NewNode, false, false);

    // 5. Mark blueprint modified
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

    return Success(NewNode->GetPathName());
}
```

**Pin Connection:**
```cpp
FAgentCommandResult ConnectBlueprintPins(
    UK2Node* FromNode,
    const FString& FromPinName,
    UK2Node* ToNode,
    const FString& ToPinName)
{
    // 1. Find pins
    UEdGraphPin* FromPin = FromNode->FindPin(FName(*FromPinName));
    UEdGraphPin* ToPin = ToNode->FindPin(FName(*ToPinName));

    if (!FromPin || !ToPin) return Error("Pin not found");

    // 2. Verify compatibility
    if (!FromPin->GetSchema()->ArePinsCompatible(FromPin, ToPin, nullptr, false))
        return Error("Pins incompatible");

    // 3. Make connection
    FromPin->MakeLinkTo(ToPin);

    return Success();
}
```

### K2Node Types to Support

Priority order based on common usage:

| Node Type | Class | Purpose | Complexity |
|-----------|-------|---------|------------|
| Event | `K2Node_Event` | BeginPlay, Tick, Custom | Medium |
| Function Call | `K2Node_CallFunction` | Call any function | Medium |
| Variable Get | `K2Node_VariableGet` | Read variable | Low |
| Variable Set | `K2Node_VariableSet` | Write variable | Low |
| Branch | `K2Node_IfThenElse` | Conditional | Low |
| Sequence | `K2Node_ExecutionSequence` | Multiple outputs | Low |
| Cast | `K2Node_DynamicCast` | Type casting | Medium |
| Loop | `K2Node_ForEachLoop` | Iteration | Medium |
| Return | `K2Node_FunctionResult` | Function output | Low |
| Math | `K2Node_*` | Arithmetic | Low |

### Proposed MCP Tools

```python
def bp_create_node(
    blueprint_path: str,
    graph_name: str,           # "EventGraph", custom function name
    node_type: str,            # "CallFunction", "Event", "Branch", etc.
    position: tuple = (0, 0),
    properties: dict = None    # Node-specific: FunctionReference, EventReference, etc.
) -> str:
    """Create a Blueprint node. Returns node path."""

def bp_connect_pins(
    blueprint_path: str,
    graph_name: str,
    from_node: str,
    from_pin: str,
    to_node: str,
    to_pin: str
) -> bool:
    """Connect two Blueprint pins."""

def bp_delete_node(
    blueprint_path: str,
    graph_name: str,
    node: str
) -> bool:
    """Delete a Blueprint node."""

def bp_list_pins(
    blueprint_path: str,
    graph_name: str,
    node: str
) -> list:
    """List all pins on a Blueprint node."""

def bp_add_variable(
    blueprint_path: str,
    var_name: str,
    var_type: str,            # "bool", "int", "float", "FVector", etc.
    default_value: str = None
) -> bool:
    """Add a variable to a Blueprint."""

def bp_add_function(
    blueprint_path: str,
    func_name: str,
    inputs: list = None,      # [{"name": "X", "type": "float"}]
    outputs: list = None,
    is_pure: bool = False
) -> str:
    """Add a function to a Blueprint. Returns function graph name."""
```

### Implementation Strategy

1. **Check for ElgKismetEditorWidget** at runtime
2. **If available:** Route calls through its static library functions
3. **If not:** Implement direct K2Node manipulation (Phase 3b)

### Effort:
- ElgKismetEditorWidget integration: ~3-4 hours
- Direct K2Node manipulation: ~8-12 hours

---

## Implementation Priority

| Phase | Task | Effort | Impact |
|-------|------|--------|--------|
| 1a | `call_asset_function` tool | 4-6 hrs | **HIGH** - Unlocks PCG manipulation |
| 1b | gRPC/proto changes | 1-2 hrs | Required for 1a |
| 2a | Test PCG with function calls | 1 hr | Validates Phase 1 |
| 2b | High-level PCG tools | 2-3 hrs | Better UX |
| 3a | ElgKismetEditorWidget check | 1 hr | Determines 3b scope |
| 3b | Blueprint node tools | 4-12 hrs | Depends on 3a |

**Total estimated effort:** 13-25 hours

---

## Testing Plan

### Phase 1 Verification

```python
# Test 1: Load and call function on existing asset
result = call_asset_function(
    asset_path="/Game/PCG/TPL_BiomeCore_Generator",
    function_name="GetNodeCount"  # If exists
)
assert result["success"]

# Test 2: Add node to PCG graph
result = call_asset_function(
    asset_path="/Game/AgentBridge/Test_PCG",
    function_name="AddNodeOfType",
    parameters={"SettingsClass": "PCGSurfaceSamplerSettings"}
)
assert "PCGNode" in result["return_value"]
```

### Phase 2 Verification

```python
# Create complete PCG graph programmatically
graph = create_asset("PCGGraph", "/Game/Test", "TestPCG")

# Add nodes
sampler = pcg_add_node(graph, "SurfaceSampler", (0, 0), {
    "PointsPerSquaredMeter": 0.5
})
spawner = pcg_add_node(graph, "StaticMeshSpawner", (300, 0), {
    "MeshPath": "/Game/Meshes/Rock"
})

# Connect: Sampler -> Spawner -> Output
pcg_connect(graph, sampler, "Out", spawner, "In")
pcg_connect(graph, spawner, "Out", "OutputNode", "In")

# Connect Input -> Sampler
pcg_connect(graph, "InputNode", "Out", sampler, "In")

save_asset(graph)
```

### Phase 3 Verification

```python
# Create Blueprint with BeginPlay -> PrintString
bp = create_asset("Blueprint", "/Game/Test", "BP_Test")

# Add BeginPlay event
event = bp_create_node(bp, "EventGraph", "Event", (0, 0), {
    "EventReference": "ReceiveBeginPlay"
})

# Add PrintString call
print_node = bp_create_node(bp, "EventGraph", "CallFunction", (300, 0), {
    "FunctionReference": "/Script/Engine.KismetSystemLibrary:PrintString",
    "InString": "Hello from Claude!"
})

# Connect execution
bp_connect_pins(bp, "EventGraph", event, "then", print_node, "execute")

save_asset(bp)
```

---

## Alternative: bp_toolkit Enhancement

If runtime approach is blocked, bp_toolkit could be enhanced for UE 5.6:

1. **Fork UAssetAPI** and add VER_UE5_5/5_6 support
2. **Contribute upstream** to UAssetGUI
3. **Binary patching** - Fix size/offset fields post-export

However, this requires:
- Deep understanding of UE serialization changes between 5.4 and 5.6
- Significant reverse engineering effort
- Ongoing maintenance as UE versions change

**Recommendation:** Runtime MCP approach is more maintainable.

---

## Appendix: PCG Pin Names

Common pin names for PCG nodes:

| Node Type | Input Pins | Output Pins |
|-----------|------------|-------------|
| Input | - | `Out` |
| Output | `In` | - |
| SurfaceSampler | `Bounding Shape`, `Execution` | `Out` |
| StaticMeshSpawner | `In` | `Out` |
| FilterByTag | `In` | `Out`, `Filtered Out` |
| TransformPoints | `In` | `Out` |
| Branch | `In`, `Condition` | `True`, `False` |
| Union | `Primary`, `Secondary` | `Out` |
| Difference | `Source`, `Difference` | `Out` |

---

## Appendix: K2Node Pin Types

| Pin Category | Direction | Color | Purpose |
|--------------|-----------|-------|---------|
| Exec | Input | White | Execution flow in |
| Then | Output | White | Execution flow out |
| Boolean | Both | Red | Bool values |
| Integer | Both | Cyan | Int values |
| Float | Both | Green | Float values |
| String | Both | Magenta | String values |
| Object | Both | Blue | UObject refs |
| Struct | Both | Dark Blue | Struct values |
| Wildcard | Both | Gray | Any type |

---

*Plan created 2026-01-02 based on comprehensive bp_toolkit and MCP tool analysis*

---

## Implementation Progress (2026-01-02)

### Phase 1: call_asset_function - PARTIAL

| Step | Status | Notes |
|------|--------|-------|
| Add command structs | ✅ Done | `FCallAssetFunctionCommand/Response` |
| Add CommandExecutor | ✅ Done | Execute implementation |
| Add proto RPC | ✅ Done | `CallAssetFunction` |
| Add gRPC handler | ✅ Done | + registration in `RegisterScriptingServices` |
| Add Python tool | ✅ Done | In `agentbridge.py` |
| Test parameterless | ✅ Works | `GetInputNode()` returns correctly |
| Test with params | ❌ Crashes | `TSubclassOf<>` needs special handling |

### Discovered Issues

1. **Missing `FDuplicateActorCommand`** - Was used but never defined; added during this session
2. **Handler registration** - Must add to `RegisterScriptingServices()`, not just declare handler
3. **`TSubclassOf<>` parameters** - FFunctionInvoker doesn't handle class loading

### Next Session

1. Fix FFunctionInvoker to detect `FClassProperty` and use `LoadClass<>()`
2. Or create dedicated `pcg_add_node` abstraction

---

## Session 2: FClassProperty Fix Complete (2026-01-02)

### Completed

| Step | Status | Notes |
|------|--------|-------|
| Fix `FClassProperty` in PropertyAccessor | ✅ Done | Uses `StaticLoadClass()` |
| Test `AddNodeOfType` with TSubclassOf | ✅ Done | All node types work |
| Test `AddEdge` for node connections | ✅ Done | Full pipeline works |
| Document changes | ✅ Done | Updated analysis doc |

### Code Changes

**PropertyAccessor.cpp** - Added FClassProperty handling before regular object handling:
```cpp
// Handle FClassProperty (TSubclassOf<>) - need to load a UClass, not a UObject
if (FClassProperty* ClassProp = CastField<FClassProperty>(Property))
{
    LoadedClass = StaticLoadClass(ClassProp->MetaClass, nullptr, *ClassPath, nullptr, LOAD_None, nullptr);
    // + fallbacks for short names and _C suffix
}
```

### Test Results

Created `/Game/AgentBridge/Test/PCG_NodeTest` with:
- 4 node types: SurfaceSampler, StaticMeshSpawner, FilterDataByTag, TransformPoints
- Full connected pipeline: Input -> Sampler -> Spawner -> Output
- Graph saved successfully

### Phase 1 Status: COMPLETE ✅

`call_asset_function` now handles all tested parameter types:
- `TSubclassOf<>` (FClassProperty) - via StaticLoadClass
- `UPCGNode*` (FObjectProperty) - via path resolution
- `FName` - via string conversion
- Primitives (int, float, bool, string) - existing support

**Next Phase:** Blueprint K2Node manipulation (Phase 3 in original plan)

---

## Session 3: Blueprint Research Complete (2026-01-02)

### Findings

Blueprint node manipulation differs fundamentally from PCG:

| Aspect | PCG | Blueprint |
|--------|-----|-----------|
| Node creation API | `AddNodeOfType()` UFUNCTION | C++ only (`FBlueprintEditorUtils`) |
| Edge/pin API | `AddEdge()` UFUNCTION | C++ only (`MakeLinkTo`) |
| Exposed via reflection | ✅ Yes | ❌ No |

### What Works via BlueprintEditorLibrary

```python
# Create Blueprint with parent
call_static_function("BlueprintEditorLibrary", "CreateBlueprintAssetWithParent",
    {"AssetPath": "/Game/Test/BP_Test", "ParentClass": "/Script/Engine.Actor"})

# Add function
call_static_function("BlueprintEditorLibrary", "AddFunctionGraph",
    {"Blueprint": bp_path, "FuncName": "MyFunction"})

# Add variable
call_static_function("BlueprintEditorLibrary", "AddMemberVariable",
    {"Blueprint": bp_path, "MemberName": "Health",
     "VariableType": {"PinCategory": "real", "PinSubCategory": "double"}})

# Compile
call_static_function("BlueprintEditorLibrary", "CompileBlueprint", {"Blueprint": bp_path})
```

### What Doesn't Work

- Adding K2Nodes (events, function calls, branches, etc.)
- Connecting pins between nodes
- Any logic/visual scripting content

### Recommendation

For full Blueprint node manipulation, options are:

1. **Install ElgKismetEditorWidget** - Third-party plugin with full API
2. **Extend AgentBridge** - Add custom UFUNCTIONs wrapping FBlueprintEditorUtils
3. **Accept limitation** - Use BlueprintEditorLibrary for structure, duplicate for logic

### Phase 3 Status: RESEARCH COMPLETE ⚠️

Structure manipulation works, but node/pin manipulation requires additional development or third-party plugin.

### Overall Project Status

| Phase | Status | Capability |
|-------|--------|------------|
| Phase 1 | ✅ COMPLETE | `call_asset_function` with TSubclassOf |
| Phase 2 | ✅ COMPLETE | Full PCG graph manipulation |
| Phase 3 | ⚠️ PARTIAL | Blueprint structure only, no nodes |
