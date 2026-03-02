# UE 5.7 Migration Analysis for AgentBridge

**Date:** 2026-02-18
**Current Engine:** UE 5.6
**Target Engine:** UE 5.7 (released late 2025)
**Research basis:** 3 parallel research agents, UE 5.7 release notes, Epic forums, API docs

---

## Executive Summary

AgentBridge should be **largely compatible** with UE 5.7 out of the box. No explicit C++
standard is set in any `.Build.cs` file, and no deprecated property reflection APIs are used.
The main concerns are:

1. **LANDSCAPE_OPS plan** - Landscape paint weight blending has a **known regression in UE 5.7**
   that affects `FAlphamapAccessor` behavior. The plan needs verification against 5.7 headers.
2. **PCG Biome Core V2** - Architectural shift to local-per-actor generation. The
   `NotifyPropertiesChangedFromBlueprint()` workflow may need updates.
3. **Header include cleanup** - UE 5.7 removed some transitive includes. Expect compile errors
   that are fixed by adding explicit `#include` statements.
4. **New opportunities** - New landscape APIs (`AddTargetLayer`, etc.) and PCG features
   (standalone graph execution, Python scripting node) could enhance AgentBridge capabilities.

---

## 1. Build System

### C++20 Requirement (LOW RISK for AgentBridge)

UE 5.7 marks `CppStandardVersion::Cpp17` as **obsolete** - code will not compile if
any `.Build.cs` explicitly sets C++17.

**AgentBridge status:** SAFE. Grep confirms none of our 4 `.Build.cs` files set
`CppStandardVersion` explicitly. UE defaults to C++20.

### MSVC 14.44 Preferred (LOW RISK)

Preferred MSVC toolchain updated from 14.38 to 14.44. Configured in
`Engine/Config/Windows/Windows_SDK.json`. Ensure Visual Studio 2022 has 14.44 toolset.

### SDL2 to SDL3 (Linux only, N/A)

Linux builds now use SDL3 instead of SDL2. Any `.Build.cs` referencing `SDL2` module must
change to `SDL3`. **AgentBridge does not reference SDL** - no impact.

---

## 2. Landscape System (HIGH RISK for LANDSCAPE_OPS Plan)

### 2.1 Paint Weight Blending Regression (CRITICAL for plan)

**Multiple forum reports confirm broken landscape weight blending in UE 5.7:**

- Landscape brush now uses ~50% weight alpha instead of full weight
- Edit layer ordering affects paint weight (top layer gets full weight, bottom layers
  are not properly erased)
- Some layers can only paint onto base layer, fail to paint over other layers
- This is a **known regression** from the Advanced Paint Layers changes

**Impact on LANDSCAPE_OPS `PaintLandscapeLayers`:**
The plan uses `FAlphamapAccessor<false, false>` to programmatically set layer weights.
This accessor writes directly to weight textures, which may or may not be affected by
the same regression (the bug reports are about the brush/editor painting UI, not direct
accessor writes). **Must test with UE 5.7 before relying on this approach.**

**Mitigation options:**
1. Test `FAlphamapAccessor::SetData()` directly on 5.7 - it may bypass the UI regression
2. If accessor is also affected, check if the new `AddTargetLayer()` API provides a
   working alternative path
3. Wait for Epic hotfix - these are acknowledged regressions

**Forum references:**
- https://forums.unrealengine.com/t/landscape-paint-weight-blending-is-broken/2673901
- https://forums.unrealengine.com/t/5-7-landscape-paint-layer-blend-issue/2677415
- https://forums.unrealengine.com/t/unreal-engine-5-7-problem-with-the-paint-material-on-the-landscape/2674846

### 2.2 New Landscape APIs (Additive, OPPORTUNITY)

UE 5.7 adds several new functions to `ALandscapeProxy`:

| New Function | Purpose | Relevance |
|---|---|---|
| `AddTargetLayer()` | Add target layer with optional unique name | Could simplify PaintLandscapeLayers |
| `RemoveTargetLayer()` | Remove named target layers | Useful for cleanup |
| `UpdateTargetLayer()` | Update existing target layer settings | Layer management |
| `GetTargetLayers()` | Retrieve all target layers | Query capabilities |
| `SetLandscapeMaterialScalarParameterValue()` | Set float params on all components | Material parameter control |
| `SetLandscapeMaterialTextureParameterValue()` | Set texture params on all components | Texture parameter control |
| `SetLandscapeMaterialVectorParameterValue()` | Set vector/color params on all components | Color parameter control |
| `CreateLandscapeTextureArray()` | Creates Texture2DArray for proxy | Texture management |

