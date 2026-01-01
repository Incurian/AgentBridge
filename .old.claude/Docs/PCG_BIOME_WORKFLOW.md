# PCG Biome Workflow Log

> Running log of discoveries while testing the PCG Biome workflow via MCP tools.
> Goal: Programmatically set up a PCG Biome system to populate a landscape.

---

## Session: January 1, 2026

### What Works

1. **Spawning PCG Biome Blueprints** - All three core actors spawn successfully using full paths:
   - `/PCGBiomeCore/Blueprints/BP_PCGBiomeCore.BP_PCGBiomeCore_C`
   - `/PCGBiomeCore/Blueprints/BP_PCGBiomeCore_Runtime.BP_PCGBiomeCore_Runtime_C`
   - `/PCGBiomeCore/Blueprints/BP_PCGBiomeTexture.BP_PCGBiomeTexture_C`

2. **Setting Actor Transforms** - `set_actor_transform` works to position and scale the actors.

3. **Reading Component Properties** - `tempo_get_component_properties` reveals:
   - `Volume` component has `BoxExtent` property (default: 100, 100, 100)
   - `RelativeScale3D` on Volume is (512, 512, 128) by default
   - The actual box size = BoxExtent × RelativeScale3D

4. **Reading Actor Properties** - `get_actor(include_properties=True)` shows:
   - `BP_PCGBiomeTexture` has `Definition`, `Assets`, `BiomeTexture` properties
   - These are object references that need asset paths

### What Doesn't Work (Yet)

1. **Setting Object Reference Properties** - `set_property` with asset path fails:
   ```
   set_property(actor="MCP_BiomeTexture", path="Definition",
                value="/PCGBiomeSample/BiomeDefinitions/BroadleafForest.BroadleafForest")
   → INVALID_ARGUMENT - Failed to set path 'Definition'
   ```
   **Hypothesis:** Need to use a different value format or the asset isn't loaded.

2. **Class Schema for DataAssets** - `get_class_schema` can't find:
   - `PCGBiomeDefinitionAsset`
   - `PCGBiomeAsset`

   These classes may be Blueprint-based or need specific loading.

3. **list_classes with Biome pattern** - Returns 0 results even with `base_class_name="Object"`.
   The PCG Biome classes aren't registered in reflection until used.

### Surprising Discoveries

1. **Blueprint Path Format** - Must use full path with `_C` suffix:
   - Wrong: `BP_PCGBiomeCore`
   - Right: `/PCGBiomeCore/Blueprints/BP_PCGBiomeCore.BP_PCGBiomeCore_C`

2. **Landscape Bounds Calculation Algorithm:**
   - Landscape actor location is the CORNER, not center
   - `total_size = num_proxies_per_axis × quads_per_component × scale`
   - Where: `quads_per_component = GridSize` property on Landscape
   - And: `num_proxies_per_axis = sqrt(count of LandscapeStreamingProxy actors)`
   - Center XY = `landscape_location + (total_size / 2)`
   - PCG scale for XY = `(total_size / 2) / default_box_extent`

3. **Default Scale on BP_PCGBiomeTexture** - Spawns with scale (512, 512, 128), not (1, 1, 1).
   The Blueprint has default values that override spawn defaults.

4. **PCGBiomeCore Source is Minimal** - Only one header file in Public/:
   - `PCGBiomeCore.h` (329 bytes)
   - Most functionality is in Blueprints/DataAssets, not C++ classes

### Information That Should Be In Workflow Description

1. **Asset Paths for Sample Content:**
   - BiomeDefinitions: `/PCGBiomeSample/BiomeDefinitions/BroadleafForest`
   - BiomeAssets: `/PCGBiomeSample/BiomeAssets/BroadleafForest`
   - Core Blueprints: `/PCGBiomeCore/Blueprints/BP_PCGBiome*`

