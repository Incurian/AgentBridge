# AGENTS.md Corrections & Improvements

Discovered during PCG biome workflow validation (2026-02-13).
Branch: `feature/agentbridge-bugs`

---

## Bug Fix Updates (stale warnings/info)

### 1. call_function WARNING is stale (lines 1272-1274)
- **Current:** "WARNING: `call_function` is currently broken... 'AgentBridgeClient' object has no attribute 'host'"
- **Fix:** Bug 1 was fixed. Remove the WARNING block. Replace with note about current limitation: only supports zero-arg void functions.

### 2. list_classes plugin BP warning is stale (lines 1321-1323)
- **Current:** "`list_classes(name_pattern=...)` cannot find Blueprint classes from plugins"
- **Fix:** Bug 6 fixed this. `list_classes` now discovers plugin BPs and acts as a class loader for `spawn_actor`. Update to reflect current behavior.

### 3. Rule 6 "silently fails" is partially stale (lines 219-230)
- **Current:** "set_property on object reference properties reports success: true even when the asset is the wrong UClass. The value silently doesn't persist"
- **Fix:** Bug 4 now properly rejects type-mismatched object refs with an error. Update Rule 6 to note that type validation now works, but still recommend verification as good practice.

### 4. Troubleshooting: call_function (lines 1450-1452)
- **Current:** "No fix yet. Use set_property, set_transform, or console commands instead."
- **Fix:** Bug 1 fixed the AttributeError. Update to note call_function works but only for zero-arg void functions.

### 5. Troubleshooting: save_asset crashes (lines 1459-1461)
- **Current:** Mentions crashes on plugin assets
- **Note:** Still accurate. Keep as-is but could add: "This is an engine limitation, not an AgentBridge bug."

---

## New Features to Document

### 6. biome_volume_scale field in get_landscape_bounds
- **Where:** Volume & Bounds section (line 532+), PCG workflow Phase 5/7
- **What:** `get_landscape_bounds()` now returns `biome_volume_scale: [x, y, z]` - pre-computed scale factors for sizing a 100-unit-extent BoxComponent to cover the landscape.
- **Impact:** Phase 7 (Set Volume Bounds) can be simplified:
  ```python
  bounds = get_landscape_bounds()
  scale = bounds["biome_volume_scale"]  # e.g. [2040, 2040, 101]
  set_transform(target="BiomeCore->Volume", scale=scale, world_space=true)
  ```
  No manual calculation needed. The Z headroom (+10000) is already baked in.

---

## Workflow Validation Notes

(Updated as testing progresses)

### Phase 0: Prepare Template Assets
- SKIPPED (test map has templates pre-copied to `/Game/AB_PCG_TESTING/BiomeTemplates/`)
- Phase 0 instructions are correct in principle (copy from plugin to /Game/ first)

### Phase 1: Duplicate Templates
- PASS: `duplicate_asset` from `/Game/` templates to `/Game/.../Generated/` worked for both Definition and Asset

### Phase 2: Configure Definitions
- PASS: BiomeName, BiomeColor, BiomePriority all set and verified via readback
- **NOTE:** Color write/read range mismatch not documented. We write `(R=1,G=0,B=0,A=1)` (LinearColor 0-1 range) but readback returns `{r: 255, g: 0, b: 0, a: 255}` (FColor 0-255 range). The Read/Write Format Asymmetry table (line 236) documents lowercase keys but NOT the value range conversion. Add a note about this.

### Phase 3: Configure Assets
- PASS: JSON array with embedded object ref `[{"Enabled": true, "Weight": 1, "Mesh": "/Engine/BasicShapes/Cube.Cube"}]` worked in single call
- Verified: Mesh ref and Enabled both read back correctly via element-level access
- **NOTE:** AGENTS.md lines 308-317 suggest element-level access "for reliability" but single-call JSON array worked fine here. Could soften the language to "recommended for complex struct arrays" rather than implying it's required.

### Phase 4: Save All Data Assets
- PASS: Both TestBiome and TestAssets saved successfully

