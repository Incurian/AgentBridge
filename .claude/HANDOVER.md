# AgentBridge Session Handover

> Last Updated: January 1, 2026 (Session 20)

## Current State

AgentBridge is **feature-complete** with:
- 38 gRPC RPCs
- 90 MCP tools across 12 services
- Self-documenting help system
- World Partition support
- PIE/Runtime support
- **Nested BP struct writes now working!**
- **UObject property access (DataAssets, Materials) now working!**
- **Component property paths now fully working (GET and SET)!**

## Completed Items

### Session 20 (Current)

| Task | Status | Notes |
|------|--------|-------|
| **Fix nested property SET** | ✅ DONE | Root cause: `IsPropertyWritable()` was blocking `BlueprintReadOnly` properties |
| **Component path GET/SET** | ✅ DONE | `LightComponent0.Intensity`, `RootComponent.RelativeLocation` all work |

**The Fix:** Removed `CPF_BlueprintReadOnly` check from `IsPropertyWritable()`.
- `BlueprintReadOnly` only prevents Blueprint scripts from writing
- C++ and Editor code (like AgentBridge) can still modify these properties
- This was a single-line change in `PropertyAccessor.cpp` with huge impact

### Session 19

| Task | Status | Notes |
|------|--------|-------|
| **Fix nested BP struct writes** | ✅ DONE | Added `WritePropertyDirect()` for pre-resolved value pointers |
| **UObject property access** | ✅ DONE | Added `ResolveObject()` - works on actors AND assets |
| **Component path fallback** | ✅ DONE | Added `FindComponentByName()` in AgentPropertyPath.cpp |
| Blueprint class normalization | ✅ DONE | Already implemented in `FindClassByName()` |
| Documentation reorganization | ✅ DONE | Per-module CLAUDE.md files |

### Remaining Stretch Goals

| Feature | Module | Effort | Notes |
|---------|--------|--------|-------|
| Blueprint graph editing | Scripting | Very High | Competitor parity |
| Niagara particle control | Scripting | High | Similar to PCG pattern |
| Sequencer control | Scripting | High | Complex API surface |
| INI/Config automation | Scripting | Medium | Read/write DefaultEngine.ini |
| Standalone gRPC server | Server | High | Remove Tempo dependency |

### Known Limitations

| Issue | Status | Workaround |
|-------|--------|------------|
| TSoftObjectPtr assignment | Won't fix | Use TObjectPtr properties |
| tempo_call_function params | Won't fix | Use `call_static_function` |
| FunctionInvoker struct returns | **AUTO-FIXED** | Transparent fallback to property access |

**FunctionInvoker struct returns - auto-fixed!**
- Common getter functions (K2_GetActorLocation, K2_GetActorRotation, GetActorScale3D, etc.)
  are automatically redirected to property access under the hood
- Users call the function, get correct results - no workarounds needed
- Following "tools should just work" philosophy

---

## Session 20 Technical Details

### Nested Property SET Fix

**Root Cause:** `IsPropertyWritable()` was checking for `CPF_BlueprintReadOnly` flag and rejecting writes.

```cpp
// OLD - Too restrictive:
const EPropertyFlags ReadOnlyFlags = CPF_BlueprintReadOnly | CPF_EditConst;

// NEW - Only block truly read-only properties:
const EPropertyFlags ReadOnlyFlags = CPF_EditConst;
```

**Key Insight:** `CPF_BlueprintReadOnly` prevents Blueprint scripts from writing, but NOT C++/Editor code. Since AgentBridge acts as editor automation, we should allow writing to these properties.

**Verified Working:**
- `LightComponent0.Intensity` - GET and SET ✅
- `LightComponent.Intensity` (partial name) - GET and SET ✅
- `RootComponent.RelativeLocation` - GET and SET ✅
- `LightComponent0.AttenuationRadius` - GET and SET ✅

Files changed:
- `PropertyAccessor.cpp` - Removed `CPF_BlueprintReadOnly` from `IsPropertyWritable()`

## Session 19 Technical Details

### Nested Struct Write Fix

