# AGENTS.md — Required Changes

Changes identified during end-to-end PCG Biome Workflow testing (2026-02-12).
See `PCG_BIOME_WORKFLOW_TEST.md` for full test log and reproduction steps.

---

## CRITICAL — Workflow-Breaking

### 1. Rewrite Phase 7 (Volume Sizing)

**Current docs:** Reset component `RelativeScale3D` to (1,1,1), then set `BoxExtent` via `set_property`.

**Problem:** `set_property` on `BoxExtent` stores the value in UE reflection but does NOT trigger `UpdateBounds()` / `MarkRenderStateDirty()`. The visual wireframe never changes. No combination of nudges, saves, or transforms refreshes it. `call_function` to call `SetBoxExtent()` (which would trigger the update) is also broken (see AgentBridge bugs). **(Findings 13, 16)**

**Replace with:**
```python
# Formula: scale = desired_world_half_extent / default_box_extent(100)
bounds = get_landscape_bounds()
sx = bounds["extent"][0] / 100   # 100800 / 100 = 1008
sy = bounds["extent"][1] / 100   # 100800 / 100 = 1008
sz = (bounds["extent"][2] + 10000) / 100  # Z with headroom

# BiomeCore — scale lives on component
set_transform(target="BiomeCore->Volume", scale=[sx, sy, sz], world_space=true)

# Each BiomeTexture — scale lives on actor, but component world_space works uniformly
set_transform(target="<Name>BiomeTexture->BiomeTextureVolume",
    scale=[sx, sy, sz], world_space=true)
```

**Key details to document:**
- Default BoxExtent is [100, 100, 100] for both BP types
- Do NOT modify BoxExtent via `set_property` — it stores but never visually updates
- Do NOT try to reset RelativeScale3D — it doesn't affect the result
- `set_transform` with `world_space=true` on the component is the only reliable approach

### 2. Rewrite Critical Rule 2 (Volume Sizing Rule)

**Current:** "Reset scale to 1, then set BoxExtent."

**Replace with:** "Use `set_transform` on the volume component with `world_space=true` to control volume size. Scale = desired_half_extent / 100 (default BoxExtent). Do NOT use `set_property` on BoxExtent — it stores the value but doesn't visually update."

### 3. Phase 6 — Always Use Full Class Paths

**Current docs:** Show short names like `BP_PCGBiomeCore`.

**Problem:** Short names only work if the Blueprint is already loaded in memory. On a fresh level, they fail with "Class not found". **(Finding 17)**

**Replace with full paths:**
```
/PCGBiomeCore/Blueprints/BP_PCGBiomeCore.BP_PCGBiomeCore_C
/PCGBiomeCore/Blueprints/BP_PCGBiomeTexture.BP_PCGBiomeTexture_C
```

Or if using `/Game/` copies (recommended for save_asset compatibility):
```
/Game/<YourFolder>/BP_PCGBiomeCore.BP_PCGBiomeCore_C
/Game/<YourFolder>/BP_PCGBiomeTexture.BP_PCGBiomeTexture_C
```

### 4. Add Phase 0 — Pre-copy Template Assets to /Game/

**Problem:** `duplicate_asset` from plugin content paths (`/PCGBiomeCore/...`) creates partially-loaded duplicates. `save_asset` on these crashes the editor with "Asset cannot be saved as it has only been partially loaded." Duplicating from `/Game/` paths works perfectly. **(Findings 19, 20)**

**Add new phase before Phase 1:**

> **Phase 0: Prepare Template Assets**
>
> Copy the following template assets from the plugin to your project's `/Game/` folder:
> - `BP_PCGBiomeCore`
> - `BP_PCGBiomeTexture`
> - `DefaultBiomeDefinition` (or equivalent definition DataAsset)
> - `DefaultBiomeAssets` (or equivalent assets DataAsset)
>
> This is required because `duplicate_asset` + `save_asset` only works reliably on assets in `/Game/`, not plugin content.

### 5. New Critical Rule — Silent Type Mismatch on Object References

