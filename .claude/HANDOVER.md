# AgentBridge Session Handover

> Last Updated: January 1, 2026 (Session 21)

## Current State

AgentBridge is **feature-complete** with:
- 38 gRPC RPCs
- **104 MCP tools across 13 services** (90 core + 14 bp_toolkit)
- Self-documenting help system with bp_toolkit integration
- World Partition support
- PIE/Runtime support
- Comprehensive README.md documentation
- **GitHub repos configured** (private)

## GitHub Repositories

| Repo | URL | Notes |
|------|-----|-------|
| AgentBridge | https://github.com/Incurian/AgentBridge | Main plugin |
| BP_Toolkit | https://github.com/Incurian/BP_Toolkit | Submodule for offline asset manipulation |

**Workflow:**
```bash
# AgentBridge changes
cd D:/tempo/TempoSample/Plugins/AgentBridge
git add . && git commit -m "msg" && git push

# bp_toolkit changes
cd bp_toolkit
git add . && git commit -m "msg" && git push
cd .. && git add bp_toolkit && git commit -m "Update bp_toolkit" && git push
```

## Session 21 (Current)

| Task | Status | Notes |
|------|--------|-------|
| Create README.md | DONE | Comprehensive 617-line documentation |
| Scrub CLAUDE.md | DONE | Updated tool counts, added status section |
| GitHub setup | DONE | Both repos private, submodule URL updated |
| Update HANDOVER.md | DONE | This file |

---

## Centaur Testing Plan (Tomorrow)

> **Goal:** Human-AI collaborative testing. Claude drives the tools, human inspects results visually in the editor.

### Prerequisites

1. **Start editor with GUI** (not headless):
   ```bash
   cd D:/tempo/TempoSample
   # Use regular editor launch, not Run.sh (which is headless)
   "D:/EL_UE/UE_5.6/Engine/Binaries/Win64/UnrealEditor.exe" TempoSample.uproject
   ```

2. **Wait for gRPC** (~30 seconds after "LogTempo: Scripting server started")

3. **Verify connection:**
   ```bash
   cd D:/tempo/TempoSample/Plugins/AgentBridge/Python
   PYTHONPATH="D:/tempo/TempoSample/Plugins/Tempo/TempoCore/Content/Python/API/tempo" \
     D:/tempo/TempoSample/TempoEnv/Scripts/python.exe -c \
     "from mcp.services.agentbridge import connect; c = connect('localhost', 10001); print(c.list_worlds())"
   ```

---

### Test Suite 1: Actor Operations

| Test | Claude Does | Human Verifies |
|------|-------------|----------------|
| **1.1 Query actors** | `query_actors(class_name="Light")` | Correct count in World Outliner |
| **1.2 Spawn actor** | `spawn_actor("PointLight", location=[0,0,500], label="TestLight")` | Light visible at origin, 5m up |
| **1.3 Move actor** | `set_actor_transform("TestLight", location=[1000,0,500])` | Light moved 10m on X axis |
| **1.4 Rotate actor** | `set_actor_transform("TestLight", rotation=[0,45,0])` | Light cone rotated 45 degrees |
| **1.5 Scale actor** | `set_actor_transform("TestLight", scale=[2,2,2])` | Light icon appears larger |
| **1.6 Delete actor** | `delete_actor("TestLight")` | Light gone from scene |

**Visual checkpoint:** World Outliner should return to original state.

---

### Test Suite 2: Property Paths

| Test | Claude Does | Human Verifies |
|------|-------------|----------------|
| **2.1 Spawn test light** | `spawn_actor("PointLight", label="PropTest")` | Light appears |
| **2.2 Read intensity** | `get_property("PropTest", "LightComponent0.Intensity")` | Returns default (5000?) |
| **2.3 Set intensity** | `set_property("PropTest", "LightComponent0.Intensity", "50000")` | Light visibly brighter |
| **2.4 Read color** | `get_property("PropTest", "LightComponent0.LightColor")` | Returns current color |
| **2.5 Set color (red)** | `tempo_set_color_property("PropTest", "LightColor", r=255, g=0, b=0, component="LightComponent0")` | Light turns red |
| **2.6 Set color (blue)** | `tempo_set_color_property("PropTest", "LightColor", r=0, g=0, b=255, component="LightComponent0")` | Light turns blue |
| **2.7 Read location** | `get_property("PropTest", "RootComponent.RelativeLocation")` | Returns current pos |
| **2.8 Set location** | `set_property("PropTest", "RootComponent.RelativeLocation", "(X=500,Y=500,Z=200)")` | Light moves |
| **2.9 Cleanup** | `delete_actor("PropTest")` | Light gone |

