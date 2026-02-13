# AgentBridge — Bugs and Required Changes

Code-level fixes needed in the AgentBridge plugin, identified during PCG Biome Workflow testing (2026-02-12).
See `.archive/PCG_BIOME_WORKFLOW_TEST.md` for full test log and reproduction steps.

**Investigation completed 2026-02-12** — root causes traced to exact file/line for all 8 bugs.
**Validation round 1** — all fixes verified against actual source code.
**Validation round 2** — implementation details confirmed sufficient for uncontexted subagents.
Bug 3 Option A redesigned after finding architectural flaw (component recursion issue).
All proto field names, line numbers, and insertion points double-checked.

**Implementation completed 2026-02-12** — all 8 bugs fixed on branch `feature/agentbridge-bugs`.
Pending: code review validation, proto regeneration (Bug 5), build + live testing.

---

## CRITICAL

### 1. `call_function` — Completely Broken

**Status:** Validated. Fix is trivial and correct.

**Symptom:** Every invocation returns `'AgentBridgeClient' object has no attribute 'host'`.

**Reproduction:**
```python
call_function(call="BiomeCore.SetActorHiddenInGame", parameters={"bNewHidden": true})
# → {"error": "'AgentBridgeClient' object has no attribute 'host'"}
```

**Root Cause:** `AgentBridgeClient.__init__()` receives `host` and `port` as constructor args
but never stores them as instance attributes. Four tools reference `client.host` / `client.port`
to create secondary Tempo gRPC clients via `_get_tempo_client()` and `_get_tempo_core_client()`.

**Affected Tools (4 total):**

| Tool | Line | Code |
|------|------|------|
| `quit` | 2495 | `_get_tempo_core_client(client.host, client.port)` |
| `spawn_actor` (with `relative_to`) | 2616 | `_get_tempo_client(client.host, client.port)` |
| `add_component` | 2684 | `_get_tempo_client(client.host, client.port)` |
| `call_function` (actor type) | 2884 | `_get_tempo_client(client.host, client.port)` |

**Helper functions (lines 27-43):** `_get_tempo_client(host, port)` and `_get_tempo_core_client(host, port)`
are lazy-loading factory functions that maintain global caches of Tempo clients. They require
`host` and `port` as arguments.

#### Implementation

**File:** `mcp/services/agentbridge.py`

**Find this code (lines 719-724):**
```python
class AgentBridgeClient:
    """Client for AgentBridge gRPC service."""

    def __init__(self, host: str = "localhost", port: int = 50051):
        self.channel = create_channel(host, port)
        self.stub = pb_grpc.AgentBridgeServiceStub(self.channel)
```

**Replace with:**
```python
class AgentBridgeClient:
    """Client for AgentBridge gRPC service."""

    def __init__(self, host: str = "localhost", port: int = 50051):
        self.host = host
        self.port = port
        self.channel = create_channel(host, port)
        self.stub = pb_grpc.AgentBridgeServiceStub(self.channel)
```

**That's the entire fix.** No other files need changes. The class is standalone (no inheritance).
`create_channel()` is imported from `base.py` (line 5) and takes `(host, port)`.

**Effort:** Trivial (2 lines). **Risk:** None — purely additive, no existing logic changed.

**Verification:** After fix, test `call_function(call="KismetSystemLibrary::PrintString", parameters={"InString": "Hello"})`.

---

### 2. `duplicate_asset` + `save_asset` — Crash on Plugin Content Sources

**Status:** Validated. Fix is correct and well-positioned.

**Symptom:** `save_asset` crashes the editor with "Asset cannot be saved as it has only been
partially loaded" when the asset was duplicated from a plugin content path.

**Root Cause:** Plugin assets use async/lazy loading. `LoadObject<UObject>()` may return a
partially-loaded object. `StaticDuplicateObject()` copies it including its partial-load state.
When `save_asset` later tries to serialize, UE's `SavePackage()` hits the "partially loaded"
guard and crashes.

#### Implementation

**File:** `Source/AgentBridgeScripting/Private/CommandExecutor.cpp`

**Handler:** `FCommandExecutor::Execute(const FDuplicateAssetCommand& Command, FDuplicateAssetResponse& Response)` at line 2757

**Command struct** (from `AgentCommands.h` lines 935-947):
```cpp
struct FDuplicateAssetCommand : FAgentCommandBase
{
    FString SourcePath;
    FString DestPackagePath;
    FString DestAssetName;
};
```

**Find this code (lines 2788-2810):**
```cpp
	// Load source asset
	UObject* SourceAsset = LoadObject<UObject>(nullptr, *Command.SourcePath);
	if (!SourceAsset)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = FString::Printf(TEXT("Source asset '%s' not found"), *Command.SourcePath);
		Response.ExecutionTimeMs = EndTiming(StartTime);
		return;
	}

	// Create destination package
	UPackage* DestPackage = CreatePackage(*DestPackageName);
```