2. **Required Actor Types and Their Roles:**
   - `BP_PCGBiomeCore` - Main PCG generation logic
   - `BP_PCGBiomeCore_Runtime` - Runtime generation (separate from editor)
   - `BP_PCGBiomeTexture` - Texture-based biome mapping

3. **Key Properties to Set:**
   - Volume extents to match landscape bounds
   - `Definition` → BiomeDefinition DataAsset
   - `Assets` → Array of BiomeAsset DataAssets
   - `BiomeTexture` → Texture2D with color-coded biomes

4. **Landscape Bounds Retrieval:**
   - Query `Landscape` actor for location/scale
   - Get `GridSize` property (252 in our case)
   - Count streaming proxies or use component count

### Z Bounds Discovery Algorithm

To determine proper vertical coverage for PCG Biome volumes on ANY landscape:

**Step 1: Sample Collision Components**
Query `LandscapeStreamingProxy` actors at corners and center, then read `CachedLocalBox`
from their `LandscapeHeightfieldCollisionComponent` children.

**Step 2: Convert Local to World Z**
```
World_Z = Proxy_Actor_Z + (Local_Z × Landscape_Scale)
```

**Step 3: Find Min/Max Across All Samples**
```
terrain_min_z = min(all_world_z_min_values)
terrain_max_z = max(all_world_z_max_values)
```

**Step 4: Add Margin for Spawned Meshes**
```
margin = (terrain_max_z - terrain_min_z) * 0.5  # 50% margin
final_min_z = terrain_min_z - margin
final_max_z = terrain_max_z + margin + mesh_height_allowance  # e.g., 5000 for trees
```

**Step 5: Calculate PCG Volume Settings**
```
center_z = (final_min_z + final_max_z) / 2
half_extent_z = (final_max_z - final_min_z) / 2
scale_z = half_extent_z / default_box_extent  # default_box_extent = 100
```

**Note:** This algorithm should be automated with a `get_landscape_bounds` command.

---

## Feature Requests

### `tempo_call_function` with Parameters (BUG/LIMITATION)

**Current Behavior:**
```
tempo_call_function(actor="X", component="Y", function="Generate")
→ Error: "Only functions with no arguments and void return type are currently supported"
```

**Expected Behavior:** Should support any function signature:
```python
tempo_call_function(actor="X", component="Y", function="Generate",
                    parameters={"bForce": True})  # → returns result
```

**Impact:** Can't call `PCGComponent.Generate(bForce)` or many other useful functions.

**Workaround:** Use `NotifyPropertiesChangedFromBlueprint()` which has no args, but doesn't force regeneration.

---

### `get_landscape_bounds` MCP Command

**Problem:** Getting landscape world bounds requires:
1. Querying all LandscapeStreamingProxy actors
2. Sampling CachedLocalBox from collision components at multiple positions
3. Converting local coordinates to world space with scale factor
4. Computing min/max across all samples

**Proposed Solution:** A single `get_landscape_bounds` command that returns:
```json
{
  "min": [-100800, -100800, -100],
  "max": [100800, 100800, 12160],
  "center": [0, 0, 6030],
  "extent": [100800, 100800, 6130]
}
```

This would dramatically simplify PCG setup workflows.

---

### `TSoftObjectPtr` Support in `tempo_set_asset_property`

**Problem:** Properties using `TSoftObjectPtr<T>` (common for textures, materials) fail:
```
tempo_set_asset_property(actor="X", property="BiomeTexture", value="/Path/To/Texture")
→ Error: "Property did not have correct type"
```

**Current Behavior:** Only `TObjectPtr<T>` properties work.

**Expected Behavior:** Should support both hard and soft object references.

**Impact:** Can't assign textures to PCG Biome system, blocking the entire workflow.

---

### Blueprint/C++ Agnostic API

**Problem:** Agent must know implementation details to use tools correctly:
- Must use `_C` suffix for Blueprint classes
- Must know if property is `TObjectPtr` vs `TSoftObjectPtr`
- Must know if class is Blueprint DataAsset vs C++ class
- Different discovery paths for BP vs native types

