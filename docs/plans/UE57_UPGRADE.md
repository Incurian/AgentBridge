# Plan: UE 5.7 Upgrade for AgentBridge

## Overview

**Current state:** AgentBridge runs on UE 5.6 with 4 plugins (Core, Runtime, Scripting,
Server), ~100 MCP tools, and a validated LANDSCAPE_OPS plan ready for implementation.

**Goal:** Migrate AgentBridge to UE 5.7.3 (latest hotfix) with zero regressions in
existing tool behavior, verify the LANDSCAPE_OPS plan against 5.7 APIs, and selectively
adopt beneficial new 5.7 APIs.

**User-facing behavior:** All existing MCP tools work identically on UE 5.7. The
LANDSCAPE_OPS plan is updated with any 5.7 API differences. No new tools are added in
this plan (new 5.7 capabilities are documented for future work).

**Research basis:** `docs/plans/UE57_MIGRATION.md` (synthesis) + 3 raw agent outputs in
`docs/plans/research/`. Validated by 5 agents (Round 1) - reports in
`docs/plans/validation/ue57_v{1-5}_*.txt`.

**Target engine version:** UE 5.7.3 (not base 5.7.0 - three hotfixes with 150+
cumulative fixes have been released).

## Scope

### Included in v1

- Compile all 4 AgentBridge plugins against UE 5.7.3
- Fix any transitive include or deprecated API errors
- Verify all existing MCP tool categories work (actors, properties, transforms, WP, PCG,
  Blueprint editing, console commands, assets) - ALL 12 BP+PCG editing tools tested
- Verify PCG Biome compatibility (component names, regen trigger, data asset paths)
- Verify landscape APIs for LANDSCAPE_OPS plan accuracy
- Update LANDSCAPE_OPS plan with 5.7 delta (API changes, new opportunities, paint changes)
- Verify bp_toolkit UAsset round-trip with 5.7 assets
- Documentation updates (CLAUDE.md, README.md, plan files)

### Deferred to Future

- Exposing new 5.7 landscape material parameter functions as MCP tools
- PCG standalone graph execution support
- PCG Editor Mode interactive tool access via MCP
- FastGeo awareness in query_actors (experimental, opt-in)
- Procedural Vegetation Editor integration
- New PCG node type documentation in help text

### Explicitly Excluded

- Modifying Tempo plugin source (Tempo has its own 5.7 migration path)
- Modifying Unreal Engine source
- Implementing LANDSCAPE_OPS (separate plan, separate branch)
- Adopting C++20 features proactively (just ensure compatibility)

## Prerequisite: Tempo on 5.7

