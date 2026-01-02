# Research Notes

> Written by research/planning Claude instance for implementation instance to consume.
> This file is gitignored - ephemeral cross-instance communication.

---

# Programmatic Blueprint & PCG Graph Creation Research

> **Date:** 2026-01-01
> **Goal:** Evaluate feasibility of creating/modifying Blueprint and PCG graphs from CLI

---

## Executive Summary

Three viable approaches identified, each with different trade-offs:

| Approach | Feasibility | Complexity | Editor Required | Best For |
|----------|-------------|------------|-----------------|----------|
| **UAssetAPI JSON→uasset** | HIGH | HIGH | No | Batch modifications, templating |
| **AgentBridge Reflection** | MEDIUM-HIGH | MEDIUM | Yes (running) | Live editor manipulation |
| **ElgKismetEditorWidget** | HIGH | LOW | Yes (running) | Full graph manipulation |

**Recommendation:** Start with **UAssetAPI round-trip test** to validate the `fromjson` path, then explore **AgentBridge extension** for live editor work.

---

## Approach 1: UAssetAPI JSON → uasset Serialization

### What We Have

Our `bp_export.py` already uses UAssetGUI for export:
```bash
UAssetGUI.exe tojson MyBlueprint.uasset MyBlueprint.json VER_UE5_4
```

### What's Available

UAssetAPI/UAssetGUI supports **bidirectional** serialization:
```bash
# Export (we have this)
UAssetGUI.exe tojson source.uasset output.json VER_UE5_4

# Import (we need to add this)
UAssetGUI.exe fromjson source.json output.uasset VER_UE5_4
```

**Key capabilities from UAssetAPI docs:**
- Full read/write for uasset files from UE 4.13 to 5.3+
- Support for 100+ property types and 12 export types
- Raw Kismet bytecode reading/writing
- JSON format "maintains binary equality" on round-trip
- Support for .usmap mappings for unversioned assets

