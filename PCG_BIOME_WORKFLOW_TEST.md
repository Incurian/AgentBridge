# PCG Biome Workflow Test Log

**Date:** 2026-02-12
**Goal:** Follow AGENTS.md PCG Biome Workflow instructions exactly, record all issues.
**Biomes:** Red, Green (2 biomes to keep it manageable)
**Meshes:** Engine basic shapes (Cube for Red, Sphere for Green)
**Texture:** `/Engine/EngineResources/DefaultTexture.DefaultTexture`

---

## Summary of Findings

| # | Severity | Finding | Docs Section to Update |
|---|----------|---------|----------------------|
| 1 | **Medium** | `list_classes(name_pattern=...)` cannot find BP classes from plugins | Type Discovery |
| 2 | Info | BiomeCore component names match docs | _(none)_ |
| 3 | Info | `include_properties=true` returns 150+ props of noise | _(tip in Actor Operations)_ |
| 4 | **Medium** | Color write/read format asymmetry (FLinearColor→FColor, string→JSON, uppercase→lowercase) | Property Access, Value Format Cheat Sheet |
| 5 | Info | Struct array with object refs persisted in single call | _(none — docs already cover this)_ |
| 6 | Info | `Enabled` field reads as `"(complex)"` in array readback but is actually `true` | Property Access |
| 7 | **HIGH** | BP_PCGBiomeTexture spawns with actor scale [512,512,128] — docs Phase 7 only resets component scale, NOT actor scale. Component world scale inherits actor scale, so volume is still 512x too large. | PCG Biome Workflow Phase 7, Volume & Bounds |
| 8 | **HIGH** | `set_property` on `BiomeTexture` silently fails for type-mismatched textures (non-Texture2D). Reports `success: true` but value is empty on readback. Same silent failure pattern as Rule 1 but for object references, not structs. | Critical Rules (new rule?), Troubleshooting |
| 9 | Info | BiomeTexture actor has `PCG_LocalBiomeCore` component (PCGComponent) — not documented in Schema Reference | PCG Biome Workflow Schema |
| 10 | **Medium** | Vector/struct write/read format asymmetry — set with `"(X=100800,...)"` uppercase, read back as `{"x": 100800.0,...}` lowercase JSON. Same pattern as Finding 4. | Property Access |
| 11 | Info | `get_property` on struct container `BiomeDefinition` returns `{}` empty — docs already warn about this | _(already documented)_ |
| 12 | **HIGH** | `call_function` tool completely broken — `'AgentBridgeClient' object has no attribute 'host'` | Function Calls section |
| 13 | **HIGH** | `set_property` on BoxExtent stores value but never triggers visual update | Volume & Bounds, Critical Rules |
| 14 | **HIGH** | BiomeCore Volume has Blueprint-defined scale [512,512,128] on component; BiomeTexture has it on actor | Phase 7 |
| 15 | ~~obsolete~~ | Replaced by Finding 16 | |
| 16 | **HIGH+SOLUTION** | Use `set_transform` on component scale (`world_space=true`) instead of `set_property` on BoxExtent. Scale = desired_extent / 100. | Phase 7 rewrite |
| 17 | **Medium** | Short class names only work if BP is already loaded in memory. Fresh level requires full path. | Phase 6 |
| 18 | Info | Default scale lives in different places (Core: component, Texture: actor) but same world result | Schema Reference |
| 19 | **CRITICAL** | `save_asset` on duplicated biome definition crashes the editor. Error: "Asset cannot be saved as it has only been partially loaded". Known UE5 bug — `duplicate_asset` uses async loading, duplicate inherits partial-load state. Fix: AgentBridge needs `FlushAsyncLoading()` or `LoadSynchronous()` before duplicating. Workaround: skip `save_asset`, use in-memory assets. | Phase 4, AgentBridge C++ fix needed |
| 20 | **CRITICAL+WORKAROUND** | `save_asset` crash is SOURCE-PATH dependent. Duplicating from `/Game/` works fine; only plugin content (`/PCGBiomeCore/...`) crashes. Workaround: pre-copy templates to `/Game/` first. | Phase 4, Phase 0 (new) |
| 12 | **HIGH** | `call_function` tool is completely broken — `'AgentBridgeClient' object has no attribute 'host'` on every call | Function Calls section (mark as broken or fix) |
| 13 | **HIGH** | `set_property` on `BoxExtent` stores value but doesn't trigger visual update (no `UpdateBounds()`/`MarkRenderStateDirty()`) | Volume & Bounds, Critical Rules |
| 14 | **HIGH** | BiomeCore Volume has immutable Blueprint-defined world scale [512,512,128] — cannot be overridden via `set_property` or `set_transform` | PCG Biome Workflow Phase 7 |
| 15 | **Medium** | After volume issues, no MCP tool could force a visual refresh — editor restart required | Troubleshooting |

