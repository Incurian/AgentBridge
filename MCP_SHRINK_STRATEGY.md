# MCP Tool Size Reduction Strategy

> **Goal:** Reduce MCP tool definition size by ~75% while maintaining full functionality
>
> **Current State:** 116 tools, ~69,600 tokens in context (~600 tokens/tool)

---

## Executive Summary

The MCP tool definitions are consuming significant context window space. This strategy proposes three complementary approaches to achieve a ~75% reduction:

| Approach | Reduction | Effort | Risk |
|----------|-----------|--------|------|
| **1. Modular Loading** | 25-50% of loaded tools | Low | Low |
| **2. Tool Consolidation** | 40-60% of tool count | Medium | Medium |
| **3. Description Compression** | 50-70% of token count | Low | Low |

**Combined effect:** A user who only needs core editing capabilities could see ~25 tools instead of 104, with each tool using ~60% fewer tokens.

---

## Current Tool Inventory

### AgentBridge Service (49 tools)

| Category | Tools | Consolidation Opportunity |
|----------|-------|---------------------------|
| Help | 1 | Keep as-is |
| World | 2 | Keep as-is |
| Actor Discovery | 2 | Merge with World Partition queries |
| Actor Manipulation | 4 | Keep as-is |
| Property Operations | 2 | Keep as-is (core) |
| Type Discovery | 4 | Keep as-is |
| World Partition | 7 | Merge into 2 tools |
| Console Commands | 2 | Keep as-is |
| Asset Operations | 4 | Merge into 1 tool |
| Component Operations | 6 | Merge into 2 tools |
| File Operations | 4 | Merge into 1 tool |
| **Blueprint Nodes** | 6 | Keep as-is (well-designed) |
| **PCG Graph** | 6 | Keep as-is (well-designed) |

### Tempo Services (53 tools across 11 files)

| Module | Tools | Consolidation Opportunity |
|--------|-------|---------------------------|
| Time | 6 | Merge into 1-2 tools |
| Actor Control | 17 | **HIGH** - 9 typed setters → 1 |
| Core | 6 | Keep as-is |
| Core Editor | 6 | Merge into 2 tools |
| Geographic | 5 | Merge into 2 tools |
| Movement | 5 | Keep as-is |
| World State | 2 | Keep as-is |
| Labels | 1 | Keep as-is |
| Sensors | 1 | Keep as-is |
| Map Query | 3 | Keep as-is |
| Agents Editor | 1 | Keep as-is |

### bp_toolkit (14 tools - already optional)

Already modular - only loads when submodule present. Could further consolidate into 4-5 tools.

---

## Approach 1: Modular Loading (25-50% reduction)

### Current State

The service registry in `services/__init__.py` loads ALL service modules on startup. Even though Tempo modules are separate files, they're all registered unconditionally.