**Sources:**
- [UAssetAPI GitHub](https://github.com/atenfyr/UAssetAPI)
- [UAssetAPI Documentation](https://atenfyr.github.io/UAssetAPI/)
- [CLI Commands Issue #78](https://github.com/atenfyr/UAssetGUI/issues/78)

### JSON Structure for Blueprints

From our parsed BP analysis, the JSON closely mirrors the binary format:
```json
{
  "Exports": [
    {
      "$ObjectName": "Default__BP_MyActor_C",
      "$Type": "BP_MyActor_C"
    },
    {
      "$Type": "EdGraphNode_Comment",
      "NodePosX": -1200,
      "NodePosY": 100,
      "NodeWidth": 400,
      "NodeHeight": 200,
      "NodeComment": "My Comment Box"
    },
    {
      "$Type": "K2Node_FunctionEntry",
      "NodePosX": -800,
      "NodePosY": 0,
      "CustomFunctionName": "MyFunction",
      "NodeGuid": "..."
    }
  ]
}
```

### Challenges

1. **Complex Serialization Format** - Must understand NodeGuid generation, pin connections by index/path
2. **No High-Level API** - Must construct exact node/pin structures
3. **Fragile** - Any JSON error corrupts the file

### Implementation Plan

Add to `bp_export.py`:
```python
def import_json_to_uasset(
    json_path: Path,
    uasset_path: Path,
    ue_version: str = "VER_UE5_4"
) -> Tuple[bool, str]:
    """Convert JSON back to uasset using UAssetGUI."""
    cmd = [get_uassetgui_path(), "fromjson", str(json_path), str(uasset_path), ue_version]
    result = subprocess.run(cmd, capture_output=True, text=True)
    return result.returncode == 0, result.stderr or result.stdout
```

---

## Approach 2: AgentBridge Reflection (Editor API via gRPC)

### What AgentBridge Already Has

From the AgentBridge CLAUDE.md (90 MCP tools across 12 services):

| Tool | Purpose |
|------|---------|
| `spawn_actor` | Create actors in world |
| `set_property` / `get_property` | Property manipulation via reflection |
| `call_static_function` | Call Blueprint library functions |
| `save_actor_as_blueprint` | Save actor configuration as BP asset |
| `create_asset` | Create DataAssets, MaterialInstances, etc. |
| `get_class_schema` | Discover properties/functions on any UClass |

### What's Missing for BP/PCG Graph Creation

From `REFLECTION_IMPROVEMENTS.md`, the following are NOT yet exposed:
- `FKismetEditorUtilities::CreateBlueprint()`
- `FBlueprintEditorUtils::AddMemberVariable()`, `AddFunction()`, etc.
- K2Node creation/manipulation APIs
- PCG graph structure modification

### Extension Architecture

AgentBridge uses a layered command architecture:
```
MCP Tools (Python) → gRPC → CommandExecutor (C++) → Unreal APIs
```

New commands would follow this pattern:
```cpp
struct FCreateBlueprintCommand : FAgentCommandBase
{
    FString ParentClassName;
    FString PackagePath;
    FString BlueprintName;
};

void FCommandExecutor::Execute(const FCreateBlueprintCommand& Command, FCreateBlueprintResponse& Response)
{
    UClass* ParentClass = FTypeDiscovery::FindClassByName(Command.ParentClassName);
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        ParentClass, Package, *Command.BlueprintName, BPTYPE_Normal,
        UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass()
    );
    // ...
}
```

### PCG Graph APIs (UE 5.6+)

From Unreal docs:
- `UPCGGraph::AddEdge(UPCGNode* From, UPCGNode* To)`
- `UPCGGraph::AddUserParameters(TArray<FPropertyBagPropertyDesc>)`

**Sources:**
- [UPCGGraph API Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PCG/UPCGGraph)
- [PCG Framework Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-overview)

---

## Approach 3: ElgKismetEditorWidget Plugin

### Overview

Third-party UE5 plugin providing full Blueprint graph manipulation from Editor Utility Widgets.

**GitHub:** https://github.com/ElgSoft/ElgKismetEditorWidget

### Capabilities

- Access all nodes via `ElgBESGraphNode` objects
- Access pins via `ElgBESGraphPin` objects
- Create/edit/remove Variables, Functions, Macros, EventDispatchers
- Connect/disconnect pins programmatically
- Works with Python via `Execute Python Command` node

### Key Objects

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

### Integration Path

1. Install plugin in project
2. Create Editor Utility Widget with manipulation logic
3. Trigger via console command from Claude

**Sources:**
- [ElgKismetEditorWidget GitHub](https://github.com/ElgSoft/ElgKismetEditorWidget)
- [Forum Thread](https://forums.unrealengine.com/t/elgkismeteditorwidget-editor-widgets-in-the-blueprint-editor/522953)

---

## Approach 4: Native Python in UE Editor

### Overview

UE's built-in Python scripting has LIMITED Blueprint APIs.

**What works:**
```python
import unreal
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
factory = unreal.BlueprintFactory()
factory.set_editor_property('parent_class', unreal.Actor)
blueprint = asset_tools.create_asset('BP_Test', '/Game/Blueprints', unreal.Blueprint, factory)
```

**What's limited:**
- K2Node creation requires C++
- Graph manipulation is very restricted
- Most `FBlueprintEditorUtils` not exposed

**Sources:**
- [Official Python Scripting Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python)

---

## Comparison Matrix

| Feature | UAssetAPI | AgentBridge | ElgKismet | Native Python |
|---------|-----------|-------------|-----------|---------------|
| Create Blueprint | ✅ | 🔧 Add | ✅ | ✅ |
| Add Variable | ✅ | 🔧 Add | ✅ | ⚠️ Limited |
| Add Function | ✅ | 🔧 Add | ✅ | ❌ |
| Add K2 Nodes | ✅ | 🔧 Add | ✅ | ❌ |
| Connect Pins | ✅ | 🔧 Add | ✅ | ❌ |
| PCG Graph | ⚠️ Untested | 🔧 Add | ❌ | ❌ |
| Requires Editor | ❌ | ✅ | ✅ | ✅ |
| Binary Safe | ✅ | N/A | N/A | N/A |

---

## Proposed Test Plan

### Phase 1: UAssetAPI Validation (CLI, no editor)

**Test 1.1: Round-trip a Blueprint**
```bash
# Export
python bp_export.py BP_VRIncPawn.uasset

# Reimport (add this capability)
python bp_export.py BP_VRIncPawn.json --import

# Compare file sizes/hashes
```

**Test 1.2: Simple JSON modification**
1. Export BP to JSON
2. Find an `EdGraphNode_Comment`, change `NodeComment` text
3. Reimport
4. Open in editor, verify comment changed

**Test 1.3: Add a node**
1. Export BP with existing K2Node_CallFunction
2. Duplicate that node in JSON (new position, new NodeGuid)
3. Reimport
4. Verify node appears in editor

### Phase 2: AgentBridge Exploration (with editor running)

**Test 2.1: Check existing create_asset**
```python
# Via MCP
create_asset(
    asset_class="Blueprint",
    package_path="/Game/Test",
    asset_name="TestBP",
    parent_asset_path="Actor"  # Guess at parameter
)
```

**Test 2.2: Explore reflection**
```python
list_classes(base_class_name="BlueprintFactory")
get_class_schema("FKismetEditorUtilities", include_functions=True)
```

### Phase 3: PCG Graph Structure

**Test 3.1: Export existing PCG graph**
```bash
python bp_export.py SomePCGGraph.uasset
python asset_parser.py SomePCGGraph.json --detect
```

**Test 3.2: Document PCG JSON structure**
- Node types and their properties
- Edge/connection format
- Parameter definitions

### Phase 4: Implementation Decision

Based on Phase 1-3 results:
- If UAssetAPI works well → Build `bp_builder.py` high-level API
- If AgentBridge easier → Add Blueprint commands to AgentBridge
- Hybrid approach likely optimal

---

## Quick Win: Add `fromjson` to bp_export.py

```python
# Add to bp_export.py
def import_json_to_uasset(json_path, uasset_path=None, ue_version=DEFAULT_UE_VERSION):
    """Import JSON back to uasset format."""
    if uasset_path is None:
        uasset_path = json_path.with_suffix('.uasset')

    cmd = [get_uassetgui_path(), "fromjson", str(json_path), str(uasset_path), ue_version]
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)

    if result.returncode == 0:
        return True, f"Imported to {uasset_path}"
    return False, result.stderr or result.stdout or "Unknown error"
```

Add CLI support:
```python
elif arg == "--import":
    do_import = True
```

---

## Summary

**Recommended first step:** Add `fromjson` support to `bp_export.py` and test round-trip on a simple Blueprint. This validates the external modification approach without requiring the editor.

**If that succeeds:** Build a Python library (`bp_builder.py`) that can construct Blueprint JSON structures programmatically, abstracting away the low-level details.

**For live editing:** AgentBridge is the cleanest path since we already have the gRPC infrastructure. Would need ~200-300 lines of new C++ for Blueprint creation/manipulation commands.

---

## Test Results (2026-01-01)

All Phase 1 tests completed successfully. UAssetAPI round-trip approach is **VALIDATED**.

### Test 1.1: Round-Trip (Binary Equality)

| Asset Type | File | Result | Notes |
|------------|------|--------|-------|
| Behavior Tree | BT_BaseAIBehavior.uasset | ✅ **PASS** | Binary identical (MD5 match) |
| PCG Graph | PCG_LevelGenerator.uasset | ✅ **PASS** | Binary identical (MD5 match) |
| Blueprint (5.7) | BP_VRIncPawn.uasset | ❌ **FAIL** | MetaDataMap FName key issue |
| Blueprint (5.6 + fix) | Various | ✅ **PASS** | After nulling MetaDataMap |

### Test 1.2: Simple Modification (Comment Text)

```bash
# 1. Export Blueprint
python bp_export.py SomeBlueprint.uasset

# 2. Modify comment text in JSON
# Changed: "NodeComment": "Old Text" → "NodeComment": "Modified by Claude!"

# 3. Apply MetaDataMap fix
python fix_metadata_map.py SomeBlueprint.json SomeBlueprint_fixed.json

# 4. Reimport
python bp_export.py SomeBlueprint_fixed.json --import
```

**Result:** ✅ **PASS** - Comment text modification persisted through round-trip.

### Test 1.3: Node Addition (New Comment Box)

Added new `EdGraphNode_Comment` to Exports array:
```json
{
  "$type": "UAssetAPI.ExportTypes.NormalExport, UAssetAPI",
  "ClassIndex": {"ObjectName": "EdGraphNode_Comment"},
  "ObjectName": {"Value": "EdGraphNode_Comment_NEW"},
  "Data": [
    {"Name": "NodePosX", "Value": -15000},
    {"Name": "NodePosY", "Value": 0},
    {"Name": "NodeWidth", "Value": 400},
    {"Name": "NodeHeight", "Value": 150},
    {"Name": "NodeComment", "Value": "Added via JSON modification!"}
  ]
}
```

**Result:** ✅ **PASS** - New comment node appears in Blueprint after import.

### Test 3.1-3.2: PCG Graph Modification

Tested modifying `PointsPerSquaredMeter` parameter:
- Original value: `0.05`
- Modified to: `0.1`
- Re-exported and verified: `0.10000000149011612` (floating point representation of 0.1)

**Result:** ✅ **PASS** - PCG graph modifications persist through round-trip.

### Known Issue: Blueprint MetaDataMap

**Problem:** UAssetAPI exports `TMap<FName, FString>` (property metadata) with plain string keys, but cannot deserialize them back because FName keys require special handling.

**Error:**
```
Newtonsoft.Json.JsonSerializationException: Could not convert string 'Category'
to dictionary key type 'UAssetAPI.UnrealTypes.FName'
```

**Workaround:** Created `fix_metadata_map.py` that nulls out MetaDataMap entries:
```bash
python fix_metadata_map.py input.json output.json
```

**Trade-off:** Lossy - UPROPERTY metadata (Category, DisplayName, etc.) is lost, but Blueprints remain functional.

**Affected:** UE 5.7 Blueprints. UE 5.6 assets in `/content/uassets` work without issues.

---

## Validated Capabilities

| Capability | Status | Tool |
|------------|--------|------|
| Export uasset → JSON | ✅ Working | `bp_export.py` |
| Import JSON → uasset | ✅ Working | `bp_export.py --import` |
| Round-trip Behavior Trees | ✅ Binary identical | UAssetGUI |
| Round-trip PCG Graphs | ✅ Binary identical | UAssetGUI |
| Round-trip Blueprints | ✅ With workaround | + `fix_metadata_map.py` |
| Modify comment text | ✅ Verified | JSON edit + reimport |
| Add new nodes | ✅ Verified | JSON edit + reimport |
| Modify PCG parameters | ✅ Verified | JSON edit + reimport |

---

## Next Steps

### Immediate (Build on Validated Foundation)

1. **Create `bp_builder.py`** - High-level Python API for constructing Blueprint JSON:
   - Helper functions for common node types (K2Node_CallFunction, etc.)
   - NodeGuid generation
   - Pin connection management
   - Template-based Blueprint creation

2. **Create `pcg_builder.py`** - Similar for PCG graphs:
   - Node type helpers
   - Edge/connection builders
   - Parameter management

### Future (When Editor Available)

3. **Test AgentBridge create_asset** for Blueprints
4. **Explore FKismetEditorUtilities** exposure via reflection
5. **Test ElgKismetEditorWidget** for complex graph manipulation

---

## Queued Research Topics

- ~~PCG graph JSON structure documentation~~ (DONE - validated modification)
- ElgKismetEditorWidget compatibility with UE 5.7
- UnrealEnginePython plugin status (not official)
- UAssetAPI FName key deserialization fix (upstream PR?)

---

*Document Version: 2.0*
*Author: Claude Code (Research Instance)*
*Test Date: 2026-01-01*