---

## Pre-flight

- World: `FreshMap_1` (Editor), 151 actors
- Landscape exists: center [0, 0, 7444.9], extent [100800, 100800, 7544.9]
- No existing biome actors (clean slate)
- PCGBiomeCore plugin NOT in `Plugins/` dir (only AgentBridge, Greeter, Tempo) — must be a sub-plugin of Tempo or an engine plugin

---

## Finding 1: `list_classes` cannot find BP_PCGBiome classes

`list_classes(name_pattern="BP_PCGBiomeCore")` → 0 results
`list_classes(name_pattern="Biome")` → 0 results
`list_classes(name_pattern="PCGBiome")` → 0 results

**But `spawn_actor(class_name="BP_PCGBiomeCore")` succeeds!** The short name works for spawning.

**Impact:** The AGENTS.md "Type Discovery" section says to use `list_classes(name_pattern=...)` to find classes. This completely fails for BP classes from plugins. An agent following the discovery workflow would conclude the classes don't exist when they do.

**Possible cause:** The docs note `name_pattern` is "case-insensitive exact match only" — but even exact `BP_PCGBiomeCore` returns nothing. This seems like a tool limitation for Blueprint classes from plugins.

**Suggestion for docs:** Add a warning that `list_classes(name_pattern=...)` may not find Blueprint classes from plugins. Recommend trying `spawn_actor` directly with the short name if docs give you the class name, rather than discovering it first.

## Finding 2: BiomeCore component names match docs

`get_actor(include_components=true)` confirmed:
- `Volume` (BoxComponent) ✓ — matches docs
- `BiomeCore` (PCGComponent) ✓ — matches docs
- `DefaultSceneRoot` (SceneComponent) — not mentioned in docs but expected
- `BillboardComponent_1` (BillboardComponent) — not mentioned, editor-only

The component names in the Schema Reference section are accurate.

## Finding 3: `include_properties=true` returns massive output

The full properties dump for a single BiomeCore actor is enormous (~150+ properties), mostly inherited UE boilerplate (replication, net, ticking). This is fine for discovery but the docs should note you'll need to wade through a lot of noise. The actually useful properties (`BiomeCore`, `Volume`, `Tags`) are buried in the output.

## Finding 4: Color write/read format asymmetry

**Write:** `set_property(..., path="BiomeDefinition.BiomeColor", value="(R=1,G=0,B=0,A=1)")` — FLinearColor, 0-1 range, Unreal string format

**Read back:** `get_property(...)` returns `{"r": 255, "g": 0, "b": 0, "a": 255}` — FColor, 0-255 range, JSON object format

The value is correct (1.0 maps to 255), but:
1. The **type** changed: set as FLinearColor (0-1), read back as FColor (0-255)
2. The **format** changed: set as Unreal string `"(R=1,...)"`, read back as JSON object `{"r": 255,...}`
3. The **key case** changed: set with uppercase `R,G,B,A`, read back with lowercase `r,g,b,a`

**Impact:** An agent doing read-modify-write on a color would need to know about this conversion. If it naively reads the JSON `{"r": 255,...}` and tries to set it back as-is, Rule 1 says JSON objects "silently fail." The docs should note this asymmetry in the Property Access section, or at minimum in the BiomeDefinition schema where BiomeColor is documented.

## Finding 5: Struct arrays with embedded object refs persist in single call

