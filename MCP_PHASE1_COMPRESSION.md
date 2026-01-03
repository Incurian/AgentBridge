# Phase 1: Token Bloat Investigation & Description Compression

> **Goal:** Understand why tools consume ~600 tokens each, then reduce through compression

---

## Token Bloat Investigation

### Observed Problem

Claude Code's `/mcp` command reports each tool consuming approximately **600 tokens** in context.
With 116 tools, this consumes **~69,600 tokens** - a significant portion of context window.

### Expected vs Actual

| Metric | Expected | Actual | Ratio |
|--------|----------|--------|-------|
| Tokens per tool | ~90-120 | ~600 | 5-6x |
| Total for 116 tools | ~11,600 | ~69,600 | 6x |

### Sources of Token Usage

#### 1. JSON Schema Verbosity (Primary Contributor)

Each tool definition includes:

```python
{
    "name": "tool_name",                    # ~3 tokens
    "description": "...",                   # ~20-50 tokens
    "inputSchema": {
        "type": "object",                   # ~4 tokens
        "properties": {
            "param1": {
                "type": "string",           # ~4 tokens
                "description": "...",       # ~15-30 tokens per param
                "default": "...",           # ~4 tokens
            },
            # Multiply by N parameters
        },
        "required": ["..."],               # ~5 tokens
    },
}
```

For a tool with 6 parameters like `query_actors`:
- Structure overhead: ~25 tokens
- Description: ~35 tokens
- 6 parameters × ~25 tokens each: ~150 tokens
- **Subtotal: ~210 tokens**

Still doesn't explain 600 tokens per tool.

#### 2. Claude Code MCP Wrapper (Likely Contributor)

Claude Code adds its own system prompt context around MCP tools. Based on MCP protocol
and Claude Code's implementation, each tool likely gets wrapped with:

```
<tool>
<name>query_actors</name>
<description>Search for actors in the current world...</description>
<input_schema>
{
  "type": "object",
  "properties": {
    ...full JSON...
  }
}
</input_schema>
</tool>
```

This XML wrapper adds:
- Opening/closing tags: ~15 tokens
- Indentation and formatting: ~20-30 tokens
- JSON representation of schema: ~150-200 tokens
- **Wrapper total: ~200-250 tokens**

#### 3. Parameter Descriptions (High Impact)

Current descriptions are verbose with examples:

```python
"description": "Substring pattern for internal actor name (e.g., 'Light', 'Door'). "
               "Matches GetName() - the internal unique identifier."
```

This single parameter description: **~30 tokens**

With 6 parameters × 30 tokens = **180 tokens just in parameter descriptions**

#### 4. Repeated Schema Patterns

Array parameters are particularly verbose:

```python
"location": {
    "type": "array",
    "items": {"type": "number"},
    "description": "World location [X, Y, Z] in Unreal units (cm)",
    "minItems": 3,
    "maxItems": 3,
}
```

This pattern appears 3 times per tool with transforms = **~75 tokens** repeated unnecessarily.

### Token Breakdown Estimate (query_actors)

| Component | Tokens |
|-----------|--------|
| Tool name | 3 |
| Tool description | 35 |
| Schema structure | 25 |
| 6 parameter descriptions | 150 |
| 6 parameter type/default | 35 |
| JSON punctuation/keys | 40 |
| Claude Code wrapper | 100 |
| Formatting/whitespace | 50 |
| **Subtotal** | **~440 tokens** |

For tools with more parameters (like `spawn_actor`, `attach_actor`), add ~50-100 more tokens.

**Conclusion:** 600 tokens is believable for complex tools. Simple tools (like `help`, `list_worlds`)
are probably ~150-200 tokens, bringing the average to ~400-500.

---

## Compression Strategy

### Principle: Rely on help() for Details

The `help()` tool provides comprehensive documentation. Tool descriptions only need to be:
1. **Recognizable** - LLM can identify correct tool
2. **Disambiguating** - Distinguish from similar tools
3. **Minimal** - No examples, no edge cases

### Compression Rules

1. **Tool descriptions:** 1 sentence, no examples
2. **Parameter descriptions:** Remove entirely if self-evident
3. **Examples:** Remove (use help() instead)
4. **Type hints:** Keep type/enum, remove min/max items
5. **Defaults:** Remove description, keep value only

---

## Compressed Tool Definitions

### Format

For each tool, providing:
- **Before:** Current definition
- **After:** Compressed definition
- **Savings:** Token reduction estimate

---

### AgentBridge Service (37 tools)

#### help

```python
# BEFORE (48 tokens)
{
    "name": "help",
    "description": "Get help on using AgentBridge tools. Call this first if you're unsure how to interact with Unreal Engine.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "topic": {
                "type": "string",
                "description": "Optional topic: 'actors', 'properties', 'classes', 'console', 'workflows', 'pcg_volume', 'volume_sizing', 'bp_toolkit', or leave empty for overview",
            },
        },
        "required": [],
    },
}

# AFTER (28 tokens)
{
    "name": "help",
    "description": "Get help on AgentBridge tools and workflows.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "topic": {"type": "string"}
        }
    }
}
```

#### list_worlds