The bug was a container/value pointer confusion:
- `ResolveSegments()` returns direct value pointer for nested paths
- `WriteProperty()` was calling `ContainerPtrToValuePtr()` again
- Added `WritePropertyDirect()` that skips the offset calculation

Files changed:
- `AgentPropertyPath.cpp` - Use `WritePropertyDirect()` for resolved paths
- `PropertyAccessor.cpp` - Add `WritePropertyDirect()`, refactor helpers
- `PropertyAccessor.h` - Document new API

### Component Path Resolution

Added `FindComponentByName()` helper to handle paths like `LightComponent0.Intensity`:
- Tries exact match first
- Falls back to case-insensitive match
- Falls back to partial match (e.g., "LightComponent" matches "LightComponent0")

Files changed:
- `AgentPropertyPath.cpp` - Added `FindComponentByName()`, updated `GetValue()`/`SetValue()`

### UObject Property Access

Following "tools should just work" philosophy, property commands now auto-detect whether
the target is an actor or an asset:

```cpp
// ResolveObject() in CommandExecutor.cpp:
// 1. First tries actor resolution (most common case)
// 2. Falls back to asset path loading if that fails
// 3. Auto-appends ".AssetName" suffix if path is missing it
```

## Recent Sessions Summary

### Session 18
- Documentation reorganization to per-module CLAUDE.md files
- Consolidated todos from all improvement docs

### Session 17
- Completed all Priority 1/2/3 wishlist items
- Added `get_landscape_bounds` with accurate calculation
- Added `label_pattern` parameter to `query_actors`
- Added flexible value formats (hex colors, named colors, etc.)
- Enhanced error messages with property type information

### Session 16
- PCG Biome workflow testing - successful end-to-end mesh generation
- Identified TSoftObjectPtr and nested struct write limitations
- Landscape bounds fix applied to WorldPartitionOps.cpp

### Session 15
- Static function support (`call_static_function`)
- Full class schema for non-Actor classes
- Fixed `get_actor` to include properties/components

## Quick Resume Checklist

1. **Check editor is running** with TempoSample project
2. **Test connectivity**:
   ```bash
   PYTHONPATH="D:/tempo/TempoSample/Plugins/Tempo/TempoCore/Content/Python/API/tempo" \
     D:/tempo/TempoSample/TempoEnv/Scripts/python.exe -c \
     "from mcp.services.agentbridge import connect; c = connect('localhost', 10001); print(c.list_worlds())"
   ```
3. **Check build status**: `git status` for uncommitted changes

## File Locations Quick Reference

| What | Path |
|------|------|
| Root CLAUDE.md | `CLAUDE.md` |
| Core Module | `Source/AgentBridgeCore/CLAUDE.md` |
| Runtime Module | `Source/AgentBridgeRuntime/CLAUDE.md` |
| Scripting Module | `Source/AgentBridgeScripting/CLAUDE.md` |
| Server Module | `Source/AgentBridgeServer/CLAUDE.md` |
| Python/MCP | `Python/CLAUDE.md` |
| Old Docs (Backup) | `.old.claude/` |

## Key Workflows (Reference)

### PCG Biome Setup
1. `get_landscape_bounds()` - Get world-space bounds
2. `spawn_actor(class_name="/PCGBiomeCore/Blueprints/BP_PCGBiomeCore.BP_PCGBiomeCore_C")`
3. `set_actor_transform()` - Scale to cover landscape
4. `tempo_set_asset_property()` - Assign Definition and Assets[0]
5. `execute_console_command("pcg.GenerateAll")` - Trigger regeneration

### Testing New Features
```bash
cd D:/tempo/TempoSample/Plugins/AgentBridge/Python
PYTHONPATH="D:/tempo/TempoSample/Plugins/Tempo/TempoCore/Content/Python/API/tempo" \
  D:/tempo/TempoSample/TempoEnv/Scripts/python.exe test_wishlist.py
```

---

*Session 20: Fixed nested property SET (BlueprintReadOnly flag issue)*
*All component and nested paths now fully working!*