**Visual checkpoint:** Verify each color/brightness change is immediately visible.

---

### Test Suite 3: Static Mesh Actors

| Test | Claude Does | Human Verifies |
|------|-------------|----------------|
| **3.1 Spawn cube** | `spawn_actor("StaticMeshActor", label="TestCube")` | Empty mesh actor appears |
| **3.2 Assign mesh** | `tempo_set_asset_property("TestCube", "StaticMesh", "/Engine/BasicShapes/Cube.Cube", component="StaticMeshComponent0")` | Cube visible |
| **3.3 Scale up** | `set_actor_transform("TestCube", scale=[3,3,3])` | Cube 3x larger |
| **3.4 Assign sphere** | `tempo_set_asset_property("TestCube", "StaticMesh", "/Engine/BasicShapes/Sphere.Sphere", component="StaticMeshComponent0")` | Mesh changes to sphere |
| **3.5 Cleanup** | `delete_actor("TestCube")` | Gone |

---

### Test Suite 4: Complex Workflows

#### 4.1 Lighting Rig

Claude creates a 3-light setup:

```
spawn_actor("PointLight", location=[0,0,300], label="KeyLight")
spawn_actor("PointLight", location=[-500,500,200], label="FillLight")
spawn_actor("PointLight", location=[500,-500,400], label="BackLight")

# Set intensities (key brightest, fill medium, back accent)
set_property("KeyLight", "LightComponent0.Intensity", "10000")
set_property("FillLight", "LightComponent0.Intensity", "5000")
set_property("BackLight", "LightComponent0.Intensity", "3000")

# Set colors
tempo_set_color_property("KeyLight", "LightColor", r=255, g=250, b=240, component="LightComponent0")
tempo_set_color_property("FillLight", "LightColor", r=200, g=220, b=255, component="LightComponent0")
tempo_set_color_property("BackLight", "LightColor", r=255, g=200, b=150, component="LightComponent0")
```

**Human verifies:** Classic 3-point lighting setup visible, warm key, cool fill, warm backlight.

#### 4.2 Actor Hierarchy (Attachment)

```
spawn_actor("StaticMeshActor", label="Parent")
spawn_actor("PointLight", location=[100,0,0], label="Child")
attach_actor(child="Child", parent="Parent")
set_actor_transform("Parent", location=[500,500,100])
```

**Human verifies:** Light moves with mesh when parent moves.

---

### Test Suite 5: bp_toolkit (Offline Asset Manipulation)

> **Note:** These tests work WITHOUT the editor running. Close editor first.

#### 5.1 Asset Export/Import Round-Trip

| Test | Claude Does | Human Verifies |
|------|-------------|----------------|
| **5.1.1 Export asset** | `bp_export_asset("D:/tempo/TempoSample/Content/SomeBlueprint.uasset")` | JSON file created |
| **5.1.2 Detect type** | `bp_detect_type("SomeBlueprint.json")` | Returns correct type |
| **5.1.3 Get info** | `bp_get_info("SomeBlueprint.json")` | Summary looks correct |
| **5.1.4 List properties** | `bp_list_properties("SomeBlueprint.json")` | Properties listed |
| **5.1.5 Import back** | `bp_import_asset("SomeBlueprint.json")` | .uasset recreated |

**Human verifies:** Open asset in editor, verify it's unchanged.

#### 5.2 Blueprint Modification