**Recommendation:** Consider using `AddTargetLayer()` instead of (or in addition to)
`CreateTargetLayerSettingsFor()` for layer registration in `PaintLandscapeLayers`.
The material parameter functions could become new MCP tools in a future iteration.

### 2.3 New Edit Layer Types (Additive)

UE 5.7 introduces:
- **Splines Edit Layer** - Non-destructive landscape spline management
- **Patches Edit Layer** - Non-destructive landscape patch management
- **Edit Layer Inspector Panel** - New UI for configuring edit layer settings

These don't break existing code but may change how the `HasLayersContent()` check
in `GetLandscapeInfoFromProxy()` behaves (the plan already rejects landscapes with
edit layers - this should still work).

### 2.4 ALandscapeProxy::Import() Signature (MUST VERIFY)

The LANDSCAPE_OPS plan was validated against UE 5.6 source headers. The `Import()`
function signature (11 parameters) should be verified against UE 5.7 headers. The
edit layer system changes may have added or modified parameters.

**Key signatures to verify:**
```cpp
// Verify these still match in UE 5.7:
void ALandscapeProxy::Import(const FGuid& InGuid, int32 MinX, int32 MinY,
    int32 MaxX, int32 MaxY, int32 InNumSubsections, int32 InSubsectionSizeQuads,
    const TMap<FGuid, TArray<uint16>>& InImportHeightData,
    const TCHAR* InHeightmapFileName,
    const TMap<FGuid, TArray<FLandscapeImportLayerInfo>>& InImportMaterialLayerInfos,
    ELandscapeImportAlphamapType InImportAlphamapType,
    const TArrayView<const FLandscapeLayer>& InImportLayers);

FLandscapeImportHelper::ChooseBestComponentSizeForImport(...)
ULandscapeSubsystem::ChangeGridSize(...)
```

### 2.5 Landscape Grass Density (Known Bug, LOW RISK)

Grass appears sparser in 5.7 vs 5.5.4. This is a visual regression, not an API change.
Does not affect AgentBridge's landscape creation/import operations.

---

## 3. PCG / PCG Biome (MEDIUM RISK)

### 3.1 PCG is Now Production-Ready (Status Change)

PCG moved from Experimental/Beta to **Production-Ready** in UE 5.7. This signals API
stability going forward. Performance is ~2x vs UE 5.5 with GPU and game-thread optimizations.

### 3.2 Biome Core V2 (MEDIUM RISK for existing workflows)

**Architectural shift:** Everything is now generated **locally per Biome Actor** with a
new `Local Biome Core PCG Component and Graph`.

Key changes:
- Local assets and biome definitions can be embedded directly per-actor
- New local preview mode for more efficient biome creation
- Graph simplification: inline constants, removed unnecessary loops
- New GPU ground scatter via PCG GPU landscape RVT interface

**Impact on AgentBridge:**
The existing PCG biome workflow in `AGENTS.md` Phase 10 uses:
```
call_function(call="<Name>BiomeTexture.PCG_LocalBiomeCore.NotifyPropertiesChangedFromBlueprint")
```

This targets the `PCG_LocalBiomeCore` component on BiomeTexture actors. In Biome Core V2,
the architecture is "local per biome actor" which aligns with what we're already doing.
**However, the component name, property paths, and data asset structures may have changed.**

**Action items:**
1. Verify `PCG_LocalBiomeCore` component still exists on BiomeTexture actors in V2
2. Verify `NotifyPropertiesChangedFromBlueprint()` still triggers regeneration
3. Check if biome data asset property paths changed (BiomeDefinition, BiomeAssets, etc.)
4. Update AGENTS.md workflow examples if component/property names changed

### 3.3 Standalone Graph Execution (Additive, OPPORTUNITY)

PCG graphs can now execute as standalone flows without a world context or
`UPCGComponent`. New `Is Standalone Graph` setting enables asset-creation workflows.

**Impact on AgentBridge:** The `pcg_list_nodes`, `pcg_add_node`, `pcg_connect` MCP
tools operate on graph assets via the live editor. Standalone execution is a new
feature that doesn't break existing tools but could enable new workflows.

