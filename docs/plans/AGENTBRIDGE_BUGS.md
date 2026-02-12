# AgentBridge — Bugs and Required Changes

Code-level fixes needed in the AgentBridge plugin, identified during PCG Biome Workflow testing (2026-02-12).
See `PCG_BIOME_WORKFLOW_TEST.md` for full test log and reproduction steps.

---

## CRITICAL

### 1. `call_function` — Completely Broken

**Symptom:** Every invocation returns `'AgentBridgeClient' object has no attribute 'host'`.

**Reproduction:**
```python
call_function(call="BiomeCore.SetActorHiddenInGame", parameters={"bNewHidden": true})
# → {"error": "'AgentBridgeClient' object has no attribute 'host'"}
```

Tested with multiple targets and parameter formats — always the same error. This is an internal MCP server bug (Python side), not a parameter/syntax issue.

**Impact:** Blocks any workflow that needs to call Blueprint/C++ functions. In particular, `SetBoxExtent()` would be the proper fix for volume sizing (it triggers `UpdateBounds()`), but it can't be called.

**Fix location:** MCP Python server — `AgentBridgeClient` class is missing a `host` attribute.

**(Finding 12)**

---

### 2. `duplicate_asset` + `save_asset` — Crash on Plugin Content Sources

**Symptom:** `save_asset` crashes the editor with "Asset cannot be saved as it has only been partially loaded" when the asset was duplicated from a plugin content path.

**Reproduction:**
```python
# CRASHES:
duplicate_asset(source_path="/PCGBiomeCore/Templates/DefaultBiomeDefinition.DefaultBiomeDefinition",
    dest_package_path="/Game/MyProject", dest_asset_name="MyDefinition")
save_asset(asset_path="/Game/MyProject/MyDefinition.MyDefinition")  # → CRASH

# WORKS:
duplicate_asset(source_path="/Game/MyProject/DefaultBiomeDefinition.DefaultBiomeDefinition",
    dest_package_path="/Game/MyProject", dest_asset_name="MyDefinition")
save_asset(asset_path="/Game/MyProject/MyDefinition.MyDefinition")  # → OK
```

**Root cause:** Plugin assets use async/lazy loading. `DuplicateAsset()` copies the asset including its partial-load state. When `save_asset` tries to serialize, it hits UE's "partially loaded" guard.

**Fix:** Call `FlushAsyncLoading()` or use `LoadObject<T>()` / `StaticLoadObject()` (synchronous) to fully load the source asset before `UEditorAssetLibrary::DuplicateAsset()`.

**Location:** C++ side — wherever `DuplicateAsset` is implemented in AgentBridge (likely in `ActorOperations.cpp` or an asset operations file).

**Workaround:** Users must manually copy template assets from plugin to `/Game/` before programmatic duplication.

**(Findings 19, 20)**

---

## HIGH

### 3. `set_property` on BoxExtent — No Visual Update

**Symptom:** `set_property` on `UBoxComponent::BoxExtent` stores the value (verified via `get_property` readback) but the visual wireframe in the editor never updates. No amount of transform changes, saves, or other operations triggers a refresh.

**Root cause:** `set_property` uses UE reflection to write the UPROPERTY directly. This bypasses `UBoxComponent::SetBoxExtent()`, which calls `UpdateBounds()` and `MarkRenderStateDirty()`. Without these calls, the component doesn't know its extent changed.

**Impact:** Any workflow that sizes box volumes via `set_property` on `BoxExtent` appears to work (property reads back correctly) but visually does nothing. This is extremely confusing.

**Possible fixes (pick one):**
1. **Post-edit notification:** After `set_property`, call `PostEditChangeProperty()` on the component — this is what the editor details panel does when a user types a value
2. **Special-case BoxExtent:** Detect when `BoxExtent` is being set on a `UBoxComponent` and call `SetBoxExtent()` instead of raw property write
3. **General fix:** After any `set_property` on a scene component, call `MarkRenderStateDirty()` + `UpdateBounds()`
4. **Fix `call_function`:** If `call_function` worked, agents could call `SetBoxExtent()` directly as a workaround

**Current workaround:** Use `set_transform` on the component scale with `world_space=true` instead. Scale = desired_half_extent / 100 (default BoxExtent).

**(Findings 13, 16)**

---

### 4. `set_property` — Silent Failure on Type-Mismatched Object References

**Symptom:** Setting an object reference property to an asset of the wrong UClass reports `success: true` but the value doesn't persist. `get_property` readback shows empty.

**Reproduction:**
```python
# BiomeTexture expects a UTexture2D
set_property(actor_id="RedBiomeTexture", path="BiomeTexture",
    value="/Game/SomeNonTexture.SomeNonTexture")
# → {"success": true}

get_property(actor_id="RedBiomeTexture", path="BiomeTexture")
# → {"value": "", "type": "Object"}  ← silently empty!
```

**Expected:** Return an error like "Type mismatch: expected Texture2D, got StaticMesh" or at minimum `success: false`.

**Fix:** In the property setter, after resolving the object path, check that the loaded object is assignable to the property's expected UClass. Return an error if not.

**Location:** C++ property setter implementation.

**(Finding 8)**

---

## MEDIUM

### 5. `get_landscape_bounds` — Add Biome Volume Scale Factor

**Current output:**
```json
{
  "extent": [100800.0, 100800.0, 7544.9],
  "center": [0.0, 0.0, 7444.9],
  ...
}
```

**Requested addition:** Return a pre-computed `biome_volume_scale` field so agents can pass it directly to `set_transform` without doing division. OSS models cannot be trusted to do math reliably.

```json
{
  "extent": [100800.0, 100800.0, 7544.9],
  "center": [0.0, 0.0, 7444.9],
  "biome_volume_scale": [1008.0, 1008.0, 175.45],
  ...
}
```

**Formula:** `scale = [extent_x / 100, extent_y / 100, (extent_z + 10000) / 100]`

The Z headroom (+10000) ensures the volume extends above the highest landscape point. The divisor (100) is the default BoxExtent of both BiomeCore and BiomeTexture BPs.

**Location:** Wherever `get_landscape_bounds` response is built.

**(Finding 16 recommendation)**

---

### 6. `list_classes` — Cannot Find Plugin Blueprint Classes

**Symptom:** `list_classes(name_pattern="BP_PCGBiomeCore")` returns 0 results, but `spawn_actor(class_name="BP_PCGBiomeCore")` succeeds (when BP is loaded).

**Impact:** The documented type discovery workflow fails for plugin BPs. Agents conclude the class doesn't exist.

**Possible fix:** Ensure `list_classes` searches loaded Blueprint assets from plugins, not just native C++ classes.

**(Finding 1)**

---

### 7. `set_property` on Struct Arrays — Array-Level Set Fails

**Symptom:** Setting a struct array property at the array level with JSON objects fails, but element-level access works.

```python
# FAILS:
set_property(path="BiomeAssets", value=[{"Generator": "/Engine/...", "Enabled": true}])

# WORKS:
set_property(path="BiomeAssets[0].Mesh", value="/Engine/BasicShapes/Cube.Cube")
```

**Impact:** Agents cannot add new entries to struct arrays or replace the array wholesale. They can only modify existing entries element by element.

**Possible fix:** Support JSON object arrays as input for struct array properties, deserializing each object into the appropriate UStruct.

**(Finding 22)**

---

## LOW

### 8. `set_property` on Object Ref Arrays — Requires JSON Array Format

Plain string fails for array-type object reference properties; JSON array format `["/path"]` is required. This is arguably correct behavior, but should be documented clearly and error messages should hint at the correct format.

**(Finding 21)**