```python
# BEFORE (35 tokens)
{
    "name": "list_worlds",
    "description": "List all available Unreal world contexts (Editor, PIE, Game). Use this to see what worlds are available and their current state.",
    "inputSchema": {"type": "object", "properties": {}, "required": []},
}

# AFTER (18 tokens)
{
    "name": "list_worlds",
    "description": "List available world contexts.",
    "inputSchema": {"type": "object"}
}
```

#### set_target_world

```python
# BEFORE (45 tokens)
{
    "name": "set_target_world",
    "description": "Set the target world for subsequent operations. Use 'editor' for the editor world or 'pie' for Play-In-Editor.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "world_identifier": {
                "type": "string",
                "description": "World identifier: 'editor', 'pie', world name, or numeric index",
            },
        },
        "required": ["world_identifier"],
    },
}

# AFTER (25 tokens)
{
    "name": "set_target_world",
    "description": "Switch target world for operations.",
    "inputSchema": {
        "type": "object",
        "properties": {"world_identifier": {"type": "string"}},
        "required": ["world_identifier"]
    }
}
```

#### query_actors

```python
# BEFORE (~160 tokens)
{
    "name": "query_actors",
    "description": "Search for actors in the current world. You can filter by class, name pattern, label pattern, or tag. Returns a list of matching actors with their transforms.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "class_name": {
                "type": "string",
                "description": "Filter by class name (e.g., 'PointLight', 'StaticMeshActor', 'BP_MyActor')",
            },
            "name_pattern": {
                "type": "string",
                "description": "Substring pattern for internal actor name (e.g., 'Light', 'Door'). Matches GetName() - the internal unique identifier.",
            },
            "label_pattern": {
                "type": "string",
                "description": "Substring pattern for display label (e.g., 'MyLight', 'MainDoor'). Matches GetActorLabel() - the human-readable name shown in the editor.",
            },
            "tag": {
                "type": "string",
                "description": "Filter by actor tag",
            },
            "limit": {
                "type": "integer",
                "description": "Maximum number of results (default: 100)",
                "default": 100,
            },
            "include_hidden": {
                "type": "boolean",
                "description": "Include hidden actors in results",
                "default": False,
            },
        },
        "required": [],
    },
}

# AFTER (~55 tokens)
{
    "name": "query_actors",
    "description": "Find actors by class, name, label, or tag.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "class_name": {"type": "string"},
            "name_pattern": {"type": "string"},
            "label_pattern": {"type": "string"},
            "tag": {"type": "string"},
            "limit": {"type": "integer", "default": 100},
            "include_hidden": {"type": "boolean"}
        }
    }
}
```

#### get_actor

```python
# BEFORE (~85 tokens)
{
    "name": "get_actor",
    "description": "Get detailed information about a specific actor, including properties and components. Accepts friendly labels (e.g., 'PlayerStart') which resolve to the full internal name.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "actor_id": {
                "type": "string",
                "description": "Actor identifier: name, label, path, or GUID",
            },
            "include_properties": {
                "type": "boolean",
                "description": "Include property values in response",
                "default": False,
            },
            "include_components": {
                "type": "boolean",
                "description": "Include component list in response",
                "default": False,
            },
        },
        "required": ["actor_id"],
    },
}

# AFTER (~40 tokens)
{
    "name": "get_actor",
    "description": "Get actor details, optionally with properties/components.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "actor_id": {"type": "string"},
            "include_properties": {"type": "boolean"},
            "include_components": {"type": "boolean"}
        },
        "required": ["actor_id"]
    }
}
```

#### spawn_actor

```python
# BEFORE (~130 tokens)
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
            "rotation": {
                "type": "array",
                "items": {"type": "number"},
                "description": "Rotation [Pitch, Yaw, Roll] in degrees",
                "minItems": 3,
                "maxItems": 3,
            },
            "scale": {
                "type": "array",
                "items": {"type": "number"},
                "description": "Scale [X, Y, Z] (default: [1, 1, 1])",
                "minItems": 3,
                "maxItems": 3,
            },
            "label": {
                "type": "string",
                "description": "Editor display name for the actor",
            },
            "folder_path": {
                "type": "string",
                "description": "World Outliner folder path (e.g., 'Lights/Dynamic')",
            },
        },
        "required": ["class_name"],
    },
}

# AFTER (~50 tokens)
{
    "name": "spawn_actor",
    "description": "Create actor from class with optional transform.",
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

#### delete_actor

```python
# BEFORE (~40 tokens)
{
    "name": "delete_actor",
    "description": "Delete an actor from the world.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "actor_id": {
                "type": "string",
                "description": "Actor identifier: name, label, path, or GUID",
            },
        },
        "required": ["actor_id"],
    },
}