**Insert after the null check (after line 2796, before "Create destination package"):**
```cpp
	// Ensure fully loaded (critical for plugin content with async loading)
	UPackage* SourcePackage = SourceAsset->GetOutermost();
	if (SourcePackage)
	{
		SourcePackage->FullyLoad();
	}
```

**No additional headers needed.** `UPackage::FullyLoad()` and `GetOutermost()` are available
via existing includes (`UObject/SavePackage.h` at line 22, `Engine/DataAsset.h` at line 11).

**No existing `FullyLoad()` calls** exist in the codebase — this is the first.

**Why the save_asset handler crashes (for context):** The save handler at line 2598 calls
`UPackage::SavePackage(Package, Asset, *PackageFileName, SaveArgs)` at line 2628. This hits
UE's internal guard on partially-loaded packages.

**Effort:** Low (5 lines). **Risk:** Low — `FullyLoad()` is synchronous, called once per duplication.

**Verification:** After fix, test:
```python
duplicate_asset(source_path="/PCGBiomeCore/Templates/DefaultBiomeDefinition.DefaultBiomeDefinition",
    dest_package_path="/Game/Test", dest_asset_name="TestDef")
save_asset(asset_path="/Game/Test/TestDef.TestDef")  # Should succeed, not crash
```

---

## HIGH

### 3. `set_property` on BoxExtent — No Visual Update

**Status:** Validated. Two options provided. Option A (recommended) works at the right
architectural level. Option B is a future enhancement requiring refactoring.

**Symptom:** `set_property` on `UBoxComponent::BoxExtent` stores the value (verified via
`get_property` readback) but the visual wireframe in the editor never updates.

**This is a systemic issue** — not just BoxExtent. ANY property that requires side-effects
(color updates, material rebuilds, physics state changes) will silently fail to update visually.

**Root Cause:** `set_property` writes via `FProperty` reflection (raw memory write) without
calling `PostEditChangeProperty()` or `MarkRenderStateDirty()`. These are the notifications
that trigger editor visual updates.

**Call chain:**
```
MCP set_property()
  → CommandExecutor.cpp:748 (FSetPropertyPathCommand handler)
    → AgentPropertyPath.cpp:366 SetValue(Object, Segments, Value)
      → Line 409: Recurse into component if path starts with component name
      → Line 418: FPropertyAccessor::WriteProperty(Object, FirstProp, Value)      [single-segment]
      → Line 429: FPropertyAccessor::WritePropertyDirect(Resolution.ValuePtr, ...) [nested path]
        → FProperty::SetPropertyValue()  ← raw write, NO notification
```

**Key architectural detail:** When `SetValue()` processes a component path like
`"LightComponent0.Intensity"`, it recursively calls itself with the component as `Object`:
```cpp
// AgentPropertyPath.cpp line 409:
return SetValue(Component, RemainingSegments, Value);  // Object is now the Component
```
After recursion, `Object` at the write points (lines 418/429) is the **component**, not the
original actor. This is important for notification — the notification must happen where
`Object` is the correct target.

#### Header Constraint

`FPropertyChangedEvent` requires `Editor.h`, which is **PROHIBITED** in AgentBridgeCore due
to Windows SDK conflicts with gRPC headers. However, `MarkRenderStateDirty()` and
`MarkComponentsRenderStateDirty()` are available in runtime headers that AgentBridgeCore
already includes (`Components/ActorComponent.h`, `GameFramework/Actor.h`).

#### Implementation — Option A: MarkRenderStateDirty in SetValue (Recommended)

This option places the notification inside `AgentPropertyPath::SetValue()` where `Object`
is already the correct target (component or actor) after path resolution. Uses runtime-safe
methods that don't require editor headers.

**File:** `Source/AgentBridgeCore/Private/AgentPropertyPath.cpp`

**First, add the required include (after line 6):**
```cpp
#include "Components/SceneComponent.h"  // For USceneComponent::UpdateBounds()
```

`USceneComponent` is NOT transitively included by `GameFramework/Actor.h` — it must be
explicitly added. Evidence: `TargetResolution.h` in the same codebase explicitly includes
both `GameFramework/Actor.h` and `Components/SceneComponent.h` separately.

**Then add helper function (after the new include):**
```cpp
static void NotifyPropertyChanged(UObject* Object)
{
	if (!Object) return;

	// Handle component properties — trigger visual updates (bounds, wireframes)
	if (USceneComponent* SceneComp = Cast<USceneComponent>(Object))
	{
		SceneComp->MarkRenderStateDirty();
		SceneComp->UpdateBounds();
	}
	else if (UActorComponent* Component = Cast<UActorComponent>(Object))
	{
		Component->MarkRenderStateDirty();
	}

	// Handle actor properties — mark all components for visual refresh
	if (AActor* Actor = Cast<AActor>(Object))
	{
		Actor->MarkComponentsRenderStateDirty();
	}
}
```