AgentBridge depends on Tempo for gRPC transport. Tempo must build and run on UE 5.7
before AgentBridge can be tested. If Tempo has its own 5.7 issues, those must be resolved
first (outside this plan's scope).

**Check:** Build TempoSample project on UE 5.7.3 WITHOUT AgentBridge changes first. If
that fails, Tempo needs fixing before we proceed.

## Key Corrections from Validation

These corrections from validation Round 1 are integrated throughout the plan:

| # | Correction | Source | Impact |
|---|-----------|--------|--------|
| 1 | Landscape paint weight change is **deliberate design change** by Epic, NOT a regression. Will NOT be hotfixed. | V1 | LANDSCAPE_OPS must adapt, not wait for fix |
| 2 | Biome Core V2 shipped in **UE 5.6**, not 5.7. Already active on current engine. | V1 | PCG verification is lower risk than estimated |
| 3 | Target **5.7.3** specifically (150+ cumulative hotfixes) | V1 | Better stability baseline |
| 4 | `FProperty::ImportText_Direct` used in 5 call sites - has been renamed before | V2b | Added to HIGH risk table |
| 5 | `StructUtils` module may need explicit Build.cs dependency | V2b | Pre-emptive fix in P1 |
| 6 | Plan tests only 5/12 editing tools (read-only bias) | V5 | Expanded to test ALL 12 |
| 7 | No PASS/FAIL criteria, no commit points, placeholder paths | V3 | All fixed below |

## Design Decisions

| Question | Decision | Rationale |
|----------|----------|-----------|
| Migrate to 5.7 before or after LANDSCAPE_OPS? | **Before** | 5.7 has new landscape APIs (AddTargetLayer) that could simplify LANDSCAPE_OPS, and we need to verify the paint weight behavior (deliberate change) before committing to the FAlphamapAccessor approach |
| Adopt new 5.7 APIs proactively? | **No - document only** | Migration scope is compatibility. New API adoption belongs in LANDSCAPE_OPS or future plans |
| Update LANDSCAPE_OPS plan inline or separate doc? | **Inline update** | Add a "UE 5.7 Delta" section to existing plan with verified API differences |
| Branch strategy | `feature/ue57-upgrade` off `master` | Standard branch-per-plan convention |
| How to handle Tempo dependency? | **Verify Tempo builds first, then proceed** | Can't test AgentBridge gRPC if Tempo is broken on 5.7 |
| bp_toolkit .NET compatibility | **Test only, fix if broken** | UAssetAPI is external; we just verify round-trip works |
| FAlphamapAccessor fallback if paint behavior changed? | **Document alternatives, test first** | Direct accessor may bypass the brush behavior change - test before deciding |

## Risk Assessment

| Risk | Severity | Likelihood | Mitigation |
|------|----------|------------|------------|
| Transitive include errors | LOW | HIGH | Straightforward - add explicit #includes. See P1 guidance. |
| `FProperty::ImportText_Direct` renamed/removed | HIGH | LOW | Used in 5 call sites in PropertyAccessor.cpp. Verify in 5.7 FProperty header. If renamed, update all call sites. |
| `FAlphamapAccessor` behavior changed by deliberate paint redesign | HIGH | MEDIUM | Test direct SetData() writes in P4. If broken, alternatives: direct texture writes, AddTargetLayer() path, or defer PaintLandscapeLayers to future UE version. |
| Landscape `Import()` signature changed | HIGH | MEDIUM | Compare 5.7 LandscapeProxy.h header in P4.1 |
| `NotifyPropertiesChangedFromBlueprint()` removed | HIGH | LOW | If removed, find replacement regen trigger. Note: Biome V2 already shipped in 5.6 (works today). |
| `WorldPartitionEditorHash.h` moved/renamed | MEDIUM | LOW | Grep 5.7 headers for replacement |
| `FWorldPartitionHandle` / `FWorldPartitionActorDesc` method changes | MEDIUM | LOW | 7 methods + handle scope used in WorldPartitionOps.cpp. Verify in P2.3. |
| `DataLayerManager` / `DataLayerInstance` API changes | MEDIUM | LOW | Used for data layer queries. Test in P2.3. |
| K2Node / Blueprint graph API changes | MEDIUM | LOW | V5 confirmed all BP APIs present in 5.7 docs. Test all 6 tools in P3.4. |
| `StructUtils` transitive include removed | MEDIUM | MEDIUM | Pre-emptively add to AgentBridgeCore.Build.cs in P1. |
| `FAssetDataTagMapSharedView::FFindTagResult` changed | MEDIUM | LOW | Used for BP class discovery in TypeDiscovery.cpp |
| `PCG_LocalBiomeCore` renamed in Biome V2 | LOW | LOW | Biome V2 already active in 5.6 - if it worked before, it works now. |
| Tempo itself broken on 5.7 | BLOCKING | UNKNOWN | Out of scope - verify before starting |
| gRPC + MSVC 14.44 incompatibility | MEDIUM | LOW | Existing header isolation pattern should work |
| UAssetAPI can't parse 5.7 assets | LOW | LOW | External tool; test and report upstream if broken |

## Architecture

No architectural changes. This plan modifies existing files to maintain compatibility.

### Concrete Paths

All paths are relative to the project. The key directories:

| What | Path |
|------|------|
| Project root | `/mnt/d/tempofresh/TempoSample` |
| Plugin root | `/mnt/d/tempofresh/TempoSample/Plugins/AgentBridge` |
| .uproject file | `/mnt/d/tempofresh/TempoSample/TempoSample.uproject` |
| Build script | `/mnt/d/tempofresh/TempoSample/Scripts/Build.sh` |
| Run script | `/mnt/d/tempofresh/TempoSample/Plugins/Tempo/Scripts/Run.sh` |
| Tempo SyncDeps | `/mnt/d/tempofresh/TempoSample/Plugins/Tempo/Scripts/SyncDeps.sh` |
| Tempo InstallEngineMods | `/mnt/d/tempofresh/TempoSample/Plugins/Tempo/Scripts/InstallEngineMods.sh` |
| Tempo ROS SyncDeps | `/mnt/d/tempofresh/TempoSample/Plugins/Tempo/TempoROS/Scripts/SyncDeps.sh` |
| UE 5.7 engine | User must specify `<ENGINE_ROOT>` (varies per installation) |
| Project log | `/mnt/d/tempofresh/TempoSample/Saved/Logs/TempoSample.log` |

### Files Most Likely to Need Changes

Based on the transitive include analysis (75+ unique UE includes across 24 source files):

| Module | Source Files | UE Includes | Risk |
|--------|-------------|-------------|------|
| AgentBridgeScripting | CommandExecutor.cpp (2200+ lines), AgentCommands.h | ~57 unique | HIGHEST |
| AgentBridgeRuntime | WorldPartitionOps.cpp, ActorOperations.cpp, + 4 others | ~25 unique | MEDIUM |
| AgentBridgeCore | PropertyAccessor.cpp, TypeDiscovery.cpp, + 4 others | ~11 unique | LOW (but ImportText_Direct risk) |
| AgentBridgeServer | AgentBridgeServiceSubsystem.cpp, AgentHttpServer.cpp | ~9 unique | LOW |

### Key Headers to Watch

**Property Reflection (Core) - HIGH PRIORITY:**
- `UObject/UnrealType.h` - FProperty, ImportText_Direct (5 call sites in PropertyAccessor.cpp)

**World Partition (Runtime):**
- `WorldPartition/WorldPartitionEditorHash.h` - may be reorganized
- `WorldPartition/WorldPartitionActorDesc.h` - 7 methods used (GetActorSoftPath, GetActorNativeClass, etc.)
- `WorldPartition/WorldPartitionHandle.h` - FWorldPartitionHandle + PinRefScope
- `WorldPartition/DataLayer/DataLayerManager.h` - data layer queries
- `WorldPartition/DataLayer/DataLayerInstance.h` - data layer instances

**Landscape (Runtime, will affect LANDSCAPE_OPS):**
- `Landscape.h`, `LandscapeProxy.h` - Import() signature lives here
- `LandscapeStreamingProxy.h` - streaming landscape chunks
- `LandscapeEdit.h` - FAlphamapAccessor, FHeightmapAccessor template definitions
- `LandscapeEditorModule.h` - heightmap file loading chain
- `LandscapeImportHelper.h` - ChooseBestComponentSizeForImport
- `LandscapeDataAccess.h` - landscape data access utilities
- `LandscapeSubsystem.h` - ChangeGridSize()
- `LandscapeInfo.h` - FLandscapeInfoLayerSettings

**Blueprint K2 (Scripting):**
- `K2Node_CallFunction.h`, `K2Node_Event.h`, `K2Node_VariableGet.h`, etc.
- `Kismet2/BlueprintEditorUtils.h` - MarkBlueprintAsModified, FindEventGraph
- `EdGraphSchema_K2.h` - TryCreateConnection, TypeToText, pin types

**Struct Utils (Core):**
- `StructUtils/UserDefinedStruct.h` - may need explicit `"StructUtils"` module dep

**Image/Asset (Scripting):**
- `IImageWrapper.h`, `IImageWrapperModule.h` - known gRPC conflict area
- `ObjectTools.h` - asset duplication
- `AssetRegistry/AssetRegistryModule.h` - FAssetDataTagMapSharedView

## Implementation Details

### Phase 0: Prerequisites

**P0.0 - Create branch:**
```bash
cd /mnt/d/tempofresh/TempoSample/Plugins/AgentBridge
git checkout master
git checkout -b feature/ue57-upgrade
```

**P0.1 - Install UE 5.7.3:**
Ensure UE 5.7.3 (latest hotfix) is installed via Epic Games Launcher or source build.
Update `/mnt/d/tempofresh/TempoSample/TempoSample.uproject` - change
`EngineAssociation` to the 5.7.3 engine GUID or version string.

**P0.2 - Re-patch Tempo for 5.7:**
Run all three Tempo setup scripts in Git Bash (NOT WSL):
```bash
cd /mnt/d/tempofresh/TempoSample
./Plugins/Tempo/Scripts/SyncDeps.sh
./Plugins/Tempo/Scripts/InstallEngineMods.sh
./Plugins/Tempo/TempoROS/Scripts/SyncDeps.sh
```

**P0.3 - Verify Tempo builds on 5.7:**
```bash
# Kill any running editor first
cmd //c "taskkill /F /IM UnrealEditor.exe"

# Build
cd /mnt/d/tempofresh/TempoSample/Scripts && ./Build.sh
```
**PASS:** Build succeeds with zero errors. **FAIL:** Build errors mentioning Tempo modules.
If FAIL, resolve Tempo issues first (outside this plan).

**P0.4 - Verify editor launches:**
```bash
cd /mnt/d/tempofresh/TempoSample && ./Plugins/Tempo/Scripts/Run.sh
```
Wait ~30 seconds. Check log at `/mnt/d/tempofresh/TempoSample/Saved/Logs/TempoSample.log`
for module load messages. **PASS:** Editor opens, no crash. **FAIL:** Editor crashes or
hangs.

> **COMMIT POINT:** After P0 passes, commit the .uproject change:
> `git add TempoSample.uproject && git commit -m "chore: update EngineAssociation to UE 5.7.3"`
> (Only if .uproject was modified. Skip if engine is associated by path, not GUID.)

### Phase 1: Compile & Fix

**P1.0 - Pre-emptive Build.cs fix:**
Add `"StructUtils"` to `AgentBridgeCore.Build.cs` PublicDependencyModuleNames if not
already present. This avoids a likely transitive include failure for
`StructUtils/UserDefinedStruct.h`.

File: `/mnt/d/tempofresh/TempoSample/Plugins/AgentBridge/AgentBridgeCore/Source/AgentBridgeCore/AgentBridgeCore.Build.cs`

**P1.1 - Initial build attempt:**
```bash
cmd //c "taskkill /F /IM UnrealEditor.exe"
cd /mnt/d/tempofresh/TempoSample/Scripts && ./Build.sh 2>&1 | tee /tmp/ue57_build_output.txt
```
Capture the full error output. Expected errors: missing `#include` directives from
transitive include cleanup, possibly deprecated API usage.

**P1.2 - Fix transitive includes:**
For each compile error of the form "undeclared identifier" or "incomplete type":

1. Identify the missing type from the error message
2. Search UE 5.7 engine headers for which header defines it:
   ```bash
   # Example: find which header defines FSomeType
   grep -r "class FSomeType" "<ENGINE_ROOT>/Engine/Source/" --include="*.h" -l
   # or
   grep -r "struct FSomeType" "<ENGINE_ROOT>/Engine/Source/" --include="*.h" -l
   ```
3. Add the explicit `#include` at the top of the affected file
4. After editing, verify line endings: `file <edited_file>` should say "ASCII text"
   not "CRLF". If CRLF: `sed -i 's/\r$//' <edited_file>`

Priority order (by risk from include analysis):
1. `AgentBridgeScripting` (57 UE includes - most likely to break)
2. `AgentBridgeRuntime` (25 UE includes)
3. `AgentBridgeCore` (11 UE includes)
4. `AgentBridgeServer` (9 UE includes)

**P1.3 - Verify ImportText_Direct exists:**
Specifically check that `FProperty::ImportText_Direct` still exists in UE 5.7:
```bash
grep -r "ImportText_Direct" "<ENGINE_ROOT>/Engine/Source/Runtime/CoreUObject/" --include="*.h"
```
If renamed or removed, update all 5 call sites in:
`AgentBridgeCore/Source/AgentBridgeCore/Private/PropertyAccessor.cpp`

**P1.4 - Fix deprecated APIs (if any):**
Research confirmed no deprecated patterns in our code, but the compile may reveal
deprecation warnings promoted to errors in 5.7. Fix case-by-case. Known candidates:
- `FProperty::ImportText` -> `FProperty::ImportText_Direct` (already using correct one)
- `FProperty::ElementSize` -> `FProperty::GetElementSize()` (not used by us)
- `TAtomic` -> `std::atomic` (likely not used by us)

**P1.5 - Verify clean build:**
```bash
cd /mnt/d/tempofresh/TempoSample/Scripts && ./Build.sh 2>&1 | tee /tmp/ue57_build_clean.txt
```
**PASS:** Zero errors AND zero new warnings from AgentBridge modules. (Pre-existing Tempo
or engine warnings are OK.) **FAIL:** Any error or new warning from AgentBridge code.

**P1.6 - Verify editor launch + plugins:**
```bash
cd /mnt/d/tempofresh/TempoSample && ./Plugins/Tempo/Scripts/Run.sh
```
Wait ~30 seconds. Check log for all 4 plugin load messages:
```bash
grep -E "AgentBridge(Core|Runtime|Scripting|Server)" /mnt/d/tempofresh/TempoSample/Saved/Logs/TempoSample.log
```
Also verify gRPC starts:
```bash
grep "10001" /mnt/d/tempofresh/TempoSample/Saved/Logs/TempoSample.log
```
**PASS:** All 4 modules loaded, gRPC listening on 10001. **FAIL:** Missing module or no
gRPC.

> **COMMIT POINT:** After P1 passes:
> `git add -A && git commit -m "fix: add missing includes and deps for UE 5.7.3 compatibility"`
> Verify line endings before committing: `file $(git diff --cached --name-only)`

### Phase 2: Core Verification (Live Testing)

Start the editor and connect MCP. Test each tool category. Record results in
`docs/tests/UE57_UPGRADE_TEST.md` using the template in the Testing Strategy section.

**P2.1 - Actor operations:**
```python
# 1. Query existing actors
result = query_actors(class_name="StaticMeshActor")
# PASS: Returns non-empty list with actor names/labels. FAIL: Error or empty when actors exist.

# 2. Spawn a test actor
spawned = spawn_actor(class_name="PointLight", location=[0, 0, 200], label="UE57_TestLight")
# PASS: Returns actor info with name. FAIL: Error or no actor in viewport.
# VISUAL CHECK: PointLight visible in viewport at (0,0,200).

# 3. Get actor details
details = get_actor(actor_id="UE57_TestLight", include_properties=True, include_components=True)
# PASS: Returns properties dict with LightComponent0. FAIL: Error or missing components.

# 4. Set transform
set_transform(target="UE57_TestLight", location=[100, 0, 200])
# PASS: Returns success. FAIL: Error.
# VISUAL CHECK: Light moved to (100,0,200) in viewport.

# 5. Get transform
xform = get_transform(target="UE57_TestLight")
# PASS: Location ~= [100, 0, 200]. FAIL: Wrong location or error.

# 6. Delete actor
delete_actor(actor_id="UE57_TestLight")
# PASS: Returns success, actor gone from viewport. FAIL: Error or actor persists.
```

**P2.2 - Property operations:**
```python
# Spawn a test light for property tests
spawn_actor(class_name="PointLight", location=[0, 0, 200], label="UE57_PropTest")

# 1. Set float property
set_property(actor_id="UE57_PropTest", path="LightComponent0.Intensity", value=5000)
# PASS: Returns success. FAIL: Error.

# 2. Get float property
val = get_property(actor_id="UE57_PropTest", path="LightComponent0.Intensity")
# PASS: val == 5000 (or close). FAIL: Wrong value or error.

# 3. Set color property (tests ImportText_Direct path)
set_property(actor_id="UE57_PropTest", path="LightComponent0.LightColor", value=[255, 0, 0])
# PASS: Returns success. FAIL: Error.
# VISUAL CHECK: Light color changed to red in viewport.

# 4. Get color property
color = get_property(actor_id="UE57_PropTest", path="LightComponent0.LightColor")
# PASS: R=255, G=0, B=0. FAIL: Wrong color or error.

# 5. Set struct property (tests nested path resolution)
set_property(actor_id="UE57_PropTest", path="RootComponent.RelativeLocation", value=[50, 50, 300])
# PASS: Returns success. FAIL: Error.

# 6. Get struct property
loc = get_property(actor_id="UE57_PropTest", path="RootComponent.RelativeLocation")
# PASS: X~=50, Y~=50, Z~=300. FAIL: Wrong values or error.

# Cleanup
delete_actor(actor_id="UE57_PropTest")
```

**P2.3 - World Partition and Data Layers:**
```python
# 1. Check WP status
wp = is_world_partitioned()
# PASS: Returns True/False without error. FAIL: Error.

# 2. Query with unloaded actors (if WP enabled)
actors = query_actors(include_unloaded=True, limit=10)
# PASS: Returns results. FAIL: Error (NOT empty - that's OK if map is small).

# 3. Landscape queries
landscapes = query_landscape()
# PASS: Returns landscape list. FAIL: Error.

# 4. Landscape bounds
bounds = get_landscape_bounds()
# PASS: Returns min/max/center/half_extents. FAIL: Error (OK if no landscape).

# 5. Data layers
layers = get_data_layers()
# PASS: Returns list (may be empty). FAIL: Error.
```

**P2.4 - Type discovery:**
```python
# 1. List classes
classes = list_classes(name_pattern="PointLight")
# PASS: Returns matching classes including APointLight. FAIL: Error or empty.

# 2. Get class schema
schema = get_class_schema(class_name="PointLight", include_functions=True)
# PASS: Returns properties and functions. FAIL: Error or empty schema.
```

**P2.5 - Console commands:**
```python
# 1. Search commands
cmds = search_console_commands(keyword="landscape")
# PASS: Returns command list. FAIL: Error.

# 2. Execute command
execute_console_command(command="stat fps")
# PASS: Returns without error. FAIL: Error. VISUAL CHECK: FPS counter appears.
```

**P2.6 - Asset operations:**
```python
# 1. Create asset
create_asset(asset_class="DataAsset", package_path="/Game/Test", asset_name="TestDA57")
# PASS: Returns success. FAIL: Error.

# 2. Save asset
save_asset(asset_path="/Game/Test/TestDA57")
# PASS: Returns success. FAIL: Error.

# 3. Verify in Content Browser, then cleanup
delete_actor(actor_id="TestDA57")  # if applicable, or delete via editor
```

> **COMMIT POINT:** After P2, commit test results only (no code changes expected):
> `git add docs/tests/UE57_UPGRADE_TEST.md && git commit -m "test: Phase 2 core verification results for UE 5.7.3"`

### Phase 3: PCG, Biome & Blueprint Verification (Live Testing)

**P3.1 - PCG graph tools (ALL 6 tools):**

First, discover a PCG graph asset to test with:
```python
# Find PCG graph assets in the project
query_actors(class_name="PCGVolume", limit=5)
# Or search Content Browser for PCG graph assets
```

Then test all 6 PCG tools:
```python
# Use a discovered graph path, e.g. "/Game/PCG/MyPCGGraph"
graph_path = "<discovered_path>"

# 1. List nodes (READ)
nodes = pcg_list_nodes(graph_path=graph_path)
# PASS: Returns node list with InputNode and OutputNode. FAIL: Error.

# 2. Get input/output nodes (READ)
io = pcg_get_input_output_nodes(graph_path=graph_path)
# PASS: Returns input_node and output_node paths. FAIL: Error.

# 3. Add node (WRITE)
new_node = pcg_add_node(graph_path=graph_path, node_type="SurfaceSampler", pos_x=200)
# PASS: Returns created node path. FAIL: Error.

# 4. Connect nodes (WRITE)
pcg_connect(graph_path=graph_path, from_node=io["input_node"], from_pin="In",
            to_node=new_node, to_pin="Input")
# PASS: Returns success. FAIL: Error or wrong pin name.

# 5. Disconnect nodes (WRITE)
pcg_disconnect(graph_path=graph_path, from_node=io["input_node"], from_pin="In",
               to_node=new_node, to_pin="Input")
# PASS: Returns success. FAIL: Error.

# 6. Delete node (WRITE)
pcg_delete_node(graph_path=graph_path, node_path=new_node)
# PASS: Returns success, node gone from graph. FAIL: Error.
```

**P3.2 - Biome component verification:**
```python
# Find BiomeTexture actors
biomes = query_actors(label_pattern="*BiomeTexture*")
# If no results, try: query_actors(label_pattern="*Biome*")
# If still no results, biome testing requires loading a biome-enabled level.

# Check component exists
if biomes:
    actor = get_actor(actor_id=biomes[0]["name"], include_components=True)
    # PASS: Component list includes "PCG_LocalBiomeCore". FAIL: Component missing or renamed.
    # Document the actual component name if different.

    # Also check for BiomeTextureVolume (BoxComponent used for sizing)
    # PASS: Component list includes a BoxComponent. FAIL: Missing.
```

**P3.3 - Biome regen trigger:**
```python
# Test the regen workflow from AGENTS.md Phase 10
# Replace <BiomeTextureName> with actual actor name from P3.2
call_function(call="<BiomeTextureName>.PCG_LocalBiomeCore.NotifyPropertiesChangedFromBlueprint")
# PASS: No error returned AND visual regeneration occurs in viewport.
# FAIL: Error, or no visual change.
# VISUAL CHECK: PCG-generated content refreshes around the biome actor.
```

If `PCG_LocalBiomeCore` is renamed or `NotifyPropertiesChangedFromBlueprint()` is removed:
- Search for replacement component/function on the biome actor
- Update AGENTS.md workflow
- Update help text in `mcp/services/agentbridge.py` (references are in help text strings only,
  not in executable code paths - so this is a documentation fix, not a code fix)

**P3.4 - Biome data asset property paths:**
```python
# If biome actors exist, verify property paths haven't changed
if biomes:
    # Test BiomeDefinition path
    val = get_property(actor_id=biomes[0]["name"],
                       path="PCG_LocalBiomeCore.BiomeDefinition")
    # PASS: Returns a value (asset ref or None). FAIL: "Property not found" error.

    # If the project has biome assets configured, also test:
    # get_property(..., path="PCG_LocalBiomeCore.BiomeAssets")
```

**P3.5 - Blueprint editing tools (ALL 6 tools):**

First, discover a Blueprint to test with:
```python
# Find Blueprint assets - use any BP in the project
list_classes(name_pattern="BP_", base_class_name="Actor")
```

Then test all 6 Blueprint tools:
```python
bp_path = "<discovered_blueprint_path>"  # e.g. "/Game/Blueprints/BP_TestActor"

# 1. List nodes (READ)
nodes = bp_list_nodes(blueprint_path=bp_path)
# PASS: Returns node list. FAIL: Error.

# 2. Create a comment node (WRITE)
comment = bp_create_node(blueprint_path=bp_path, node_type="Comment",
                         comment="UE57 upgrade test")
# PASS: Returns node GUID. FAIL: Error.

# 3. List pins on a node (READ)
if nodes:
    pins = bp_list_pins(blueprint_path=bp_path, node_id=nodes[0]["guid"])
    # PASS: Returns pin list with names and types. FAIL: Error.

# 4. Create a function call node for pin testing (WRITE)
func_node = bp_create_node(blueprint_path=bp_path, node_type="CallFunction",
                           function_reference="KismetSystemLibrary::PrintString")
# PASS: Returns node GUID. FAIL: Error.

# 5. Connect pins (WRITE) - connect exec pins if possible
# Find exec output pin on one node and exec input on another
bp_connect_pins(blueprint_path=bp_path,
                source_node=comment["guid"], source_pin="OutputPin",
                target_node=func_node["guid"], target_pin="execute")
# Note: Comment nodes may not have exec pins - adjust test to use two function nodes
# PASS: Returns success. FAIL: Error.

# 6. Disconnect pins (WRITE)
bp_disconnect_pins(blueprint_path=bp_path,
                   source_node=comment["guid"], source_pin="OutputPin",
                   target_node=func_node["guid"], target_pin="execute")
# PASS: Returns success. FAIL: Error.

# Cleanup: delete test nodes
bp_delete_node(blueprint_path=bp_path, node_id=comment["guid"])
bp_delete_node(blueprint_path=bp_path, node_id=func_node["guid"])
```

> **COMMIT POINT:** After P3:
> `git add docs/tests/UE57_UPGRADE_TEST.md && git commit -m "test: Phase 3 PCG/Biome/BP verification results for UE 5.7.3"`

### Phase 4: Landscape API Verification

This phase verifies the LANDSCAPE_OPS plan's assumptions against UE 5.7. Does NOT
implement LANDSCAPE_OPS - just checks that the planned APIs exist and behave correctly.

**P4.1 - Header verification (source inspection):**

Search UE 5.7 engine headers at `<ENGINE_ROOT>/Engine/Source/`. Verify EACH of these APIs:

```bash
# 1. ALandscapeProxy::Import() - check signature (currently 11 params in 5.6)
grep -A 20 "void Import(" "<ENGINE_ROOT>/Engine/Source/Runtime/Landscape/Public/LandscapeProxy.h"
# PASS: Same 11-param signature. DOCUMENT any differences.

# 2. FAlphamapAccessor template
grep "FAlphamapAccessor" "<ENGINE_ROOT>/Engine/Source/Editor/LandscapeEditor/Public/LandscapeEdit.h"
# PASS: Template exists with <false,false> specialization. FAIL: Missing or renamed.

# 3. FHeightmapAccessor template
grep "FHeightmapAccessor" "<ENGINE_ROOT>/Engine/Source/Editor/LandscapeEditor/Public/LandscapeEdit.h"
# PASS: Template exists. FAIL: Missing or renamed.

# 4. ChooseBestComponentSizeForImport
grep "ChooseBestComponentSizeForImport" "<ENGINE_ROOT>/Engine/Source/" -r --include="*.h"
# PASS: Function exists in LandscapeImportHelper.h. FAIL: Missing or moved.

# 5. CreateTargetLayerSettingsFor
grep "CreateTargetLayerSettingsFor" "<ENGINE_ROOT>/Engine/Source/" -r --include="*.h"
# PASS: Still exists. Note if deprecated in favor of AddTargetLayer().

# 6. AddTargetLayer (new in 5.7)
grep "AddTargetLayer" "<ENGINE_ROOT>/Engine/Source/" -r --include="*.h"
# DOCUMENT: Full signature, parameters, return type.

# 7. ULandscapeSubsystem::ChangeGridSize
grep "ChangeGridSize" "<ENGINE_ROOT>/Engine/Source/" -r --include="*.h"
# PASS: Function exists. FAIL: Missing or signature changed.

# 8. ILandscapeEditorModule (heightmap loading chain)
grep "GetHeightmapFormatByExtension\|ILandscapeEditorModule" "<ENGINE_ROOT>/Engine/Source/" -r --include="*.h"
# PASS: Interface and format handlers exist. FAIL: Missing or reorganized.

# 9. FLandscapeImportLayerInfo struct
grep "struct FLandscapeImportLayerInfo" "<ENGINE_ROOT>/Engine/Source/" -r --include="*.h"
# PASS: Struct exists with expected fields. FAIL: Missing or changed.

# 10. HasLayersContent (edit layer check - must still reject edit-layer landscapes)
grep "HasLayersContent" "<ENGINE_ROOT>/Engine/Source/" -r --include="*.h"
# DOCUMENT: Behavior with new 5.7 Splines/Patches edit layer types.
# CHECK: Does a freshly created 5.7 landscape have HasLayersContent() == false?

# 11. FComponentReregisterContext
grep "FComponentReregisterContext" "<ENGINE_ROOT>/Engine/Source/" -r --include="*.h"
# PASS: Still in Components.h or similar. FAIL: Moved.
```

**P4.2 - New API inventory:**
Check which new 5.7 APIs from UE57_MIGRATION.md section 2.2 actually exist:
```bash
# New landscape management APIs
grep -E "AddTargetLayer|RemoveTargetLayer|UpdateTargetLayer|GetTargetLayers" \
  "<ENGINE_ROOT>/Engine/Source/" -r --include="*.h"

# New material parameter APIs
grep -E "SetLandscapeMaterial(Scalar|Texture|Vector)ParameterValue" \
  "<ENGINE_ROOT>/Engine/Source/" -r --include="*.h"

# CreateLandscapeTextureArray
grep "CreateLandscapeTextureArray" "<ENGINE_ROOT>/Engine/Source/" -r --include="*.h"
```
Document full signatures for each found API.

**P4.3 - FAlphamapAccessor behavioral test (if editor + landscape available):**

This tests whether the deliberate paint weight change affects programmatic accessor writes:
```python
# If the test level has a landscape with multiple layers:
# 1. Read current weight at a known location
# 2. Write a known weight value via set_property or direct accessor test
# 3. Read back and verify
# PASS: Written value matches read-back value.
# FAIL: Values differ (indicates accessor writes are also affected by paint redesign).
```

**If FAIL (accessor writes are broken):**
Document in LANDSCAPE_OPS.md UE 5.7 Delta section. Alternatives:
1. Direct texture writes bypassing the accessor
2. Use `AddTargetLayer()` if it supports weight initialization
3. Defer `PaintLandscapeLayers` to a future UE version where behavior is clarified
4. Implement on 5.6 first, port later

**P4.4 - AddTargetLayer decision:**
Based on P4.1 findings, decide and document:
- If `CreateTargetLayerSettingsFor()` still exists AND works: keep using it in LANDSCAPE_OPS
- If `AddTargetLayer()` is a direct replacement: update LANDSCAPE_OPS to use it instead
- If both exist: document both, recommend the higher-level API (AddTargetLayer)

**P4.5 - Update LANDSCAPE_OPS plan:**
Add a "UE 5.7 Delta" section to `docs/plans/LANDSCAPE_OPS.md` documenting:
- Any Import() signature changes (with corrected code snippets if needed)
- AddTargetLayer() vs CreateTargetLayerSettingsFor() recommendation
- Paint weight behavior: deliberate design change (NOT a bug), accessor test results
- HasLayersContent() behavior with new edit layer types
- Any new include paths needed
- ILandscapeEditorModule status

> **COMMIT POINT:** After P4:
> `git add docs/ && git commit -m "docs: Phase 4 landscape API verification for UE 5.7.3"`

### Phase 5: bp_toolkit Verification

**P5.1 - Blueprint UAsset round-trip:**
```python
# Find a Blueprint in the 5.7 project's Content directory
# Export it to JSON
bp_export_asset(uasset_path="D:/tempofresh/TempoSample/Content/<SomeBP>.uasset")
# PASS: JSON file created. FAIL: Export error.

# Re-import
bp_import_asset(json_path="<exported_json_path>")
# PASS: .uasset recreated. FAIL: Import error.

# Open in editor - verify no corruption
# VISUAL CHECK: Blueprint opens in BP editor without errors.
```

**P5.2 - PCG graph round-trip:**
Same test with a PCG graph .uasset file.

**P5.3 - MetaDataMap workaround:**
Check if the MetaDataMap nulling workaround (documented in `.old.claude/`) is still
needed for 5.7 assets, or if UAssetAPI has been updated. Test by round-tripping a
Blueprint WITHOUT the workaround first.

> **COMMIT POINT:** After P5 (test results only):
> `git add docs/tests/ && git commit -m "test: Phase 5 bp_toolkit verification for UE 5.7.3"`

### Phase 6: Apply Fixes

Based on findings from Phases 2-5, make code changes.

**P6.1 - Fix any tool regressions:**
For each FAIL in the test results:
1. Identify root cause (API rename, behavior change, missing include)
2. Implement the fix in the appropriate module
3. Verify line endings after editing: `file <edited_file>` (must be "ASCII text")
4. Rebuild and retest:
   ```bash
   cmd //c "taskkill /F /IM UnrealEditor.exe"
   cd /mnt/d/tempofresh/TempoSample/Scripts && ./Build.sh
   cd /mnt/d/tempofresh/TempoSample && ./Plugins/Tempo/Scripts/Run.sh
   # Wait for gRPC, then re-run the specific failing test
   ```
5. Mark test as PASS in `docs/tests/UE57_UPGRADE_TEST.md`

If a fix is non-trivial (>50 lines or architectural), scope it as a sub-task with its
own description and commit.

**P6.2 - Update LANDSCAPE_OPS.md:**
Integrate Phase 4 findings into the plan document (if not already done in P4.5).

> **COMMIT POINT:** After each fix:
> `git add <changed_files> && git commit -m "fix(<module>): <description> (UE57_UPGRADE P6)"`

### Phase 7: Documentation

**P7.1 - Update root CLAUDE.md:**
- Change "UE 5.6" references to "UE 5.7" throughout
- Update Known Issues if any new limitations found
- Document MSVC 14.44 as preferred toolchain
- Update any build command examples

**P7.2 - Update module CLAUDE.md files:**
- Document any header changes in affected modules
- Update include conflict warnings if new conflicts discovered
- Note any new module dependencies added (e.g., StructUtils)

**P7.3 - Update README.md:**
- Update engine version references
- Update any tool behavior changes
- Note 5.7.3 as minimum supported version

**P7.4 - Update UE57_MIGRATION.md:**
- Mark each prediction as CONFIRMED / CHANGED / NOT_APPLICABLE
- Add actual findings vs predictions
- Keep in `docs/plans/` as reference (do NOT archive yet - useful during LANDSCAPE_OPS)

**P7.5 - Update MEMORY.md:**
- Record migration outcome
- Note any surprising findings for future reference
- Update "Current Status" section

**P7.6 - Update Python help text (if needed):**
If any component names, property paths, or workflow steps changed in P3:
- Update `_get_help_text()` in `mcp/services/agentbridge.py`
- Update `workflows` topic with corrected biome workflow
- Update `pcg_volume` topic if volume component names changed

> **COMMIT POINT:** After P7:
> `git add -A && git commit -m "docs: update documentation for UE 5.7.3 migration"`

## Testing Strategy

This plan IS the test plan - each verification phase (P2-P5) is a structured test suite
run against the live editor.

### Protocol (Centaur Testing)

1. **Human starts** the UE 5.7.3 editor and waits for gRPC on port 10001
2. **Human connects** MCP server (Claude Code or manual)
3. **Agent executes** each test command in order
4. **Agent explains** what the human should see before each visual check
5. **Human confirms** visual results (viewport, BP editor, Content Browser)
6. **Agent records** results in test log
7. Any FAIL -> investigate -> fix in Phase 6 -> **retest the specific failure**
8. **Human confirms** cleanup before deleting test actors

### Test Results Template

Create `docs/tests/UE57_UPGRADE_TEST.md`:
```markdown
# UE 5.7.3 Upgrade Test Results

**Date:** YYYY-MM-DD
**Engine:** UE 5.7.3
**Branch:** feature/ue57-upgrade
**Tester:** [human + agent]

## Phase 2: Core Verification

| Test | Command | Expected | Actual | PASS/FAIL | Notes |
|------|---------|----------|--------|-----------|-------|
| P2.1a | query_actors(class="StaticMeshActor") | Non-empty list | | | |
| P2.1b | spawn_actor(class="PointLight") | Actor created | | | |
| ... | ... | ... | | | |

## Phase 3: PCG/Biome/BP Verification
...

## Phase 4: Landscape API Verification
...

## Phase 5: bp_toolkit Verification
...

## Summary
- Total tests: XX
- Passed: XX
- Failed: XX
- Fixes needed: [list]
```

### Visual Verification Required

| Test | What Human Should See |
|------|----------------------|
| P2.1b | PointLight icon appears at origin+200Z in viewport |
| P2.1d | Light icon moves to (100,0,200) |
| P2.2c | Light color changes from white to red |
| P3.3 | PCG-generated meshes/foliage refresh around biome actor |
| P3.5b | Comment node appears in Blueprint graph editor |
| P5.1 | Re-imported Blueprint opens without errors in BP editor |

## Implementation Checklist

### Phase 0: Prerequisites (Required First)
- [ ] **P0.0** Create branch `feature/ue57-upgrade` off `master`
- [ ] **P0.1** Install UE 5.7.3, update .uproject EngineAssociation
- [ ] **P0.2** Run Tempo SyncDeps.sh and InstallEngineMods.sh for 5.7
- [ ] **P0.3** Verify Tempo builds on 5.7 (no AgentBridge changes)
- [ ] **P0.4** Verify editor launches on 5.7
- [ ] **COMMIT** .uproject change

### Phase 1: Compile & Fix (After Phase 0)
- [ ] **P1.0** Pre-emptive: add StructUtils to AgentBridgeCore.Build.cs
- [ ] **P1.1** Attempt full build, capture error output
- [ ] **P1.2** Fix transitive include errors (requires P1.1)
- [ ] **P1.3** Verify ImportText_Direct still exists in 5.7 (requires P1.1)
- [ ] **P1.4** Fix deprecated API errors if any (requires P1.1)
- [ ] **P1.5** Verify clean build with zero errors (requires P1.2-P1.4)
- [ ] **P1.6** Verify editor launch + all 4 plugins load + gRPC starts (requires P1.5)
- [ ] **COMMIT** include/dep fixes

### Phase 2: Core Verification (After Phase 1)
- [ ] **P2.1** Test actor operations (spawn, query, transform, delete) - 6 subtests
- [ ] **P2.2** Test property operations (float, color, struct) - 6 subtests
- [ ] **P2.3** Test World Partition + Data Layer queries - 5 subtests
- [ ] **P2.4** Test type discovery (list_classes, get_class_schema) - 2 subtests
- [ ] **P2.5** Test console commands (search, execute) - 2 subtests
- [ ] **P2.6** Test asset operations (create, save) - 3 subtests
- [ ] **COMMIT** test results

All P2 items can run in parallel.

### Phase 3: PCG, Biome & Blueprint Verification (After Phase 1, parallel with Phase 2)
- [ ] **P3.1** Test ALL 6 PCG graph tools (list, io_nodes, add, connect, disconnect, delete)
- [ ] **P3.2** Verify PCG_LocalBiomeCore component name and presence
- [ ] **P3.3** Test biome regen trigger (NotifyPropertiesChangedFromBlueprint)
- [ ] **P3.4** Test biome data asset property paths
- [ ] **P3.5** Test ALL 6 Blueprint editing tools (list, pins, create, connect, disconnect, delete)
- [ ] **COMMIT** test results

P3.1-P3.5 can run in parallel.

### Phase 4: Landscape API Verification (After Phase 1, parallel with Phases 2-3)
- [ ] **P4.1** Verify ALL LANDSCAPE_OPS API signatures against 5.7 headers (11 items)
- [ ] **P4.2** Inventory new 5.7 landscape APIs
- [ ] **P4.3** Behavioral test: FAlphamapAccessor SetData() writes correct values
- [ ] **P4.4** Decide AddTargetLayer() vs CreateTargetLayerSettingsFor()
- [ ] **P4.5** Update LANDSCAPE_OPS.md with "UE 5.7 Delta" section
- [ ] **COMMIT** landscape verification + plan update

P4.1 and P4.2 can run in parallel (source inspection). P4.3 requires live editor.
P4.4-P4.5 require P4.1-P4.3.

### Phase 5: bp_toolkit Verification (After Phase 1, parallel with Phases 2-4)
- [ ] **P5.1** Test Blueprint UAsset export/import round-trip
- [ ] **P5.2** Test PCG graph UAsset export/import round-trip
- [ ] **P5.3** Check MetaDataMap workaround status
- [ ] **COMMIT** test results

P5.1-P5.3 can run in parallel.

### Phase 6: Apply Fixes (After Phases 2-5)
- [ ] **P6.1** Fix each FAIL from verification phases (rebuild + retest per fix)
- [ ] **P6.2** Update LANDSCAPE_OPS.md with verified 5.7 delta
- [ ] **COMMIT** per fix

### Phase 7: Documentation (After Phase 6)
- [ ] **P7.1** Update root CLAUDE.md (engine version, MSVC 14.44, build requirements)
- [ ] **P7.2** Update module CLAUDE.md files (header changes, new deps)
- [ ] **P7.3** Update README.md (engine version references)
- [ ] **P7.4** Update UE57_MIGRATION.md (mark predictions confirmed/changed)
- [ ] **P7.5** Update MEMORY.md (migration outcome)
- [ ] **P7.6** Update Python help text if component/property names changed
- [ ] **COMMIT** documentation updates

P7.1-P7.6 can run in parallel.

## Dependency Graph

```
P0 (Prerequisites + branch creation)
 |
 v
P1 (Compile & Fix)
 |
 +---> P2 (Core Verify) ----------+
 |                                 |
 +---> P3 (PCG/Biome/BP Verify) --+---> P6 (Apply Fixes) ---> P7 (Docs)
 |                                 |
 +---> P4 (Landscape Verify) -----+
 |                                 |
 +---> P5 (bp_toolkit Verify) ----+
```

Phases 2, 3, 4, 5 can all run in parallel after Phase 1 completes.
Phase 6 waits for all verification phases. Phase 7 waits for Phase 6.

## Estimated Scope

| Phase | Effort | Notes |
|-------|--------|-------|
| P0 | Setup | Depends on UE 5.7 install + Tempo status |
| P1 | Small-Medium | Likely just adding #includes + StructUtils dep; could be zero changes |
| P2 | Medium | 24 subtests, ~30 min with visual checks |
| P3 | Medium | 18 subtests across PCG/Biome/BP, ~30 min |
| P4 | Medium | 11 header checks + behavioral test, ~45 min |
| P5 | Small | 3 round-trip tests, ~15 min |
| P6 | Unknown | Depends entirely on what breaks; could be zero changes |
| P7 | Small | Documentation updates, ~30 min |

**Most likely outcome:** P1 requires 2-10 missing `#include` fixes + StructUtils dep.
P2-P5 pass cleanly (V5 confirmed all BP/PCG APIs present in 5.7 docs, Biome V2 already
active in 5.6). P6 has 0-3 small fixes. Total code changes: under 50 lines.

**Worst case:** A UE 5.7 API redesign (e.g., WorldPartition or landscape accessor)
requires significant refactoring. This would be scoped as a separate sub-plan.

## Validation History

| Round | Agents | Findings | Status |
|-------|--------|----------|--------|
| R1 | 5 (V1-V5) | 2C, 12H, 48M, 17L | INTEGRATED - all Critical and High fixes applied to this plan |

Reports: `docs/plans/validation/ue57_v{1-5}_*.txt`

Key corrections integrated:
- V1: Paint change is deliberate (not regression), Biome V2 in 5.6, target 5.7.3
- V2b: ImportText_Direct HIGH risk, StructUtils dep, WP Handle/DataLayer coverage
- V3: Concrete paths, PASS/FAIL criteria, commit points, branch creation, Phase 6 loop
- V4: Behavioral FAlphamapAccessor test, fallback plan, HasLayersContent check, full API list
- V5: Test all 12 editing tools, biome data asset paths, help text references