# AFTER (~22 tokens)
{
    "name": "delete_actor",
    "description": "Remove actor from world.",
    "inputSchema": {
        "type": "object",
        "properties": {"actor_id": {"type": "string"}},
        "required": ["actor_id"]
    }
}
```

#### duplicate_actor

```python
# AFTER (~45 tokens)
{
    "name": "duplicate_actor",
    "description": "Clone actor with optional new transform.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "actor_id": {"type": "string"},
            "location": {"type": "array"},
            "rotation": {"type": "array"},
            "scale": {"type": "array"},
            "new_label": {"type": "string"}
        },
        "required": ["actor_id"]
    }
}
```

#### set_actor_transform

```python
# AFTER (~40 tokens)
{
    "name": "set_actor_transform",
    "description": "Move, rotate, or scale actor.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "actor_id": {"type": "string"},
            "location": {"type": "array"},
            "rotation": {"type": "array"},
            "scale": {"type": "array"}
        },
        "required": ["actor_id"]
    }
}
```

#### get_property

```python
# AFTER (~28 tokens)
{
    "name": "get_property",
    "description": "Read property by path from actor/asset.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "actor_id": {"type": "string"},
            "path": {"type": "string"}
        },
        "required": ["actor_id", "path"]
    }
}
```

#### set_property

```python
# AFTER (~32 tokens)
{
    "name": "set_property",
    "description": "Write property by path on actor/asset.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "actor_id": {"type": "string"},
            "path": {"type": "string"},
            "value": {}
        },
        "required": ["actor_id", "path", "value"]
    }
}
```

#### list_classes

```python
# AFTER (~40 tokens)
{
    "name": "list_classes",
    "description": "Find actor/component/object classes.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "base_class_name": {"type": "string", "default": "Actor"},
            "name_pattern": {"type": "string"},
            "include_blueprint": {"type": "boolean"},
            "limit": {"type": "integer", "default": 50}
        }
    }
}
```

#### get_class_schema

```python
# AFTER (~35 tokens)
{
    "name": "get_class_schema",
    "description": "Get properties and functions of a class.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "class_name": {"type": "string"},
            "include_inherited": {"type": "boolean"},
            "include_functions": {"type": "boolean"}
        },
        "required": ["class_name"]
    }
}
```

#### call_static_function

```python
# AFTER (~40 tokens)
{
    "name": "call_static_function",
    "description": "Call static Blueprint library function.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "class_name": {"type": "string"},
            "function_name": {"type": "string"},
            "parameters": {"type": "object"}
        },
        "required": ["class_name", "function_name"]
    }
}
```

#### call_asset_function

```python
# AFTER (~40 tokens)
{
    "name": "call_asset_function",
    "description": "Call function on loaded UObject asset.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "asset_path": {"type": "string"},
            "function_name": {"type": "string"},
            "subobject_path": {"type": "string"},
            "parameters": {"type": "object"}
        },
        "required": ["asset_path", "function_name"]
    }
}
```

#### World Partition tools (7 tools)

```python
# is_world_partitioned - AFTER (~18 tokens)
{"name": "is_world_partitioned", "description": "Check if world uses streaming.", "inputSchema": {"type": "object"}}

# query_all_actors - AFTER (~45 tokens)
{
    "name": "query_all_actors",
    "description": "Query actors including unloaded streaming cells.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "class_name": {"type": "string"},
            "name_pattern": {"type": "string"},
            "include_loaded": {"type": "boolean"},
            "include_unloaded": {"type": "boolean"},
            "data_layer": {"type": "string"},
            "limit": {"type": "integer", "default": 100}
        }
    }
}

# get_streaming_state - AFTER (~25 tokens)
{
    "name": "get_streaming_state",
    "description": "Get actor load state by GUID.",
    "inputSchema": {
        "type": "object",
        "properties": {"actor_guid": {"type": "string"}},
        "required": ["actor_guid"]
    }
}

# query_landscape - AFTER (~22 tokens)
{
    "name": "query_landscape",
    "description": "List landscape proxies.",
    "inputSchema": {
        "type": "object",
        "properties": {"include_unloaded": {"type": "boolean"}}
    }
}

# get_landscape_bounds - AFTER (~15 tokens)
{"name": "get_landscape_bounds", "description": "Get full landscape extents.", "inputSchema": {"type": "object"}}

# get_data_layers - AFTER (~15 tokens)
{"name": "get_data_layers", "description": "List world data layers.", "inputSchema": {"type": "object"}}

# get_actors_in_data_layer - AFTER (~32 tokens)
{
    "name": "get_actors_in_data_layer",
    "description": "Get actors in a data layer.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "data_layer": {"type": "string"},
            "include_unloaded": {"type": "boolean"},
            "limit": {"type": "integer", "default": 100}
        },
        "required": ["data_layer"]
    }
}
```

#### Console Commands (2 tools)

```python
# execute_console_command - AFTER (~25 tokens)
{
    "name": "execute_console_command",
    "description": "Run console command.",
    "inputSchema": {
        "type": "object",
        "properties": {"command": {"type": "string"}},
        "required": ["command"]
    }
}

# search_console_commands - AFTER (~35 tokens)
{
    "name": "search_console_commands",
    "description": "Search console commands/CVars.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "keyword": {"type": "string"},
            "limit": {"type": "integer", "default": 50},
            "offset": {"type": "integer"},
            "search_help": {"type": "boolean"}
        },
        "required": ["keyword"]
    }
}
```

#### Asset Operations (4 tools)

```python
# create_asset - AFTER (~40 tokens)
{
    "name": "create_asset",
    "description": "Create DataAsset, MaterialInstance, etc.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "asset_class": {"type": "string"},
            "package_path": {"type": "string"},
            "asset_name": {"type": "string"},
            "parent_asset_path": {"type": "string"},
            "properties": {"type": "object"}
        },
        "required": ["asset_class", "package_path", "asset_name"]
    }
}