**Expected Behavior:** Tools should be implementation-agnostic:
1. `spawn_actor("BP_PCGBiomeCore")` should work without `_C` suffix
2. `set_property` should auto-detect property type and route correctly
3. `list_classes` should find both BP and C++ classes uniformly
4. On failure, service should try alternative methods before returning error

**Principle:** The agent describes WHAT it wants, the service figures out HOW.

---

### UObject Property Access (not just Actors)

**Problem:** Can create DataAssets but can't modify their properties:
```python
create_asset(class="BiomeDefinitionTemplate_C", path="/Game/Test", name="MyDef")  # ✓ Works
set_property(actor="/Game/Test/MyDef.MyDef", path="BiomeDefinition.BiomeName", value="X")
→ Error: "Actor not found"  # Only actors work, not UObjects
```

**Expected:** Property tools should work on ANY UObject, not just actors.

**Workaround:** Use inline struct properties on actors (e.g., `DefaultDefinition.BiomeName`)
instead of creating separate DataAssets.

---

### Next Steps to Try

1. Try `obj load` to preload the DataAsset, then retry `set_property`
2. Check if there's a different property path (maybe nested)
3. Try creating a new DataAsset with `create_asset`
4. Look for a texture asset to assign to `BiomeTexture`

---

## Key File Locations

| Asset Type | Engine Path |
|------------|-------------|
| Core Blueprints | `Engine/Plugins/Experimental/PCGBiomeCore/Content/Blueprints/` |
| BiomeDefinitions | `Engine/Plugins/Experimental/PCGBiomeSample/Content/BiomeDefinitions/` |
| BiomeAssets | `Engine/Plugins/Experimental/PCGBiomeSample/Content/BiomeAssets/` |
| BiomeGenerators | `Engine/Plugins/Experimental/PCGBiomeSample/Content/BiomeGenerators/` |

---

---

## Summary: Current Workflow Status

### ✅ What Works via MCP

| Step | Tool | Notes |
|------|------|-------|
| Spawn PCG Biome Blueprints | `spawn_actor` | Use full path with `_C` suffix |
| Set actor transforms | `set_actor_transform` | Position and scale |
| Read component properties | `tempo_get_component_properties` | BoxExtent, CachedLocalBox, etc. |
| Set Definition property | `tempo_set_asset_property` | TObjectPtr works |
| Set Assets[0] property | `tempo_set_asset_property` | Array index syntax works |
| Trigger PCG notification | `tempo_call_function` | `NotifyPropertiesChangedFromBlueprint()` |
| **Create DataAssets** | `create_asset` | Works with BP class paths! |
| **Set inline struct properties** | `set_property` | `DefaultDefinition.BiomeName` works |
| **Set struct color** | `set_property` | Use `(R=0.0,G=1.0,B=0.0,A=1.0)` format |

### ❌ What's Blocked

| Step | Issue | Needed Fix |
|------|-------|------------|
| Set BiomeTexture | `TSoftObjectPtr` not supported | Add soft object ptr support |
| Call Generate(true) | No parameter support | Add function parameter support |
| Get landscape bounds | Too manual | Add `get_landscape_bounds` command |
| **Modify DataAsset properties** | Only actors supported | Add UObject property access |
| **Typed setters on nested structs** | `tempo_set_color_property` fails on `Struct.Color` | Fix nested path resolution |

### Result

**✅ SUCCESS!** PCG Biome workflow now works end-to-end:
- Trees, boulders, and saplings spawn as ISM components on BP_PCGBiomeCore
- ISM components found: `ISM_PCG_Sapling_01`, `ISM_PCG_Boulder_01`, `ISM_PCG_Spruce_01`, `ISM_PCG_Tree_01`, etc.
- `GeneratedGraphOutput` contains point data for mesh placement
- `LastGeneratedBounds` correctly covers full landscape