Setting `BiomeAssets` as a JSON array string with both simple fields AND object references (Generator, Mesh) worked in a single `set_property` call. The docs mention this should generally work but offer a two-step fallback. In our test, the single-call approach was sufficient.

Verified with `get_property`:
- `BiomeAssets[0].Mesh` → `/Engine/BasicShapes/Cube.Cube` ✓
- `BiomeAssets[0].Generator` → `/PCGBiomeCore/BiomeGenerators/DefaultGenerator.DefaultGenerator` ✓
- `BiomeAssets[0].Enabled` → `true` ✓

## Finding 6: Bool fields show as `"(complex)"` in array readback

When reading back the full `BiomeAssets` array, `Enabled` displays as `"(complex)"` instead of its actual value. Reading the specific indexed path `BiomeAssets[0].Enabled` returns `true` correctly. Similarly, sub-structs like `DebugOptions`, `FilterOptions`, etc. all show `"(complex)"`.

**Impact:** An agent reading back an array to verify its contents would see `"(complex)"` for bools and nested structs. This could be confusing — the agent might think the value wasn't set. The docs should mention that array-level readback summarizes complex/nested fields as `"(complex)"`.

## Finding 7 (HIGH): Phase 7 scale reset is incomplete — actor scale not addressed

**Problem:** BP_PCGBiomeTexture spawns with **actor-level scale [512, 512, 128]**. The docs Phase 7 says to reset `BiomeTextureVolume.RelativeScale3D` to (1,1,1), which sets the component's LOCAL scale to 1. But the component inherits the actor's scale through the scene hierarchy.

**Evidence:**
```
After setting BiomeTextureVolume.RelativeScale3D = (1,1,1):
  get_transform(target="RedBiomeTexture->BiomeTextureVolume", world_space=false)
    → scale: {x: 1.0, y: 1.0, z: 1.0}  (component local: correct)
  get_transform(target="RedBiomeTexture->BiomeTextureVolume", world_space=true)
    → scale: {x: 512.0, y: 512.0, z: 128.0}  (world: STILL 512!)
```

The effective volume size is BoxExtent * WorldScale = 100800 * 512 = **51,609,600 units** — way larger than intended.

**Fix:** Phase 7 must ALSO reset the actor's transform scale:
```python
set_transform(target="<Name>BiomeTexture", scale=[1, 1, 1])
```

**Or alternatively**, the component scale reset should use `set_transform` with `world_space=true` to override inherited scale. The current `set_property` approach only affects the local relative value.

**Note:** BiomeCore actor spawns with scale [1,1,1] by default — no issue there. Only BiomeTexture has this problem.

## Finding 8 (HIGH): Silent failure on BiomeTexture property — type-mismatched object references

**Problem:** Setting `BiomeTexture` (expects `UTexture2D*`) with a non-Texture2D asset silently fails.

```python
# Reports success but does NOT persist:
set_property(actor_id="RedBiomeTexture", path="BiomeTexture",
    value="/Game/Plumage/T_Fire_Tiled_D.T_Fire_Tiled_D")  → success: true
get_property(..., path="BiomeTexture")  → ""  (EMPTY)

# Actually works (engine Texture2D):
set_property(actor_id="RedBiomeTexture", path="BiomeTexture",
    value="/Engine/EngineResources/DefaultTexture.DefaultTexture")  → success: true
get_property(..., path="BiomeTexture")  → "/Engine/EngineResources/DefaultTexture.DefaultTexture"  ✓
```

This is the same pattern as Rule 1 (struct JSON silently fails) but for **object reference type mismatches**. The property expects `UTexture2D*`; if you provide an asset that isn't a Texture2D subclass, it reports success but the value doesn't persist.

**Suggestion for docs:**
- Add a new Critical Rule or extend Rule 1: "Object reference type mismatches also silently fail. If `set_property` succeeds but `get_property` returns empty, the referenced asset may be the wrong type."
- Add to Troubleshooting: "set_property succeeds but object reference is empty on readback → asset type mismatch (e.g., not a Texture2D)"

