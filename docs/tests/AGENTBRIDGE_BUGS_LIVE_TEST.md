# Live Testing Plan: AGENTBRIDGE_BUGS Fixes

Branch: `feature/agentbridge-bugs`

## Prerequisites

1. **Proto regeneration** (required for Bug 5):
   ```bash
   cd <PROJECT_ROOT>/Plugins/Tempo/TempoCore/Scripts
   ./GenProtos.sh
   ```

2. **Kill editor, rebuild, restart**:
   ```bash
   cmd //c "taskkill /F /IM UnrealEditor.exe"
   cd <PROJECT_ROOT>/Scripts && ./Build.sh
   cd <PROJECT_ROOT> && ./Plugins/Tempo/Scripts/Run.sh
   ```
   Wait ~30 seconds for gRPC on port 10001.

3. **MCP connected** — verify with `/mcp` in Claude Code.

4. **Test level** — any level with a landscape (for Bug 5). The default TempoSample level works.

---

## Test Execution

Mark each test as you go: `[ ]` → `[x]` PASS or `[!]` FAIL (add notes).

---

### Bug 1: `call_function` — host/port stored

**What was broken:** Every tool using `client.host`/`client.port` threw `AttributeError`.

#### Test 1.1 — call_function with static function
```python
call_function(call="KismetSystemLibrary::PrintString", parameters={"InString": "Bug1 Test"})
```
- [ ] Returns success (no `'AgentBridgeClient' object has no attribute 'host'` error)
- [ ] Check UE output log for "Bug1 Test" string

#### Test 1.2 — call_function with actor instance
```python
# First spawn a test actor
spawn_actor(class_name="StaticMeshActor", label="Bug1TestActor")
call_function(call="Bug1TestActor.SetActorHiddenInGame", parameters={"bNewHidden": true})
```
- [ ] Returns success
- [ ] Actor becomes hidden in viewport

#### Test 1.3 — spawn_actor with relative_to (also uses client.host)
```python
spawn_actor(class_name="PointLight", label="Bug1Anchor", location=[0, 0, 200])
spawn_actor(class_name="PointLight", label="Bug1Relative", location=[500, 0, 0], relative_to="Bug1Anchor")
```
- [ ] Second actor spawns at (500, 0, 200) — offset relative to anchor

#### Test 1.4 — add_component (also uses client.host)
```python
spawn_actor(class_name="StaticMeshActor", label="Bug1CompTest")
add_component(actor_id="Bug1CompTest", component_type="PointLightComponent")
```
- [ ] Component added successfully

#### Test 1.5 — quit (also uses client.host — test LAST)
Skip or test manually. This will close the editor.

---

### Bug 2: `duplicate_asset` + `save_asset` — plugin content

**What was broken:** Duplicating assets from plugin content paths then saving crashed the editor.

#### Test 2.1 — Duplicate a plugin content asset
```python
# Find a plugin content asset to duplicate (PCGBiome templates, or any plugin asset)
duplicate_asset(
    source_path="/PCGBiomeCore/Templates/DefaultBiomeDefinition.DefaultBiomeDefinition",
    dest_package_path="/Game/Test",
    dest_asset_name="Bug2TestDef")
```
- [ ] Returns success (if PCGBiomeCore plugin exists)
- [ ] If plugin not available, try any engine plugin asset

#### Test 2.2 — Save the duplicated asset
```python
save_asset(asset_path="/Game/Test/Bug2TestDef.Bug2TestDef")
```
- [ ] Returns success WITHOUT crash
- [ ] Editor remains running (the old bug crashed the editor here)

#### Test 2.3 — Regression: duplicate a /Game/ asset (should still work)
```python
# Create a test asset first
create_asset(asset_class="DataAsset", package_path="/Game/Test", asset_name="Bug2Source")
save_asset(asset_path="/Game/Test/Bug2Source.Bug2Source")
duplicate_asset(
    source_path="/Game/Test/Bug2Source.Bug2Source",
    dest_package_path="/Game/Test",
    dest_asset_name="Bug2Copy")
save_asset(asset_path="/Game/Test/Bug2Copy.Bug2Copy")
```
- [ ] All operations succeed (no regression for non-plugin assets)

---

### Bug 3: `set_property` on BoxExtent — visual update

**What was broken:** Setting BoxExtent via set_property stored the value but wireframe didn't update.

#### Test 3.1 — BoxExtent visual update
```python
# Spawn an actor with a BoxComponent
spawn_actor(class_name="TriggerBox", label="Bug3Box", location=[0, 0, 200])

# Check current extent
get_property(actor_id="Bug3Box", path="CollisionComponent.BoxExtent")

# Set new extent
set_property(actor_id="Bug3Box", path="CollisionComponent.BoxExtent",
    value={"X": 500, "Y": 500, "Z": 200})
```
- [ ] get_property readback shows new values
- [ ] **VISUAL CHECK**: Wireframe box in viewport is visibly larger (was the main bug)