### Proposed Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      ALWAYS LOADED                          │
│  Core: help, list_worlds, set_target_world                  │
│  Actors: query_actors, get_actor, spawn_actor, delete_actor │
│  Properties: get_property, set_property                     │
│  Types: list_classes, get_class_schema                      │
│                         (12 tools)                          │
├─────────────────────────────────────────────────────────────┤
│                    LOAD ON DEMAND                           │
├──────────────────────┬──────────────────────────────────────┤
│ simulation           │ tempo_play, tempo_pause, tempo_step, │
│                      │ tempo_set_time_mode, tempo_set_rate  │
├──────────────────────┼──────────────────────────────────────┤
│ world_partition      │ query_all_actors, streaming_state,   │
│                      │ landscape, data_layers               │
├──────────────────────┼──────────────────────────────────────┤
│ assets               │ create_asset, save_asset, duplicate  │
├──────────────────────┼──────────────────────────────────────┤
│ components           │ component transforms, attach/detach  │
├──────────────────────┼──────────────────────────────────────┤
│ files                │ read, write, list, copy              │
├──────────────────────┼──────────────────────────────────────┤
│ tempo_actor_control  │ spawn, destroy, components, props    │
├──────────────────────┼──────────────────────────────────────┤
│ tempo_geographic     │ date, time, coordinates              │
├──────────────────────┼──────────────────────────────────────┤
│ tempo_movement       │ vehicles, pawns, navigation          │
├──────────────────────┼──────────────────────────────────────┤
│ tempo_editor         │ PIE, simulate, save, load levels     │
├──────────────────────┼──────────────────────────────────────┤
│ bp_toolkit           │ (already optional)                   │
└──────────────────────┴──────────────────────────────────────┘
```

### Implementation

**Option A: MCP Configuration (Recommended)**

Expose module selection in the MCP config:

```json
{
  "mcpServers": {
    "agentbridge": {
      "command": "python",
      "args": ["-m", "mcp", "--modules", "core,simulation,assets"],
      ...
    }
  }
}
```

**Option B: Environment Variable**

```bash
AGENTBRIDGE_MODULES=core,simulation,assets python -m mcp
```

**Option C: Profiles**

Predefined profiles for common use cases:

| Profile | Modules Loaded | Tool Count |
|---------|----------------|------------|
| `minimal` | core | ~12 |
| `editor` | core, assets, components, world_partition | ~30 |
| `simulation` | core, simulation, tempo_actor_control | ~25 |
| `full` | all | ~104 |

---

## Approach 2: Tool Consolidation (40-60% reduction)

### High-Impact Consolidations

#### 2.1 Unified Property Setter (9 → 1 tools)

**Current:** 9 separate tools with type in the name
```
tempo_set_float_property
tempo_set_int_property
tempo_set_bool_property
tempo_set_string_property
tempo_set_vector_property
tempo_set_rotator_property
tempo_set_color_property
tempo_set_asset_property
tempo_set_actor_transform (partial)
```

**Proposed:** 1 tool with auto-detection
```python
{
    "name": "tempo_set_property",
    "description": "Set any property on an actor/component. Auto-detects type.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "actor": {"type": "string"},
            "property": {"type": "string"},
            "value": {},  # Any type - auto-detected
            "component": {"type": "string"}
        },
        "required": ["actor", "property", "value"]
    }
}
```

The implementation detects the value type:
- `5000` → int
- `5000.0` → float
- `true/false` → bool
- `"string"` → string
- `[1, 2, 3]` → vector (3) or color (4)
- `{"r": 255, "g": 0, "b": 0}` → color (named)
- `"/Game/..."` → asset path

**Savings:** 8 tool definitions eliminated (~2,400 tokens)

#### 2.2 Unified Query Tool (3 → 1 tools)

**Current:**
```
query_actors          - loaded actors only
query_all_actors      - includes unloaded (World Partition)
get_actors_in_data_layer
```

**Proposed:**
```python
{
    "name": "query_actors",
    "description": "Find actors. Use include_unloaded for streaming actors.",
    "inputSchema": {
        "properties": {
            "class_name": {"type": "string"},
            "pattern": {"type": "string"},  # Searches name, label
            "tag": {"type": "string"},
            "data_layer": {"type": "string"},
            "include_unloaded": {"type": "boolean", "default": false},
            "limit": {"type": "integer", "default": 100}
        }
    }
}
```

**Savings:** 2 tool definitions (~800 tokens)

#### 2.3 Unified File Operations (4 → 1 tools)

**Current:**
```
read_project_file
write_project_file
list_project_directory
copy_project_file
```

**Proposed:**
```python
{
    "name": "project_file",
    "description": "File operations within project directory.",
    "inputSchema": {
        "properties": {
            "action": {"enum": ["read", "write", "list", "copy"]},
            "path": {"type": "string"},
            "content": {"type": "string"},  # for write
            "dest": {"type": "string"},     # for copy
            "pattern": {"type": "string"},  # for list
            "recursive": {"type": "boolean"}
        },
        "required": ["action", "path"]
    }
}
```

**Savings:** 3 tool definitions (~1,200 tokens)

#### 2.4 Unified Asset Operations (4 → 1 tools)

**Current:**
```
create_asset
save_asset
duplicate_asset
save_actor_as_blueprint
```

**Proposed:**
```python
{
    "name": "asset",
    "description": "Asset operations: create, save, duplicate, or convert actor to BP.",
    "inputSchema": {
        "properties": {
            "action": {"enum": ["create", "save", "duplicate", "actor_to_bp"]},
            "path": {"type": "string"},        # Asset/package path
            "class": {"type": "string"},       # For create
            "name": {"type": "string"},        # Asset/blueprint name
            "source": {"type": "string"},      # For duplicate (source path)
            "actor_id": {"type": "string"}     # For actor_to_bp
        },
        "required": ["action"]
    }
}
```

**Savings:** 3 tool definitions (~1,200 tokens)

#### 2.5 Unified Component Operations (6 → 2 tools)

**Current:**
```
get_component_transform
set_component_transform
attach_actor
detach_actor
attach_component
detach_component
```

**Proposed:**
```python
# Tool 1: Component transforms
{
    "name": "component_transform",
    "description": "Get or set component transform.",
    "inputSchema": {
        "properties": {
            "actor_id": {"type": "string"},
            "component": {"type": "string"},
            "location": {"type": "array"},    # Set if provided
            "rotation": {"type": "array"},
            "scale": {"type": "array"},
            "world_space": {"type": "boolean", "default": true}
        }
    }
}