**Modify the single-segment write at line 418.** Find:
```cpp
	// If this is the only segment, write directly
	if (Segments.Num() == 1)
	{
		return FPropertyAccessor::WriteProperty(Object, FirstProp, Value);
	}
```

**Replace with:**
```cpp
	// If this is the only segment, write directly
	if (Segments.Num() == 1)
	{
		bool bSuccess = FPropertyAccessor::WriteProperty(Object, FirstProp, Value);
		if (bSuccess)
		{
			NotifyPropertyChanged(Object);
		}
		return bSuccess;
	}
```

**Modify the nested path write at line 429.** Find:
```cpp
	return FPropertyAccessor::WritePropertyDirect(Resolution.ValuePtr, Resolution.FinalProperty, Value);
}
```

**Replace with:**
```cpp
	bool bSuccess = FPropertyAccessor::WritePropertyDirect(Resolution.ValuePtr, Resolution.FinalProperty, Value);
	if (bSuccess)
	{
		NotifyPropertyChanged(Object);
	}
	return bSuccess;
}
```

**Why this works:**
- At line 418: For direct property paths like `"BoxExtent"` on a component, `Object` IS the
  component. `MarkRenderStateDirty()` + `UpdateBounds()` triggers the visual refresh.
- At line 429: For nested paths, `Object` is the root object that owns the nested property.
- When `SetValue` recurses into a component (line 409), the recursive call's `Object` is the
  component, so the notification correctly targets the component.
- All required headers are already included (no `Editor.h` needed).

**Limitations:** `MarkRenderStateDirty()` handles visual updates (wireframes, bounds, render
state) but doesn't trigger all side-effects that `PostEditChangeProperty()` does (e.g.,
material parameter rebuilds, physics body recreation). This covers ~90% of use cases including
BoxExtent, colors, visibility, and transform-related properties.

**Effort:** Low (20 lines). **Risk:** Low — runtime-safe, no header issues.

#### Implementation — Option B: PostEditChangeProperty (Future Enhancement)

For full `PostEditChangeProperty()` support, the architecture needs a small refactor:
`SetValue()` must return which `UObject*` was actually modified (which may differ from the
input `Object` due to component recursion). Then CommandExecutor.cpp (in AgentBridgeScripting,
which CAN include `Editor.h` within `#if WITH_EDITOR`) can call `PostEditChangeProperty()`
on the correct target.

**This is deferred** because:
1. Option A solves the immediate BoxExtent problem
2. The refactor changes `SetValue()`'s return type (from `bool` to a struct), affecting all callers
3. CommandExecutor.cpp already has `#if WITH_EDITOR` blocks (lines 42-63) and `UnrealEd`
   dependency in its `.Build.cs` (line 31), so the header access is confirmed available

**If implementing later:** Change `SetValue` signature to return `FSetValueResult { bool bSuccess; UObject* ModifiedObject; }`, then in the CommandExecutor handler, call
`ModifiedObject->PostEditChangeProperty(Event)` within `#if WITH_EDITOR`.

**Current workaround:** Use `set_transform` on component scale with `world_space=true`.

---

### 4. `set_property` — Silent Failure on Type-Mismatched Object References

**Status:** Validated. Fix is correct. Minor addition: FSoftClassProperty uses `MetaClass`.

**Symptom:** Setting an object reference property to an asset of the wrong UClass reports
`success: true` but the value doesn't persist. `get_property` readback shows empty.

**Root Cause:** `WriteObjectProperty()` for `FSoftObjectProperty` (lines 868-874) accepts
any path without validating UClass compatibility. Compare with `FObjectProperty` (lines 972-975)
and `FClassProperty` (lines 933-938), which both DO validate types correctly.

#### Implementation

**File:** `Source/AgentBridgeCore/Private/PropertyAccessor.cpp`

**Fix A — FSoftObjectProperty (lines 868-874):**

**Find this code:**
```cpp
// Handle FSoftObjectProperty specially - it stores a path, not a loaded object
if (FSoftObjectProperty* SoftProp = CastField<FSoftObjectProperty>(Property))
{
    FSoftObjectPath SoftPath(Value.StringValue);
    FSoftObjectPtr SoftPtr(SoftPath);
    SoftProp->SetPropertyValue(ValuePtr, SoftPtr);
    return true;
}
```

