# AgentBridge Session Handover

> Last Updated: January 1, 2026

## Current State

AgentBridge is **feature-complete** with:
- 38 gRPC RPCs
- 90 MCP tools across 12 services
- Self-documenting help system
- World Partition support
- PIE/Runtime support

## Consolidated Todos

### Priority 1: Top Priority

| Task | Module | Effort | Status |
|------|--------|--------|--------|
| **Fix nested BP struct writes** | Scripting | 4hr | **TOP PRIORITY** |
| Blueprint class normalization (auto-add `_C`) | Scripting | 2hr | Pending |
| Unified property setter (auto-detect type) | Scripting | 4hr | Pending |
| UObject property access (DataAssets, Materials) | Core/Runtime | 4hr | Pending |

### Priority 2: Known Issues to Fix

| Issue | Module | Notes |
|-------|--------|-------|
| TSoftObjectPtr assignment | Scripting | Use TObjectPtr properties as workaround |
| Function parameters in `tempo_call_function` | Server | Tempo proto limitation, use `call_static_function` |

### Priority 3: Stretch Goals

| Feature | Module | Effort | Notes |
|---------|--------|--------|-------|
| Blueprint graph editing | Scripting | Very High | Competitor parity |
| Niagara particle control | Scripting | High | Similar to PCG pattern |
| Sequencer control | Scripting | High | Complex API surface |
| INI/Config automation | Scripting | Medium | Read/write DefaultEngine.ini |
| Landscape sculpting | Runtime | High | Import/export heightmaps |
| Sound capture | Python | Medium | TempoAudio integration |
| Standalone gRPC server | Server | High | Remove Tempo dependency |

### Won't Fix (By Design)

- **FunctionInvoker return values**: Returns default values for structs - use property queries
- **TSoftObjectPtr direct assignment**: Complex UE limitation - use TObjectPtr properties

## Recent Sessions Summary

### Session 18 (Most Recent)
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

*Documentation reorganized January 1, 2026*
*Per-module CLAUDE.md files now contain module-specific context*