### 3.4 FastGeo / Componentless Primitives (Experimental, LOW RISK)

PCG GPU can now spawn primitives via FastGeo components instead of actor components.
Enable via `PCGFastGeoInterop` plugin.

**Impact on AgentBridge:** FastGeo-spawned meshes may NOT appear as regular actors
in `query_actors` results since they bypass the actor/component system. This is
experimental and opt-in, so existing workflows are unaffected unless users enable it.

### 3.5 New PCG Node Types (Additive)

| New Node | Purpose |
|---|---|
| Polygon2D (Create, Convert) | Closed area representations |
| Spline Intersection | Intersect splines in 3D |
| Split Splines | Divide splines by various criteria |
| Get Segment | Extract spline segments |
| Execute Python Script | Python scripting in PCG graphs (new plugin) |
| Save Texture to Asset (GPU) | Bake PCGTextureData to UTexture2D |
| Cull Points (GPU) | GPU-accelerated point culling |
| Transform Points (GPU) | GPU-accelerated transforms |

These are additive - the existing `pcg_add_node` MCP tool dispatches by string name
and should handle new node types automatically.

### 3.6 Custom Data Types (Additive)

UE 5.7 removes the hardcoded PCG data type list. Plugins can register custom data types.
This is additive and shouldn't break existing graph operations.

### 3.7 PCG Editor Mode (NEW, Future Opportunity)

UE 5.7 introduces a dedicated PCG Editor Mode with interactive tools:
- **Draw Spline tool** - Define spline-bound areas for PCG graphs
- **Paint tool** - Paint points via raycasts (like Foliage mode)
- **Volume tool** - Create PCG volumes by dragging

These tools are not currently accessible via gRPC/MCP. Agent-driven workflows that
need these interactive tools would require new RPCs. Consider for future plans.

### 3.8 Procedural Vegetation Editor (Experimental, Future Opportunity)

New plugin for creating/editing vegetation with Nanite support, wind animation.
Built on PCG nodes. Separate plugin, does not change core PCG.

---

## 4. Property System / Reflection (LOW RISK)

### No Breaking Changes Detected

AgentBridgeCore uses `FProperty`, `FStructProperty`, `FArrayProperty`, `UFunction`
extensively. Grep confirms:
- No usage of deprecated `ElementSize` member (vs `GetElementSize()`)
- No usage of deprecated `ImportText()` overload (vs `ImportText_Direct()`)
- Core reflection APIs (`FProperty`, `UFunction`, property paths) appear unchanged in 5.7

### Potential Deprecation Removals

Some UE 5.1+ deprecation warnings may become hard errors in 5.7. These would manifest
as compile errors and are straightforward to fix:
- `FProperty::ElementSize` -> `FProperty::GetElementSize()`
- `FProperty::ImportText()` -> `FProperty::ImportText_Direct()` or `ImportText_InContainer()`

**AgentBridge status:** SAFE - our code doesn't use these deprecated patterns.

---

## 5. Actor Operations (LOW RISK)

### SpawnActor API - Unchanged

`UWorld::SpawnActor()`, `UWorld::SpawnActorDeferred()`, `FActorSpawnParameters` are
all present and documented in UE 5.7 with no signature changes.

### Transitive Include Cleanup (MEDIUM RISK)

UE 5.7 removed some transitive includes. Common issue: code that used `UWorld*` without
explicitly including `Engine/World.h`. Forum reports confirm this affected template code.

**AgentBridge status:** Our code already includes `Engine/World.h` explicitly in
Runtime and Scripting modules. May still hit issues with other transitively-included
headers. **This is the #1 most likely source of compile errors** - easy to fix case-by-case.

---

## 6. World Partition (LOW RISK)

### Core APIs Unchanged

`FWorldPartitionActorDescInstance`, `FWorldPartitionActorDesc`,
`UWorldPartition::GetActorDescInstance()` - all still documented in 5.7.
AgentBridge Runtime module uses these heavily in `WorldPartitionOps.cpp`.

### New Custom HLOD Actors (Additive)

New `World Partition Custom HLOD` actor class for injecting custom HLOD representations.
These would appear as new actor types in `query_actors` results but don't break
existing queries.

### WorldPartitionEditorHash Header