**Replace with:**
```cpp
// Handle FSoftObjectProperty specially - it stores a path, not a loaded object
if (FSoftObjectProperty* SoftProp = CastField<FSoftObjectProperty>(Property))
{
    FSoftObjectPath SoftPath(Value.StringValue);

    // Validate the asset class matches the property's expected type
    if (!Value.StringValue.IsEmpty() && SoftProp->PropertyClass)
    {
        UObject* ResolvedObject = SoftPath.ResolveObject();
        if (!ResolvedObject)
        {
            ResolvedObject = SoftPath.TryLoad();
        }
        if (ResolvedObject && !ResolvedObject->IsA(SoftProp->PropertyClass))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("WriteObjectProperty: Asset '%s' (class %s) is not compatible with property type '%s'"),
                *Value.StringValue, *ResolvedObject->GetClass()->GetName(),
                *SoftProp->PropertyClass->GetName());
            return false;
        }
    }

    FSoftObjectPtr SoftPtr(SoftPath);
    SoftProp->SetPropertyValue(ValuePtr, SoftPtr);
    return true;
}
```

**Fix B — FSoftClassProperty (lines 877-883):**

**Find this code:**
```cpp
// Handle FSoftClassProperty similarly
if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
{
    FSoftObjectPath SoftPath(Value.StringValue);
    FSoftObjectPtr SoftPtr(SoftPath);
    SoftClassProp->SetPropertyValue(ValuePtr, SoftPtr);
    return true;
}
```

**Replace with:**
```cpp
// Handle FSoftClassProperty similarly
if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
{
    FSoftObjectPath SoftPath(Value.StringValue);

    // Validate the class matches the property's expected meta class
    // NOTE: FSoftClassProperty uses MetaClass, not PropertyClass
    if (!Value.StringValue.IsEmpty() && SoftClassProp->MetaClass)
    {
        UObject* LoadedClass = SoftPath.TryLoad();
        if (LoadedClass && !LoadedClass->IsA(SoftClassProp->MetaClass))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("WriteObjectProperty: Soft class '%s' is not a subclass of '%s'"),
                *Value.StringValue, *SoftClassProp->MetaClass->GetName());
            return false;
        }
    }

    FSoftObjectPtr SoftPtr(SoftPath);
    SoftClassProp->SetPropertyValue(ValuePtr, SoftPtr);
    return true;
}
```

**Key API difference:** `FSoftObjectProperty` uses `PropertyClass` while `FSoftClassProperty`
uses `MetaClass`. This matches the pattern in `FObjectProperty` vs `FClassProperty`.

**No additional headers needed.** `FSoftObjectPath::TryLoad()` is available via the existing
`#include "UObject/SoftObjectPath.h"` (line 5). `LogTemp` is acceptable — no custom log
category is defined in this module.

**For reference, here are the handlers that ALREADY validate correctly:**

| Property Type | Handler Lines | Type Check Field | Validated |
|---------------|---------------|------------------|-----------|
| `FObjectProperty` | 944-979 | `Property->PropertyClass` | `IsA()` at line 972 |
| `FClassProperty` | 887-942 | `ClassProp->MetaClass` | `IsChildOf()` at line 933 |
| `FSoftObjectProperty` | 868-874 | `SoftProp->PropertyClass` | **MISSING — fix above** |
| `FSoftClassProperty` | 877-883 | `SoftClassProp->MetaClass` | **MISSING — fix above** |

**Effort:** Medium (30 lines total for both handlers). **Risk:** Low — adds validation only.

**Verification:** After fix, test:
```python
set_property(actor_id="SomeBiomeTexture", path="BiomeTexture",
    value="/Game/SomeNonTexture.SomeNonTexture")
# Should return {"success": false} with type mismatch warning
```

---

## MEDIUM

### 5. `get_landscape_bounds` — Add Biome Volume Scale Factor

**Status:** Validated. All 4 file locations confirmed. Formula empirically validated.

**Requested addition:** Return a pre-computed `biome_volume_scale` field so agents can pass
it directly to `set_transform` without doing manual division.

**Formula:** `scale = [extent_x / 100, extent_y / 100, (extent_z + 10000) / 100]`
- `/ 100`: BoxComponent default half-extent is `(100, 100, 100)`, so `scale = extent / 100`
- `+ 10000` on Z: Provides ~5000 unit headroom above and below the landscape (empirically
  validated during PCG workflow testing, documented in AGENTS.md)

#### Implementation

**File 1: Proto** — `Source/AgentBridgeServer/Public/AgentBridge.proto`

The `Scale` message type already exists (lines 15-19) and is used by `ActorTransform` and
`SetTransformRequest`. Field 8 is the next available number.

**Find `GetLandscapeBoundsResponse` (lines 412-420):**
```protobuf
message GetLandscapeBoundsResponse {
  bool valid = 1;
  TempoScripting.Vector min = 2;
  TempoScripting.Vector max = 3;
  TempoScripting.Vector center = 4;
  TempoScripting.Vector extent = 5;
  int32 proxy_count = 6;
  string landscape_name = 7;
}
```

**Add field 8:**
```protobuf
message GetLandscapeBoundsResponse {
  bool valid = 1;
  TempoScripting.Vector min = 2;
  TempoScripting.Vector max = 3;
  TempoScripting.Vector center = 4;
  TempoScripting.Vector extent = 5;
  int32 proxy_count = 6;
  string landscape_name = 7;
  Scale biome_volume_scale = 8;  // Scale factor for BoxComponent to match landscape bounds
}
```

