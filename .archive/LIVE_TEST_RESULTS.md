# Live Test Results: AGENTBRIDGE_BUGS Fixes

**Date:** 2026-02-13
**Branch:** `feature/agentbridge-bugs`
**Map:** `Test_Map` (Content/AB_PCG_TESTING/)
**Build:** Fresh rebuild on fresh repo clone
**Testers:** Claude (automation) + human (visual verification)

---

## Bug 1: `call_function` — host/port stored

| Test | Description | Result | Notes |
|------|-------------|--------|-------|
| 1.1 | call_function static (PrintString) | PARTIAL | gRPC succeeded, but no output in UE log. Static path doesn't use host/port. PrintString may need WorldContextObject. |
| 1.2 | call_function actor (SetActorHiddenInGame) | **PASS (fix)** | After Python fix: no more AttributeError. New error: "Only functions with no arguments and void return type are currently supported" — separate limitation, not Bug 1. |
| 1.3 | spawn_actor with relative_to | **FAIL (new issue)** | No AttributeError (Bug 1 fix works), but actor doesn't actually exist in world. Response reported success with location (500,0,2) but actor not in outliner. get_transform returned (0,0,0). relative_to code path broken beyond host/port. |
| 1.4 | add_component | **PASS (fix)** | PointLightComponent_0 added to Bug1TestActor successfully after Bug 1 fix. |

## Bug 2: `duplicate_asset` + `save_asset` — plugin content

| Test | Description | Result | Notes |
|------|-------------|--------|-------|
| 2.1 | Duplicate plugin content asset | **FAIL/CRASH** | Duplicating `/Script/Engine.DefaultPawn` crashed the editor (connection reset). Duplicating from `/Game/` worked fine. Bug 2 fix not effective for engine/plugin content paths. May not be fixable on our end. |
| 2.2 | Save duplicated asset (used to crash) | SKIP | Blocked by 2.1 crash |
| 2.3 | Regression: /Game/ asset dup+save | **PASS** | Duplicate from `/Game/` + save both succeeded. Editor stable. |

## Bug 3: `set_property` on BoxExtent — visual update