## Finding 9: Undocumented component on BP_PCGBiomeTexture

`get_actor(actor_id="RedBiomeTexture", include_components=true)` shows:
- `DefaultSceneRoot` (SceneComponent)
- `BillboardComponent_1` (BillboardComponent)
- **`PCG_LocalBiomeCore` (PCGComponent)** — NOT in docs
- `BiomeTextureVolume` (BoxComponent) ✓

The Schema Reference doesn't mention `PCG_LocalBiomeCore`. Minor, but worth adding for completeness.

## Finding 10: Vector write/read format asymmetry

Same pattern as Finding 4 but for vectors:

**Write:** `set_property(..., path="Volume.BoxExtent", value="(X=100800,Y=100800,Z=17545)")`
**Read:** `get_property(...)` returns `{"x": 100800.0, "y": 100800.0, "z": 17545.0}`

Uppercase keys in write, lowercase in read. Unreal string format for write, JSON object for read. This is consistent across all struct types tested (FVector, FLinearColor, FVector/Scale).

**Docs suggestion:** Add a note in the Property Access section: "Reading properties back returns JSON objects with lowercase keys, even though setting them requires Unreal string format with uppercase keys. Do NOT use the read format for writing — it will silently fail per Rule 1."

---

## Workflow Execution

### Phase 1: Duplicate Templates ✅

All 4 `duplicate_asset` calls succeeded on first try. Template paths from the docs worked exactly:
- `/PCGBiomeCore/BiomeDefinitions/DefaultBiome` → `/Game/PCGBiomes/Definitions/RedBiome`
- `/PCGBiomeCore/BiomeDefinitions/DefaultBiome` → `/Game/PCGBiomes/Definitions/GreenBiome`
- `/PCGBiomeCore/BiomeAssets/DefaultAsset` → `/Game/PCGBiomes/Assets/RedAssets`
- `/PCGBiomeCore/BiomeAssets/DefaultAsset` → `/Game/PCGBiomes/Assets/GreenAssets`

No issues. Docs are accurate.

### Phase 2: Configure Definitions ✅

All 6 `set_property` calls reported success. Verified with `get_property` — values persisted correctly.
See Finding 4 for color format asymmetry.

### Phase 3: Configure Assets ✅

Set BiomeAssets arrays with single-call JSON approach. Both persisted correctly including object references (Generator, Mesh). See Findings 5 and 6.

### Phase 4: Save All Data Assets ✅

All 4 `save_asset` calls succeeded. No issues.

### Phase 5: Get Landscape Bounds ✅

Done in pre-flight. center=[0,0,7444.9], extent=[100800,100800,7544.9].

### Phase 6: Spawn Level Actors ✅

- BiomeCore spawned with short name `BP_PCGBiomeCore` ✓
- Both BiomeTexture actors spawned with full path `/PCGBiomeCore/Blueprints/BP_PCGBiomeTexture.BP_PCGBiomeTexture_C` ✓
- `folder_path="PCGBiomes"` worked ✓
- **Note:** BiomeTexture default actor scale is [512, 512, 128] — see Finding 7

### Phase 7: Fix Volume Bounds ⚠️ (INCOMPLETE — see Finding 7)

Following the docs exactly:
1. Set `BiomeTextureVolume.RelativeScale3D` to (1,1,1) — sets component LOCAL scale only
2. Set `BiomeTextureVolume.BoxExtent` to landscape extent + headroom

**The component local scale resets correctly, but the actor's root scale [512,512,128] is inherited, so world-space volume is still 512x too large.**

**Fix applied during testing:** Also ran `set_transform(target="<Name>BiomeTexture", scale=[1,1,1])` to reset actor scale. This should be added to the Phase 7 instructions.

### Phase 8: Assign References ⚠️ (with silent texture failure — see Finding 8)

- `Definition` references set and verified ✓
- `Assets` array references set and verified ✓
- `BiomeTexture` initially failed silently (type mismatch with non-Texture2D asset)
- Fixed by using known engine Texture2D `/Engine/EngineResources/DefaultTexture.DefaultTexture` ✓

### Phase 9: Verify ✅