---

**File 2: C++ struct** — `Source/AgentBridgeRuntime/Public/WorldPartitionOps.h`

**Find `FLandscapeBounds` struct (lines 21-43). Add field after `Extent` (line 36):**
```cpp
	/** Half-extents (distance from center to edge) */
	FVector Extent = FVector::ZeroVector;

	/** Scale factor to make a 100-unit-extent BoxComponent match landscape bounds.
	 *  XY = Extent / 100. Z = (Extent.Z + 10000) / 100 for elevation headroom. */
	FVector BiomeVolumeScale = FVector::OneVector;

	/** Number of landscape proxies sampled */
	int32 ProxyCount = 0;
```

---

**File 3: Calculation** — `Source/AgentBridgeRuntime/Private/WorldPartitionOps.cpp`

**Find extent calculation (lines 525-530):**
```cpp
	Result.bValid = true;
	Result.Min = MinBounds;
	Result.Max = MaxBounds;
	Result.Center = (MinBounds + MaxBounds) * 0.5;
	Result.Extent = (MaxBounds - MinBounds) * 0.5;
	Result.ProxyCount = ProxyCount;
```

**Insert after `Result.Extent` line (before `Result.ProxyCount`):**
```cpp
	Result.Extent = (MaxBounds - MinBounds) * 0.5;

	// BoxComponent default half-extent is (100, 100, 100).
	// Scale factor: extent / 100.  Z gets +10000 for elevation headroom.
	Result.BiomeVolumeScale = FVector(
		Result.Extent.X / 100.0,
		Result.Extent.Y / 100.0,
		(Result.Extent.Z + 10000.0) / 100.0);

	Result.ProxyCount = ProxyCount;
```

---

**File 4: gRPC handler** — `Source/AgentBridgeServer/Private/AgentBridgeServiceSubsystem.cpp`

**Find `GetLandscapeBounds` handler (lines 1435-1472). Find where existing fields are
populated (after `landscape_name` at line 1468):**
```cpp
		Response.set_proxy_count(Bounds.ProxyCount);
		Response.set_landscape_name(TCHAR_TO_UTF8(*Bounds.LandscapeName));
```

**Add after `landscape_name`:**
```cpp
		Response.set_landscape_name(TCHAR_TO_UTF8(*Bounds.LandscapeName));

		auto* BiomeScale = Response.mutable_biome_volume_scale();
		BiomeScale->set_x(Bounds.BiomeVolumeScale.X);
		BiomeScale->set_y(Bounds.BiomeVolumeScale.Y);
		BiomeScale->set_z(Bounds.BiomeVolumeScale.Z);
```

This follows the exact same pattern used for `min`, `max`, `center`, `extent` above it.

---

**Proto regeneration required after proto change:**
```bash
cd <PROJECT_ROOT>/Plugins/Tempo/TempoCore/Scripts
./GenProtos.sh
```

Then rebuild the plugin (kill editor first):
```bash
cmd //c "taskkill /F /IM UnrealEditor.exe"
cd <PROJECT_ROOT>/Scripts && ./Build.sh
```

**Python MCP:** No changes needed — new proto field automatically serializes to Python after
regeneration. The MCP handler already returns all proto fields.

**Effort:** Low (4 files, 1-3 lines each + proto regen). **Risk:** None — purely additive.

---

### 6. `list_classes` — Cannot Find Plugin Blueprint Classes

**Status:** Validated. Recommended fix: add "Phase 1.5" using `FindClassByName()`.

**Symptom:** `list_classes(name_pattern="BP_PCGBiomeCore")` returns 0 results, but
`spawn_actor(class_name="BP_PCGBiomeCore")` succeeds.

**Root Cause:** Two-phase enumeration has gaps:

| Phase | Lines | Method | Limitation |
|-------|-------|--------|------------|
| Phase 1 | 1294-1335 | `TObjectIterator<UClass>` | Only finds **loaded** classes |
| Phase 2 | 1337-1403 | AssetRegistry query | Gated by `!Command.NamePattern.IsEmpty()` |

`spawn_actor` uses `FTypeDiscovery::FindClassByName()` (TypeDiscovery.cpp lines 12-71) which
has 4 fallback strategies:
1. `FindFirstObject<UClass>()` — fast lookup for loaded classes
2. Try with `_C` suffix — finds BPs by short name
3. `TObjectIterator<UClass>()` — brute-force loaded class search
4. **`LoadClass<UObject>()`** — explicitly loads unloaded plugin BPs (the key difference)

**Fix option analysis:**
- **Option 2 (remove Phase 2 gate):** REJECTED — performance hazard. Without the gate, every
  `list_classes()` call loads ALL Blueprint classes from AssetRegistry (thousands of assets).