---

## Session 2: January 1, 2026 (Continued)

### Major Success: PCG Generation Works!

After switching to the sample BroadleafForest BiomeDefinition (which has the correct green color), the PCG system successfully generates meshes as ISM components on the BP_PCGBiomeCore actor.

**Generated ISM Components:**
- `ISM_PCG_Sapling_01_1`
- `ISM_PCG_Boulder_01_1`
- `ISM_PCG_Spruce_01_1`
- `ISM_PCG_Tree_01_1`
- `ISM_PCG_Sapling_02_1`
- `ISM_PCG_Tree_02_1`

### New Bug Discovered: Nested Struct Properties Don't Actually Set

**Problem:** `set_property` returns `success: true` but values aren't changed:
```python
set_property(actor="MCP_PCGBiomeTexture", path="DefaultDefinition.BiomeColor",
             value="(R=0.0,G=1.0,B=0.0,A=1.0)")
→ Returns: {"success": true}
→ Actual: Color is still white (1,1,1,0)
```

**Verification:** The `AgentBridge.DumpActor` command shows values unchanged after "successful" set.

**Root Cause:** The property path resolution works for READING nested struct properties, but WRITING to nested struct members in Blueprint-generated classes fails silently.

**Additional Evidence:**
- `tempo_set_color_property` fails with "Inner property not found" for nested paths
- Console command `SetByName` also doesn't work on nested BP struct properties

**Workaround:** Use pre-configured DataAssets (like `/PCGBiomeSample/BiomeDefinitions/BroadleafForest`) instead of trying to configure DefaultDefinition inline.

### Landscape Bounds Off-by-One Fix

**Problem:** `get_landscape_bounds` returned max of 88200 instead of 100800 (12600 units = half segment short)

**Root Cause:** `CachedLocalBox` on collision components doesn't account for full landscape segment extent.

**Fix Applied:** Modified `WorldPartitionOps.cpp` to use `GetComponentsBoundingBox(false, true)` as primary bounds source instead of `CachedLocalBox`:

```cpp
// Use the proxy's full bounding box for accurate bounds
FBox ProxyBounds = Proxy->GetComponentsBoundingBox(false, true);
if (ProxyBounds.IsValid)
{
    MinBounds = MinBounds.ComponentMin(ProxyBounds.Min);
    MaxBounds = MaxBounds.ComponentMax(ProxyBounds.Max);
}
```

**Status:** Code fix applied, needs rebuild and test.

### Updated Workflow Status

#### ✅ Fully Working

| Step | Tool | Notes |
|------|------|-------|
| Spawn PCG Biome Blueprints | `spawn_actor` | Use full path with `_C` suffix |
| Set actor transforms | `set_actor_transform` | Position and scale |
| Set Definition property | `tempo_set_asset_property` | Use sample BiomeDefinition |
| Set Assets[0] property | `tempo_set_asset_property` | Array index syntax works |
| Get landscape bounds | `get_landscape_bounds` | After code fix is rebuilt |
| Trigger PCG generation | `pcg.GenerateAll` | Console command works |
| **PCG mesh generation** | N/A | ISM components spawn on BP_PCGBiomeCore! |

#### ⚠️ Workarounds Required

| Step | Issue | Workaround |
|------|-------|------------|
| Set nested struct colors | `set_property` silently fails | Use sample assets with pre-configured colors |
| Modify DataAsset properties | Only actors supported | Use sample assets or inline actor properties |

#### ❌ Still Blocked (Future Work)

| Step | Issue | Needed Fix |
|------|-------|------------|
| Custom BiomeColor on actors | Nested struct write fails | Fix CommandExecutor property setting |
| Custom BiomeAsset meshes | Can't modify DataAsset | Add UObject property access |
| Parameterized function calls | tempo_call_function limited | Add parameter support |

---

*Document created: Jan 1, 2026*
*Updated: Jan 1, 2026 - Session 2*