```
query_actors(label_pattern="Biome") → 3 actors (BiomeCore, RedBiomeTexture, GreenBiomeTexture)
All at correct location [0, 0, 7444.9]
All at scale [1, 1, 1] (after fix)
Both biomes Enabled=true
Definition and Assets references verified on both biomes
BiomeTexture references verified on both biomes
```

---

## Tool Call Count

| Phase | Expected (docs) | Actual | Notes |
|-------|-----------------|--------|-------|
| Phase 1: Duplicate | 4 | 4 | Exact match |
| Phase 2: Configure defs | 6 | 6 + 4 verify | Verification adds calls |
| Phase 3: Configure assets | 2 | 2 + 2 verify | |
| Phase 4: Save | 4 | 4 | Exact match |
| Phase 5: Bounds | 1 | 0 (done in preflight) | |
| Phase 6: Spawn | 3 | 3 | Exact match |
| Phase 7: Fix volumes | 6 | 6 + 3 verify + 2 actor scale fix | Docs incomplete, needed 2 extra set_transform |
| Phase 8: Assign refs | 6 | 6 + 2 retry + 2 fix | Texture type mismatch required retry |
| Phase 9: Verify | 3 | 7 | More thorough verification |
| **Total** | ~35 | ~57 | Verification and fixes add ~60% overhead |

---

## Finding 12 (HIGH): `call_function` tool is completely broken

Every `call_function` invocation returns:
```
{"error": "'AgentBridgeClient' object has no attribute 'host'"}
```

Tested with multiple targets:
- `BiomeCore.Volume.SetBoxExtent` with various parameter formats
- `BiomeCore.SetActorHiddenInGame` (simple bool param)

This is an internal MCP server bug, not a parameter/syntax issue. The tool cannot be used at all.

**Impact:** The Function Calls section of AGENTS.md documents `call_function` as a working tool, but it's non-functional. This blocks any workflow that needs to call Blueprint/C++ functions (e.g., `SetBoxExtent()` to trigger visual refresh).

## Finding 13 (HIGH): `set_property` on BoxExtent doesn't trigger visual update

After setting `Volume.BoxExtent` via `set_property`:
- `get_property` reads back the correct value
- But the visual wireframe box in the editor does NOT change
- Saving the level (`SaveCurrentLevel` console command) does NOT help
- The property value is stored but UE's component bounds are not recalculated

**Root cause theory:** In UE, `UBoxComponent::SetBoxExtent()` (the function) triggers `UpdateBounds()` and `MarkRenderStateDirty()`. Setting the `BoxExtent` UPROPERTY directly via reflection (which is what `set_property` does) bypasses these side effects. The component never knows its extent changed.

**This means `set_property` on BoxExtent is fundamentally broken for visual results.** The only fix would be `call_function` to call `SetBoxExtent()`, but that's also broken (Finding 12).

**Workaround:** Use `set_transform` with scale to control volume size instead of BoxExtent. Or delete and re-spawn actors (not tested yet).

## Finding 14 (HIGH): BiomeCore Volume has immutable Blueprint-defined world scale [512,512,128]

The Volume component on BP_PCGBiomeCore has a world scale of [512,512,128] that CANNOT be changed:
- `set_property(path="Volume.RelativeScale3D", value="(X=1,Y=1,Z=1)")` → property reads back as (1,1,1) but world scale still 512
- `set_transform(target="BiomeCore->Volume", scale=[1,1,1], world_space=true)` → world scale still 512
- Actor scale is (1,1,1), DefaultSceneRoot scale is (1,1,1) — the 512 comes from somewhere not exposed

This scale is baked into the Blueprint component template/CDO and is not overridable via available tools.

**Impact:** The docs' Phase 7 approach of "reset RelativeScale3D then set BoxExtent" fundamentally doesn't work for BiomeCore. The Volume's effective world scale will always be [512,512,128] regardless of what you set.

**Workaround:** Accept the 512 scale and calculate BoxExtent values accordingly:
```python
# To get a visual half-extent of 100800:
# BoxExtent = desired_extent / component_world_scale
# BoxExtent.X = 100800 / 512 = 196.875
# BoxExtent.Z = 17545 / 128 = 137.07
```