**Problem:** `set_property` on object reference properties reports `success: true` even when the asset is the wrong UClass. The value silently doesn't persist — readback shows empty. **(Finding 8)**

**Add rule:** "Always verify object reference properties after setting them with `get_property`. Type mismatches (e.g., setting a non-Texture2D on a Texture2D property) silently fail — `set_property` reports success but the value is empty on readback."

---

## HIGH — Important Corrections

### 6. Document Property Read/Write Format Asymmetry

**Problem:** All struct types must be WRITTEN in Unreal string format `"(X=100,Y=200,Z=300)"` with uppercase field names, but READ BACK as lowercase JSON objects `{"x": 100.0, "y": 200.0, "z": 300.0}`. Same for colors: write `"(R=1.0,G=0.0,B=0.0,A=1.0)"`, read `{"r": 1.0, "g": 0.0, "b": 0.0, "a": 1.0}`. **(Findings 4, 10)**

**Add to Property Access section and Value Format Cheat Sheet.**

### 7. Document Array Property Setting Rules

Two distinct behaviors discovered **(Findings 21, 22)**:

**Object ref arrays** (e.g., `Assets` on BiomeTexture actor):
- Plain string fails: `set_property(path="Assets", value="/Game/Path")` → INVALID_ARGUMENT
- JSON array works: `set_property(path="Assets", value=["/Game/Path"])` → success

**Struct arrays** (e.g., `BiomeAssets` on DataAsset):
- Array-level set fails entirely with JSON objects
- Element-level works: `set_property(path="BiomeAssets[0].Mesh", value="/Engine/BasicShapes/Cube.Cube")` → success

**Add to Property Access section with examples.**

### 8. Mark `call_function` as Broken (or Remove)

Every invocation returns `'AgentBridgeClient' object has no attribute 'host'`. The tool is completely non-functional. **(Finding 12)**

Either fix in AgentBridge (see bug list) or add a prominent warning in the Function Calls section.

---

## MEDIUM — Helpful Additions

### 9. Type Discovery — Plugin BP Warning

`list_classes(name_pattern=...)` cannot find Blueprint classes from plugins. Returns 0 results for known-valid classes like `BP_PCGBiomeCore`. **(Finding 1)**

**Add note:** "Plugin Blueprint classes may not appear in `list_classes` results. Use full asset paths for `spawn_actor` if discovery fails."

### 10. Schema Reference — Document Scale Architecture

BiomeCore and BiomeTexture have different default scale locations but same world result **(Finding 18)**:

| Property | BiomeCore | BiomeTexture |
|----------|-----------|--------------|
| Actor scale | [1, 1, 1] | [512, 512, 128] |
| Component RelativeScale3D | [512, 512, 128] | [1, 1, 1] |
| Component world scale | [512, 512, 128] | [512, 512, 128] |

Document this so agents understand why `set_transform` with `world_space=true` is the uniform approach.

### 11. Schema Reference — Add PCG_LocalBiomeCore Component

BP_PCGBiomeTexture has an undocumented `PCG_LocalBiomeCore` (PCGComponent). Add to schema. **(Finding 9)**

### 12. Add Troubleshooting — Zero Spawn Points

**Add:** "If zero points spawn after completing the workflow, verify your BiomeTexture has regions whose colors match your biome colors within the configured color tolerance. Adjust tolerance if needed." **(Finding 24)**

---

## LOW — Polish

### 13. Array Readback "(complex)" Note

Array-level readback shows `"(complex)"` for bools and nested structs. The actual values are correct — this is just a display limitation. **(Finding 6)**

### 14. Phase 9 — Verify BiomeTexture Property

Add `BiomeTexture` (texture ref) to the verification checklist alongside Definition and Assets. **(Finding 8)**

### 15. `include_properties=true` Noise Warning

Consider noting that `get_actor(include_properties=true)` returns 150+ properties, most of which are UE engine defaults. Recommend using targeted `get_property` calls instead. **(Finding 3)**