# save_asset - AFTER (~25 tokens)
{
    "name": "save_asset",
    "description": "Save asset to disk.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "asset_path": {"type": "string"},
            "prompt_for_checkout": {"type": "boolean"}
        },
        "required": ["asset_path"]
    }
}

# save_actor_as_blueprint - AFTER (~35 tokens)
{
    "name": "save_actor_as_blueprint",
    "description": "Convert actor to Blueprint asset.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "actor_id": {"type": "string"},
            "package_path": {"type": "string"},
            "blueprint_name": {"type": "string"},
            "replace_existing": {"type": "boolean"}
        },
        "required": ["actor_id", "package_path", "blueprint_name"]
    }
}

# duplicate_asset - AFTER (~32 tokens)
{
    "name": "duplicate_asset",
    "description": "Copy existing asset.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "source_path": {"type": "string"},
            "dest_package_path": {"type": "string"},
            "dest_asset_name": {"type": "string"}
        },
        "required": ["source_path", "dest_package_path", "dest_asset_name"]
    }
}
```

#### Component Operations (6 tools)

```python
# get_component_transform - AFTER (~32 tokens)
{
    "name": "get_component_transform",
    "description": "Get component transform.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "actor_id": {"type": "string"},
            "component_name": {"type": "string"},
            "world_space": {"type": "boolean"}
        },
        "required": ["actor_id", "component_name"]
    }
}

# set_component_transform - AFTER (~42 tokens)
{
    "name": "set_component_transform",
    "description": "Set component transform.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "actor_id": {"type": "string"},
            "component_name": {"type": "string"},
            "location": {"type": "array"},
            "rotation": {"type": "array"},
            "scale": {"type": "array"},
            "world_space": {"type": "boolean"}
        },
        "required": ["actor_id", "component_name"]
    }
}

# attach_actor - AFTER (~42 tokens)
{
    "name": "attach_actor",
    "description": "Attach actor to another.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "child_actor_id": {"type": "string"},
            "parent_actor_id": {"type": "string"},
            "parent_component_name": {"type": "string"},
            "socket_name": {"type": "string"},
            "location_rule": {"type": "string", "enum": ["keep_relative", "keep_world", "snap_to_target"]}
        },
        "required": ["child_actor_id", "parent_actor_id"]
    }
}

# detach_actor - AFTER (~28 tokens)
{
    "name": "detach_actor",
    "description": "Detach actor from parent.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "actor_id": {"type": "string"},
            "maintain_world_position": {"type": "boolean"}
        },
        "required": ["actor_id"]
    }
}

# attach_component - AFTER (~45 tokens)
{
    "name": "attach_component",
    "description": "Attach component to another.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "actor_id": {"type": "string"},
            "component_name": {"type": "string"},
            "parent_component_name": {"type": "string"},
            "socket_name": {"type": "string"},
            "location_rule": {"type": "string", "enum": ["keep_relative", "keep_world", "snap_to_target"]}
        },
        "required": ["actor_id", "component_name", "parent_component_name"]
    }
}

# detach_component - AFTER (~30 tokens)
{
    "name": "detach_component",
    "description": "Detach component from parent.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "actor_id": {"type": "string"},
            "component_name": {"type": "string"},
            "maintain_world_transform": {"type": "boolean"}
        },
        "required": ["actor_id", "component_name"]
    }
}
```

#### File Operations (4 tools)

```python
# read_project_file - AFTER (~28 tokens)
{
    "name": "read_project_file",
    "description": "Read file from project directory.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "relative_path": {"type": "string"},
            "as_base64": {"type": "boolean"}
        },
        "required": ["relative_path"]
    }
}

# write_project_file - AFTER (~38 tokens)
{
    "name": "write_project_file",
    "description": "Write file to project directory.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "relative_path": {"type": "string"},
            "content": {"type": "string"},
            "is_base64": {"type": "boolean"},
            "create_directories": {"type": "boolean"},
            "append": {"type": "boolean"}
        },
        "required": ["relative_path", "content"]
    }
}

# list_project_directory - AFTER (~35 tokens)
{
    "name": "list_project_directory",
    "description": "List files in project directory.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "relative_path": {"type": "string"},
            "pattern": {"type": "string"},
            "recursive": {"type": "boolean"},
            "limit": {"type": "integer", "default": 100}
        }
    }
}

# copy_project_file - AFTER (~30 tokens)
{
    "name": "copy_project_file",
    "description": "Copy file within project.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "source_path": {"type": "string"},
            "dest_path": {"type": "string"},
            "overwrite": {"type": "boolean"}
        },
        "required": ["source_path", "dest_path"]
    }
}
```

---

### Tempo Services (53 tools) - Compressed Definitions

#### tempo_time (6 tools)

```python
{"name": "tempo_play", "description": "Start/resume simulation.", "inputSchema": {"type": "object"}}
{"name": "tempo_pause", "description": "Pause simulation.", "inputSchema": {"type": "object"}}
{"name": "tempo_step", "description": "Advance one frame.", "inputSchema": {"type": "object"}}