### Phase 5: Get Landscape Bounds
- PASS: Already tested. Returns biome_volume_scale now (not in docs yet)

### Phase 6: Spawn Level Actors
- PASS: Both BiomeCore and RedBiomeTexture spawned with full `/Game/` class paths
- Confirmed Default Scale Architecture (line 657): Core=[1,1,1], Texture=[512,512,128]
- folder_path="PCGBiomes" worked for outliner organization

### Phase 7: Set Volume Bounds
- PASS: `set_transform` with `world_space=true` on both Volume and BiomeTextureVolume
- Used `biome_volume_scale` directly from `get_landscape_bounds` - no manual calculation needed
- **IMPROVEMENT:** Phase 7 instructions (line 881-899) show manual formula. Should add biome_volume_scale shortcut.

### Phase 8: Assign References
- PASS: BiomeTexture, Definition, and Assets all set on RedBiomeTexture

### Phase 9: Verify
- PASS: All references read back correctly. Both actors found via query_actors.
- Total tool calls for single-biome setup: ~19 (vs estimated ~67 for 4 biomes)

---

## Additional Corrections Found During Testing

### 7. Color readback range conversion (line 236-245)
- Write `(R=1,G=0,B=0,A=1)` (FLinearColor 0-1) reads back as `{r: 255, g: 0, b: 0, a: 255}` (FColor 0-255)
- The Read/Write Format Asymmetry table documents lowercase keys but NOT the value range conversion
- Add a note: "BiomeColor writes in 0-1 range (FLinearColor) but reads back in 0-255 range (FColor)"

### 8. Troubleshooting: BiomeColorTolerance debugging ladder (line 1463-1467)
- Current text just says "Verify colors match within BiomeColorTolerance (default: 0.01)"
- **Improvement:** Add a debugging ladder to Phase 9 and troubleshooting:
  1. Try `BiomeColorTolerance = 0.1` (catches minor color imprecision)
  2. Try `BiomeColorTolerance = 0.5` (catches major mismatches)
  3. Try `BiomeColorTolerance = 0.99` (sanity check - if this doesn't work, it's not a color issue)
  - If 0.99 works but 0.01 doesn't, your texture colors don't exactly match BiomeColor values. Adjust tolerance or fix colors.
  - If 0.99 doesn't work either, the issue is elsewhere (missing refs, volume bounds, etc.)
- **Also:** Move this hint into Phase 9 instructions, not just buried in troubleshooting. After verification, add: "If no PCG generation occurs, adjust BiomeColorTolerance on the BiomeTexture actor (default 0.01 is very tight)."
- Tested: workflow worked at 0.1 tolerance with `(R=1,G=0,B=0,A=1)` color on the test texture.

### 9. Missing sub-struct field tables (lines 734-736)
- `FBiomeAsset_MeshOptions` - listed but NO field table
- `FBiomeAsset_AssetOptions` - listed but NO field table
- `FBiomeAsset_AssemblyOptions` - listed but NO field table
- `FBiomeAsset_DebugOptions` - listed but NO field table
- Only `FilterOptions` and `RuntimeOptions` have documented sub-fields
- **Impact:** An agent asked to change mesh scale, placement density, or debug settings would need to fall back to `get_class_schema` discovery - defeating the purpose of the schema reference
- **Fix:** Add field tables for all four missing option structs. Discovered via get_property:

#### FBiomeAsset_MeshOptions
| Field | Type | Default | Description |
|-------|------|---------|-------------|
| Material | UMaterialInterface* | "" | Override material |
| AllowCollision | bool | false | Enable collision on spawned meshes |
| Visible | bool | true | Mesh visibility |
| CastShadow | bool | true | Shadow casting |
| CastHiddenShadow | bool | false | Cast shadow even when hidden |
| AffectDistanceFieldLighting | bool | true | DFAO contribution |
| DetailMode | float | 0 | LOD detail mode |
| StartCullDistance | float | 0 | Begin distance culling |
| EndCullDistance | float | 0 | End distance culling |
| WorldPositionOffsetDisableDistance | float | 0 | WPO disable distance |
| IncludeInHLOD | bool | false | Include in HLOD generation |