But this requires `set_property` on BoxExtent to actually work visually (see Finding 13).

## Finding 15 (OBSOLETE): Replaced by Finding 16

Earlier conclusion about editor restart was premature — the real issue was `set_property` on BoxExtent not triggering visual updates (Finding 13). See Finding 16 for the working solution.

## Finding 16 (HIGH — SOLUTION): Use `set_transform` on component scale instead of `set_property` on BoxExtent

**`set_property` on BoxExtent does NOT trigger visual updates.** The value is stored but UE never calls `UpdateBounds()` / `MarkRenderStateDirty()`. No combination of nudges, saves, or actor transforms refreshes it programmatically. A manual edit in the editor details panel does (it calls `PostEditChangeProperty()`).

**Working approach: control volume size entirely through component scale via `set_transform`.**

Both BP_PCGBiomeCore and BP_PCGBiomeTexture have default BoxExtent of [100, 100, 100]. Use that as the base and scale to desired world size:

```python
# Formula: scale = desired_world_half_extent / default_box_extent(100)
# For landscape coverage:
bounds = get_landscape_bounds()
extent_x, extent_y, extent_z = bounds["extent"]
z_with_headroom = extent_z + 10000

scale_x = extent_x / 100       # 100800 / 100 = 1008
scale_y = extent_y / 100       # 100800 / 100 = 1008
scale_z = z_with_headroom / 100 # 17545 / 100 = 175.45

# BiomeCore — scale is on the Volume COMPONENT (actor spawns at 1,1,1)
set_transform(target="BiomeCore->Volume", scale=[1008, 1008, 175.45], world_space=true)

# BiomeTexture — scale is on the ACTOR (component RelativeScale3D is 1,1,1)
# But same approach works — set component world scale directly
set_transform(target="<Name>BiomeTexture->BiomeTextureVolume",
    scale=[1008, 1008, 175.45], world_space=true)
```

**Key details:**
- Default BoxExtent is [100, 100, 100] for BOTH Blueprint types
- BiomeCore: the default 512 scale lives on the component's RelativeScale3D
- BiomeTexture: the default 512 scale lives on the actor (component is 1,1,1)
- Both end up with component world scale [512, 512, 128] by default
- `set_transform` with `world_space=true` on the component works for BOTH — it visually updates immediately
- Do NOT modify BoxExtent via `set_property` — it stores but never refreshes

**Tested and confirmed working on both BiomeCore and BiomeTexture actors.**

## Finding 17: Short class names only work if Blueprint is already loaded

On a fresh editor session (TestingMap):
- `spawn_actor(class_name="BP_PCGBiomeCore")` → **FAILS** with "Class not found: BP_PCGBiomeCore_C"
- `spawn_actor(class_name="/PCGBiomeCore/Blueprints/BP_PCGBiomeCore.BP_PCGBiomeCore_C")` → **WORKS**

On a session where the BP was previously loaded (FreshMap_1 had biome actors):
- Short name worked fine

**Impact:** The docs' Phase 6 shows short name `BP_PCGBiomeCore` for spawning. This only works if the Blueprint asset is already loaded in memory. On a fresh level, you MUST use the full path. The docs should always use full paths in the workflow to be reliable.

## Finding 18: Default scale architecture differs between BiomeCore and BiomeTexture

| Property | BiomeCore | BiomeTexture |
|----------|-----------|--------------|
| Actor scale | [1, 1, 1] | [512, 512, 128] |
| Component RelativeScale3D | [512, 512, 128] | [1, 1, 1] |
| Component world scale | [512, 512, 128] | [512, 512, 128] |
| Default BoxExtent | [100, 100, 100] | [100, 100, 100] |
| Default visual size | 51,200 per axis | 51,200 per axis |

Same end result but the 512 lives in different places. The `set_transform` component world scale approach works uniformly for both, which is why it's the recommended approach.

---

## Recommendations for AGENTS.md Updates

### HIGH Priority (Workflow-Breaking)

