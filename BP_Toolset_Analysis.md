# BP Toolkit & MCP Tools Analysis

**Date:** 2026-01-02
**Session:** Comprehensive testing of bp_toolkit and AgentBridge MCP tools for PCG/Blueprint creation

---

## Executive Summary

We attempted to create PCG graphs and Blueprints "from scratch" using two approaches:
1. **bp_toolkit** (offline JSON manipulation via UAssetGUI)
2. **Runtime MCP tools** (live editor manipulation)

**Key Finding:** bp_toolkit fails on UE 5.6 due to serialization incompatibility, but runtime MCP tools work well for asset duplication and property modification. Full node creation/connection requires new function-calling infrastructure.

---

## Table of Contents

1. [Approach Comparison](#approach-comparison)
2. [bp_toolkit (Offline)](#approach-1-bp_toolkit-offline)
3. [Runtime MCP Tools](#approach-2-runtime-mcp-tools)
4. [PCG Graph Structure](#pcg-graph-structure)
5. [Blueprint Structure](#blueprint-structure)
6. [UAsset JSON Format Reference](#uasset-json-format-reference)
7. [K2Node Reference](#k2node-reference)
8. [Analysis Workflow Guide](#analysis-workflow-guide)
9. [Successful Workflows](#successful-workflows)
10. [Recommendations](#recommendations)

---

## Approach Comparison

| Approach | Feasibility | Complexity | Editor Required | Best For |
|----------|-------------|------------|-----------------|----------|
| **UAssetAPI JSON→uasset** | HIGH (UE ≤5.4) | HIGH | No | Batch modifications, templating |
| **AgentBridge Reflection** | MEDIUM-HIGH | MEDIUM | Yes (running) | Live editor manipulation |
| **ElgKismetEditorWidget** | HIGH | LOW | Yes (running) | Full graph manipulation |
| **Native Python** | LOW | LOW | Yes | Simple asset creation only |

### Feature Matrix

| Feature | UAssetAPI | AgentBridge | ElgKismet | Native Python |
|---------|-----------|-------------|-----------|---------------|
| Create Blueprint | ✅ | 🔧 Add | ✅ | ✅ |
| Add Variable | ✅ | 🔧 Add | ✅ | ⚠️ Limited |
| Add Function | ✅ | 🔧 Add | ✅ | ❌ |
| Add K2 Nodes | ✅ | 🔧 Add | ✅ | ❌ |
| Connect Pins | ✅ | 🔧 Add | ✅ | ❌ |
| PCG Graph | ✅ | 🔧 Add | ❌ | ❌ |
| Requires Editor | ❌ | ✅ | ✅ | ✅ |
| Binary Safe | ✅ | N/A | N/A | N/A |

---

## Approach 1: bp_toolkit (Offline)

### What It Is
- Python toolkit wrapping UAssetGUI for uasset <-> JSON conversion
- 14 MCP tools for offline asset manipulation
- Works without Unreal Editor running

### Tools Tested

| Tool | Status | Notes |
|------|--------|-------|
| `bp_export_asset` | ✅ Works | Exports .uasset to .json correctly |
| `bp_import_asset` | ❌ Fails (UE 5.6) | Corrupts serialization (see below) |
| `bp_detect_type` | ✅ Works | Correctly identifies Blueprint, PCGGraph, etc. |
| `bp_get_info` | ✅ Works | Returns exports, imports, graphs, namemap |
| `bp_list_properties` | ✅ Works | Lists all properties with types |
| `bp_get_property` | ✅ Works | Retrieves property by path |
| `bp_set_property` | ✅ Works | Modifies property in JSON |
| `bp_clone_asset` | ✅ Works | Creates new asset with different name/path |
| `bp_list_graphs` | ✅ Works | Lists graphs with node counts |
| `bp_add_comment` | ✅ Works (BP only) | Adds EdGraphNode_Comment to Blueprints |
| `bp_clone_node` | ✅ Works | Duplicates existing nodes |
| `bp_find` | ❌ Import error | `cannot import 'search_asset'` |
| `bp_query` | ❌ Import error | `cannot import 'query_asset'` |
| `bp_parse` | ✅ Works | Full parsing with call graphs |

### Critical Failure: UE 5.6 Serialization

When reimporting JSON to uasset for UE 5.6:

```
LogFileManager: Error: Requested read of 208563372 bytes when 113971 bytes remain
LogAssetRegistry: Warning: Package is unloadable. Reason: SerializeAssetRegistryDependencyData
```

**Root Cause:** UAssetGUI's maximum supported version is `VER_UE5_4`. When it serializes back to uasset, it writes incorrect size/offset values in the header for UE 5.6 assets.

**Impact:** Assets created via bp_toolkit round-trip are corrupted and won't load in UE 5.6.

### Round-Trip Validation Results (UE 5.4 and Earlier)

| Asset Type | File | Result | Notes |
|------------|------|--------|-------|
| Behavior Tree | BT_BaseAIBehavior.uasset | ✅ **PASS** | Binary identical (MD5 match) |
| PCG Graph | PCG_LevelGenerator.uasset | ✅ **PASS** | Binary identical (MD5 match) |
| Blueprint (5.7) | BP_VRIncPawn.uasset | ❌ **FAIL** | MetaDataMap FName key issue |
| Blueprint (5.6 + fix) | Various | ✅ **PASS** | After nulling MetaDataMap |

### MetaDataMap Workaround (UE 5.4-5.5)

**Problem:** UAssetAPI exports `TMap<FName, FString>` (property metadata) with plain string keys, but cannot deserialize them back because FName keys require special handling.

**Error:**
```
Newtonsoft.Json.JsonSerializationException: Could not convert string 'Category'
to dictionary key type 'UAssetAPI.UnrealTypes.FName'
```

**Workaround:** Use `fix_metadata_map.py` to null out MetaDataMap entries before reimport.

**Trade-off:** Lossy - UPROPERTY metadata (Category, DisplayName, etc.) is lost, but Blueprints remain functional.

### Still Useful For
- **Analysis only** - Export, parse, understand structure
- **UE 5.4 and earlier** - Full round-trip works
- **Offline batch processing** - When editor can't run
- **Documentation generation** - Understanding asset internals

---

## Approach 2: Runtime MCP Tools

### Asset Creation

| Tool | Status | Notes |
|------|--------|-------|
| `create_asset` | ✅ Works | Creates empty asset shells |
| `duplicate_asset` | ✅ Works | **Best approach** - copies all content |
| `save_asset` | ✅ Works | Persists changes to disk |

**Key Insight:** `create_asset` creates minimal empty shells (PCG with only Input/Output, Blueprint without compiled class). Use `duplicate_asset` to get fully-populated graphs.

### Property Manipulation

| Capability | Status | Example |
|------------|--------|---------|
| Graph-level properties | ✅ Works | `Category`, `Description`, `bIsTemplate` |
| Node properties | ✅ Works | `NodeTitle`, `NodeComment`, `PositionX/Y` |
| Node settings | ✅ Works | `SettingsInterface.PointsPerSquaredMeter` |
| Comment bubbles | ✅ Works | `bCommentBubbleVisible` |
| Blueprint metadata | ✅ Works | `BlueprintDescription`, `BlueprintCategory` |

### Property Path Syntax

```
# Direct asset property
/Game/Path/Asset.Property

# Node within asset
/Game/Path/Asset.Asset:NodeName.Property

# Settings object within node
/Game/Path/Asset.Asset:NodeName.SettingsObject.Property
```

### Function Calling (Current Limitations)

| Tool | Works On | Limitation |
|------|----------|------------|
| `call_static_function` | Blueprint libraries | Static functions only |
| `tempo_call_function` | World actors | Actors only, not assets |

**Gap:** No tool to call instance methods on UObject assets like PCGGraph or Blueprint.

---

## PCG Graph Structure

### Discovered via Analysis

```
PCGGraph
├── Nodes[] (TArray<UPCGNode>)
│   ├── InputPins[]
│   ├── OutputPins[]
│   ├── SettingsInterface -> UPCGSettings subclass
│   ├── NodeTitle, NodeComment
│   └── PositionX, PositionY
├── InputNode (special)
├── OutputNode (special)
├── CommentNodes[]
└── UserParameters
```

### Available Functions (Not Yet Callable)

| Function | Purpose | Parameters |
|----------|---------|------------|
| `AddNodeOfType` | Create new node | `TSubclassOf<UPCGSettings>` |
| `AddNodeCopy` | Clone node settings | `UPCGSettings*` |
| `AddEdge` | Connect nodes | `From, FromPin, To, ToPin` |
| `RemoveNode` | Delete node | `UPCGNode*` |
| `RemoveEdge` | Disconnect | `From, FromPin, To, ToPin` |

### PCG Settings Classes (Node Types)

Common PCG node types available:

| Class | Purpose |
|-------|---------|
| `PCGSurfaceSamplerSettings` | Surface point generation |
| `PCGStaticMeshSpawnerSettings` | Mesh spawning |
| `PCGFilterByTagSettings` | Tag-based filtering |
| `PCGTransformPointsSettings` | Point transformation |
| `PCGBranchSettings` | Control flow |
| `PCGAddTagSettings` | Tag assignment |
| `PCGDensityFilterSettings` | Density-based culling |
| `PCGDistanceSettings` | Distance calculations |
| `PCGCopyPointsSettings` | Point duplication |
| `PCGProjectionSettings` | Surface projection |
| `PCGDifferenceSettings` | Boolean difference |
| `PCGIntersectionSettings` | Boolean intersection |
| `PCGUnionSettings` | Boolean union |

### PCG Pin Names

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

## Blueprint Structure

### Discovered via Analysis

```
Blueprint
├── UbergraphPages[] (EventGraph, etc.)
├── FunctionGraphs[] (Custom functions)
├── MacroGraphs[]
├── NewVariables[] (FBPVariableDescription)
├── ComponentTemplates[]
├── SimpleConstructionScript
└── GeneratedClass
```

### Graph Node Types

- `K2Node_Event` - Event nodes (BeginPlay, Tick, etc.)
- `K2Node_CallFunction` - Function calls
- `K2Node_IfThenElse` - Branch nodes
- `K2Node_VariableGet/Set` - Variable access
- `K2Node_FunctionEntry/Result` - Function bounds
- `EdGraphNode_Comment` - Comments

### Key Difference from PCG

Blueprint nodes store pin data in binary `Extras` field, not in JSON Data properties. This makes programmatic node creation more complex - cloning existing nodes (which preserves Extras) is easier than creating from scratch.

---

## UAsset JSON Format Reference

### Top-Level Structure

```json
{
  "$type": "UAsset",
  "Info": "UAssetAPI v1.0.2.0",
  "NameMap": [...],           // All names/identifiers in the asset
  "Imports": [...],           // References to other packages
  "Exports": [...],           // The actual Blueprint content
  "DependsMap": [...],        // Dependencies
  "SoftPackageReferenceList": [...],
  "AssetRegistryData": [...]
}
```

### NameMap

A flat array of all string identifiers used in the asset:
```json
"NameMap": [
  "RootBox",
  "Camera",
  "bIsWalking",
  "PawnVelocity",
  ...
]
```

**Use for:** Discovering what variables/functions exist via grep.

### Exports

The main content array. Export 0 is the Blueprint class itself. Subsequent exports are:
- Component templates
- Function graphs
- Event graphs (UbergraphPages)
- K2Nodes (visual scripting nodes)
- Variables

Each export has:
```json
{
  "$type": "NormalExport",
  "ObjectName": "RootBox_GEN_VARIABLE",
  "ClassIndex": {...},
  "Data": [...]
}
```

### Common Data Types in Value

```json
// Boolean
{"Name": "bEnabled", "Value": true}

// Number
{"Name": "Speed", "Value": 600.0}

// Vector
{"Name": "Location", "Value": {"X": 0, "Y": 0, "Z": 100}}

// Rotator
{"Name": "Rotation", "Value": {"Pitch": 0, "Yaw": 90, "Roll": 0}}

// Object Reference
{"Name": "Mesh", "Value": {"Index": 42}}

// Enum
{"Name": "State", "Value": "Walking"}

// Array
{"Name": "Items", "Value": [{"Value": ...}, {"Value": ...}]}
```

### File Size Expectations

| Blueprint Type | JSON Size |
|----------------|-----------|
| Simple actor | ~1-5 MB |
| Complex pawn (VR template) | 47+ MB |
| Character with animation | 100+ MB |

**Always gitignore JSON exports** - they're too large for version control.

---

## K2Node Reference

### Node Types

| Node Type | Purpose | Key Properties |
|-----------|---------|----------------|
| K2Node_Event | Event entry point (Tick, BeginPlay) | EventReference |
| K2Node_CallFunction | Calls a function | FunctionReference |
| K2Node_VariableGet | Reads a variable | VariableReference |
| K2Node_VariableSet | Writes a variable | VariableReference |
| K2Node_IfThenElse | Branch node | Condition pin |
| K2Node_Select | Switch/select | Selection type |
| K2Node_MakeStruct | Create struct | Struct type |
| K2Node_BreakStruct | Decompose struct | Struct type |
| K2Node_SwitchEnum | Switch on enum | Enum type |
| K2Node_MacroInstance | Macro call | MacroGraphReference |
| K2Node_Timeline | Timeline | TimelineName |
| K2Node_FunctionEntry | Function input | InputParams |
| K2Node_FunctionResult | Function output | OutputParams |
| EdGraphNode_Comment | Comment box | NodeComment |

### Pin Types

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

### NameMap Patterns

| Pattern | Indicates |
|---------|-----------|
| `bIs*`, `bCan*`, `bHas*` | Boolean state flags |
| `*Velocity`, `*Speed` | Physics/movement |
| `*Component` | Component references |
| `*Timer`, `*Delay` | Timing systems |
| `On*`, `Event*` | Events/delegates |
| `*Trace`, `*Hit` | Raycasting/collision |
| `*Montage`, `*Anim*` | Animation |

---

## Analysis Workflow Guide

### Phase 1: Initial Assessment

1. **Check file size:** `ls -lh *.json`
   - < 5 MB: Can read directly
   - 5-50 MB: Needs parsing
   - > 50 MB: Selective extraction

2. **Verify JSON validity:**
   ```python
   import json
   with open('asset.json', 'r') as f:
       data = json.load(f)
   print(f"Exports: {len(data.get('Exports', []))}")
   ```

3. **Identify asset type** from Export 0's ObjectName

### Phase 2: Parse and Organize

Run the bp_toolkit parser:
```bash
python bp_export.py asset.uasset
python asset_parser.py asset.json output_dir/
```

Output structure:
```
asset_parsed/
├── _metadata.json          # Export info, counts
├── _namemap_full.txt       # Complete NameMap (grep-friendly)
├── _namemap_organized.json # Names grouped by category
├── _comments.json          # Human-readable comments from nodes
├── _index.json             # Quick lookup of all graphs/functions
├── graphs/
│   ├── EventGraph.json
│   └── ...
├── functions/
│   └── ...
└── components/
    └── ...
```

### Phase 3: Discover Structure

```bash
# List all graphs
cat _index.json | jq '.ubergraphs'

# Search NameMap for keywords
grep -i "movement\|velocity\|speed" _namemap_full.txt

# Read comments
cat _comments.json | jq '.[] | select(.comments | length > 0)'
```

### Phase 4: Deep Dive

For each important function/graph:
1. Find input parameters (K2Node_FunctionEntry)
2. Find return values (K2Node_FunctionResult)
3. Trace execution flow through K2Node_CallFunction and K2Node_IfThenElse
4. Document the logic as a tree

### Reference Index System

Use indices to link documentation to source:

| Index | Format | Example | Lookup |
|-------|--------|---------|--------|
| Export | `E###` | `E386: Floating_SearchFootClamp` | `--find "SearchFootClamp"` |
| Comment | `C###` | `C182: "Clamp foot if normal acceptable"` | `--comments` |
| NameMap | `[####]` | `[0782] FootMayClamp` | `--find "FootMayClamp"` |

---

## Successful Workflows

### For PCG Graphs

```python
# 1. Duplicate existing template
duplicate_asset("/Game/Templates/MyPCGTemplate", "/Game/Output", "NewGraph")

# 2. Modify graph properties
set_property("/Game/Output/NewGraph", "Description", "Custom PCG graph")

# 3. Modify individual nodes
set_property("/Game/Output/NewGraph.NewGraph:SurfaceSampler_0", "NodeTitle", "MyNode")
set_property("/Game/Output/NewGraph.NewGraph:SurfaceSampler_0.Settings", "PointsPerSquaredMeter", "0.5")

# 4. Save
save_asset("/Game/Output/NewGraph")
```

### For Blueprints

```python
# 1. Duplicate existing Blueprint
duplicate_asset("/Game/Templates/BP_Template", "/Game/Output", "BP_New")

# 2. Modify metadata
set_property("/Game/Output/BP_New", "BlueprintDescription", "My custom BP")

# 3. Save
save_asset("/Game/Output/BP_New")

# Note: Adding actual Blueprint nodes requires function calling infrastructure
```

### For Analysis (Offline)

```bash
# 1. Export to JSON
python bp_export.py MyAsset.uasset

# 2. Detect type
python asset_parser.py MyAsset.json --detect

# 3. Full parse
python asset_parser.py MyAsset.json output_dir/

# 4. Search
grep -i "pattern" output_dir/_namemap_full.txt
```

---

## Recommendations

### Immediate (Works Now)

1. Use `duplicate_asset` + `set_property` for creating customized graphs
2. Use bp_toolkit for **analysis only** (don't reimport to UE 5.6)
3. Store templates in project for duplication

### Short-term (Feature Requests)

1. Add `call_asset_function` tool for instance methods on UObjects
2. Fix bp_toolkit import errors (`search_asset`, `query_asset`)
3. Add PCG-specific comment node support to bp_toolkit

### Long-term

1. Update UAssetGUI to support `VER_UE5_5` / `VER_UE5_6`
2. Create high-level "create PCG node" abstraction
3. Create high-level "create Blueprint node" abstraction

---

## Third-Party Options

### ElgKismetEditorWidget Plugin

**GitHub:** https://github.com/ElgSoft/ElgKismetEditorWidget

Provides full Blueprint graph manipulation from Editor Utility Widgets:
- Access all nodes via `ElgBESGraphNode` objects
- Access pins via `ElgBESGraphPin` objects
- Create/edit/remove Variables, Functions, Macros, EventDispatchers
- Connect/disconnect pins programmatically
- Works with Python via `Execute Python Command` node

**Key Objects:**
```
ElgBESGraphNode - node representation
  - GetAllPins() → Array of ElgBESGraphPin
  - GetPosition() → FVector2D
  - Select() / IsSelected()

ElgBESGraphPin - pin representation
  - IsConnected() → bool
  - BreakLink() / LinkTo(OtherPin)
  - PromoteToVariable()
```

**Integration Path:**
1. Install plugin in project
2. Create Editor Utility Widget with manipulation logic
3. Trigger via console command from AgentBridge

---

## Files Created This Session

| File | Location | Description |
|------|----------|-------------|
| `Claude_PCG_Duplicated` | `/Game/AgentBridge/` | PCG with modified nodes |
| `BP_Claude_Duplicated` | `/Game/AgentBridge/` | Blueprint copy |
| `Claude_PCG_Runtime` | `/Game/AgentBridge/` | Empty PCG shell |
| `BP_Claude_Runtime` | `/Game/AgentBridge/` | Empty BP shell (broken) |

---

## Key Paths

| Purpose | Path |
|---------|------|
| bp_toolkit root | `D:/tempo/TempoSample/Plugins/AgentBridge/bp_toolkit` |
| Local bare repo | `D:/repos/bp_toolkit.git` |
| UAssetGUI | `vendor/UAssetGUI/UAssetGUI/bin/Release/net8.0-windows/UAssetGUI.exe` |
| Test assets (UE 5.6) | `D:/tempo/uassets/` |
| PCG samples (UE 5.6) | `D:/EL_UE/UE_5.6/Engine/Plugins/Experimental/PCGBiomeSample/Content/` |

---

## Archive Reference

Original research documents preserved in `.old.claude/`:

| Document | Key Contents |
|----------|--------------|
| `IMPROVEMENT_PLAN.md` | Property system fixes (TArray, GET/SET, struct schemas) - **IMPLEMENTED** |
| `RESEARCH.md` | UAssetAPI validation, ElgKismetEditorWidget, AgentBridge extension research |
| `HANDOVER.md` | Session history (Sessions 19-25), 104 MCP tools, GitHub repos |
| `BP_JSON_WORKFLOW.md` | Step-by-step UAsset→JSON→Parse workflow, K2Node types |
| `UASSET_DISSECTION_GUIDE.md` | Comprehensive AI agent guide for Blueprint JSON analysis |

All key information from these documents has been consolidated into this analysis.

---

*Analysis conducted using AgentBridge MCP tools and bp_toolkit*
*Document Version: 2.0 - Consolidated from archived research*

---

## Implementation: call_asset_function

**Date:** 2026-01-02
**Status:** Partially Working

### What Was Added

New `call_asset_function` tool for calling instance methods on UObject assets (PCGGraph, Blueprint, DataAsset, etc.).

**Files Modified:**
- `AgentCommands.h` - Added `FCallAssetFunctionCommand` and `FCallAssetFunctionResponse`
- `CommandExecutor.h/.cpp` - Added `Execute()` implementation
- `AgentBridge.proto` - Added `CallAssetFunction` RPC
- `AgentBridgeServiceSubsystem.h/.cpp` - Handler + registration
- `agentbridge.py` - Python MCP tool wrapper

### Test Results

| Function | Parameters | Status | Notes |
|----------|------------|--------|-------|
| `GetInputNode()` | None | ✅ **WORKS** | Returns UPCGNode pointer |
| `GetOutputNode()` | None | ✅ **WORKS** | Returns UPCGNode pointer |
| `AddNodeOfType()` | `TSubclassOf<UPCGSettings>` | ❌ **CRASHES** | Complex parameter passing issue |

### Successful Test

\`\`\`python
# This works!
result = call_asset_function(
    asset_path='/Game/AgentBridge/Claude_PCG_Runtime.Claude_PCG_Runtime',
    function_name='GetInputNode'
)
# Returns: success=True, return_type="Object"
\`\`\`

### Crash Case

\`\`\`python
# This crashes the editor
call_asset_function(
    asset_path='/Game/AgentBridge/Claude_PCG_Runtime.Claude_PCG_Runtime',
    function_name='AddNodeOfType',
    parameters={'InSettingsClass': '/Script/PCG.PCGSurfaceSamplerSettings'}
)
\`\`\`

**Root Cause:** The `TSubclassOf<UPCGSettings>` parameter requires special handling - passing a class path string doesn't work. The FFunctionInvoker needs to load the class via `LoadClass<>()` before passing it.

### Next Steps

1. **Fix class parameter passing** - Add special handling in FFunctionInvoker for `TSubclassOf<>` parameters
2. **Test simpler functions** - Try functions with primitive parameters first
3. **Alternative approach** - Consider adding dedicated `pcg_add_node` tool that handles class loading internally

---

## FClassProperty Fix (2026-01-02)

**Problem:** `TSubclassOf<>` parameters crashed - PropertyAccessor used `LoadObject<>()` for class paths.

**Solution:** Added dedicated `FClassProperty` handling in `WriteObjectProperty()` (PropertyAccessor.cpp):

```cpp
if (FClassProperty* ClassProp = CastField<FClassProperty>(Property))
{
    // Use StaticLoadClass instead of LoadObject
    LoadedClass = StaticLoadClass(ClassProp->MetaClass, nullptr, *ClassPath, nullptr, LOAD_None, nullptr);
    // ... with fallbacks for short names and BP classes
}
```

**Key insight:** `StaticLoadClass()` understands class paths like `/Script/PCG.PCGSurfaceSamplerSettings`, while `LoadObject<>()` does not.

### Updated Test Results

| Function | Parameters | Status | Notes |
|----------|------------|--------|-------|
| `GetInputNode()` | None | ✅ **WORKS** | Returns UPCGNode pointer |
| `GetOutputNode()` | None | ✅ **WORKS** | Returns UPCGNode pointer |
| `AddNodeOfType()` | `TSubclassOf<UPCGSettings>` | ✅ **WORKS** | Creates new PCG node |
| `AddEdge()` | `UPCGNode*, FName, UPCGNode*, FName` | ✅ **WORKS** | Connects nodes |

### Full PCG Graph Creation Example

```python
# Create a PCG graph programmatically
graph_path = '/Game/MyPCG.MyPCG'

# Get special nodes
input_node = call_asset_function(graph_path, 'GetInputNode').return_value.string_value
output_node = call_asset_function(graph_path, 'GetOutputNode').return_value.string_value

# Add processing nodes
sampler = call_asset_function(graph_path, 'AddNodeOfType',
    {'InSettingsClass': '/Script/PCG.PCGSurfaceSamplerSettings'}).return_value.string_value

spawner = call_asset_function(graph_path, 'AddNodeOfType',
    {'InSettingsClass': '/Script/PCG.PCGStaticMeshSpawnerSettings'}).return_value.string_value

# Connect the pipeline: Input -> Sampler -> Spawner -> Output
call_asset_function(graph_path, 'AddEdge',
    {'From': input_node, 'FromPinLabel': 'Out', 'To': sampler, 'ToPinLabel': 'In'})

call_asset_function(graph_path, 'AddEdge',
    {'From': sampler, 'FromPinLabel': 'Out', 'To': spawner, 'ToPinLabel': 'In'})

call_asset_function(graph_path, 'AddEdge',
    {'From': spawner, 'FromPinLabel': 'Out', 'To': output_node, 'ToPinLabel': 'In'})

# Save
save_asset('/Game/MyPCG')
```

### Verified PCG Node Types

| Settings Class | Created Node Name |
|---------------|-------------------|
| `PCGSurfaceSamplerSettings` | SurfaceSampler_N |
| `PCGStaticMeshSpawnerSettings` | StaticMeshSpawner_N |
| `PCGFilterByTagSettings` | FilterDataByTag_N |
| `PCGTransformPointsSettings` | TransformPoints_N |

### What This Unlocks

- **Full programmatic PCG graph creation** - No more "duplicate and modify"
- **Dynamic level generation** - Agents can create custom PCG pipelines
- **Biome system creation** - Combine with DataAsset property setting for complete biome setup