#### Test 3.2 — Light intensity (component property, visual update)
```python
spawn_actor(class_name="PointLight", label="Bug3Light", location=[0, 0, 300])
get_property(actor_id="Bug3Light", path="LightComponent0.Intensity")
set_property(actor_id="Bug3Light", path="LightComponent0.Intensity", value=50000.0)
```
- [ ] Readback shows 50000
- [ ] **VISUAL CHECK**: Light appears brighter in viewport

#### Test 3.3 — Actor direct property (non-component path)
```python
spawn_actor(class_name="StaticMeshActor", label="Bug3Direct", location=[200, 0, 200])
set_property(actor_id="Bug3Direct", path="bHidden", value=true)
```
- [ ] Actor disappears from viewport
- [ ] Regression: set_property still returns success

#### Test 3.4 — Nested struct property (regression)
```python
spawn_actor(class_name="StaticMeshActor", label="Bug3Nested", location=[400, 0, 200])
set_property(actor_id="Bug3Nested", path="RootComponent.RelativeLocation",
    value={"X": 400, "Y": 100, "Z": 200})
get_property(actor_id="Bug3Nested", path="RootComponent.RelativeLocation")
```
- [ ] Readback shows updated location
- [ ] Actor moved in viewport

---

### Bug 4: `set_property` — type validation for soft object refs

**What was broken:** Setting a soft object ref to a wrong-type asset reported success but value didn't persist.

#### Test 4.1 — Type mismatch detection
```python
# Need an actor/asset with a TSoftObjectPtr property
# Try setting a non-texture to a texture property (if available in your content)
# This is content-dependent — adjust paths based on what's in the project

# Create test assets
create_asset(asset_class="DataAsset", package_path="/Game/Test", asset_name="Bug4Asset")

# If there's a soft object property that expects a specific type, test it:
# set_property(actor_id="/Game/Test/Bug4Asset.Bug4Asset", path="SomeSoftRef",
#     value="/Game/SomeWrongType.SomeWrongType")
# → Should return success=false with type mismatch warning
```
- [ ] Type mismatch returns `success: false` (if testable with available content)
- [ ] Check UE output log for "WriteObjectProperty" warning

#### Test 4.2 — Valid soft object ref (regression)
```python
# Test that valid references still work
# This is content-dependent — use whatever soft refs exist in your project
```
- [ ] Valid references still set successfully

> **Note:** This bug is hard to test without specific content that has TSoftObjectPtr properties.
> The fix is defensive — it only rejects invalid types, valid types pass through unchanged.
> If no testable content is available, verify via output log by attempting a known mismatch.

---

### Bug 5: `get_landscape_bounds` — biome_volume_scale field

**What was added:** A new `biome_volume_scale` field in the response.

> **REQUIRES:** Proto regeneration + rebuild. If skipped, this test will fail.

#### Test 5.1 — biome_volume_scale present
```python
get_landscape_bounds()
```
- [ ] Response includes `biome_volume_scale` field
- [ ] `biome_volume_scale.x` = `extent.x / 100`
- [ ] `biome_volume_scale.y` = `extent.y / 100`
- [ ] `biome_volume_scale.z` = `(extent.z + 10000) / 100`

#### Test 5.2 — Use scale to size a volume
```python
bounds = get_landscape_bounds()
# Use the scale to size a test actor
spawn_actor(class_name="TriggerBox", label="Bug5Volume",
    location=[bounds.center.x, bounds.center.y, bounds.center.z])
set_transform(target="Bug5Volume",
    scale=[bounds.biome_volume_scale.x,
           bounds.biome_volume_scale.y,
           bounds.biome_volume_scale.z])
```
- [ ] Volume covers the landscape (visual check)

#### Test 5.3 — Existing fields unchanged (regression)
```python
bounds = get_landscape_bounds()
```
- [ ] `valid`, `min`, `max`, `center`, `extent`, `proxy_count`, `landscape_name` all present
- [ ] Values match pre-fix behavior

---

### Bug 6: `list_classes` — plugin Blueprint discovery

**What was broken:** `list_classes(name_pattern="BP_PCGBiomeCore")` returned 0 results.

#### Test 6.1 — Find plugin Blueprint class
```python
list_classes(name_pattern="BP_PCGBiomeCore")
```
- [ ] Returns at least 1 result (if PCGBiome plugin exists)
- [ ] Class info includes ClassName, ClassPath, bIsBlueprint=true