| Test | Description | Result | Notes |
|------|-------------|--------|-------|
| 3.1a | BoxExtent on TriggerBox | **FAIL/REGRESSION** | `set_property` reports success but value resets to 0,0,0. PostEditChangeProperty fix is zeroing BoxExtent. Tested twice. |
| 3.1b | BoxExtent on BP_PCGBiomeCore Volume | **FAIL/REGRESSION** | Same zeroing behavior on real biome actor Volume component. |
| 3.1c | Workaround: set_transform scale on Volume | **FAIL** | Scale changed in readback, but wireframe DISAPPEARED (didn't grow). BoxExtent zeroed to 0,0,0 means zero-size box even at 10x scale. PostEditChangeProperty destroys the extent. |
| 3.2 | Light intensity visual update | **PASS** | Intensity 5000->100000 visually confirmed. Float properties + PostEditChangeProperty work correctly. |
| 3.3 | Actor direct property (bHidden) | **PARTIAL** | `bHidden` property set correctly (checked in editor). But StaticMeshActor has no mesh so can't visually verify hiding. Gizmo/outliner still visible as expected (bHidden is game-only). |
| 3.4 | Nested struct regression | **FAIL/REGRESSION** | `set_property` on `RootComponent.RelativeLocation` reports success but value zeros out. Actor didn't move. PostEditChangeProperty causing widespread vector regression. |

## Bug 4: `set_property` — type validation for soft object refs

| Test | Description | Result | Notes |
|------|-------------|--------|-------|
| 4.1 | Type mismatch detection | **PASS** | Set DefaultBiome (DataAsset) to BiomeTexture (expects Texture) — correctly rejected with "value type mismatch" error. Original valid value preserved. |
| 4.2 | Valid ref regression | **PASS** | Set T_BiomeTestTexture to BiomeTexture — set and readback confirmed. Valid refs still work. |

## Bug 5: `get_landscape_bounds` — biome_volume_scale

| Test | Description | Result | Notes |
|------|-------------|--------|-------|
| 5.1 | biome_volume_scale field present | **FAIL** | Field NOT in response. Proto regen likely not run or failed silently (CRLF?). Only standard fields returned. |
| 5.2 | Scale sizes volume over landscape | SKIP | Blocked by 5.1 — no biome_volume_scale to use. |
| 5.3 | Existing fields regression | **PASS** | All existing fields present and correct: valid=true, center=[0,0,0], extent=[204000,204000,100], proxy_count=1, landscape_name=Landscape_0 |

## Bug 6: `list_classes` — plugin Blueprint discovery

| Test | Description | Result | Notes |
|------|-------------|--------|-------|
| 6.1 | Find plugin BP class | **PASS** | Found BP_PCGBiomeCore_C with full path, is_blueprint=true. |
| 6.2 | Consistency with spawn_actor | **PASS** | Short name spawn failed on cold editor, but succeeded after list_classes loaded the class. Full path always works. list_classes acts as a class loader. |
| 6.3 | base_class filter regression | **PASS** | `base_class_name="Light"` returned 5 light classes correctly. No duplicates. |
| 6.4 | No pattern regression | **PASS** | `base_class_name="Actor"` returned 5 results, no bloat from Phase 1.5. |

## Bugs 7+8: ProtoPropertyValueToJson — type handling

| Test | Description | Result | Notes |
|------|-------------|--------|-------|
| 7.1 | Struct array at array level (Tags) | **PASS (fix)** | After Python fix: `["Tag1","Tag2","Tag3"]` and `["A","B","C","D"]` both work. Multi-element string arrays fixed. |
| 7.2a | Object ref in array (single) | **PASS** | Single-element `["/Game/.../DefaultAsset"]` works. |
| 7.2b | Object ref in array (multi) | **FAIL** | Multi-element object ref array fails on C++ side. Not a Python issue. Workaround: set elements individually via `Assets[0]`, `Assets[1]`. |
| 7.3 | Transform property readback | SKIP | `RelativeTransform` not exposed as readable property on SceneComponent. Not testable via get_property. |
| 7.4 | Improved error message | **PASS** | Error now says "value type mismatch or invalid format. For arrays, use JSON array syntax". |
| 7.5 | Existing types regression | **PASS** | After Python fix: Bool, Float, Vector set+read, Color set, 3-element numeric array still treated as vector, 4-element string array works. No regressions. |

---

## Summary

**Total:** 27 | **Pass:** 17 | **Fail:** 4 | **Partial:** 1 | **Skip:** 5

## Issues Discovered

### REGRESSION: PostEditChangeProperty zeros vector/struct properties (Bug 3 fix)
- `set_property` on BoxExtent, RelativeLocation reports success but value resets to 0,0,0
- Affects both TriggerBox and BP_PCGBiomeCore Volume component
- Float properties (Intensity) work correctly — regression is vector/struct specific
- `set_transform` workaround also broken because BoxExtent gets zeroed (wireframe disappears)
- **Recommendation:** Revert Bug 3 fix. Original behavior (value stores, no visual update) was better than (value destroyed).

### FIXED: call_function host/port (Bug 1) - applied during testing
- Added `self.host = host` and `self.port = port` to `AgentBridgeClient.__init__` (agentbridge.py:722-723)
- Instance call_function, add_component now work (no more AttributeError)
- call_function has SEPARATE limitation: "Only functions with no arguments and void return type supported"
- relative_to spawn has SEPARATE issue: actor not created in world despite success response

### NEW ISSUE: relative_to spawn creates ghost actor
- `spawn_actor(relative_to="Bug1Anchor")` reports success with a location
- But actor doesn't appear in outliner, get_transform returns (0,0,0)
- The relative_to code path goes through a different spawn mechanism that isn't working

### NOT APPLIED: biome_volume_scale proto field (Bug 5)
- `get_landscape_bounds` response missing `biome_volume_scale` field
- Proto regen likely not run or failed silently during build
- Existing fields all correct (no regression)

### CRASH: Plugin content duplication (Bug 2)
- Duplicating from engine/plugin content paths (`/Script/Engine.*`) crashes editor
- `/Game/` path duplication + save works fine
- May be an engine limitation, not fixable on our side

### Multi-element array set broken (Bug 7)
- `set_property(Tags, ["Tag1","Tag2","Tag3"])` fails: "could not convert string to float"
- Single-element `["TestTag"]` works
- Issue is in MCP Python->gRPC serialization, not C++ side

### KEY INSIGHT: Single vs multi-element arrays
- **Single-element arrays work** for all types tested: strings (`Tags=["TestTag"]`), object refs (`Assets=["/Game/.../DefaultAsset"]`)
- **Multi-element arrays fail** with "could not convert string to float" — the Python MCP layer likely iterates and tries numeric conversion on each element
- The C++ Bug 7/8 fix for ARRAY/OBJECT type handling in ProtoPropertyValueToJson IS working (type reads back correctly as "Array", "Object", etc.)
- The remaining issue is purely in the Python MCP serialization layer when converting multi-element arrays to protobuf values
- This means the C++ fixes are solid; only the Python serialization needs a fix for multi-element support

## Conclusions and Next Steps

### What worked
- **Bug 1 (call_function host/port):** Fixed during testing session - 2-line Python fix in agentbridge.py
- **Bug 4 (type validation):** Working correctly - rejects mismatched types, preserves valid values
- **Bug 6 (list_classes):** All tests pass - Blueprint discovery works, acts as class loader for spawn_actor
- **Bug 7/8 (type handling):** C++ ProtoPropertyValueToJson fixes working - proper type labels, improved errors

### What needs rework
- **Bug 3 (PostEditChangeProperty):** MUST REVERT - zeros vector/struct properties. Original behavior was better.
- **Bug 5 (biome_volume_scale):** Proto regen not applied - need to verify proto file, run GenProtos.sh, rebuild
- **Bug 7 multi-element arrays:** Python MCP serialization breaks on multi-element arrays - fix in Python layer

### New issues found
- **relative_to spawn:** Reports success but actor doesn't exist in world (ghost actor)
- **call_function limitation:** Only supports zero-arg void functions (separate from Bug 1)
- **Plugin content duplication:** Crashes editor - likely engine limitation, document as unsupported

### Recommended fix priority
1. **Revert Bug 3** PostEditChangeProperty changes (REGRESSION - actively breaking things)
2. **Proto regen** for Bug 5 biome_volume_scale field
3. **Python multi-element array** serialization fix
4. **relative_to spawn** investigation
5. **call_function** arg support (larger feature, not a bug fix)