| Test | Claude Does | Human Verifies |
|------|-------------|----------------|
| **5.2.1 Export BP** | `bp_export_asset("path/to/BP_Test.uasset")` | JSON created |
| **5.2.2 Add comment** | `bp_add_comment("BP_Test.json", "EventGraph", "Added by AgentBridge", x=100, y=100)` | Returns success |
| **5.2.3 Import back** | `bp_import_asset("BP_Test.json")` | .uasset updated |

**Human verifies:** Open Blueprint, see comment node at specified location.

#### 5.3 Asset Cloning

```
bp_clone_asset("Template.json", "NewAsset", package_path="/Game/Generated")
bp_import_asset("NewAsset.json")
```

**Human verifies:** New asset exists in Content/Generated folder.

#### 5.4 Property Path Modification

```
bp_export_asset("SomeDataAsset.uasset")
bp_get_property("SomeDataAsset.json", "SomeProperty")
bp_set_property("SomeDataAsset.json", "SomeProperty", "NewValue")
bp_import_asset("SomeDataAsset.json")
```

**Human verifies:** Open DataAsset, property has new value.

---

### Test Suite 6: World Partition (If Available)

| Test | Claude Does | Human Verifies |
|------|-------------|----------------|
| **6.1 Check partitioned** | `is_world_partitioned()` | Returns true/false |
| **6.2 Query all actors** | `query_all_actors(include_unloaded=True)` | Shows loaded + unloaded |
| **6.3 Get landscape bounds** | `get_landscape_bounds()` | Bounds match visible landscape |
| **6.4 Get data layers** | `get_data_layers()` | Lists configured layers |

---

### Test Suite 7: Console Commands

| Test | Claude Does | Human Verifies |
|------|-------------|----------------|
| **7.1 Search commands** | `search_console_commands("stat")` | Returns stat commands |
| **7.2 Execute stat fps** | `execute_console_command("stat fps")` | FPS overlay appears |
| **7.3 Disable stat** | `execute_console_command("stat fps")` (toggle) | FPS overlay gone |
| **7.4 Search shadows** | `search_console_commands("shadow")` | Returns shadow CVars |

---

### Test Suite 8: Edge Cases & Error Handling

| Test | Expected Result |
|------|-----------------|
| `get_actor("NonExistent")` | Clear error: "Actor not found" |
| `set_property("Light", "BadPath.Nope")` | Error with valid paths hint |
| `spawn_actor("NotARealClass")` | Error listing similar classes |
| `bp_export_asset("nonexistent.uasset")` | File not found error |

---

### Suggested Test Order

1. **Morning:** Suites 1-4 (live editor, visual verification)
2. **Midday:** Suite 5 (bp_toolkit, editor closed)
3. **Afternoon:** Suites 6-8 (advanced features, edge cases)

### Success Criteria

- [ ] All actor operations work and are visually verified
- [ ] Property paths work for components and nested structs
- [ ] Color/intensity changes are immediately visible
- [ ] bp_toolkit round-trip preserves asset integrity
- [ ] bp_toolkit modifications appear in editor
- [ ] Error messages are helpful, not cryptic
- [ ] No crashes or hangs

---

## Previous Sessions Summary

### Session 20
- Fixed nested property SET (BlueprintReadOnly flag issue)
- Component path GET/SET fully working

### Session 19
- Fixed nested BP struct writes
- UObject property access (DataAssets, Materials)
- Component path fallback

### Session 17-18
- Documentation reorganization
- All Priority 1/2/3 wishlist items complete
- Landscape bounds, label patterns, flexible value formats

### Session 15-16
- Static function support
- PCG Biome workflow testing
- Full class schema for non-Actor classes

---

## Known Limitations

| Issue | Status | Workaround |
|-------|--------|------------|
| TSoftObjectPtr assignment | Won't fix | Use TObjectPtr properties |
| FunctionInvoker struct returns | Auto-fixed | Transparent fallback to property access |

---

## Quick Resume Checklist

1. **Start editor** (GUI mode for testing, headless for automation)
2. **Verify connection** (see Prerequisites above)
3. **Check git status** for any uncommitted work
4. **Review this testing plan** and pick up where you left off

---

*Session 21: GitHub repos configured, README created, testing plan prepared*