{
    "name": "tempo_advance_steps",
    "description": "Advance N frames.",
    "inputSchema": {"type": "object", "properties": {"steps": {"type": "integer"}}, "required": ["steps"]}
}

{
    "name": "tempo_set_time_mode",
    "description": "Set time mode.",
    "inputSchema": {
        "type": "object",
        "properties": {"mode": {"type": "string", "enum": ["wall_clock", "fixed_step"]}},
        "required": ["mode"]
    }
}

{
    "name": "tempo_set_sim_rate",
    "description": "Set simulation rate.",
    "inputSchema": {"type": "object", "properties": {"rate": {"type": "number"}}, "required": ["rate"]}
}
```

#### tempo_actor_control (17 tools)

```python
{"name": "tempo_get_all_actors", "description": "List all actors.", "inputSchema": {"type": "object"}}

{
    "name": "tempo_spawn_actor",
    "description": "Spawn actor with optional relative transform.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "type": {"type": "string"},
            "location": {"type": "array"},
            "rotation": {"type": "array"},
            "relative_to": {"type": "string"}
        },
        "required": ["type"]
    }
}

{
    "name": "tempo_destroy_actor",
    "description": "Destroy actor.",
    "inputSchema": {"type": "object", "properties": {"actor": {"type": "string"}}, "required": ["actor"]}
}

{
    "name": "tempo_get_components",
    "description": "List actor components.",
    "inputSchema": {"type": "object", "properties": {"actor": {"type": "string"}}, "required": ["actor"]}
}

{
    "name": "tempo_add_component",
    "description": "Add component to actor.",
    "inputSchema": {
        "type": "object",
        "properties": {"actor": {"type": "string"}, "type": {"type": "string"}, "name": {"type": "string"}},
        "required": ["actor", "type"]
    }
}

{
    "name": "tempo_get_actor_properties",
    "description": "Get actor properties.",
    "inputSchema": {
        "type": "object",
        "properties": {"actor": {"type": "string"}, "details": {"type": "boolean"}},
        "required": ["actor"]
    }
}

{
    "name": "tempo_get_component_properties",
    "description": "Get component properties.",
    "inputSchema": {
        "type": "object",
        "properties": {"actor": {"type": "string"}, "component": {"type": "string"}, "details": {"type": "boolean"}},
        "required": ["actor", "component"]
    }
}

# Typed property setters (9 tools) - all follow same pattern
{
    "name": "tempo_set_float_property",
    "description": "Set float property.",
    "inputSchema": {
        "type": "object",
        "properties": {"actor": {"type": "string"}, "property": {"type": "string"}, "value": {"type": "number"}, "component": {"type": "string"}},
        "required": ["actor", "property", "value"]
    }
}
# tempo_set_int_property - same pattern
# tempo_set_bool_property - same pattern
# tempo_set_string_property - same pattern
# tempo_set_vector_property - value is array
# tempo_set_rotator_property - value is array
# tempo_set_color_property - value is array (RGBA)
# tempo_set_asset_property - value is string (path)

{
    "name": "tempo_set_actor_transform",
    "description": "Set actor transform.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "actor": {"type": "string"},
            "location": {"type": "array"},
            "rotation": {"type": "array"},
            "scale": {"type": "array"},
            "relative_to": {"type": "string"}
        },
        "required": ["actor"]
    }
}

{
    "name": "tempo_call_function",
    "description": "Call function on actor.",
    "inputSchema": {
        "type": "object",
        "properties": {"actor": {"type": "string"}, "function": {"type": "string"}},
        "required": ["actor", "function"]
    }
}
```

#### tempo_core (6 tools)

```python
{
    "name": "tempo_load_level",
    "description": "Load level by path.",
    "inputSchema": {
        "type": "object",
        "properties": {"path": {"type": "string"}, "async": {"type": "boolean"}},
        "required": ["path"]
    }
}

{"name": "tempo_finish_loading_level", "description": "Wait for level load.", "inputSchema": {"type": "object"}}
{"name": "tempo_get_current_level", "description": "Get loaded level name.", "inputSchema": {"type": "object"}}
{"name": "tempo_quit", "description": "Quit editor.", "inputSchema": {"type": "object"}}

{
    "name": "tempo_set_viewport_render",
    "description": "Enable/disable viewport rendering.",
    "inputSchema": {"type": "object", "properties": {"enabled": {"type": "boolean"}}, "required": ["enabled"]}
}

{
    "name": "tempo_set_control_mode",
    "description": "Set control mode.",
    "inputSchema": {
        "type": "object",
        "properties": {"mode": {"type": "string", "enum": ["manual", "scripted"]}},
        "required": ["mode"]
    }
}
```

#### tempo_core_editor (6 tools)

```python
{"name": "tempo_play_in_editor", "description": "Start PIE session.", "inputSchema": {"type": "object"}}
{"name": "tempo_simulate", "description": "Start simulate mode.", "inputSchema": {"type": "object"}}
{"name": "tempo_stop", "description": "Stop PIE/simulate.", "inputSchema": {"type": "object"}}

{
    "name": "tempo_save_level",
    "description": "Save current level.",
    "inputSchema": {"type": "object", "properties": {"path": {"type": "string"}}}
}