AgentBridge includes `WorldPartition/WorldPartitionEditorHash.h`. This header may have
been reorganized in 5.7. Verify it still exists and provides the expected types.

---

## 7. Asset Operations (LOW RISK)

### No Known Changes

`FAssetRegistryModule`, `UAssetManager`, asset creation/duplication/saving APIs are
unchanged in UE 5.7 documentation. The `SavePackage`, `ObjectTools`, and related
headers should still work.

### UAssetAPI Compatibility (bp_toolkit)

The offline Blueprint/PCG modification tools (bp_toolkit) use UAssetAPI for JSON
round-tripping. UE 5.7 may introduce new serialization formats for:
- PCG graphs (given Biome Core V2 and standalone graph changes)
- Blueprints (possible MetaDataMap changes similar to the 5.7 workaround already documented)

**Action:** Test bp_toolkit export/import with UE 5.7 assets after upgrading.

---

## 8. gRPC / Networking (LOW RISK)

### No Direct UE Changes Affect gRPC

AgentBridge's gRPC integration goes through the Tempo plugin, which manages its own
networking stack. UE 5.7's Iris networking changes (Beta, `UReplicationBridge` removal)
are irrelevant - we don't use Iris.

### MSVC 14.44 + gRPC Compatibility

gRPC's Windows SDK usage should be tested with MSVC 14.44. The existing header conflict
isolation pattern (business logic in Scripting, not Server) should continue to work.

---

## 9. Rendering Changes (Informational, No AgentBridge Impact)

UE 5.7 brings significant rendering improvements that don't directly affect AgentBridge
but are worth noting for level-building workflows:

- **MegaLights** (Beta) - Unified direct lighting, supports directional lights
- **Nanite Foliage** (Production-Ready) - Nanite for foliage rendering
- **Virtual Shadow Maps** - Improved VSM integration with MegaLights
- **Runtime Virtual Texturing** - Landscape material interaction improvements

These affect how levels look but not how AgentBridge controls them.

---

## Impact on LANDSCAPE_OPS Plan

The LANDSCAPE_OPS plan was validated against UE 5.6. For UE 5.7 compatibility:

### Must Verify Before Implementation

| Item | Plan Reference | Action |
|---|---|---|
| `ALandscapeProxy::Import()` signature | P3.3, step 8 | Compare against 5.7 `LandscapeProxy.h` |
| `FAlphamapAccessor::SetData()` behavior | P3.6, step 4d | Test weight painting on 5.7 |
| `FHeightmapAccessor::SetData()` behavior | P3.4, step 7 | Test heightmap import on 5.7 |
| `CreateTargetLayerSettingsFor()` | P3.6, step 4b | Verify still exists; consider `AddTargetLayer()` |
| `LandscapeImportHelper` header | P3.1 | Verify header path unchanged |
| `ULandscapeSubsystem::ChangeGridSize()` | P3.3, step 10 | Verify signature unchanged |
| `EditorSetLandscapeMaterial()` | P3.5 (not used, but documented) | Verify `PostEditChangeProperty` path still works |

### Potential Improvements for 5.7

| Opportunity | Plan Section | Benefit |
|---|---|---|
| Use `AddTargetLayer()` instead of `CreateTargetLayerSettingsFor()` | P3.6 | Higher-level API, possibly more reliable |
| Expose `SetLandscapeMaterialScalarParameterValue()` | Future | Direct material param control |
| Expose `GetTargetLayers()` | Future | Query layer configuration |

### Known 5.7 Bugs That May Affect Plan

| Bug | Plan Section | Severity | Workaround |
|---|---|---|---|
| Paint weight blending regression | P3.6 (PaintLandscapeLayers) | HIGH | Test accessor path; may bypass UI bug |
| Grass density sparser | N/A | LOW | Visual only, no API impact |
| Edit layer ordering affects weights | P3.6 (if edit layers enabled) | MEDIUM | Plan already rejects edit-layer landscapes |

---

## Migration Checklist

### Phase 0: Quick Compile Test (do first)

- [ ] Build AgentBridge against UE 5.7 - note all compile errors
- [ ] Fix any missing `#include` statements (transitive include cleanup)
- [ ] Fix any deprecated API usage (unlikely based on grep analysis)
- [ ] Verify all 4 plugins load in 5.7 editor

### Phase 1: Runtime Verification