# Tool 2: Attachment operations
{
    "name": "attach",
    "description": "Attach/detach actors or components.",
    "inputSchema": {
        "properties": {
            "action": {"enum": ["attach", "detach"]},
            "actor_id": {"type": "string"},
            "parent_actor_id": {"type": "string"},  # For attach
            "component": {"type": "string"},        # Optional
            "parent_component": {"type": "string"}, # Optional
            "keep_world_transform": {"type": "boolean", "default": true}
        }
    }
}
```

**Savings:** 4 tool definitions (~1,600 tokens)

#### 2.6 Unified Simulation Control (6 → 1 tool)

**Current:**
```
tempo_play
tempo_pause
tempo_step
tempo_advance_steps
tempo_set_time_mode
tempo_set_sim_rate
```

**Proposed:**
```python
{
    "name": "simulation",
    "description": "Control simulation playback and timing.",
    "inputSchema": {
        "properties": {
            "action": {"enum": ["play", "pause", "step"]},
            "steps": {"type": "integer"},           # For step action
            "time_mode": {"enum": ["wall_clock", "fixed_step"]},
            "rate": {"type": "number"}              # Steps per second
        }
    }
}
```

**Savings:** 5 tool definitions (~1,500 tokens)

#### 2.7 Unified Editor Control (6 → 1 tool)

**Current:**
```
tempo_play_in_editor
tempo_simulate
tempo_stop
tempo_save_level
tempo_open_level
tempo_new_level
```

**Proposed:**
```python
{
    "name": "editor",
    "description": "Editor session and level operations.",
    "inputSchema": {
        "properties": {
            "action": {"enum": ["pie", "simulate", "stop", "save", "open", "new"]},
            "path": {"type": "string"}  # For save/open
        }
    }
}
```

**Savings:** 5 tool definitions (~1,500 tokens)

#### 2.8 bp_toolkit Consolidation (14 → 5 tools)

**Proposed groupings:**

| New Tool | Merges |
|----------|--------|
| `bp_export` | bp_export_asset, bp_import_asset |
| `bp_info` | bp_detect_type, bp_get_info, bp_list_properties |
| `bp_property` | bp_get_property, bp_set_property |
| `bp_asset` | bp_clone_asset, bp_list_graphs |
| `bp_edit` | bp_add_comment, bp_clone_node, bp_find, bp_query, bp_parse |

**Savings:** 9 tool definitions (~2,000 tokens)

### Consolidation Summary

| Consolidation | Before | After | Savings |
|---------------|--------|-------|---------|
| Property setters | 9 | 1 | 8 tools |
| Query tools | 3 | 1 | 2 tools |
| File operations | 4 | 1 | 3 tools |
| Asset operations | 4 | 1 | 3 tools |
| Component ops | 6 | 2 | 4 tools |
| Simulation control | 6 | 1 | 5 tools |
| Editor control | 6 | 1 | 5 tools |
| bp_toolkit | 14 | 5 | 9 tools |
| **Total** | **52** | **13** | **39 tools** |

Remaining tools untouched: 52 (these are already appropriately scoped)

**Final tool count:** 104 → ~65 tools (37% reduction)

---

## Approach 3: Description Compression (50-70% reduction)

### Current Pattern (Verbose)

```python
{
    "name": "tempo_set_float_property",
    "description": "Set a float property on an actor or component (e.g., Intensity, Radius).",
    "inputSchema": {
        "type": "object",
        "properties": {
            "actor": {
                "type": "string",
                "description": "Actor name",
            },
            "property": {
                "type": "string",
                "description": "Property name",
            },
            "value": {
                "type": "number",
                "description": "Float value",
            },
            "component": {
                "type": "string",
                "description": "Component name (optional)",
            },
        },
        "required": ["actor", "property", "value"],
    },
}
```

**Token count:** ~85 tokens

### Proposed Pattern (Terse)

```python
{
    "name": "tempo_set_float_property",
    "description": "Set float property on actor/component.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "actor": {"type": "string"},
            "property": {"type": "string"},
            "value": {"type": "number"},
            "component": {"type": "string"}
        },
        "required": ["actor", "property", "value"]
    }
}
```

**Token count:** ~45 tokens (47% reduction)

### Compression Guidelines

1. **Descriptions:** Remove examples, reduce to action + target
   - Before: `"Get a property value from an actor using a property path. Supports nested properties like 'RootComponent.RelativeLocation.X'."`
   - After: `"Get property by path from actor."`

2. **Parameter descriptions:** Remove entirely unless non-obvious
   - Standard params (`actor_id`, `path`, `value`) are self-documenting
   - Only describe unusual semantics

3. **Rely on help():** Move examples and detailed docs to help system
   - The `help(topic="properties")` tool provides detailed guidance
   - Tool descriptions just need to be recognizable

4. **Use shared schemas:** Define common parameter patterns once
   ```python
   ACTOR_PARAM = {"actor_id": {"type": "string"}}
   TRANSFORM_PARAMS = {
       "location": {"type": "array", "items": {"type": "number"}},
       "rotation": {"type": "array", "items": {"type": "number"}},
       "scale": {"type": "array", "items": {"type": "number"}}
   }
   ```

### Before/After Examples

#### spawn_actor

**Before (24 lines, ~180 tokens):**
```python
{
    "name": "spawn_actor",
    "description": "Spawn a new actor in the world. Specify the class and optionally the transform and properties.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "class_name": {
                "type": "string",
                "description": "Class to spawn (e.g., 'PointLight', 'StaticMeshActor', '/Game/BP_MyActor.BP_MyActor_C')",
            },
            "location": {
                "type": "array",
                "items": {"type": "number"},
                "description": "World location [X, Y, Z] in Unreal units (cm)",
                "minItems": 3,
                "maxItems": 3,
            },
            # ... 3 more parameters with full descriptions
        },
        "required": ["class_name"],
    },
}
```

**After (12 lines, ~70 tokens):**
```python
{
    "name": "spawn_actor",
    "description": "Spawn actor by class. Use help('actors') for examples.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "class_name": {"type": "string"},
            "location": {"type": "array"},
            "rotation": {"type": "array"},
            "scale": {"type": "array"},
            "label": {"type": "string"},
            "folder_path": {"type": "string"}
        },
        "required": ["class_name"]
    }
}
```

**Reduction:** 61%

---

## Combined Impact Analysis

### Scenario: Full Application

| Approach | Tool Count | Tokens/Tool | Total Tokens |
|----------|------------|-------------|--------------|
| **Current** | 104 | ~90 | ~9,360 |
| + Modular (editor profile) | 30 | ~90 | ~2,700 |
| + Consolidation | 22 | ~90 | ~1,980 |
| + Compression | 22 | ~40 | ~880 |

**Reduction:** 9,360 → 880 = **91% reduction** for editor profile

### Scenario: Minimal Application

| Approach | Tool Count | Tokens/Tool | Total Tokens |
|----------|------------|-------------|--------------|
| **Current** | 104 | ~90 | ~9,360 |
| + Modular (minimal) | 12 | ~90 | ~1,080 |
| + Compression | 12 | ~40 | ~480 |

**Reduction:** 9,360 → 480 = **95% reduction** for minimal profile

### Scenario: Full Tools, Just Compression

| Approach | Tool Count | Tokens/Tool | Total Tokens |
|----------|------------|-------------|--------------|
| **Current** | 104 | ~90 | ~9,360 |
| + Consolidation | 65 | ~90 | ~5,850 |
| + Compression | 65 | ~40 | ~2,600 |

**Reduction:** 9,360 → 2,600 = **72% reduction** maintaining all functionality

---

## Implementation Roadmap

### Phase 1: Description Compression (1-2 days)
- Low risk, immediate impact
- Can be done incrementally
- No API changes

**Tasks:**
1. Create shared schema definitions
2. Compress all tool descriptions
3. Update help() topics to compensate
4. Test with Claude Code

### Phase 2: Tool Consolidation (3-5 days)
- Medium risk, requires testing
- Breaking change for direct API users
- Significant reduction in cognitive load

**Tasks:**
1. Implement unified property setter with type detection
2. Consolidate file/asset/component operations
3. Add migration layer for old tool names (optional)
4. Update documentation

### Phase 3: Modular Loading (2-3 days)
- Low risk if done carefully
- Provides flexibility for different use cases
- Enables future scaling

**Tasks:**
1. Implement module selection in service registry
2. Add CLI arguments for module selection
3. Create profile presets
4. Update MCP config documentation

---

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| Consolidated tools harder to discover | Keep help() comprehensive; add examples |
| Type detection fails edge cases | Explicit type hints as fallback parameter |
| Modular loading breaks workflows | Default to "full" profile; document profiles |
| Terse descriptions reduce usability | LLMs handle terse well; help() for details |

---

## Appendix: Token Counting Methodology

Tokens estimated using cl100k_base tokenizer (GPT-4/Claude):
- Average tool definition: ~90 tokens (current)
- Average parameter description: ~12 tokens
- Average tool name: ~3 tokens
- JSON structure overhead: ~15 tokens per tool

Sample counts from current codebase:
- `spawn_actor`: 184 tokens
- `tempo_set_float_property`: 86 tokens
- `help`: 48 tokens
- `query_actors`: 156 tokens

---

## Recommendation

**For immediate impact with minimal risk:** Implement Phase 1 (compression) first. This alone achieves ~50% reduction with no API changes.

**For maximum reduction:** Implement all three phases, prioritizing:
1. Compression (quick win)
2. Consolidation (biggest structural improvement)
3. Modular loading (future-proofing)

The target of 75% reduction is achievable with Phase 1 + Phase 2, or with all three phases for larger projects.