#### Test 6.2 — Consistency with spawn_actor
```python
# Whatever list_classes returns should be spawnable
result = list_classes(name_pattern="BP_PCGBiomeCore")
# If found, try spawning
spawn_actor(class_name="BP_PCGBiomeCore", label="Bug6Test")
```
- [ ] Both list_classes and spawn_actor agree (both find or both don't find)

#### Test 6.3 — base_class_name filter still works (regression)
```python
list_classes(base_class_name="Light", limit=5)
```
- [ ] Returns light-related classes
- [ ] Results are not bloated (Phase 1.5 didn't add duplicates)

#### Test 6.4 — No name_pattern (regression)
```python
list_classes(base_class_name="Actor", limit=5)
```
- [ ] Returns results from Phase 1 only (Phase 1.5 skipped when pattern is empty)
- [ ] No performance regression

---

### Bugs 7+8: ProtoPropertyValueToJson — complete type handling

**What was broken:** ARRAY, STRUCT, TRANSFORM, OBJECT, CLASS, MAP, ENUM types fell through to the `default` case, corrupting JSON serialization.

#### Test 7.1 — Set struct array at array level
```python
# Need an actor with a struct array property
# Create test actor and try array-level set
spawn_actor(class_name="StaticMeshActor", label="Bug7Test")
set_property(actor_id="Bug7Test", path="Tags", value=["Tag1", "Tag2", "Tag3"])
get_property(actor_id="Bug7Test", path="Tags")
```
- [ ] Tags array set and readback shows ["Tag1", "Tag2", "Tag3"]

#### Test 7.2 — Set object reference in array
```python
# Content-dependent — test if available
```
- [ ] Object ref in array sets correctly

#### Test 7.3 — Transform property (new type case)
```python
spawn_actor(class_name="StaticMeshActor", label="Bug7Transform")
get_property(actor_id="Bug7Transform", path="RootComponent.RelativeTransform")
```
- [ ] Transform readback includes Location, Rotation, Scale (not raw string)

#### Test 7.4 — Improved error message
```python
spawn_actor(class_name="StaticMeshActor", label="Bug7Error")
set_property(actor_id="Bug7Error", path="NonExistentPath", value="test")
```
- [ ] Error message mentions "value type mismatch or invalid format"
- [ ] Error suggests JSON array syntax

#### Test 7.5 — Existing property types unchanged (regression)
```python
spawn_actor(class_name="PointLight", label="Bug7Regression", location=[0, 0, 300])

# Bool
set_property(actor_id="Bug7Regression", path="bHidden", value=false)
get_property(actor_id="Bug7Regression", path="bHidden")

# Float
set_property(actor_id="Bug7Regression", path="LightComponent0.Intensity", value=5000.0)
get_property(actor_id="Bug7Regression", path="LightComponent0.Intensity")

# Vector
get_property(actor_id="Bug7Regression", path="RootComponent.RelativeLocation")

# String
set_property(actor_id="Bug7Regression", path="Tags", value=["TestTag"])
get_property(actor_id="Bug7Regression", path="Tags")
```
- [ ] All existing types still read/write correctly

---

## Cleanup

After all tests pass, remove test actors and assets:

```python
# Delete test actors
for label in ["Bug1TestActor", "Bug1Anchor", "Bug1Relative", "Bug1CompTest",
              "Bug3Box", "Bug3Light", "Bug3Direct", "Bug3Nested",
              "Bug5Volume", "Bug6Test",
              "Bug7Test", "Bug7Transform", "Bug7Error", "Bug7Regression"]:
    delete_actor(actor_id=label)

# Test assets in /Game/Test/ can be deleted manually or left for CI
```

---

## Test Results Summary

| Bug | Test | Result | Notes |
|-----|------|--------|-------|
| 1 | 1.1 call_function static | | |
| 1 | 1.2 call_function actor | | |
| 1 | 1.3 spawn relative_to | | |
| 1 | 1.4 add_component | | |
| 2 | 2.1 duplicate plugin asset | | |
| 2 | 2.2 save duplicated asset | | |
| 2 | 2.3 regression: /Game/ dup | | |
| 3 | 3.1 BoxExtent visual | | |
| 3 | 3.2 Light intensity visual | | |
| 3 | 3.3 Actor direct property | | |
| 3 | 3.4 Nested struct regression | | |
| 4 | 4.1 Type mismatch detection | | |
| 4 | 4.2 Valid ref regression | | |
| 5 | 5.1 biome_volume_scale field | | |
| 5 | 5.2 Scale sizes volume | | |
| 5 | 5.3 Existing fields regression | | |
| 6 | 6.1 Find plugin BP class | | |
| 6 | 6.2 Consistency with spawn | | |
| 6 | 6.3 base_class filter regression | | |
| 6 | 6.4 No pattern regression | | |
| 7 | 7.1 Struct array at array level | | |
| 7 | 7.2 Object ref in array | | |
| 7 | 7.3 Transform readback | | |
| 7 | 7.4 Improved error message | | |
| 7 | 7.5 Existing types regression | | |

**Total Tests:** 25
**Pass:** ___
**Fail:** ___
**Skip:** ___