- **Option 3 (GetDerivedClasses):** REJECTED — also only finds loaded classes, same limitation.
- **Option 1 (FindClassByName):** RECOMMENDED — leverages proven code path from `spawn_actor`.

#### Implementation

**File:** `Source/AgentBridgeScripting/Private/CommandExecutor.cpp`

**Command struct** (from `AgentCommands.h` lines 468-486):
```cpp
struct FListClassesCommand : FAgentCommandBase
{
    FString BaseClassName;    // Filter by base class (default empty = AActor)
    FString NamePattern;      // Wildcard pattern (default empty)
    bool bIncludeBlueprint = true;
    bool bIncludeAbstract = false;
    int32 Limit = 100;
};
```

**Existing code structure:**
- `BaseClass` is resolved from `Command.BaseClassName` at lines 1278-1292
- `AddedClassPaths` (TSet) tracks already-added class paths to prevent duplicates
- `Count` tracks result count against `Command.Limit`

**Find the end of Phase 1 (line 1335) — the closing brace of the `for (TObjectIterator...)` loop:**
```cpp
        Response.Classes.Add(Info);
        Count++;
    }

    // Phase 2: If requesting Blueprints and we have room, check AssetRegistry...
```

**Insert Phase 1.5 between Phase 1 and Phase 2 (after line 1335, before line 1337):**

```cpp
    // Phase 1.5: Try FindClassByName for exact pattern to load unloaded plugin classes
    // This mirrors what spawn_actor does — uses FTypeDiscovery which has LoadClass fallback
    if (!Command.NamePattern.IsEmpty() && Count < Command.Limit)
    {
        UClass* PatternClass = FTypeDiscovery::FindClassByName(Command.NamePattern);
        if (PatternClass && PatternClass->IsChildOf(BaseClass))
        {
            FString ClassPath = PatternClass->GetPathName();
            if (!AddedClassPaths.Contains(ClassPath))
            {
                bool bIsBP = PatternClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
                if ((Command.bIncludeBlueprint || !bIsBP) &&
                    (Command.bIncludeAbstract || !PatternClass->HasAnyClassFlags(CLASS_Abstract)))
                {
                    FClassInfo Info;
                    Info.ClassName = PatternClass->GetName();
                    Info.DisplayName = PatternClass->GetName();
                    Info.ClassPath = ClassPath;
                    Info.bIsBlueprint = bIsBP;
                    Info.bIsAbstract = PatternClass->HasAnyClassFlags(CLASS_Abstract);

                    if (PatternClass->GetSuperClass())
                    {
                        Info.ParentClassName = PatternClass->GetSuperClass()->GetName();
                    }

                    AddedClassPaths.Add(ClassPath);
                    Response.Classes.Add(Info);
                    Count++;
                }
            }
        }
    }
```

**Required header:** Verify `#include "TypeDiscovery.h"` is present in CommandExecutor.cpp
includes. If not, add it.

**Phase 2 remains unchanged** — the `!NamePattern.IsEmpty()` gate is intentional for performance.

**Effort:** Medium (25 lines). **Risk:** Low — reuses proven `FindClassByName()` code.

**Verification:** After fix, test:
```python
list_classes(name_pattern="BP_PCGBiomeCore")
# Should return the class (loaded via FindClassByName if not already loaded)
```

---

### 7. `set_property` on Struct Arrays — Array-Level Set Fails

**Status:** Validated. Root cause shared with Bug #8 — fix is in `ProtoPropertyValueToJson()`.

**Symptom:** Setting a struct array property at the array level fails, but element-level works.

```python
# FAILS:
set_property(path="BiomeAssets", value=[{"Generator": "/Engine/...", "Enabled": true}])

# WORKS:
set_property(path="BiomeAssets[0].Mesh", value="/Engine/BasicShapes/Cube.Cube")
```

**Root Cause:** Two interacting issues:

**Issue A — `ProtoPropertyValueToJson()` is missing 6 type cases** (see Bug #8 for full fix).
ARRAY values fall through to the `default` case which returns raw `string_value()`. The STRING
case wraps in quotes, corrupting array/struct JSON. `JsonToPropertyValue()` (the inverse, at
CommandExecutor.cpp lines 5012-5214) already handles all types correctly — only the proto→JSON
direction is broken.

**Issue B — `WriteStructProperty()` validation gate** at PropertyAccessor.cpp lines 994-997:
```cpp
if (Value.Type != EAgentPropertyType::Struct || Value.StructValue.Num() == 0)
{
    return false;
}
```
This is correct behavior — it requires elements to be parsed as STRUCT type with StructValue
members. The ProtoPropertyValueToJson fix ensures STRUCT values produce valid JSON objects
(`{...}`) which `JsonToPropertyValue()` correctly parses into `EAgentPropertyType::Struct`.

**Element-level access works because:** Path resolution (`AgentPropertyPath.cpp:649-693`)
parses `[0]` to get a direct pointer to the struct element, then writes individual fields
via `WriteProperty()` — bypasses all array/struct serialization.

**Fix:** See Bug #8 — the shared `ProtoPropertyValueToJson()` fix resolves both bugs.

---

## LOW

### 8. `set_property` on Object Ref Arrays — Requires JSON Array Format

**Status:** Validated with EXPANDED SCOPE. 6 missing cases found (not 3 as originally reported).

**Symptom:** Plain string fails for array-type object reference properties; JSON array format
`["/path"]` is required.

**Root Cause:** `ProtoPropertyValueToJson()` is missing switch cases for 6 of the 15 proto
`PropertyType` enum values:

| Proto Type | Enum Value | Status | Proto Field |
|------------|------------|--------|-------------|
| `PROPERTY_TYPE_BOOL` | 1 | Has case (line 584) | `bool_value` |
| `PROPERTY_TYPE_INT` | 2 | Has case (line 586) | `int_value` |
| `PROPERTY_TYPE_FLOAT` | 3 | Has case (line 588) | `float_value` |
| `PROPERTY_TYPE_STRING` | 4 | Has case (line 590) | `string_value` |
| `PROPERTY_TYPE_NAME` | 5 | Has case (line 591) | `string_value` |
| `PROPERTY_TYPE_VECTOR` | 6 | Has case (line 593) | `vector_value` |
| `PROPERTY_TYPE_ROTATOR` | 7 | Has case (line 596) | `rotation_value` |
| `PROPERTY_TYPE_TRANSFORM` | 8 | **MISSING** | `transform_value` |
| `PROPERTY_TYPE_COLOR` | 9 | Has case (line 599) | `color_value` |
| `PROPERTY_TYPE_OBJECT` | 10 | **MISSING** | `object_path` |
| `PROPERTY_TYPE_CLASS` | 11 | **MISSING** | `object_path` |
| `PROPERTY_TYPE_STRUCT` | 12 | **MISSING** | `struct_values` (repeated `PropertyKeyValue`) |
| `PROPERTY_TYPE_ARRAY` | 13 | **MISSING** | `array_values` (repeated `PropertyValue`) |
| `PROPERTY_TYPE_MAP` | 14 | **MISSING** | `struct_values` |
| `PROPERTY_TYPE_ENUM` | 15 | Falls to default | `enum_name` + `enum_value` |

**The inverse function `JsonToPropertyValue()`** (CommandExecutor.cpp lines 5012-5214) already
handles all type conversions correctly. Only the proto→JSON direction is broken.

#### Implementation

**File:** `Source/AgentBridgeServer/Private/AgentBridgeServiceSubsystem.cpp`

**Find `ProtoPropertyValueToJson()` (lines 580-606):**
```cpp
	// Convert proto PropertyValue to JSON string for CommandExecutor
	FString ProtoPropertyValueToJson(const PropertyValue& Value)
	{
		switch (Value.type())
		{
		case PROPERTY_TYPE_BOOL:
			return Value.bool_value() ? TEXT("true") : TEXT("false");
		case PROPERTY_TYPE_INT:
			return FString::Printf(TEXT("%lld"), Value.int_value());
		case PROPERTY_TYPE_FLOAT:
			return FString::Printf(TEXT("%f"), Value.float_value());
		case PROPERTY_TYPE_STRING:
		case PROPERTY_TYPE_NAME:
			return FString::Printf(TEXT("\"%s\""), UTF8_TO_TCHAR(Value.string_value().c_str()));
		case PROPERTY_TYPE_VECTOR:
			return FString::Printf(TEXT("{\"X\":%f,\"Y\":%f,\"Z\":%f}"),
				Value.vector_value().x(), Value.vector_value().y(), Value.vector_value().z());
		case PROPERTY_TYPE_ROTATOR:
			return FString::Printf(TEXT("{\"Pitch\":%f,\"Yaw\":%f,\"Roll\":%f}"),
				Value.rotation_value().p(), Value.rotation_value().y(), Value.rotation_value().r());
		case PROPERTY_TYPE_COLOR:
			return FString::Printf(TEXT("{\"r\":%f,\"g\":%f,\"b\":%f,\"a\":%f}"),
			 	Value.color_value().r() / 255.0, Value.color_value().g() / 255.0,
			 	Value.color_value().b() / 255.0, Value.color_value().a() / 255.0);
		default:
			return UTF8_TO_TCHAR(Value.string_value().c_str());
		}
	}
```

**Replace the entire function with:**
```cpp
	// Convert proto PropertyValue to JSON string for CommandExecutor
	FString ProtoPropertyValueToJson(const PropertyValue& Value)
	{
		switch (Value.type())
		{
		case PROPERTY_TYPE_BOOL:
			return Value.bool_value() ? TEXT("true") : TEXT("false");
		case PROPERTY_TYPE_INT:
			return FString::Printf(TEXT("%lld"), Value.int_value());
		case PROPERTY_TYPE_FLOAT:
			return FString::Printf(TEXT("%f"), Value.float_value());
		case PROPERTY_TYPE_STRING:
		case PROPERTY_TYPE_NAME:
			return FString::Printf(TEXT("\"%s\""), UTF8_TO_TCHAR(Value.string_value().c_str()));
		case PROPERTY_TYPE_VECTOR:
			return FString::Printf(TEXT("{\"X\":%f,\"Y\":%f,\"Z\":%f}"),
				Value.vector_value().x(), Value.vector_value().y(), Value.vector_value().z());
		case PROPERTY_TYPE_ROTATOR:
			return FString::Printf(TEXT("{\"Pitch\":%f,\"Yaw\":%f,\"Roll\":%f}"),
				Value.rotation_value().p(), Value.rotation_value().y(), Value.rotation_value().r());
		case PROPERTY_TYPE_TRANSFORM:
		{
			const auto& T = Value.transform_value();
			return FString::Printf(
				TEXT("{\"Location\":{\"X\":%f,\"Y\":%f,\"Z\":%f},"
				     "\"Rotation\":{\"Pitch\":%f,\"Yaw\":%f,\"Roll\":%f},"
				     "\"Scale\":{\"X\":%f,\"Y\":%f,\"Z\":%f}}"),
				T.location().x(), T.location().y(), T.location().z(),
				T.rotation().p(), T.rotation().y(), T.rotation().r(),
				T.scale().x(), T.scale().y(), T.scale().z());
		}
		case PROPERTY_TYPE_COLOR:
			return FString::Printf(TEXT("{\"r\":%f,\"g\":%f,\"b\":%f,\"a\":%f}"),
			 	Value.color_value().r() / 255.0, Value.color_value().g() / 255.0,
			 	Value.color_value().b() / 255.0, Value.color_value().a() / 255.0);
		case PROPERTY_TYPE_OBJECT:
		case PROPERTY_TYPE_CLASS:
			return FString::Printf(TEXT("\"%s\""), UTF8_TO_TCHAR(Value.object_path().c_str()));
		case PROPERTY_TYPE_STRUCT:
		case PROPERTY_TYPE_MAP:
		{
			FString Result = TEXT("{");
			bool bFirst = true;
			for (const auto& KV : Value.struct_values())
			{
				if (!bFirst) Result += TEXT(",");
				bFirst = false;
				Result += FString::Printf(TEXT("\"%s\":"), UTF8_TO_TCHAR(KV.key().c_str()));
				Result += ProtoPropertyValueToJson(KV.value());
			}
			Result += TEXT("}");
			return Result;
		}
		case PROPERTY_TYPE_ARRAY:
		{
			FString Result = TEXT("[");
			for (int32 i = 0; i < Value.array_values_size(); i++)
			{
				if (i > 0) Result += TEXT(",");
				Result += ProtoPropertyValueToJson(Value.array_values(i));
			}
			Result += TEXT("]");
			return Result;
		}
		case PROPERTY_TYPE_ENUM:
			return FString::Printf(TEXT("\"%s\""), UTF8_TO_TCHAR(Value.enum_name().c_str()));
		default:
			return UTF8_TO_TCHAR(Value.string_value().c_str());
		}
	}
```

**Proto field names verified against `PropertyValue` message (AgentBridge.proto lines 57-83):**
- `array_values` → `repeated PropertyValue array_values = 11;` (line 76)
- `object_path` → `string object_path = 10;` (line 73)
- `struct_values` → `repeated PropertyKeyValue struct_values = 12;` (line 78)
- `transform_value` → `ActorTransform transform_value = 8;` (line 69)
- `enum_name` → `string enum_name = 13;` (line 81)

---

**Also improve error message in CommandExecutor.cpp:**

**Find (line 746):**
```cpp
		Response.ErrorMessage = FString::Printf(TEXT("Failed to set path '%s'"), *Command.Path);
```

**Replace with:**
```cpp
		Response.ErrorMessage = FString::Printf(
			TEXT("Failed to set path '%s': value type mismatch or invalid format. "
			     "For arrays, use JSON array syntax: [\"value1\", \"value2\"]"),
			*Command.Path);
```

**Effort:** Medium (full function replacement, ~45 lines). **Risk:** Low — additive cases, existing cases unchanged.

**Verification:** After fix, test:
```python
# Bug 7 — struct array should work at array level:
set_property(actor_id="MyAsset", path="BiomeAssets",
    value=[{"Generator": "/Engine/BasicShapes/Cube.Cube", "Enabled": true}])

# Bug 8 — object ref array with plain string:
set_property(actor_id="MyAsset", path="Assets", value=["/Game/Path.Path"])
```