#### FBiomeAsset_AssetOptions (KEY for placement transforms)
| Field | Type | Default | Description |
|-------|------|---------|-------------|
| OverlapWithChildren | bool | false | Allow overlap with child assets |
| ForceAssetScale | bool | false | Force scale override |
| ExtentsMultiplier | FVector | (1,1,1) | Bounds extents multiplier |
| BoundsOffset | FVector | (0,0,0) | Bounds offset |
| Translation | FVector | (0,0,0) | Position offset |
| Rotation | FRotator | (0,0,0) | Rotation offset |
| Scale | FVector | (1,1,1) | **Mesh scale** - THIS is what controls asset size |
| OrientUpward | float | 0 | Upward orientation strength |
| SelfPrune | bool | false | Enable self-pruning |
| SelfPrunIngExtentsMultiplier | float | 1.0 | Self-prune extents multiplier |

#### FBiomeAsset_AssemblyOptions
| Field | Type | Default |
|-------|------|---------|
| AllowCollision | bool | false |

#### FBiomeAsset_DebugOptions
| Field | Type | Default |
|-------|------|---------|
| Isolate | bool | false |
| ShowBounds | bool | false |

**Critical note for AGENTS.md:** Add a hint near the Weight/Mesh fields:
- "To change mesh scale, use `AssetOptions.Scale` (not `RuntimeOptions.ScaleMultiplier`)"
- "To offset mesh placement, use `AssetOptions.Translation` and `AssetOptions.Rotation`"

### 10. Add common BiomeAsset modification examples
- AGENTS.md has no examples for modifying individual asset properties after creation
- These are common operations an agent would need. All tested and verified:
  ```python
  # Change mesh
  set_property(actor_id="/Game/.../TestAssets.TestAssets",
      path="BiomeAssets[0].Mesh", value="/Engine/BasicShapes/Sphere.Sphere")

  # Change weight
  set_property(actor_id="/Game/.../TestAssets.TestAssets",
      path="BiomeAssets[0].Weight", value=2.5)

  # Change scale (3-level nested: Array[index].Struct.Field)
  set_property(actor_id="/Game/.../TestAssets.TestAssets",
      path="BiomeAssets[0].AssetOptions.Scale", value="(X=3,Y=3,Z=3)")
  ```
- **Where:** Add after Phase 3 or in a new "Modifying Biome Assets" subsection
- Shows that nested struct access within arrays works to 3+ levels

### 11. PCG regeneration trigger not documented
- **Current:** AGENTS.md says "PCG generation runs automatically or on editor interaction" (no concrete method)
- **Tested approaches:**
  - `bRegenerateInEditor = true` on BiomeCore -> no effect
  - Toggle `bActivated` false/true on BiomeCore -> no effect
  - `NotifyPropertiesChangedFromBlueprint()` on BiomeCore.BiomeCore -> no effect
  - **`NotifyPropertiesChangedFromBlueprint()` on BiomeTexture.PCG_LocalBiomeCore -> WORKS!**
- **Key insight:** Each BiomeTexture has its own local PCG component (`PCG_LocalBiomeCore`) that does the actual generation. Regen must be triggered on THIS component, not the top-level BiomeCore.
- **Add to workflow:** After Phase 8 or as a new "Phase 9.5: Trigger Regeneration":
  ```python
  # After modifying DataAssets, save then trigger regen per BiomeTexture:
  save_asset(asset_path="/Game/.../TestAssets")
  call_function(call="<Name>BiomeTexture.PCG_LocalBiomeCore.NotifyPropertiesChangedFromBlueprint")
  ```
- Also update the `call_function` limitation notes — this is a real use case that works!

### 12. Struct array reliability language (lines 308-317)
- Current text implies element-level access is needed "for reliability"
- Testing showed single-call JSON array with embedded object refs works fine
- Suggest: soften to "recommended for complex nested structs" and note single-call works for simple cases