1. **Phase 7 — REWRITE: Use `set_transform` on component scale, not `set_property` on BoxExtent.**
   `set_property` on BoxExtent stores the value but never triggers a visual update. The working approach:
   ```python
   bounds = get_landscape_bounds()
   sx = bounds["extent"][0] / 100  # default BoxExtent is 100
   sy = bounds["extent"][1] / 100
   sz = (bounds["extent"][2] + 10000) / 100

   # BiomeCore
   set_transform(target="BiomeCore->Volume", scale=[sx, sy, sz], world_space=true)

   # Each BiomeTexture
   set_transform(target="<Name>BiomeTexture->BiomeTextureVolume",
       scale=[sx, sy, sz], world_space=true)
   ```

2. **Rule 2 — REWRITE:** Current rule says "reset scale to 1, then set BoxExtent." This doesn't work. Replace with: "Use `set_transform` on the volume component with `world_space=true` to control volume size. Do NOT use `set_property` on BoxExtent — it stores the value but doesn't visually update."

3. **Phase 6 — Use full class paths:** Short names only work if the BP is already loaded. Always use full paths:
   - `/PCGBiomeCore/Blueprints/BP_PCGBiomeCore.BP_PCGBiomeCore_C`
   - `/PCGBiomeCore/Blueprints/BP_PCGBiomeTexture.BP_PCGBiomeTexture_C`

4. **New Critical Rule:** Object reference type mismatches silently fail. `set_property` reports success but the value doesn't persist if the asset is the wrong UClass.

5. **`call_function` — mark as broken or fix.** Currently non-functional: `'AgentBridgeClient' object has no attribute 'host'`.

### MEDIUM Priority

6. **Property Access — read/write asymmetry note:** All struct types read back as lowercase JSON objects, but MUST be written as uppercase Unreal string format.

7. **Type Discovery — plugin BP warning:** `list_classes(name_pattern=...)` may not find Blueprint classes from plugins.

8. **`get_landscape_bounds` — MUST return biome volume scaling factor.** Add a `biome_volume_scale` field (extent/100 with Z headroom) to the response so agents can pass it directly to `set_transform` without doing math. OSS models cannot be trusted to do division reliably. Example response addition:
   ```json
   "biome_volume_scale": [1008.0, 1008.0, 175.45]
   ```

### LOW Priority

9. **Phase 9 — recommend verification of BiomeTexture property:** Add `BiomeTexture` (texture ref) to verification checklist.

10. **Array readback — "(complex)" note:** Mention that array-level readback shows `"(complex)"` for bools and nested structs.

11. **Schema — add PCG_LocalBiomeCore component:** BP_PCGBiomeTexture has an undocumented PCGComponent.

---

## Finding 19 Update: save_asset crash is SOURCE-PATH DEPENDENT

### Finding 20: save_asset works when source is in /Game/, crashes only for plugin content

User manually copied `DefaultBiomeDefinition` from plugin content into `/Game/FreshTest/DefaultBiome.DefaultBiome`.

```
duplicate_asset(source_path="/Game/FreshTest/DefaultBiome.DefaultBiome",
    dest_package_path="/Game/FreshTest", dest_asset_name="TestDupe") → SUCCESS
save_asset(asset_path="/Game/FreshTest/TestDupe.TestDupe") → SUCCESS (no crash!)
```

**Root cause confirmed:** The crash is specific to duplicating from plugin content paths (`/PCGBiomeCore/...`). Plugin assets use async/partial loading. When `duplicate_asset` copies them, the duplicate inherits the partial-load state, and `save_asset` crashes when it tries to serialize partially-loaded data.

**Assets already in `/Game/` are fully loaded**, so duplicates are also fully loaded and save fine.

**Workaround for AGENTS.md workflow:** Step 0 — manually copy template assets from plugin to `/Game/` folder first, THEN use `duplicate_asset` + `save_asset` from the `/Game/` copies. Or: AgentBridge C++ fix to call `FlushAsyncLoading()` before `DuplicateAsset()`.

**Updated recommendation:** Add to Recommendations section — workflow should document that template assets must be pre-copied to `/Game/` before programmatic duplication.