- [ ] Test `query_actors` with existing level content
- [ ] Test `set_property` / `get_property` on various types
- [ ] Test `spawn_actor` / `delete_actor`
- [ ] Test `set_transform` / `get_transform`
- [ ] Test World Partition queries (`include_unloaded`)

### Phase 2: PCG Verification

- [ ] Verify `PCG_LocalBiomeCore` component exists on Biome V2 actors
- [ ] Test `NotifyPropertiesChangedFromBlueprint()` triggers regen
- [ ] Test `pcg_list_nodes` / `pcg_add_node` on 5.7 PCG graphs
- [ ] Verify biome data asset property paths haven't changed

### Phase 3: Landscape Verification (Before LANDSCAPE_OPS Implementation)

- [ ] Verify `ALandscapeProxy::Import()` signature matches plan
- [ ] Test `FHeightmapAccessor::SetData()` writes heightmap correctly
- [ ] Test `FAlphamapAccessor::SetData()` writes weights correctly
- [ ] Verify `CreateTargetLayerSettingsFor()` or `AddTargetLayer()` works
- [ ] Test `ULandscapeSubsystem::ChangeGridSize()` for WP grid

### Phase 4: bp_toolkit Verification

- [ ] Test UAsset export/import round-trip with 5.7 Blueprint assets
- [ ] Test UAsset export/import with 5.7 PCG graph assets
- [ ] Verify MetaDataMap workaround still needed/works

---

## Raw Research Outputs

Full agent research outputs preserved in `docs/plans/research/`:
- `agent1_codebase_api_inventory.txt` - Complete inventory of UE APIs used by AgentBridge
- `agent2_ue57_api_changes.txt` - General UE 5.7 API changes research
- `agent3_pcg_biome_changes.txt` - PCG and Biome-specific 5.7 changes research

---

## Sources

### Official Documentation
- [UE 5.7 Release Notes](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-7-release-notes)
- [ALandscapeProxy API (5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Landscape/ALandscapeProxy)
- [Landscape Edit Layers (5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-edit-layers-in-unreal-engine)
- [PCG Overview (5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-overview)
- [PCG Biome Core Reference (5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-pcg-biome-core-and-sample-plugins-reference-guide-in-unreal-engine)
- [PCG with World Partition (5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-with-world-partition-in-unreal-engine)
- [PCG GPU Processing (5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-with-gpu-processing-in-unreal-engine)

### Community Reports
- [Landscape Paint Weight Blending Broken](https://forums.unrealengine.com/t/landscape-paint-weight-blending-is-broken/2673901)
- [5.7 Landscape Paint Layer Blend Issue](https://forums.unrealengine.com/t/5-7-landscape-paint-layer-blend-issue/2677415)
- [UE 5.7 Paint Material Problem](https://forums.unrealengine.com/t/unreal-engine-5-7-problem-with-the-paint-material-on-the-landscape/2674846)
- [PCG 5.5->5.6 Migration Issue](https://forums.unrealengine.com/t/pcg-problem-going-from-ue-5-5-to-5-6-get-point-data-not-working-anymore/2651694)
- [CppStandard Cpp17 Obsolete Fix](https://forums.unrealengine.com/t/fix-for-riderlink-5-7-error-riderlink-plugin-fails-to-build-on-ue5-main-cppstandardversion-cpp17-obsolete/2684868)
- [MSVC 14.42/14.44 Build Thread](https://forums.unrealengine.com/t/building-unreal-with-msvc-14-42-14-44/2609153)
- [UE 5.7 Preview Forum Thread](https://forums.unrealengine.com/t/unreal-engine-5-7-preview/2658958)

### Analysis Articles
- [Production-Ready PCG in UE 5.7 (80.lv)](https://80.lv/articles/what-s-new-in-pcg-in-ue5-6-ue5-7)
- [UE 5.7 PCG Grows Up (Digital Production)](https://digitalproduction.com/2025/10/17/unreal-5-7-preview-pcg-grows-up-foliage-gets-fancy/)
- [UE 5.7 Performance Highlights (Tom Looman)](https://tomlooman.com/unreal-engine-5-7-performance-highlights/)
- [Biome Core V2 Roadmap](https://portal.productboard.com/epicgames/1-unreal-engine-public-roadmap/c/2015-biome-core-v2-plugin-experimental-)