{
    "name": "tempo_open_level",
    "description": "Open level in editor.",
    "inputSchema": {"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}
}

{"name": "tempo_new_level", "description": "Create empty level.", "inputSchema": {"type": "object"}}
```

#### tempo_geographic (5 tools)

```python
{
    "name": "tempo_set_date",
    "description": "Set simulation date.",
    "inputSchema": {
        "type": "object",
        "properties": {"year": {"type": "integer"}, "month": {"type": "integer"}, "day": {"type": "integer"}},
        "required": ["year", "month", "day"]
    }
}

{
    "name": "tempo_set_time_of_day",
    "description": "Set time of day.",
    "inputSchema": {
        "type": "object",
        "properties": {"hour": {"type": "integer"}, "minute": {"type": "integer"}, "second": {"type": "integer"}},
        "required": ["hour", "minute", "second"]
    }
}

{
    "name": "tempo_set_day_cycle_rate",
    "description": "Set day/night cycle rate.",
    "inputSchema": {"type": "object", "properties": {"rate": {"type": "number"}}, "required": ["rate"]}
}

{"name": "tempo_get_datetime", "description": "Get simulation datetime.", "inputSchema": {"type": "object"}}

{
    "name": "tempo_set_geographic_reference",
    "description": "Set lat/lon/alt origin.",
    "inputSchema": {
        "type": "object",
        "properties": {"latitude": {"type": "number"}, "longitude": {"type": "number"}, "altitude": {"type": "number"}},
        "required": ["latitude", "longitude"]
    }
}
```

#### tempo_movement (5 tools)

```python
{"name": "tempo_get_commandable_vehicles", "description": "List controllable vehicles.", "inputSchema": {"type": "object"}}

{
    "name": "tempo_command_vehicle",
    "description": "Send vehicle commands.",
    "inputSchema": {
        "type": "object",
        "properties": {"vehicle": {"type": "string"}, "steering": {"type": "number"}, "throttle": {"type": "number"}, "brake": {"type": "number"}},
        "required": ["vehicle"]
    }
}

{"name": "tempo_get_commandable_pawns", "description": "List controllable pawns.", "inputSchema": {"type": "object"}}

{
    "name": "tempo_pawn_move_to",
    "description": "Navigate pawn to location.",
    "inputSchema": {
        "type": "object",
        "properties": {"pawn": {"type": "string"}, "location": {"type": "array"}},
        "required": ["pawn", "location"]
    }
}

{"name": "tempo_rebuild_navigation", "description": "Rebuild navmesh.", "inputSchema": {"type": "object"}}
```

#### Remaining Tempo services

```python
# tempo_world_state (2 tools)
{
    "name": "tempo_get_actor_state",
    "description": "Get actor transform, velocity, bounds.",
    "inputSchema": {"type": "object", "properties": {"actor": {"type": "string"}}, "required": ["actor"]}
}

{
    "name": "tempo_get_actors_near",
    "description": "Get actors within radius.",
    "inputSchema": {
        "type": "object",
        "properties": {"location": {"type": "array"}, "radius": {"type": "number"}, "class_name": {"type": "string"}},
        "required": ["location", "radius"]
    }
}

# tempo_labels (1 tool)
{"name": "tempo_get_label_map", "description": "Get actor label mappings.", "inputSchema": {"type": "object"}}

# tempo_sensors (1 tool)
{"name": "tempo_get_available_sensors", "description": "List available sensors.", "inputSchema": {"type": "object"}}

# tempo_map_query (3 tools)
{
    "name": "tempo_get_lanes",
    "description": "Get lane geometry.",
    "inputSchema": {"type": "object", "properties": {"area": {"type": "array"}, "filter": {"type": "string"}}}
}

{
    "name": "tempo_get_lane_accessibility",
    "description": "Check lane accessibility.",
    "inputSchema": {
        "type": "object",
        "properties": {"lane_id": {"type": "string"}, "agent_type": {"type": "string"}},
        "required": ["lane_id"]
    }
}

{
    "name": "tempo_get_zones",
    "description": "Get zone geometry.",
    "inputSchema": {"type": "object", "properties": {"area": {"type": "array"}, "filter": {"type": "string"}}}
}

# tempo_agents_editor (1 tool)
{"name": "tempo_run_zone_graph_builder", "description": "Run zone graph builder.", "inputSchema": {"type": "object"}}
```

---

### bp_toolkit (14 tools) - Compressed Definitions

```python
{
    "name": "bp_export_asset",
    "description": "Export uasset to JSON.",
    "inputSchema": {
        "type": "object",
        "properties": {"uasset_path": {"type": "string"}, "ue_version": {"type": "string"}},
        "required": ["uasset_path"]
    }
}

{
    "name": "bp_import_asset",
    "description": "Import JSON to uasset.",
    "inputSchema": {
        "type": "object",
        "properties": {"json_path": {"type": "string"}, "ue_version": {"type": "string"}},
        "required": ["json_path"]
    }
}

{
    "name": "bp_detect_type",
    "description": "Detect asset type.",
    "inputSchema": {"type": "object", "properties": {"json_path": {"type": "string"}}, "required": ["json_path"]}
}

{
    "name": "bp_get_info",
    "description": "Get asset summary.",
    "inputSchema": {"type": "object", "properties": {"json_path": {"type": "string"}}, "required": ["json_path"]}
}

{
    "name": "bp_list_properties",
    "description": "List properties with types.",
    "inputSchema": {
        "type": "object",
        "properties": {"json_path": {"type": "string"}, "export_index": {"type": "integer"}},
        "required": ["json_path"]
    }
}

{
    "name": "bp_get_property",
    "description": "Get property by path.",
    "inputSchema": {
        "type": "object",
        "properties": {"json_path": {"type": "string"}, "property_path": {"type": "string"}, "export_index": {"type": "integer"}},
        "required": ["json_path", "property_path"]
    }
}

{
    "name": "bp_set_property",
    "description": "Set property by path.",
    "inputSchema": {
        "type": "object",
        "properties": {"json_path": {"type": "string"}, "property_path": {"type": "string"}, "value": {}, "export_index": {"type": "integer"}, "output_path": {"type": "string"}},
        "required": ["json_path", "property_path", "value"]
    }
}

{
    "name": "bp_clone_asset",
    "description": "Clone asset with new name.",
    "inputSchema": {
        "type": "object",
        "properties": {"json_path": {"type": "string"}, "new_name": {"type": "string"}, "output_dir": {"type": "string"}},
        "required": ["json_path", "new_name"]
    }
}

{
    "name": "bp_list_graphs",
    "description": "List graphs in asset.",
    "inputSchema": {"type": "object", "properties": {"json_path": {"type": "string"}}, "required": ["json_path"]}
}

{
    "name": "bp_add_comment",
    "description": "Add comment node to graph.",
    "inputSchema": {
        "type": "object",
        "properties": {"json_path": {"type": "string"}, "graph_name": {"type": "string"}, "text": {"type": "string"}, "position": {"type": "array"}},
        "required": ["json_path", "text"]
    }
}

{
    "name": "bp_clone_node",
    "description": "Clone existing node.",
    "inputSchema": {
        "type": "object",
        "properties": {"json_path": {"type": "string"}, "node_name": {"type": "string"}, "new_position": {"type": "array"}},
        "required": ["json_path", "node_name"]
    }
}

{
    "name": "bp_find",
    "description": "Search namemap/exports.",
    "inputSchema": {
        "type": "object",
        "properties": {"json_path": {"type": "string"}, "pattern": {"type": "string"}, "scope": {"type": "string", "enum": ["namemap", "exports", "all"]}},
        "required": ["json_path", "pattern"]
    }
}

{
    "name": "bp_query",
    "description": "Type-specific queries.",
    "inputSchema": {
        "type": "object",
        "properties": {"json_path": {"type": "string"}, "query": {"type": "string"}},
        "required": ["json_path", "query"]
    }
}

{
    "name": "bp_parse",
    "description": "Full Blueprint parsing.",
    "inputSchema": {
        "type": "object",
        "properties": {"json_path": {"type": "string"}, "include_call_graph": {"type": "boolean"}},
        "required": ["json_path"]
    }
}
```

---

### Blueprint Node Manipulation (6 tools) - Compressed Definitions

```python
# bp_create_node - AFTER (~50 tokens)
{
    "name": "bp_create_node",
    "description": "Create Blueprint node (CallFunction, Event, Variable, Branch, Sequence, Comment).",
    "inputSchema": {
        "type": "object",
        "properties": {
            "blueprint_path": {"type": "string"},
            "graph_name": {"type": "string", "default": "EventGraph"},
            "node_type": {"type": "string", "enum": ["CallFunction", "Event", "VariableGet", "VariableSet", "Branch", "Sequence", "Comment"]},
            "function_reference": {"type": "string"},
            "event_name": {"type": "string"},
            "variable_name": {"type": "string"},
            "comment_text": {"type": "string"},
            "pos_x": {"type": "integer"},
            "pos_y": {"type": "integer"}
        },
        "required": ["blueprint_path", "node_type"]
    }
}

# bp_connect_pins - AFTER (~35 tokens)
{
    "name": "bp_connect_pins",
    "description": "Connect two Blueprint node pins.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "blueprint_path": {"type": "string"},
            "source_node": {"type": "string"},
            "source_pin": {"type": "string"},
            "target_node": {"type": "string"},
            "target_pin": {"type": "string"}
        },
        "required": ["blueprint_path", "source_node", "source_pin", "target_node", "target_pin"]
    }
}

# bp_disconnect_pins - AFTER (~35 tokens)
{
    "name": "bp_disconnect_pins",
    "description": "Disconnect two Blueprint node pins.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "blueprint_path": {"type": "string"},
            "source_node": {"type": "string"},
            "source_pin": {"type": "string"},
            "target_node": {"type": "string"},
            "target_pin": {"type": "string"}
        },
        "required": ["blueprint_path", "source_node", "source_pin", "target_node", "target_pin"]
    }
}

# bp_delete_node - AFTER (~22 tokens)
{
    "name": "bp_delete_node",
    "description": "Delete Blueprint node.",
    "inputSchema": {
        "type": "object",
        "properties": {"blueprint_path": {"type": "string"}, "node_id": {"type": "string"}},
        "required": ["blueprint_path", "node_id"]
    }
}

# bp_list_nodes - AFTER (~25 tokens)
{
    "name": "bp_list_nodes",
    "description": "List all nodes in Blueprint graph.",
    "inputSchema": {
        "type": "object",
        "properties": {"blueprint_path": {"type": "string"}, "graph_name": {"type": "string"}},
        "required": ["blueprint_path"]
    }
}

# bp_list_pins - AFTER (~22 tokens)
{
    "name": "bp_list_pins",
    "description": "List pins on a Blueprint node.",
    "inputSchema": {
        "type": "object",
        "properties": {"blueprint_path": {"type": "string"}, "node_id": {"type": "string"}},
        "required": ["blueprint_path", "node_id"]
    }
}
```

---

### PCG Graph Manipulation (6 tools) - Compressed Definitions

```python
# pcg_add_node - AFTER (~35 tokens)
{
    "name": "pcg_add_node",
    "description": "Add node to PCG graph (SurfaceSampler, StaticMeshSpawner, etc.).",
    "inputSchema": {
        "type": "object",
        "properties": {
            "graph_path": {"type": "string"},
            "node_type": {"type": "string"},
            "pos_x": {"type": "integer"},
            "pos_y": {"type": "integer"}
        },
        "required": ["graph_path", "node_type"]
    }
}

# pcg_connect - AFTER (~35 tokens)
{
    "name": "pcg_connect",
    "description": "Connect two PCG nodes.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "graph_path": {"type": "string"},
            "from_node": {"type": "string"},
            "from_pin": {"type": "string"},
            "to_node": {"type": "string"},
            "to_pin": {"type": "string"}
        },
        "required": ["graph_path", "from_node", "from_pin", "to_node", "to_pin"]
    }
}

# pcg_disconnect - AFTER (~35 tokens)
{
    "name": "pcg_disconnect",
    "description": "Disconnect two PCG nodes.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "graph_path": {"type": "string"},
            "from_node": {"type": "string"},
            "from_pin": {"type": "string"},
            "to_node": {"type": "string"},
            "to_pin": {"type": "string"}
        },
        "required": ["graph_path", "from_node", "from_pin", "to_node", "to_pin"]
    }
}

# pcg_delete_node - AFTER (~22 tokens)
{
    "name": "pcg_delete_node",
    "description": "Delete PCG node.",
    "inputSchema": {
        "type": "object",
        "properties": {"graph_path": {"type": "string"}, "node_path": {"type": "string"}},
        "required": ["graph_path", "node_path"]
    }
}

# pcg_list_nodes - AFTER (~18 tokens)
{
    "name": "pcg_list_nodes",
    "description": "List PCG nodes with pins.",
    "inputSchema": {
        "type": "object",
        "properties": {"graph_path": {"type": "string"}},
        "required": ["graph_path"]
    }
}

# pcg_get_input_output_nodes - AFTER (~18 tokens)
{
    "name": "pcg_get_input_output_nodes",
    "description": "Get PCG InputNode and OutputNode.",
    "inputSchema": {
        "type": "object",
        "properties": {"graph_path": {"type": "string"}},
        "required": ["graph_path"]
    }
}
```

---

## Estimated Savings

### Before Compression

| Service | Tools | Avg Tokens/Tool | Total |
|---------|-------|-----------------|-------|
| AgentBridge | 49 | ~95 | ~4,655 |
| Tempo (all) | 53 | ~75 | ~3,975 |
| bp_toolkit | 14 | ~70 | ~980 |
| **Total** | **116** | **~83** | **~9,610** |

*Note: These are the raw JSON tokens. Claude Code's wrapper adds ~200+ tokens per tool.*

*AgentBridge now includes 6 BP node tools + 6 PCG graph tools = 12 additional tools.*

### After Compression

| Service | Tools | Avg Tokens/Tool | Total |
|---------|-------|-----------------|-------|
| AgentBridge | 49 | ~32 | ~1,568 |
| Tempo (all) | 53 | ~28 | ~1,484 |
| bp_toolkit | 14 | ~30 | ~420 |
| **Total** | **116** | **~30** | **~3,472** |

### Reduction

- Raw tokens: 8,470 → 3,088 = **64% reduction**
- With Claude Code wrapper overhead, effective reduction should be **50-60%**

---

## Implementation Steps

1. **Create compressed TOOLS list** in a new module or replace in-place
2. **Verify help() topics still provide full documentation**
3. **Test with Claude Code** to verify tools still work and are discoverable
4. **Measure actual token usage** with `/mcp` command

---

## Additional Optimization: Remove empty schemas

For tools with no parameters, use minimal schema:

```python
# Instead of
"inputSchema": {"type": "object", "properties": {}, "required": []}

# Use
"inputSchema": {"type": "object"}
```

Saves ~10 tokens per no-param tool (approximately 15-20 tools).

---

## Risk Assessment

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| LLM misidentifies tool | Low | Names are descriptive; help() available |
| Parameter confusion | Medium | Keep type info; validate on server |
| Lost discoverability | Low | help() provides comprehensive docs |

---

*Phase 1 complete. Proceed to Phase 2 (Modular Loading) or Phase 3 (Consolidation).*
