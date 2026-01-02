"""
AgentBridge MCP Tools

Exposes AgentBridge gRPC service for world/actor manipulation.
"""

import json
from typing import Dict, Any, List, Tuple, Optional
from dataclasses import dataclass
from . import register_service, ServiceModule
from .base import create_channel, safe_call

# Import AgentBridge's generated stubs
from AgentBridgeServer import AgentBridge_pb2 as pb
from AgentBridgeServer import AgentBridge_pb2_grpc as pb_grpc
from TempoScripting import Geometry_pb2


TOOLS = [
    # =========================================================================
    # Help & Discovery
    # =========================================================================
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
    },

    # =========================================================================
    # World Operations
    # =========================================================================
    {
        "name": "list_worlds",
        "description": "List all available Unreal world contexts (Editor, PIE, Game). Use this to see what worlds are available and their current state.",
        "inputSchema": {
            "type": "object",
            "properties": {},
            "required": [],
        },
    },
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
    },

    # =========================================================================
    # Actor Discovery
    # =========================================================================
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
    },
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
    },

    # =========================================================================
    # Actor Manipulation
    # =========================================================================
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
    },
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
    },
    {
        "name": "duplicate_actor",
        "description": "Create a copy of an existing actor. Copies all non-transient properties from the source actor.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "actor_id": {
                    "type": "string",
                    "description": "Source actor identifier: name, label, path, or GUID",
                },
                "location": {
                    "type": "array",
                    "items": {"type": "number"},
                    "description": "New location [X, Y, Z] (default: same as source)",
                    "minItems": 3,
                    "maxItems": 3,
                },
                "rotation": {
                    "type": "array",
                    "items": {"type": "number"},
                    "description": "New rotation [Pitch, Yaw, Roll] in degrees (default: same as source)",
                    "minItems": 3,
                    "maxItems": 3,
                },
                "scale": {
                    "type": "array",
                    "items": {"type": "number"},
                    "description": "New scale [X, Y, Z] (default: same as source)",
                    "minItems": 3,
                    "maxItems": 3,
                },
                "new_label": {
                    "type": "string",
                    "description": "Label for the duplicate (default: auto-generated)",
                },
            },
            "required": ["actor_id"],
        },
    },
    {
        "name": "set_actor_transform",
        "description": "Move, rotate, or scale an actor. You can set any combination of location, rotation, and scale.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "actor_id": {
                    "type": "string",
                    "description": "Actor identifier: name, label, path, or GUID",
                },
                "location": {
                    "type": "array",
                    "items": {"type": "number"},
                    "description": "New location [X, Y, Z] in Unreal units (cm)",
                    "minItems": 3,
                    "maxItems": 3,
                },
                "rotation": {
                    "type": "array",
                    "items": {"type": "number"},
                    "description": "New rotation [Pitch, Yaw, Roll] in degrees",
                    "minItems": 3,
                    "maxItems": 3,
                },
                "scale": {
                    "type": "array",
                    "items": {"type": "number"},
                    "description": "New scale [X, Y, Z]",
                    "minItems": 3,
                    "maxItems": 3,
                },
            },
            "required": ["actor_id"],
        },
    },

    # =========================================================================
    # Property Operations
    # =========================================================================
    {
        "name": "get_property",
        "description": "Get a property value from an actor using a property path. Supports nested properties like 'RootComponent.RelativeLocation.X'.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "actor_id": {
                    "type": "string",
                    "description": "Actor identifier: name, label, path, or GUID",
                },
                "path": {
                    "type": "string",
                    "description": "Property path (e.g., 'LightComponent.Intensity', 'RootComponent.RelativeLocation')",
                },
            },
            "required": ["actor_id", "path"],
        },
    },
    {
        "name": "set_property",
        "description": "Set a property value on an actor using a property path.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "actor_id": {
                    "type": "string",
                    "description": "Actor identifier: name, label, path, or GUID",
                },
                "path": {
                    "type": "string",
                    "description": "Property path (e.g., 'LightComponent.Intensity', 'RootComponent.RelativeLocation')",
                },
                "value": {
                    "description": "New value as string (will be parsed according to property type)",
                },
            },
            "required": ["actor_id", "path", "value"],
        },
    },

    # =========================================================================
    # Type Discovery
    # =========================================================================
    {
        "name": "list_classes",
        "description": "List available classes (actors, components, or any UObject type). "
                       "Use base_class_name='ActorComponent' for component classes, "
                       "'Object' for all types, or any specific class name.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "base_class_name": {
                    "type": "string",
                    "description": "Base class filter: 'Actor' (default), 'ActorComponent', 'Object', or specific class",
                    "default": "Actor",
                },
                "name_pattern": {
                    "type": "string",
                    "description": "Wildcard pattern for class name (e.g., '*Light*')",
                },
                "include_blueprint": {
                    "type": "boolean",
                    "description": "Include Blueprint classes",
                    "default": True,
                },
                "limit": {
                    "type": "integer",
                    "description": "Maximum number of results",
                    "default": 50,
                },
            },
            "required": [],
        },
    },
    {
        "name": "get_class_schema",
        "description": "Get the schema (properties and functions) for ANY class - actors, components, or UObjects. "
                       "Works with 'PointLight', 'SceneCaptureComponent2D', 'TextureRenderTarget2D', etc.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "class_name": {
                    "type": "string",
                    "description": "Class name or path",
                },
                "include_inherited": {
                    "type": "boolean",
                    "description": "Include inherited properties/functions",
                    "default": True,
                },
                "include_functions": {
                    "type": "boolean",
                    "description": "Include function signatures",
                    "default": False,
                },
            },
            "required": ["class_name"],
        },
    },

    # =========================================================================
    # Static Function Invocation
    # =========================================================================
    {
        "name": "call_static_function",
        "description": "Call a static Blueprint library function. "
                       "Examples: KismetRenderingLibrary::CreateRenderTarget2D, "
                       "KismetSystemLibrary::PrintString, KismetMathLibrary::Abs.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "class_name": {
                    "type": "string",
                    "description": "Blueprint library class (e.g., 'KismetRenderingLibrary', 'KismetSystemLibrary')",
                },
                "function_name": {
                    "type": "string",
                    "description": "Static function name (e.g., 'CreateRenderTarget2D', 'PrintString')",
                },
                "parameters": {
                    "type": "object",
                    "description": "Function parameters as key-value pairs",
                    "additionalProperties": True,
                },
            },
            "required": ["class_name", "function_name"],
        },
    },

    # =========================================================================
    # World Partition & Streaming
    # =========================================================================
    {
        "name": "is_world_partitioned",
        "description": "Check if the current world uses World Partition (UE5's streaming system for large worlds).",
        "inputSchema": {
            "type": "object",
            "properties": {},
            "required": [],
        },
    },
    {
        "name": "query_all_actors",
        "description": "Query actors including those in unloaded streaming cells. Unlike query_actors, this can find actors that aren't currently loaded in memory.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "class_name": {
                    "type": "string",
                    "description": "Filter by class name (e.g., 'StaticMeshActor')",
                },
                "name_pattern": {
                    "type": "string",
                    "description": "Wildcard pattern for actor name/label",
                },
                "include_loaded": {
                    "type": "boolean",
                    "description": "Include loaded actors",
                    "default": True,
                },
                "include_unloaded": {
                    "type": "boolean",
                    "description": "Include unloaded actors (metadata only)",
                    "default": True,
                },
                "data_layer": {
                    "type": "string",
                    "description": "Filter by data layer name",
                },
                "limit": {
                    "type": "integer",
                    "description": "Maximum number of results",
                    "default": 100,
                },
            },
            "required": [],
        },
    },
    {
        "name": "get_streaming_state",
        "description": "Get the streaming state of an actor by GUID. Returns whether the actor is Loaded, Unloaded, or Invalid.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "actor_guid": {
                    "type": "string",
                    "description": "Actor GUID (from query_all_actors results)",
                },
            },
            "required": ["actor_guid"],
        },
    },
    {
        "name": "query_landscape",
        "description": "Query all landscape proxies in the world, including streaming landscape chunks.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "include_unloaded": {
                    "type": "boolean",
                    "description": "Include unloaded landscape proxies",
                    "default": True,
                },
            },
            "required": [],
        },
    },
    {
        "name": "get_landscape_bounds",
        "description": "Get complete landscape bounds in world space. Returns min/max corners, center point, and half-extents. Use this to size PCG volumes or other actors to cover the entire landscape.",
        "inputSchema": {
            "type": "object",
            "properties": {},
            "required": [],
        },
    },
    {
        "name": "get_data_layers",
        "description": "Get all data layers defined in the world. Data layers are used to group actors for streaming.",
        "inputSchema": {
            "type": "object",
            "properties": {},
            "required": [],
        },
    },
    {
        "name": "get_actors_in_data_layer",
        "description": "Get all actors in a specific data layer.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "data_layer": {
                    "type": "string",
                    "description": "Data layer name",
                },
                "include_unloaded": {
                    "type": "boolean",
                    "description": "Include unloaded actors",
                    "default": True,
                },
                "limit": {
                    "type": "integer",
                    "description": "Maximum number of results",
                    "default": 100,
                },
            },
            "required": ["data_layer"],
        },
    },

    # =========================================================================
    # Console Commands
    # =========================================================================
    {
        "name": "execute_console_command",
        "description": "Execute an arbitrary Unreal console command. Use this for operations not covered by other tools.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "command": {
                    "type": "string",
                    "description": "Console command to execute (e.g., 'stat fps', 'AgentBridge.ListWorlds')",
                },
            },
            "required": ["command"],
        },
    },
    {
        "name": "search_console_commands",
        "description": "Search Unreal console commands and CVars by keyword. Use this to discover available commands when you need to do something but don't know the exact command name. Supports pagination - use offset to get the next page of results.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "keyword": {
                    "type": "string",
                    "description": "Search term (e.g., 'fps', 'shadow', 'light', 'vsync')",
                },
                "limit": {
                    "type": "integer",
                    "description": "Maximum results to return (default: 50)",
                    "default": 50,
                },
                "offset": {
                    "type": "integer",
                    "description": "Skip first N matches for pagination (default: 0)",
                    "default": 0,
                },
                "search_help": {
                    "type": "boolean",
                    "description": "Also search in help/description text (default: false)",
                    "default": False,
                },
            },
            "required": ["keyword"],
        },
    },

    # =========================================================================
    # Asset Operations (P0)
    # =========================================================================
    {
        "name": "create_asset",
        "description": "Create a new UAsset (DataAsset, MaterialInstance, etc.) in the Content folder. Editor only.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "asset_class": {
                    "type": "string",
                    "description": "Asset class name (e.g., 'DataAsset', 'MaterialInstanceConstant')",
                },
                "package_path": {
                    "type": "string",
                    "description": "Content folder path (e.g., '/Game/MyAssets')",
                },
                "asset_name": {
                    "type": "string",
                    "description": "Name for the new asset",
                },
                "parent_asset_path": {
                    "type": "string",
                    "description": "For instances (e.g., parent material path)",
                },
                "properties": {
                    "type": "object",
                    "description": "Initial property values to set",
                },
            },
            "required": ["asset_class", "package_path", "asset_name"],
        },
    },
    {
        "name": "save_asset",
        "description": "Save a modified asset to disk. Editor only.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "asset_path": {
                    "type": "string",
                    "description": "Asset path to save (e.g., '/Game/MyAssets/MyAsset')",
                },
                "prompt_for_checkout": {
                    "type": "boolean",
                    "description": "Prompt for source control checkout",
                    "default": False,
                },
            },
            "required": ["asset_path"],
        },
    },
    {
        "name": "save_actor_as_blueprint",
        "description": "Convert an actor to a reusable Blueprint asset. Editor only.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "actor_id": {
                    "type": "string",
                    "description": "Actor to convert",
                },
                "package_path": {
                    "type": "string",
                    "description": "Content folder path for the blueprint",
                },
                "blueprint_name": {
                    "type": "string",
                    "description": "Name for the blueprint",
                },
                "replace_existing": {
                    "type": "boolean",
                    "description": "Overwrite if exists",
                    "default": False,
                },
            },
            "required": ["actor_id", "package_path", "blueprint_name"],
        },
    },
    {
        "name": "duplicate_asset",
        "description": "Create a copy of an existing asset. Editor only.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "source_path": {
                    "type": "string",
                    "description": "Source asset path",
                },
                "dest_package_path": {
                    "type": "string",
                    "description": "Destination folder",
                },
                "dest_asset_name": {
                    "type": "string",
                    "description": "Name for the copy",
                },
            },
            "required": ["source_path", "dest_package_path", "dest_asset_name"],
        },
    },

    # =========================================================================
    # Component Operations (P1)
    # =========================================================================
    {
        "name": "get_component_transform",
        "description": "Get the transform of a specific component on an actor.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "actor_id": {
                    "type": "string",
                    "description": "Actor identifier",
                },
                "component_name": {
                    "type": "string",
                    "description": "Component instance name (e.g., 'LightComponent0')",
                },
                "world_space": {
                    "type": "boolean",
                    "description": "True for world coordinates, false for relative",
                    "default": True,
                },
            },
            "required": ["actor_id", "component_name"],
        },
    },
    {
        "name": "set_component_transform",
        "description": "Set the transform of a specific component.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "actor_id": {
                    "type": "string",
                    "description": "Actor identifier",
                },
                "component_name": {
                    "type": "string",
                    "description": "Component instance name",
                },
                "location": {
                    "type": "array",
                    "items": {"type": "number"},
                    "description": "Location [X, Y, Z]",
                },
                "rotation": {
                    "type": "array",
                    "items": {"type": "number"},
                    "description": "Rotation [Pitch, Yaw, Roll]",
                },
                "scale": {
                    "type": "array",
                    "items": {"type": "number"},
                    "description": "Scale [X, Y, Z]",
                },
                "world_space": {
                    "type": "boolean",
                    "description": "True for world coordinates",
                    "default": True,
                },
            },
            "required": ["actor_id", "component_name"],
        },
    },
    {
        "name": "attach_actor",
        "description": "Attach one actor to another (parent-child relationship).",
        "inputSchema": {
            "type": "object",
            "properties": {
                "child_actor_id": {
                    "type": "string",
                    "description": "Actor to attach",
                },
                "parent_actor_id": {
                    "type": "string",
                    "description": "Actor to attach to",
                },
                "parent_component_name": {
                    "type": "string",
                    "description": "Specific component (default: root)",
                },
                "socket_name": {
                    "type": "string",
                    "description": "Optional socket name",
                },
                "location_rule": {
                    "type": "string",
                    "enum": ["KeepRelative", "KeepWorld", "SnapToTarget"],
                    "description": "How to handle location",
                    "default": "KeepWorld",
                },
            },
            "required": ["child_actor_id", "parent_actor_id"],
        },
    },
    {
        "name": "detach_actor",
        "description": "Detach an actor from its parent.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "actor_id": {
                    "type": "string",
                    "description": "Actor to detach",
                },
                "maintain_world_position": {
                    "type": "boolean",
                    "description": "Keep current world position",
                    "default": True,
                },
            },
            "required": ["actor_id"],
        },
    },

    # =========================================================================
    # File Operations (P1)
    # =========================================================================
    {
        "name": "read_project_file",
        "description": "Read a file from the project directory. Constrained to safe paths.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "relative_path": {
                    "type": "string",
                    "description": "Path relative to project root",
                },
                "as_base64": {
                    "type": "boolean",
                    "description": "Return binary content as base64",
                    "default": False,
                },
            },
            "required": ["relative_path"],
        },
    },
    {
        "name": "write_project_file",
        "description": "Write a file to the project directory. Constrained to safe paths.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "relative_path": {
                    "type": "string",
                    "description": "Path relative to project root",
                },
                "content": {
                    "type": "string",
                    "description": "Content to write",
                },
                "is_base64": {
                    "type": "boolean",
                    "description": "Content is base64 encoded",
                    "default": False,
                },
                "create_directories": {
                    "type": "boolean",
                    "description": "Create parent directories",
                    "default": True,
                },
                "append": {
                    "type": "boolean",
                    "description": "Append instead of overwrite",
                    "default": False,
                },
            },
            "required": ["relative_path", "content"],
        },
    },
    {
        "name": "list_project_directory",
        "description": "List files in a project directory. Constrained to safe paths.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "relative_path": {
                    "type": "string",
                    "description": "Path relative to project root",
                },
                "pattern": {
                    "type": "string",
                    "description": "Glob pattern (e.g., '*.uasset')",
                },
                "recursive": {
                    "type": "boolean",
                    "description": "Include subdirectories",
                    "default": False,
                },
                "limit": {
                    "type": "integer",
                    "description": "Maximum files to return",
                    "default": 100,
                },
            },
            "required": [],
        },
    },
    {
        "name": "copy_project_file",
        "description": "Copy a file within the project directory. Constrained to safe paths.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "source_path": {
                    "type": "string",
                    "description": "Source path relative to project root",
                },
                "dest_path": {
                    "type": "string",
                    "description": "Destination path relative to project root",
                },
                "overwrite": {
                    "type": "boolean",
                    "description": "Overwrite if destination exists",
                    "default": False,
                },
            },
            "required": ["source_path", "dest_path"],
        },
    },
    # =========================================================================
    # Component Operations (P1)
    # =========================================================================
    {
        "name": "attach_component",
        "description": "Attach a component to another component within the same actor.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "actor_id": {
                    "type": "string",
                    "description": "Actor containing the components",
                },
                "component_name": {
                    "type": "string",
                    "description": "Component to attach",
                },
                "parent_component_name": {
                    "type": "string",
                    "description": "Parent component to attach to",
                },
                "socket_name": {
                    "type": "string",
                    "description": "Socket on parent to attach to (optional)",
                },
                "location_rule": {
                    "type": "string",
                    "enum": ["keep_relative", "keep_world", "snap_to_target"],
                    "description": "How to handle location",
                    "default": "keep_relative",
                },
                "rotation_rule": {
                    "type": "string",
                    "enum": ["keep_relative", "keep_world", "snap_to_target"],
                    "description": "How to handle rotation",
                    "default": "keep_relative",
                },
                "scale_rule": {
                    "type": "string",
                    "enum": ["keep_relative", "keep_world", "snap_to_target"],
                    "description": "How to handle scale",
                    "default": "keep_relative",
                },
            },
            "required": ["actor_id", "component_name", "parent_component_name"],
        },
    },
    {
        "name": "attach_actor",
        "description": "Attach an actor to another actor (child becomes component of parent).",
        "inputSchema": {
            "type": "object",
            "properties": {
                "child_actor_id": {
                    "type": "string",
                    "description": "Actor to attach",
                },
                "parent_actor_id": {
                    "type": "string",
                    "description": "Actor to attach to",
                },
                "parent_component_name": {
                    "type": "string",
                    "description": "Component on parent to attach to (optional, defaults to root)",
                },
                "socket_name": {
                    "type": "string",
                    "description": "Socket on parent component (optional)",
                },
                "location_rule": {
                    "type": "string",
                    "enum": ["keep_relative", "keep_world", "snap_to_target"],
                    "description": "How to handle location",
                    "default": "keep_world",
                },
                "rotation_rule": {
                    "type": "string",
                    "enum": ["keep_relative", "keep_world", "snap_to_target"],
                    "description": "How to handle rotation",
                    "default": "keep_world",
                },
                "scale_rule": {
                    "type": "string",
                    "enum": ["keep_relative", "keep_world", "snap_to_target"],
                    "description": "How to handle scale",
                    "default": "keep_world",
                },
            },
            "required": ["child_actor_id", "parent_actor_id"],
        },
    },
    {
        "name": "detach_component",
        "description": "Detach a component from its parent, making it a root component.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "actor_id": {
                    "type": "string",
                    "description": "Actor containing the component",
                },
                "component_name": {
                    "type": "string",
                    "description": "Component to detach",
                },
                "maintain_world_transform": {
                    "type": "boolean",
                    "description": "Keep world position after detach",
                    "default": True,
                },
            },
            "required": ["actor_id", "component_name"],
        },
    },

]


@dataclass
class ActorInfo:
    """Information about an actor in the world."""
    guid: str
    path: str
    name: str
    label: str
    class_name: str
    location: Tuple[float, float, float]
    rotation: Tuple[float, float, float]
    scale: Tuple[float, float, float]
    is_hidden: bool
    parent_actor_id: str


class AgentBridgeClient:
    """Client for AgentBridge gRPC service."""

    def __init__(self, host: str = "localhost", port: int = 50051):
        self.channel = create_channel(host, port)
        self.stub = pb_grpc.AgentBridgeServiceStub(self.channel)

    def _make_vector(self, x: float, y: float, z: float):
        return Geometry_pb2.Vector(x=x, y=y, z=z)

    def _make_rotation(self, pitch: float, yaw: float, roll: float):
        return Geometry_pb2.Rotation(r=roll, p=pitch, y=yaw)

    def _parse_actor_descriptor(self, desc) -> ActorInfo:
        transform = desc.transform
        return ActorInfo(
            guid=desc.guid,
            path=desc.path,
            name=desc.name,
            label=desc.label,
            class_name=desc.class_name,
            location=(transform.location.x, transform.location.y, transform.location.z),
            rotation=(transform.rotation.p, transform.rotation.y, transform.rotation.r),
            scale=(transform.scale.x, transform.scale.y, transform.scale.z),
            is_hidden=desc.is_hidden,
            parent_actor_id=desc.parent_actor_id,
        )

    def list_worlds(self):
        return self.stub.ListWorlds(pb.ListWorldsRequest())

    def set_target_world(self, world_identifier: str):
        return self.stub.SetTargetWorld(pb.SetTargetWorldRequest(world_identifier=world_identifier))

    def query_actors(self, class_name="", name_pattern="", label_pattern="", tag="", limit=100, include_hidden=False):
        return self.stub.QueryActors(pb.QueryActorsRequest(
            class_name=class_name,
            name_pattern=name_pattern,
            label_pattern=label_pattern,
            tag=tag,
            limit=limit,
            include_hidden=include_hidden,
        ))

    def get_actor(self, actor_id: str, include_properties=False, include_components=False, property_depth=1):
        return self.stub.GetActor(pb.GetActorRequest(
            actor_id=actor_id,
            include_properties=include_properties,
            include_components=include_components,
            property_depth=property_depth,
        ))

    def spawn_actor(self, class_name: str, location=(0,0,0), rotation=(0,0,0), scale=(1,1,1), label="", folder_path=""):
        transform = pb.ActorTransform(
            location=self._make_vector(*location),
            rotation=self._make_rotation(*rotation),
            scale=pb.Scale(x=scale[0], y=scale[1], z=scale[2]),
        )
        return self.stub.SpawnActor(pb.SpawnActorRequest(
            class_name=class_name,
            transform=transform,
            label=label,
            folder_path=folder_path,
        ))

    def delete_actor(self, actor_id: str):
        return self.stub.DeleteActor(pb.DeleteActorRequest(actor_id=actor_id))

    def duplicate_actor(self, actor_id: str, location=None, rotation=None, scale=None, new_label=""):
        # Build transform only if any transform parameters are provided
        transform = None
        if location or rotation or scale:
            transform = pb.ActorTransform()
            if location:
                transform.location.CopyFrom(self._make_vector(*location))
            if rotation:
                transform.rotation.CopyFrom(self._make_rotation(*rotation))
            if scale:
                transform.scale.CopyFrom(pb.Scale(x=scale[0], y=scale[1], z=scale[2]))

        request = pb.DuplicateActorRequest(actor_id=actor_id, new_label=new_label)
        if transform:
            request.new_transform.CopyFrom(transform)
        return self.stub.DuplicateActor(request)

    def set_actor_transform(self, actor_id: str, location=None, rotation=None, scale=None, sweep=False):
        transform = pb.ActorTransform()
        if location:
            transform.location.CopyFrom(self._make_vector(*location))
        if rotation:
            transform.rotation.CopyFrom(self._make_rotation(*rotation))
        if scale:
            transform.scale.CopyFrom(pb.Scale(x=scale[0], y=scale[1], z=scale[2]))
        return self.stub.SetActorTransform(pb.SetActorTransformRequest(
            actor_id=actor_id,
            transform=transform,
            sweep=sweep,
        ))

    def get_property(self, actor_id: str, path: str):
        return self.stub.GetPropertyPath(pb.GetPropertyPathRequest(actor_id=actor_id, path=path))

    def set_property(self, actor_id: str, path: str, value: str):
        return self.stub.SetPropertyPath(pb.SetPropertyPathRequest(
            actor_id=actor_id,
            path=path,
            value=pb.PropertyValue(string_value=value),
        ))

    def list_classes(self, base_class_name="Actor", name_pattern="", include_blueprint=True, include_abstract=False, limit=100):
        return self.stub.ListClasses(pb.ListClassesRequest(
            base_class_name=base_class_name,
            name_pattern=name_pattern,
            include_blueprint=include_blueprint,
            include_abstract=include_abstract,
            limit=limit,
        ))

    def get_class_schema(self, class_name: str, include_inherited=True, include_functions=False):
        return self.stub.GetClassSchema(pb.GetClassSchemaRequest(
            class_name=class_name,
            include_inherited=include_inherited,
            include_functions=include_functions,
        ))

    # Static function invocation
    def call_static_function(self, class_name: str, function_name: str, parameters: dict = None):
        """Call a static Blueprint library function."""
        request = pb.CallFunctionRequest(
            actor_id="",  # Empty for static functions
            class_name=class_name,
            function_name=function_name,
        )
        # Add parameters if provided
        if parameters:
            for key, value in parameters.items():
                kv = request.parameters.add()
                kv.key = key
                _set_property_value(kv.value, value)
        return self.stub.CallFunction(request)

    # World Partition methods
    def is_world_partitioned(self):
        return self.stub.IsWorldPartitioned(pb.IsWorldPartitionedRequest())

    def query_all_actors(self, class_name="", name_pattern="", include_loaded=True,
                         include_unloaded=True, data_layer="", limit=100):
        return self.stub.QueryAllActors(pb.QueryAllActorsRequest(
            class_name=class_name,
            name_pattern=name_pattern,
            include_loaded=include_loaded,
            include_unloaded=include_unloaded,
            data_layer=data_layer,
            limit=limit,
        ))

    def get_streaming_state(self, actor_guid: str):
        return self.stub.GetStreamingState(pb.GetStreamingStateRequest(actor_guid=actor_guid))

    def query_landscape(self, include_unloaded=True):
        return self.stub.QueryLandscape(pb.QueryLandscapeRequest(include_unloaded=include_unloaded))

    def get_landscape_bounds(self):
        return self.stub.GetLandscapeBounds(pb.GetLandscapeBoundsRequest())

    def get_data_layers(self):
        return self.stub.GetDataLayers(pb.GetDataLayersRequest())

    def get_actors_in_data_layer(self, data_layer: str, include_unloaded=True, limit=100):
        return self.stub.GetActorsInDataLayer(pb.GetActorsInDataLayerRequest(
            data_layer=data_layer,
            include_unloaded=include_unloaded,
            limit=limit,
        ))

    def execute_console_command(self, command: str):
        return self.stub.ExecuteConsoleCommand(pb.ExecuteConsoleCommandRequest(command=command))

    def search_console_commands(self, keyword: str, limit: int = 50, offset: int = 0, search_help: bool = False):
        return self.stub.SearchConsoleCommands(pb.SearchConsoleCommandsRequest(
            keyword=keyword,
            limit=limit,
            offset=offset,
            search_help=search_help,
        ))

    # Asset Operations (P0)
    def create_asset(self, asset_class: str, package_path: str, asset_name: str,
                     parent_asset_path: str = "", properties: dict = None):
        request = pb.CreateAssetRequest(
            asset_class=asset_class,
            package_path=package_path,
            asset_name=asset_name,
            parent_asset_path=parent_asset_path,
        )
        if properties:
            for key, value in properties.items():
                kv = request.properties.add()
                kv.key = key
                _set_property_value(kv.value, value)
        return self.stub.CreateAsset(request)

    def save_asset(self, asset_path: str, prompt_for_checkout: bool = False):
        return self.stub.SaveAsset(pb.SaveAssetRequest(
            asset_path=asset_path,
            prompt_for_checkout=prompt_for_checkout,
        ))

    def save_actor_as_blueprint(self, actor_id: str, package_path: str, blueprint_name: str,
                                replace_existing: bool = False):
        return self.stub.SaveActorAsBlueprint(pb.SaveActorAsBlueprintRequest(
            actor_id=actor_id,
            package_path=package_path,
            blueprint_name=blueprint_name,
            replace_existing=replace_existing,
        ))

    def duplicate_asset(self, source_path: str, dest_package_path: str, dest_asset_name: str):
        return self.stub.DuplicateAsset(pb.DuplicateAssetRequest(
            source_path=source_path,
            dest_package_path=dest_package_path,
            dest_asset_name=dest_asset_name,
        ))

    # Component Operations (P1)
    def get_component_transform(self, actor_id: str, component_name: str, world_space: bool = True):
        return self.stub.GetComponentTransform(pb.GetComponentTransformRequest(
            actor_id=actor_id,
            component_name=component_name,
            world_space=world_space,
        ))

    def set_component_transform(self, actor_id: str, component_name: str,
                                location=None, rotation=None, scale=None,
                                world_space: bool = True, sweep: bool = False):
        transform = pb.ActorTransform()
        if location:
            transform.location.CopyFrom(self._make_vector(*location))
        if rotation:
            transform.rotation.CopyFrom(self._make_rotation(*rotation))
        if scale:
            transform.scale.CopyFrom(pb.Scale(x=scale[0], y=scale[1], z=scale[2]))
        return self.stub.SetComponentTransform(pb.SetComponentTransformRequest(
            actor_id=actor_id,
            component_name=component_name,
            transform=transform,
            world_space=world_space,
            sweep=sweep,
        ))

    def attach_actor(self, child_actor_id: str, parent_actor_id: str,
                     parent_component_name: str = "", socket_name: str = "",
                     location_rule: str = "KeepWorld"):
        rule_map = {
            "KeepRelative": pb.ATTACHMENT_RULE_KEEP_RELATIVE,
            "KeepWorld": pb.ATTACHMENT_RULE_KEEP_WORLD,
            "SnapToTarget": pb.ATTACHMENT_RULE_SNAP_TO_TARGET,
        }
        rule = rule_map.get(location_rule, pb.ATTACHMENT_RULE_KEEP_WORLD)
        return self.stub.AttachActor(pb.AttachActorRequest(
            child_actor_id=child_actor_id,
            parent_actor_id=parent_actor_id,
            parent_component_name=parent_component_name,
            socket_name=socket_name,
            location_rule=rule,
            rotation_rule=rule,
            scale_rule=rule,
        ))

    def detach_actor(self, actor_id: str, maintain_world_position: bool = True):
        return self.stub.DetachActor(pb.DetachActorRequest(
            actor_id=actor_id,
            maintain_world_position=maintain_world_position,
        ))

    # File Operations (P1)
    def read_project_file(self, relative_path: str, as_base64: bool = False, max_bytes: int = 0):
        return self.stub.ReadProjectFile(pb.ReadProjectFileRequest(
            relative_path=relative_path,
            as_base64=as_base64,
            max_bytes=max_bytes,
        ))

    def write_project_file(self, relative_path: str, content: str, is_base64: bool = False,
                           create_directories: bool = True, append: bool = False):
        return self.stub.WriteProjectFile(pb.WriteProjectFileRequest(
            relative_path=relative_path,
            content=content,
            is_base64=is_base64,
            create_directories=create_directories,
            append=append,
        ))

    def list_project_directory(self, relative_path: str = "", pattern: str = "",
                               recursive: bool = False, limit: int = 100):
        return self.stub.ListProjectDirectory(pb.ListProjectDirectoryRequest(
            relative_path=relative_path,
            pattern=pattern,
            recursive=recursive,
            limit=limit,
        ))

    def copy_project_file(self, source_path: str, dest_path: str,
                          overwrite: bool = False):
        return self.stub.CopyProjectFile(pb.CopyProjectFileRequest(
            source_path=source_path,
            dest_path=dest_path,
            overwrite=overwrite,
        ))

    # -------------------------------------------------------------------------
    # Component Operations
    # -------------------------------------------------------------------------

    def attach_component(self, actor_id: str, component_name: str,
                         parent_component_name: str, socket_name: str = "",
                         location_rule: str = "keep_relative",
                         rotation_rule: str = "keep_relative",
                         scale_rule: str = "keep_relative"):
        return self.stub.AttachComponent(pb.AttachComponentRequest(
            actor_id=actor_id,
            component_name=component_name,
            parent_component_name=parent_component_name,
            socket_name=socket_name,
            location_rule=_string_to_attachment_rule(location_rule),
            rotation_rule=_string_to_attachment_rule(rotation_rule),
            scale_rule=_string_to_attachment_rule(scale_rule),
        ))

    def attach_actor(self, child_actor_id: str, parent_actor_id: str,
                     parent_component_name: str = "", socket_name: str = "",
                     location_rule: str = "keep_world",
                     rotation_rule: str = "keep_world",
                     scale_rule: str = "keep_world"):
        return self.stub.AttachActor(pb.AttachActorRequest(
            child_actor_id=child_actor_id,
            parent_actor_id=parent_actor_id,
            parent_component_name=parent_component_name,
            socket_name=socket_name,
            location_rule=_string_to_attachment_rule(location_rule),
            rotation_rule=_string_to_attachment_rule(rotation_rule),
            scale_rule=_string_to_attachment_rule(scale_rule),
        ))

    def detach_component(self, actor_id: str, component_name: str,
                         maintain_world_transform: bool = True):
        return self.stub.DetachComponent(pb.DetachComponentRequest(
            actor_id=actor_id,
            component_name=component_name,
            maintain_world_transform=maintain_world_transform,
        ))


def _string_to_attachment_rule(rule: str) -> int:
    """Convert string attachment rule to proto enum value."""
    rules = {
        "keep_relative": pb.ATTACHMENT_RULE_KEEP_RELATIVE,
        "keep_world": pb.ATTACHMENT_RULE_KEEP_WORLD,
        "snap_to_target": pb.ATTACHMENT_RULE_SNAP_TO_TARGET,
    }
    return rules.get(rule.lower(), pb.ATTACHMENT_RULE_KEEP_RELATIVE)


def _string_to_detachment_rule(rule: str) -> int:
    """Convert string detachment rule to proto enum value."""
    rules = {
        "keep_relative": pb.ATTACHMENT_RULE_KEEP_RELATIVE,
        "keep_world": pb.ATTACHMENT_RULE_KEEP_WORLD,
    }
    return rules.get(rule.lower(), pb.ATTACHMENT_RULE_KEEP_WORLD)


def _normalize_property_value(value: any, property_hint: str = "") -> str:
    """
    Normalize a property value to Unreal's expected string format.

    Accepts flexible input formats and converts to Unreal-parseable strings:

    Colors (detects if property_hint contains 'color' or value has r/g/b):
      - [1, 0, 0] -> "(R=1.0,G=0.0,B=0.0,A=1.0)"
      - [1, 0, 0, 0.5] -> "(R=1.0,G=0.0,B=0.0,A=0.5)"
      - {"r": 1, "g": 0, "b": 0} -> "(R=1.0,G=0.0,B=0.0,A=1.0)"
      - "#FF0000" -> "(R=1.0,G=0.0,B=0.0,A=1.0)"

    Vectors:
      - [1, 2, 3] -> "(X=1.0,Y=2.0,Z=3.0)"
      - {"x": 1, "y": 2, "z": 3} -> "(X=1.0,Y=2.0,Z=3.0)"

    Rotators (detects if property_hint contains 'rotation' or value has pitch/yaw/roll):
      - [0, 90, 0] -> "(Pitch=0.0,Yaw=90.0,Roll=0.0)"
      - {"pitch": 0, "yaw": 90, "roll": 0} -> "(Pitch=0.0,Yaw=90.0,Roll=0.0)"

    Booleans:
      - True/False -> "true"/"false"

    Everything else: str(value)
    """
    hint_lower = property_hint.lower()
    is_color_hint = 'color' in hint_lower
    is_rotation_hint = 'rotation' in hint_lower or 'rotator' in hint_lower

    # Already a string - check if it needs conversion
    if isinstance(value, str):
        # Hex color string
        if value.startswith('#') and len(value) in (7, 9):
            try:
                hex_str = value[1:]
                r = int(hex_str[0:2], 16) / 255.0
                g = int(hex_str[2:4], 16) / 255.0
                b = int(hex_str[4:6], 16) / 255.0
                a = int(hex_str[6:8], 16) / 255.0 if len(hex_str) == 8 else 1.0
                return f"(R={r},G={g},B={b},A={a})"
            except ValueError:
                pass
        # Already in Unreal format or other string
        return value

    # Boolean
    if isinstance(value, bool):
        return "true" if value else "false"

    # Number
    if isinstance(value, (int, float)):
        return str(value)

    # Dict - check keys to determine type
    if isinstance(value, dict):
        # Color dict
        if 'r' in value and 'g' in value and 'b' in value:
            r = float(value.get('r', 0))
            g = float(value.get('g', 0))
            b = float(value.get('b', 0))
            a = float(value.get('a', 1.0))
            return f"(R={r},G={g},B={b},A={a})"

        # Vector dict
        if 'x' in value and 'y' in value and 'z' in value:
            x = float(value.get('x', 0))
            y = float(value.get('y', 0))
            z = float(value.get('z', 0))
            return f"(X={x},Y={y},Z={z})"

        # Rotator dict
        if 'pitch' in value or 'yaw' in value or 'roll' in value:
            pitch = float(value.get('pitch', 0))
            yaw = float(value.get('yaw', 0))
            roll = float(value.get('roll', 0))
            return f"(Pitch={pitch},Yaw={yaw},Roll={roll})"

        # Generic dict - return as JSON string for proper double-quote format
        return json.dumps(value)

    # List/tuple - determine type from hint or length
    if isinstance(value, (list, tuple)):
        if len(value) == 3:
            # 3 elements - could be vector, color (RGB), or rotator
            v = [float(x) for x in value]
            if is_color_hint:
                return f"(R={v[0]},G={v[1]},B={v[2]},A=1.0)"
            elif is_rotation_hint:
                return f"(Pitch={v[0]},Yaw={v[1]},Roll={v[2]})"
            else:
                # Default to vector for 3-element list
                return f"(X={v[0]},Y={v[1]},Z={v[2]})"
        elif len(value) == 4:
            # 4 elements - assume color (RGBA)
            v = [float(x) for x in value]
            return f"(R={v[0]},G={v[1]},B={v[2]},A={v[3]})"
        else:
            # Other array - return as JSON string (NOT str() which uses single quotes!)
            # C++ JsonToPropertyValue expects valid JSON with double quotes
            return json.dumps(value)

    # Fallback - for dicts and other types, use JSON to ensure proper quoting
    if isinstance(value, (dict, list)):
        return json.dumps(value)
    return str(value)


def _extract_property_value(prop_value) -> Any:
    """
    Extract the typed value from a PropertyValue proto response.

    The proto has typed fields (float_value, int_value, vector_value, etc.)
    and we need to return the appropriate one based on the type enum.

    Property types (from proto):
      0: NONE, 1: BOOL, 2: INT, 3: FLOAT, 4: STRING
      5: NAME, 6: VECTOR, 7: ROTATOR, 8: TRANSFORM, 9: COLOR
      10: OBJECT, 11: CLASS, 12: STRUCT, 13: ARRAY
    """
    ptype = prop_value.type

    # NONE
    if ptype == 0:
        return None

    # BOOL
    elif ptype == 1:
        return prop_value.bool_value

    # INT
    elif ptype == 2:
        return prop_value.int_value

    # FLOAT
    elif ptype == 3:
        return prop_value.float_value

    # STRING, NAME (4, 5)
    elif ptype in (4, 5):
        return prop_value.string_value

    # VECTOR
    elif ptype == 6:
        v = prop_value.vector_value
        return {"x": v.x, "y": v.y, "z": v.z}

    # ROTATOR
    elif ptype == 7:
        r = prop_value.rotation_value
        return {"pitch": r.p, "yaw": r.y, "roll": r.r}

    # TRANSFORM
    elif ptype == 8:
        t = prop_value.transform_value
        return {
            "location": {"x": t.location.x, "y": t.location.y, "z": t.location.z},
            "rotation": {"pitch": t.rotation.p, "yaw": t.rotation.y, "roll": t.rotation.r},
            "scale": {"x": t.scale.x, "y": t.scale.y, "z": t.scale.z},
        }

    # COLOR
    elif ptype == 9:
        c = prop_value.color_value
        return {"r": c.r, "g": c.g, "b": c.b, "a": c.a if hasattr(c, 'a') else 1.0}

    # OBJECT, CLASS (10, 11)
    elif ptype in (10, 11):
        return prop_value.string_value  # Object path as string

    # STRUCT (12)
    elif ptype == 12:
        # Extract struct fields from struct_values (KeyValuePair repeated field)
        if hasattr(prop_value, 'struct_values') and prop_value.struct_values:
            return {kv.key: _extract_property_value(kv.value) for kv in prop_value.struct_values}
        # Fallback to string representation (e.g., "(X=0,Y=0,Z=100)")
        if prop_value.string_value:
            return prop_value.string_value
        return {}

    # ARRAY (13)
    elif ptype == 13:
        # Return array elements
        if hasattr(prop_value, 'array_values') and prop_value.array_values:
            return [_extract_property_value(elem) for elem in prop_value.array_values]
        return prop_value.string_value or []

    # Unknown - fallback to string_value
    else:
        return prop_value.string_value


def _enhance_property_error(error_dict: Dict[str, Any], property_path: str, actor_id: str) -> Dict[str, Any]:
    """
    Enhance a property error with helpful hints based on common mistakes.
    """
    error_msg = error_dict.get("error", "")

    # Common component class names that users might mistakenly use
    class_name_hints = {
        "PointLightComponent": "LightComponent0",
        "SpotLightComponent": "LightComponent0",
        "DirectionalLightComponent": "LightComponent0",
        "StaticMeshComponent": "StaticMeshComponent0",
        "SkeletalMeshComponent": "SkeletalMeshComponent",
        "CameraComponent": "CameraComponent0",
    }

    hints = []

    # Check if path starts with a class name (common mistake)
    path_parts = property_path.split(".")
    if path_parts:
        first_part = path_parts[0]
        if first_part in class_name_hints:
            hints.append(f"Use instance name '{class_name_hints[first_part]}' instead of class name '{first_part}'")
        elif first_part.endswith("Component") and not first_part[-1].isdigit():
            hints.append(f"Component names are instance names (e.g., 'LightComponent0'), not class names. Use get_actor('{actor_id}', include_components=True) to see component names.")

    # Check if this looks like a read-only property issue
    if "Failed to set" in error_msg:
        hints.append("This property may be read-only. For light colors, use tempo_set_color_property instead.")

    if hints:
        error_dict["hints"] = hints

    return error_dict


def _find_similar_actors(client, search_term: str, limit: int = 5) -> List[str]:
    """
    Find actors with names or labels similar to the search term.
    Used to provide helpful suggestions when an actor is not found.
    """
    suggestions = []

    # Generate search terms: full term + substrings for compound names
    # e.g., "MySkyLight" -> ["MySkyLight", "SkyLight", "Light"]
    search_terms = [search_term]

    # Try to extract meaningful substrings (CamelCase splitting)
    import re
    words = re.findall(r'[A-Z][a-z]*|[a-z]+', search_term)
    if len(words) > 1:
        # Add progressively shorter suffixes: MySkyLight -> SkyLight -> Light
        for i in range(1, len(words)):
            search_terms.append(''.join(words[i:]))

    # Search with each term until we have enough suggestions
    for term in search_terms:
        if len(suggestions) >= limit:
            break

        # Try label pattern search first (most user-friendly)
        try:
            result = client.query_actors(label_pattern=term, limit=limit)
            if result and hasattr(result, 'actors'):
                for actor in result.actors[:limit]:
                    label = actor.label or actor.name
                    if label not in suggestions:
                        suggestions.append(label)
                        if len(suggestions) >= limit:
                            break
        except Exception:
            pass

        # If not enough suggestions, try name pattern search
        if len(suggestions) < limit:
            try:
                result = client.query_actors(name_pattern=term, limit=limit - len(suggestions))
                if result and hasattr(result, 'actors'):
                    for actor in result.actors:
                        label = actor.label or actor.name
                        if label not in suggestions:
                            suggestions.append(label)
                            if len(suggestions) >= limit:
                                break
            except Exception:
                pass

    return suggestions[:limit]


def _normalize_asset_path(path: str) -> str:
    """
    Auto-fix asset paths by adding the .AssetName suffix if missing.

    Unreal asset paths have two forms:
    - Package path: '/Game/Folder/MyAsset' (what users type)
    - Object path: '/Game/Folder/MyAsset.MyAsset' (what API expects)

    For DataAssets and other assets, the object name typically matches
    the package name. This function auto-appends it when missing.

    Examples:
        '/Game/Biomes/Forest' -> '/Game/Biomes/Forest.Forest'
        '/Game/Biomes/Forest.Forest' -> '/Game/Biomes/Forest.Forest' (unchanged)
        '/Game/BP_Enemy.BP_Enemy_C' -> '/Game/BP_Enemy.BP_Enemy_C' (unchanged)
        'MyActor' -> 'MyActor' (not an asset path, unchanged)

    Args:
        path: Asset path or actor identifier

    Returns:
        Normalized path with .AssetName suffix if needed
    """
    if not path:
        return path

    # Only process asset paths (start with /Game/, /Script/, etc.)
    if not path.startswith("/"):
        return path

    # Get the last component (after the last /)
    last_slash = path.rfind("/")
    if last_slash == -1:
        return path

    final_part = path[last_slash + 1:]

    # Already has object name (contains a dot)
    if "." in final_part:
        return path

    # Auto-append the asset name
    # /Game/Folder/MyAsset -> /Game/Folder/MyAsset.MyAsset
    return f"{path}.{final_part}"


def _normalize_blueprint_class(class_name: str) -> str:
    """
    Normalize Blueprint class names by auto-appending _C suffix if needed.

    Blueprint classes have two objects:
    - BP_MyActor: The UBlueprint asset (editor-only)
    - BP_MyActor_C: The UBlueprintGeneratedClass (runtime class)

    When spawning or loading classes, you need the _C version.
    This function makes the API more forgiving by auto-appending _C
    when the class looks like a Blueprint path.

    Examples:
        '/Game/BP_Enemy.BP_Enemy' -> '/Game/BP_Enemy.BP_Enemy_C'
        '/Game/BP_Enemy.BP_Enemy_C' -> '/Game/BP_Enemy.BP_Enemy_C' (unchanged)
        'PointLight' -> 'PointLight' (unchanged, not a Blueprint)
        'BP_Enemy' -> 'BP_Enemy_C' (short name, assume Blueprint)
    """
    if not class_name:
        return class_name

    # Already has _C suffix - no change needed
    if class_name.endswith("_C"):
        return class_name

    # Check if it's a Blueprint path (contains /Game/ or other content paths)
    is_blueprint_path = (
        "/Game/" in class_name or
        "/Script/" in class_name or
        class_name.startswith("/") and "." in class_name
    )

    # Check if it's a short Blueprint name (starts with BP_ but no path)
    is_short_bp_name = (
        class_name.startswith("BP_") and
        "/" not in class_name
    )

    if is_blueprint_path:
        # Full path: /Game/BP_Enemy.BP_Enemy -> /Game/BP_Enemy.BP_Enemy_C
        return class_name + "_C"
    elif is_short_bp_name:
        # Short name: BP_Enemy -> BP_Enemy_C
        return class_name + "_C"

    # Not a Blueprint, return unchanged (e.g., "PointLight", "StaticMeshActor")
    return class_name


def connect(host: str, port: int) -> AgentBridgeClient:
    """Create an AgentBridgeClient."""
    return AgentBridgeClient(host, port)


def execute(client: AgentBridgeClient, tool_name: str, args: Dict[str, Any]) -> str:
    """Execute an agentbridge tool."""
    result = _execute_impl(client, tool_name, args)
    return json.dumps(result, indent=2, default=str)


def _actor_to_dict(actor: ActorInfo) -> Dict[str, Any]:
    """Convert ActorInfo to dictionary."""
    return {
        "name": actor.name,
        "label": actor.label,
        "class_name": actor.class_name,
        "guid": actor.guid,
        "location": list(actor.location),
        "rotation": list(actor.rotation),
        "scale": list(actor.scale),
        "is_hidden": actor.is_hidden,
    }


def _property_value_to_dict(pv) -> Any:
    """Convert PropertyValue protobuf to Python value."""
    # Import here to avoid circular imports
    # Geometry_pb2 already imported at top - use Geometry_pb2.Vector, Geometry_pb2.Rotation
    ProtoVector = Geometry_pb2.Vector
    ProtoRotation = Geometry_pb2.Rotation

    # PropertyType enum values from AgentBridge.proto
    PT_NONE = 0
    PT_BOOL = 1
    PT_INT = 2
    PT_FLOAT = 3
    PT_STRING = 4
    PT_NAME = 5
    PT_VECTOR = 6
    PT_ROTATOR = 7
    PT_TRANSFORM = 8
    PT_COLOR = 9
    PT_OBJECT = 10
    PT_CLASS = 11
    PT_STRUCT = 12
    PT_ARRAY = 13
    PT_MAP = 14
    PT_ENUM = 15

    t = pv.type
    if t == PT_NONE:
        return None
    elif t == PT_BOOL:
        return pv.bool_value
    elif t == PT_INT:
        return pv.int_value
    elif t in (PT_FLOAT,):
        return pv.float_value
    elif t in (PT_STRING, PT_NAME):
        return pv.string_value
    elif t == PT_VECTOR:
        v = pv.vector_value
        return {"x": v.x, "y": v.y, "z": v.z}
    elif t == PT_ROTATOR:
        r = pv.rotation_value
        return {"pitch": r.pitch, "yaw": r.yaw, "roll": r.roll}
    elif t == PT_TRANSFORM:
        tf = pv.transform_value
        return {
            "location": [tf.location.x, tf.location.y, tf.location.z],
            "rotation": [tf.rotation.pitch, tf.rotation.yaw, tf.rotation.roll],
            "scale": [tf.scale.x, tf.scale.y, tf.scale.z],
        }
    elif t == PT_COLOR:
        c = pv.color_value
        return {"r": c.r, "g": c.g, "b": c.b, "a": c.a}
    elif t in (PT_OBJECT, PT_CLASS):
        return pv.object_path if pv.object_path else None
    elif t == PT_STRUCT:
        return {kv.key: _property_value_to_dict(kv.value) for kv in pv.struct_values}
    elif t == PT_ARRAY:
        return [_property_value_to_dict(v) for v in pv.array_values]
    elif t == PT_MAP:
        return {kv.key: _property_value_to_dict(kv.value) for kv in pv.struct_values}
    elif t == PT_ENUM:
        return {"name": pv.enum_name, "value": pv.enum_value}
    else:
        return f"<unknown type {t}>"


def _set_property_value(pv, value) -> None:
    """Set a PropertyValue protobuf from a Python value."""
    if value is None:
        pv.type = 0  # PROPERTY_TYPE_NONE
    elif isinstance(value, bool):
        pv.type = 1  # PROPERTY_TYPE_BOOL
        pv.bool_value = value
    elif isinstance(value, int):
        pv.type = 2  # PROPERTY_TYPE_INT
        pv.int_value = value
    elif isinstance(value, float):
        pv.type = 3  # PROPERTY_TYPE_FLOAT
        pv.float_value = value
    elif isinstance(value, str):
        pv.type = 4  # PROPERTY_TYPE_STRING
        pv.string_value = value
    elif isinstance(value, dict):
        # Check for vector/rotator/color/transform patterns
        if 'x' in value and 'y' in value and 'z' in value and len(value) == 3:
            pv.type = 6  # PROPERTY_TYPE_VECTOR
            pv.vector_value.x = float(value['x'])
            pv.vector_value.y = float(value['y'])
            pv.vector_value.z = float(value['z'])
        elif 'pitch' in value and 'yaw' in value and 'roll' in value:
            pv.type = 7  # PROPERTY_TYPE_ROTATOR
            pv.rotation_value.pitch = float(value['pitch'])
            pv.rotation_value.yaw = float(value['yaw'])
            pv.rotation_value.roll = float(value['roll'])
        elif 'r' in value and 'g' in value and 'b' in value:
            pv.type = 9  # PROPERTY_TYPE_COLOR
            pv.color_value.r = float(value.get('r', 0))
            pv.color_value.g = float(value.get('g', 0))
            pv.color_value.b = float(value.get('b', 0))
            pv.color_value.a = float(value.get('a', 1.0))
        else:
            # Generic struct
            pv.type = 12  # PROPERTY_TYPE_STRUCT
            for k, v in value.items():
                kv = pv.struct_values.add()
                kv.key = str(k)
                _set_property_value(kv.value, v)
    elif isinstance(value, (list, tuple)):
        pv.type = 13  # PROPERTY_TYPE_ARRAY
        for item in value:
            item_pv = pv.array_values.add()
            _set_property_value(item_pv, item)
    else:
        # Try to convert to string
        pv.type = 4  # PROPERTY_TYPE_STRING
        pv.string_value = str(value)


def _get_help_text(topic: str = "") -> Dict[str, Any]:
    """Generate help text for AI agents."""

    overview = """
AgentBridge - Unreal Engine control for AI agents

QUICK START:
1. query_actors - Find actors in the scene (e.g., name_pattern="Light*")
2. spawn_actor - Create new actors (e.g., class_name="PointLight", location=[0,0,500])
3. get_actor - Get detailed info about a specific actor
4. set_actor_transform - Move/rotate/scale actors
5. search_console_commands - Find commands by keyword (e.g., "shadow", "fps")

COMMON CLASSES:
- PointLight, SpotLight, DirectionalLight - Lights
- StaticMeshActor - Static geometry
- CameraActor - Cameras
- PlayerStart - Spawn points
- Blueprint: /Game/BP_Name.BP_Name (the _C suffix is auto-added!)

UNITS:
- Location: centimeters (100 = 1 meter)
- Rotation: degrees [Pitch, Yaw, Roll]
- Scale: multiplier [X, Y, Z] where 1.0 = normal

TIPS:
- Use query_actors first to explore what's in the scene
- Use get_class_schema to see what properties a class has
- Use search_console_commands if you need to do something unusual
- execute_console_command is the escape hatch for anything not covered

ADVANCED CAPABILITIES:
- list_classes(base_class_name="ActorComponent") - List component types
- get_class_schema(class_name="SceneCaptureComponent2D") - Works for ANY class
- call_static_function - Call Blueprint library functions (KismetRenderingLibrary, etc.)

ASSET & FILE OPERATIONS:
- create_asset - Create DataAssets, MaterialInstances, etc.
- save_asset - Save modified assets to disk
- save_actor_as_blueprint - Convert actor to reusable Blueprint
- read_project_file / write_project_file - Read/write files in project directory
- list_project_directory - List directory contents

IMPORTANT - COMPONENT NAMES:
- Components use INSTANCE names (LightComponent0), not class names (PointLightComponent)
- Use tempo_get_components(actor) to find the correct component name
- For colors, use tempo_set_color_property instead of set_property_path

Use help(topic='actors|properties|classes|console|workflows|pcg_volume|volume_sizing|bp_toolkit') for detailed help.
"""

    topics = {
        "actors": """
ACTOR OPERATIONS:

Finding actors:
- query_actors(class_name="PointLight") - Filter by type (recommended)
- query_actors(label_pattern="MainLight") - Filter by display label (NEW! Most useful!)
- query_actors(name_pattern="Door") - Filter by internal name
- query_actors(tag="Interactive") - Filter by tag
- get_actor(actor_id="MyLight", include_properties=True) - Full details

LABEL PATTERN vs NAME PATTERN:
- label_pattern: Matches display names (what you see in editor), e.g., "MainLight", "Floor"
- name_pattern: Matches internal names like "PointLight_UAID_123..."

TIP: Use label_pattern for most searches - it matches human-readable names!

Creating actors:
- spawn_actor(class_name="PointLight", location=[0,0,500], label="MyLight")
- spawn_actor(class_name="/Game/BP_Enemy.BP_Enemy", location=[100,0,0])
  (Note: _C suffix is auto-added for Blueprint classes)

Modifying actors:
- set_actor_transform(actor_id="MyLight", location=[100,200,300])
- set_property_path(actor_id="MyLight", path="LightComponent.Intensity", value=5000)
- delete_actor(actor_id="MyLight")

Identifying actors:
- actor_id can be: name, label, path, or GUID
- Labels are editor display names (human-readable)
- Names are internal unique identifiers
""",
        "properties": """
PROPERTY OPERATIONS:

Reading properties:
- get_actor(actor_id, include_properties=True) - All properties
- get_property(actor_id, path="RootComponent.RelativeLocation") - Specific path

Setting properties:
- set_property(actor_id, path="LightComponent0.Intensity", value="5000")
- set_property(actor_id, path="LightComponent0.LightColor", value=[1, 0, 0])

Property paths:
- Simple: "bHidden", "ActorLabel"
- Nested: "RootComponent.RelativeLocation.X"
- Array: "Materials[0]"
- Component: "LightComponent0.Intensity"

FLEXIBLE VALUE FORMATS:
set_property accepts multiple input formats and auto-converts to Unreal format:

Colors (auto-detected from path containing 'color' or RGBA keys):
- [1, 0, 0] -> Red (RGB, alpha=1.0)
- [1, 0, 0, 0.5] -> Red with 50% alpha (RGBA)
- {"r": 1, "g": 0, "b": 0} -> Red (dict format)
- "#FF0000" -> Red (hex format)

Vectors (default for 3-element lists):
- [100, 200, 300] -> location/offset
- {"x": 100, "y": 200, "z": 300} -> dict format

Rotators (auto-detected from path containing 'rotation'):
- [0, 90, 0] -> Pitch=0, Yaw=90, Roll=0
- {"pitch": 0, "yaw": 90, "roll": 0} -> dict format

Simple values:
- "5000" or 5000 for numbers
- true/false for booleans

CRITICAL - COMPONENT NAMING:
Component names are INSTANCE names, not class names!
- WRONG: "PointLightComponent.Intensity" (class name)
- RIGHT: "LightComponent0.Intensity" (instance name)

To find component instance names:
- get_actor(actor_id, include_components=True)
- Or use: tempo_get_components(actor="MyLight")

Common instance names:
- PointLight -> LightComponent0
- StaticMeshActor -> StaticMeshComponent0
- CameraActor -> CameraComponent0

WORKING WITH DATAASSETS:
set_property and get_property work with DataAssets, not just actors!

Use the full asset path as actor_id:
  get_property(actor_id="/Game/MyFolder/MyDataAsset.MyDataAsset",
               path="SomeProperty")

  set_property(actor_id="/Game/MyFolder/MyDataAsset.MyDataAsset",
               path="SomeProperty", value="NewValue")

This enables programmatic configuration of:
- BiomeDefinitionTemplate assets
- BiomeAssetTemplate assets
- Any PrimaryDataAsset subclass
- MaterialInstanceConstant (parent references)

Note: The double name format (AssetName.AssetName) is required for UObject resolution.

SETTING ARRAYS WITH OBJECT REFERENCES:
Arrays of structs containing object refs require a two-step process:

Step 1: Create array elements with simple properties only
  set_property(actor_id="MyActor", path="MyArray",
               value='[{"Enabled":true, "Weight":1.0}]')

Step 2: Set object references individually
  set_property(actor_id="MyActor", path="MyArray[0].Mesh",
               value="/Game/Meshes/SM_Tree.SM_Tree")

WRONG (will fail):
  set_property(path="MyArray",
      value='[{"Mesh":"/Game/..."}]')  # Object ref in initial set = FAIL

READING NESTED STRUCT PROPERTIES:
When reading nested struct properties:
  get_property(path="DefaultDefinition")
  # Returns: {} (empty, even when populated!)

  get_property(path="DefaultDefinition.BiomeName")
  # Returns: "Forest" (correct!)

Always use the full property path to read nested struct values.

Use get_class_schema(class_name) to discover available properties!
""",
        "classes": """
CLASS DISCOVERY:

Finding classes:
- list_classes(base_class_name="Light") - Find all light types
- list_classes(name_pattern="*Vehicle*") - Wildcard search
- find_class(class_name="PointLight") - Get class info
- get_class_schema(class_name, include_functions=True) - Full schema

Built-in classes (no path needed):
- PointLight, SpotLight, DirectionalLight, RectLight
- StaticMeshActor, SkeletalMeshActor
- CameraActor, CineCameraActor
- PlayerStart, TargetPoint, Note
- TriggerBox, TriggerSphere, BlockingVolume

Blueprint classes:
- /Game/Blueprints/BP_Enemy.BP_Enemy (path to the asset)
- /Game/Characters/BP_Player.BP_Player
- BP_Enemy (short name with BP_ prefix)

BLUEPRINT CLASS NORMALIZATION:
The _C suffix (for Blueprint Generated Class) is AUTOMATICALLY added!
You can use either format:
- /Game/BP_Enemy.BP_Enemy -> auto-converted to /Game/BP_Enemy.BP_Enemy_C
- BP_Enemy -> auto-converted to BP_Enemy_C
- /Game/BP_Enemy.BP_Enemy_C -> unchanged (already has _C)

This works for: spawn_actor, query_actors, get_class_schema, list_classes
""",
        "assets": """
ASSET & FILE OPERATIONS:

Creating assets WITH PROPERTIES:
- create_asset(asset_class="DataAsset", package_path="/Game/Data", asset_name="MyData",
               properties={"MyProperty": "value", "MyNumber": 42})
- create_asset(asset_class="MaterialInstanceConstant", package_path="/Game/Materials",
               asset_name="MI_Wood", parent_asset_path="/Game/Materials/M_Wood")

The 'properties' parameter sets initial values when creating DataAssets or custom assets.
Property names must match the asset class definition (use get_class_schema to check).

Saving assets:
- save_asset(asset_path="/Game/Data/MyData") - Save to disk (required to persist!)
- save_actor_as_blueprint(actor_id="MyActor", package_path="/Game/Blueprints",
                          blueprint_name="BP_MyActor") - Convert actor to Blueprint

Asset management:
- duplicate_asset(source_path="/Game/Data/MyData", dest_path="/Game/Data",
                  new_name="MyData_Copy")
- get_asset_thumbnail(asset_path="/Game/Meshes/Chair") - Get preview image (base64 PNG)

File operations (constrained to project directory):
- read_project_file(relative_path="Config/DefaultGame.ini") - Read text/binary file
- write_project_file(relative_path="Saved/MyData.json", content="...") - Write file
- list_project_directory(relative_path="Content/Blueprints") - List directory
- copy_project_file(source="A.txt", dest="B.txt") - Copy file

WORKFLOW - Creating a DataAsset:
1. create_asset(asset_class="MyDataAsset", package_path="/Game/Data",
                asset_name="Config1", properties={"Value": 100})
2. save_asset(asset_path="/Game/Data/Config1") - Persist to disk

IMPORTANT:
- All file paths are relative to project root
- File operations are sandboxed - cannot access files outside project
- Binary files are base64 encoded in transport
- Assets created but not saved will be lost when editor closes
""",
        "components": """
COMPONENT OPERATIONS:

Getting component transforms:
- get_component_transform(actor_id="MyActor", component_name="MeshComponent0")
- get_component_transform(actor_id, component_name, world_space=True)  # World coords

Setting component transforms:
- set_component_transform(actor_id, component_name, location=[100,0,0])
- set_component_transform(actor_id, component_name, rotation=[0,45,0], world_space=True)

Attaching actors to each other:
- attach_actor(child_actor_id="MovingLight", parent_actor_id="Vehicle")
- attach_actor(child_id, parent_id, parent_component_name="Turret", socket_name="GunMount")

Attachment rules (location_rule, rotation_rule, scale_rule):
- "keep_world" - Maintain world position (default for actors)
- "keep_relative" - Maintain relative offset (default for components)
- "snap_to_target" - Snap to parent's position

Attaching components within an actor:
- attach_component(actor_id, component_name="Light", parent_component_name="Arm")

Detaching:
- detach_actor(actor_id="MovingLight")  # Keep world position
- detach_component(actor_id, component_name, maintain_world_transform=True)

Common use cases:
- Attach light to moving vehicle: attach_actor(light, vehicle)
- Build component hierarchies: attach_component in sequence
- Reparent actors: detach + attach to new parent
""",
        "console": """
CONSOLE COMMANDS:

Discovery:
- search_console_commands(keyword="shadow") - Find shadow-related
- search_console_commands(keyword="fps", search_help=True) - Search descriptions too
- search_console_commands(keyword="r.", limit=20) - Find rendering CVars

Execution:
- execute_console_command(command="stat fps") - Show FPS overlay
- execute_console_command(command="r.Shadow.MaxResolution 2048") - Set CVar

Useful commands:
- stat fps / stat unit - Performance stats
- show collision - Toggle collision visualization
- viewmode lit/unlit/wireframe - Change view mode
- slomo 0.5 - Slow motion (0.0-1.0)

AgentBridge commands (for debugging):
- AgentBridge.ListWorlds - Show world contexts
- AgentBridge.QueryActors Light 10 - Quick actor search
- AgentBridge.DumpActor MyActor - Dump actor properties
""",
        "workflows": """
COMMON WORKFLOWS:

Building a simple scene:
1. query_actors() - See what's already there
2. spawn_actor(class_name="PointLight", location=[0,0,500], label="MainLight")
3. spawn_actor(class_name="StaticMeshActor", location=[0,0,0], label="Floor")
4. set_property_path("MainLight", "LightComponent0.Intensity", 10000)

Setting light colors (IMPORTANT - use typed tools!):
1. spawn_actor(class_name="PointLight", location=[0,0,500], label="MyLight")
2. tempo_get_components(actor="MyLight")  # Returns: LightComponent0
3. tempo_set_color_property(actor="MyLight", component="LightComponent0",
                            property="LightColor", r=255, g=0, b=0)
Note: Use tempo_set_color_property, NOT set_property_path for colors!

Finding and modifying actors:
1. query_actors(name_pattern="*Door*") - Find all doors
2. get_actor("Door_01", include_properties=True) - Inspect one
3. set_property_path("Door_01", "bLocked", True) - Modify it

Exploring available options:
1. list_classes(base_class_name="Light") - What lights exist?
2. get_class_schema("SpotLight", include_functions=True) - What can I set?
3. search_console_commands("shadow") - Any shadow settings?

World Partition (large worlds):
1. is_world_partitioned() - Check if WP is enabled
2. query_all_actors(include_unloaded=True) - Find unloaded actors
3. get_streaming_state(actor_guid) - Check if actor is loaded

Calling static Blueprint library functions:
1. get_class_schema("KismetSystemLibrary", include_functions=True) - See available functions
2. call_static_function("KismetSystemLibrary", "PrintString", {"InString": "Hello!"})
3. call_static_function("KismetMathLibrary", "Abs", {"A": -42})  # Returns {"return_value": 42}

Examples of useful static functions:
- KismetSystemLibrary::PrintString - Debug output
- KismetSystemLibrary::ExecuteConsoleCommand - Run console commands
- KismetRenderingLibrary::CreateRenderTarget2D - Create render targets
- KismetMathLibrary::* - Math operations

Creating and saving assets:
1. spawn_actor(class_name="PointLight", location=[0,0,500], label="MyLight")
2. set_property_path("MyLight", "LightComponent0.Intensity", 10000)
3. save_actor_as_blueprint("MyLight", "/Game/Blueprints", "BP_MyLight")
4. save_asset("/Game/Blueprints/BP_MyLight") - Save Blueprint to disk

Working with project files:
1. list_project_directory("Config") - See config files
2. read_project_file("Config/DefaultGame.ini") - Read config
3. write_project_file("Saved/MyBackup.json", '{"key": "value"}') - Write data

PCG BIOME WORKFLOW (Complete with DataAssets):
Setting up procedural content generation with biomes:

1. Get landscape bounds:
   bounds = get_landscape_bounds()
   # Returns: center, half_extents, min, max

2. Spawn biome actors at landscape center:
   spawn_actor(class_name="BP_PCGBiomeCore",
               location=bounds["center"], label="MyBiomeCore")
   spawn_actor(class_name="BP_PCGBiomeVolume",
               location=bounds["center"], label="MyBiomeVolume")

3. Size the volumes (ALWAYS verify component names first!):
   tempo_get_components(actor="MyBiomeVolume")  # Returns "BiomeVolume"

   tempo_set_vector_property(actor="MyBiomeVolume", component="BiomeVolume",
       property="BoxExtent",
       x=bounds["half_extents"][0],
       y=bounds["half_extents"][1],
       z=bounds["half_extents"][2] + 5000)  # Z margin for spawning

   tempo_set_vector_property(actor="MyBiomeVolume", component="BiomeVolume",
       property="RelativeScale3D", x=1, y=1, z=1)

4. Create and configure BiomeDefinition DataAsset:
   create_asset(asset_class="BiomeDefinitionTemplate",
                package_path="/Game/Biomes", asset_name="ForestBiome")

   # Use asset path as actor_id to set DataAsset properties!
   set_property(actor_id="/Game/Biomes/ForestBiome.ForestBiome",
                path="BiomeDefinition.BiomeName", value="Forest")
   set_property(actor_id="/Game/Biomes/ForestBiome.ForestBiome",
                path="BiomeDefinition.BiomePriority", value=10)
   set_property(actor_id="/Game/Biomes/ForestBiome.ForestBiome",
                path="BiomeDefinition.BiomeColor",
                value="(R=0.2,G=0.6,B=0.1,A=1.0)")

   save_asset(asset_path="/Game/Biomes/ForestBiome")

5. Create and configure BiomeAsset DataAsset:
   create_asset(asset_class="BiomeAssetTemplate",
                package_path="/Game/Biomes", asset_name="TreeAssets")

   # Two-step process: create array element first (no object refs)
   set_property(actor_id="/Game/Biomes/TreeAssets.TreeAssets",
                path="BiomeAssets",
                value='[{"Enabled":true, "AssetType":"Mesh", "Weight":1.0}]')

   # Then set object reference separately
   set_property(actor_id="/Game/Biomes/TreeAssets.TreeAssets",
                path="BiomeAssets[0].Mesh",
                value="/Game/Foliage/SM_Tree.SM_Tree")

   save_asset(asset_path="/Game/Biomes/TreeAssets")

6. Link DataAssets to the biome volume:
   set_property(actor_id="MyBiomeVolume", path="Definition",
                value="/Game/Biomes/ForestBiome.ForestBiome")

   set_property(actor_id="MyBiomeVolume", path="Assets",
                value='["/Game/Biomes/TreeAssets.TreeAssets"]')

7. PCG generation runs automatically or on editor interaction.

FUNCTION CALL LIMITATIONS:
tempo_call_function only supports functions with NO parameters and void return.
For functions like PCGComponent.Generate(bForce), use property setters instead
(e.g., set bRegenerateInEditor = true) or trigger via console commands.

SIZING VOLUMES TO LANDSCAPE - KEY PROPERTIES:
When you need to size a BoxComponent volume (PCGVolume, TriggerVolume, etc.):

1. Get bounds:
   bounds = get_landscape_bounds()
   # center: [X, Y, Z] center point
   # half_extents: [X, Y, Z] half-size in each axis
   # min: [X, Y, Z] minimum corner
   # max: [X, Y, Z] maximum corner

2. Find the component name:
   tempo_get_components(actor="MyVolume")
   # Common names: "Volume", "BoxComponent0", "CollisionComponent"
   # WARNING: BP classes may use custom names! E.g., BP_PCGBiomeVolume uses "BiomeVolume"
   # ALWAYS verify with tempo_get_components() first!

3. Set BoxExtent (HALF-SIZE in Unreal units/cm):
   tempo_set_vector_property(actor="MyVolume", component="Volume",
       property="BoxExtent", x=1000, y=1000, z=500)
   # This creates a 2000x2000x1000 volume!

4. Reset scale when using BoxExtent:
   tempo_set_vector_property(actor="MyVolume", component="Volume",
       property="RelativeScale3D", x=1, y=1, z=1)

COMMON MISTAKES:
- Using class name "BoxComponent" instead of instance name "Volume"
- Forgetting BoxExtent is HALF the total size
- Not resetting scale when manually setting BoxExtent
- Forgetting Z margin for PCG spawn variation

TIPS:
- Use get_landscape_bounds() to size PCG volumes correctly
- Create DataAssets with initial properties via create_asset()
- Use label_pattern to find PCG-spawned actors
- PCG regeneration may require editor commands or level reload
""",
        # Aliases for specific workflow sub-topics
        "pcg_volume": """
PCG VOLUME TYPES:

There are different PCG volume types with different component structures:

NATIVE PCGVolume (NOT recommended for biomes):
  - Uses BrushComponent (harder to resize programmatically)
  - spawn_actor(class_name="PCGVolume", ...)
  - Component name: "BrushComponent0"

BP_PCGBiomeVolume (RECOMMENDED for biomes):
  - Uses BoxComponent named "BiomeVolume"
  - spawn_actor(class_name="BP_PCGBiomeVolume", ...)
  - Has Definition, Assets, DefaultDefinition, LocalAssets properties
  - Component name: "BiomeVolume"

BP_PCGBiomeCore (main biome system):
  - Uses BoxComponent named "Volume"
  - Contains BiomeCore PCGComponent
  - spawn_actor(class_name="BP_PCGBiomeCore", ...)
  - Component name: "Volume"

SIZING BP VOLUMES:

1. GET LANDSCAPE BOUNDS:
   bounds = get_landscape_bounds()
   # Returns: center, half_extents, min, max

2. SPAWN THE VOLUME:
   spawn_actor(class_name="BP_PCGBiomeVolume", location=bounds["center"],
               label="MyBiomeVolume")

3. ALWAYS VERIFY COMPONENT NAME FIRST:
   tempo_get_components(actor="MyBiomeVolume")
   # Returns: "BiomeVolume" for BP_PCGBiomeVolume
   # Returns: "Volume" for BP_PCGBiomeCore

4. SET BOXEXTENT (CRITICAL - this is HALF-SIZE!):
   tempo_set_vector_property(actor="MyBiomeVolume", component="BiomeVolume",
       property="BoxExtent",
       x=bounds["half_extents"][0],
       y=bounds["half_extents"][1],
       z=bounds["half_extents"][2] + 5000)  # Add Z margin!

   # BoxExtent is HALF the actual volume size!
   # A BoxExtent of [1000, 1000, 500] creates a 2000x2000x1000 volume

5. RESET SCALE (when using BoxExtent directly):
   tempo_set_vector_property(actor="MyBiomeVolume", component="BiomeVolume",
       property="RelativeScale3D", x=1, y=1, z=1)

WHY ADD Z MARGIN?
PCG spawns content within the volume. Add 5000+ units to Z so spawned
objects can vary in height above the terrain surface.

COMMON MISTAKES:
- Using native PCGVolume (BrushComponent) instead of BP_PCGBiomeVolume (BoxComponent)
- Using "BoxComponent" (class) instead of "BiomeVolume" or "Volume" (instance name)
- Forgetting BoxExtent is HALF-SIZE, not full size
- Not resetting RelativeScale3D when setting BoxExtent
- Forgetting Z margin causes all spawns at exact terrain height
""",
        "volume_sizing": """
SIZING BOXCOMPONENT VOLUMES:

BoxComponent volumes (PCGVolume, TriggerVolume, BlockingVolume, etc.)
use BoxExtent for their size. Here's how to set them correctly:

KEY PROPERTIES ON BOXCOMPONENT:
- BoxExtent (FVector): HALF-SIZE in each axis
- RelativeScale3D (FVector): Scale multiplier (set to 1,1,1 when using BoxExtent)
- RelativeLocation (FVector): Offset from actor root

SIZING STEPS:

1. Find the component instance name:
   tempo_get_components(actor="MyVolume")
   # Common names: "Volume", "BoxComponent0", "CollisionComponent"
   # WARNING: BP classes may use custom names! E.g., BP_PCGBiomeVolume uses "BiomeVolume"
   # ALWAYS verify with tempo_get_components() first!

2. Set BoxExtent (HALF-SIZE):
   tempo_set_vector_property(actor="MyVolume", component="Volume",
       property="BoxExtent", x=1000, y=1000, z=500)
   # Creates a 2000x2000x1000 unit volume (double the extent!)

3. Reset scale:
   tempo_set_vector_property(actor="MyVolume", component="Volume",
       property="RelativeScale3D", x=1, y=1, z=1)

FOR LANDSCAPE COVERAGE:
   bounds = get_landscape_bounds()
   tempo_set_vector_property(actor="MyVolume", component="Volume",
       property="BoxExtent",
       x=bounds["half_extents"][0],
       y=bounds["half_extents"][1],
       z=bounds["half_extents"][2])

RELATIONSHIP: BoxExtent × Scale = Actual half-size
- If BoxExtent=[100,100,100] and Scale=[2,2,2], volume is 400x400x400
- To avoid confusion, set Scale to 1,1,1 and use BoxExtent directly

UNITS: All values in Unreal units (centimeters)
- 100 units = 1 meter
- Typical game level: 10000-50000 units per axis
- BoxExtent of [25000, 25000, 5000] covers a 500m × 500m × 100m volume
"""
    }

    # Check if bp_toolkit is available and add its workflows
    try:
        from . import get_all_services
        if "bp_toolkit" in get_all_services():
            topics["workflows"] += """

BP_TOOLKIT WORKFLOWS (Offline Asset Manipulation):
These tools work WITHOUT Unreal running - pure JSON manipulation via UAssetGUI.

Exporting and analyzing a Blueprint:
1. bp_export_asset(uasset_path="/Game/Blueprints/BP_Enemy.uasset")
   # Creates BP_Enemy.json next to the uasset
2. bp_detect_type(json_path="BP_Enemy.json")
   # Returns: {"asset_type": "Blueprint"}
3. bp_get_info(json_path="BP_Enemy.json")
   # Returns: exports count, imports, graphs, namemap size
4. bp_query(json_path="BP_Enemy.json", query_type="list-events")
   # Returns: BeginPlay, Tick, etc.

Modifying a DataAsset:
1. bp_export_asset(uasset_path="D:/Content/BiomeDefinitions/Forest.uasset")
2. bp_get_property(json_path="Forest.json", property_path="BiomeDefinition.BiomePriority")
   # Returns: {"value": 3}
3. bp_set_property(json_path="Forest.json", property_path="BiomeDefinition.BiomePriority", value=10)
4. bp_import_asset(json_path="Forest.json")
   # Converts back to .uasset - reload in editor to see changes

Cloning an asset with modifications:
1. bp_export_asset(uasset_path="D:/Content/Biomes/Forest.uasset")
2. bp_clone_asset(json_path="Forest.json", new_name="Desert")
   # Creates Desert.json with updated name/folder references
3. bp_set_property(json_path="Desert.json", property_path="BiomeDefinition.BiomeColor",
                   value={"R": 0.9, "G": 0.7, "B": 0.4, "A": 1.0})
4. bp_import_asset(json_path="Desert.json")
   # Creates Desert.uasset

Adding documentation comments to a Blueprint:
1. bp_export_asset(uasset_path="BP_Character.uasset")
2. bp_list_graphs(json_path="BP_Character.json")
   # Returns: EventGraph, Walk, Jump, etc.
3. bp_add_comment(json_path="BP_Character.json", graph_name="EventGraph",
                  text="TODO: Add death handling", x=0, y=-500, width=400, height=100)
4. bp_import_asset(json_path="BP_Character.json")

Searching and querying assets:
- bp_find(json_path="BP_Enemy.json", pattern="Health")
  # Searches namemap and exports for "Health"
- bp_query(json_path="BP_Enemy.json", query_type="variables")
  # Lists all variable Get/Set nodes
- bp_query(json_path="BT_AI.json", query_type="list-tasks")
  # Lists Behavior Tree task nodes
- bp_query(json_path="M_Wood.json", query_type="textures")
  # Lists texture references in material

Full Blueprint parsing with call graphs:
1. bp_export_asset(uasset_path="BP_ComplexCharacter.uasset")
2. bp_parse(json_path="BP_ComplexCharacter.json", output_dir="./parsed/")
   # Generates: call_graph.json, Mermaid diagrams, function docs

QUERY TYPES BY ASSET:
| Asset Type | Query Types |
|------------|-------------|
| Blueprint | list-events, list-functions, variables, comments, flow-tagged |
| PCG Graph | list-nodes, connections, input-output |
| Behavior Tree | list-tasks, list-decorators, blackboard |
| Material | textures, shader-inputs |
| Niagara | emitters, modules |
"""

            # Also add a dedicated bp_toolkit topic
            topics["bp_toolkit"] = """
BP_TOOLKIT - Offline Asset Manipulation Tools

These 14 tools work WITHOUT Unreal running. They manipulate UAssetAPI JSON exports
directly, using UAssetGUI for uasset <-> JSON conversion.

EXPORT/IMPORT:
- bp_export_asset(uasset_path) - Export .uasset to .json (uses UAssetGUI)
- bp_import_asset(json_path) - Import .json back to .uasset

ANALYSIS:
- bp_detect_type(json_path) - Detect asset type (Blueprint, PCG, DataAsset, etc.)
- bp_get_info(json_path) - Get summary (exports, imports, graphs, namemap)
- bp_list_properties(json_path, export_index=0) - List all properties with types
- bp_get_property(json_path, property_path) - Get property by path
- bp_find(json_path, pattern) - Search namemap and exports
- bp_query(json_path, query_type) - Type-specific queries

MODIFICATION:
- bp_set_property(json_path, property_path, value) - Modify property
- bp_clone_asset(json_path, new_name) - Clone with new name
- bp_add_comment(json_path, graph_name, text, x, y) - Add comment node
- bp_clone_node(json_path, node_name) - Clone existing node
- bp_list_graphs(json_path) - List graphs in Blueprint/PCG

PARSING:
- bp_parse(json_path, output_dir) - Full parsing with call graphs

PROPERTY PATH SYNTAX:
- Simple: "BiomePriority"
- Nested struct: "BiomeDefinition.BiomePriority"
- Array access: "BiomeAssets[0].Generator"
- Deep nesting: "BiomeAssets[0].FilterOptions.MinScale"

Note: Property names from Blueprints include GUID suffixes internally
(e.g., "BiomePriority_29_308259B0449F...") but you can use just the base name.

SETUP REQUIRED:
1. git submodule update --init --recursive
2. cd bp_toolkit/vendor/UAssetGUI && dotnet build -c Release
"""
    except ImportError:
        pass  # bp_toolkit not available, skip additional help

    topic = topic.lower().strip() if topic else ""

    if topic and topic in topics:
        return {"topic": topic, "help": topics[topic].strip()}
    elif topic:
        return {"error": f"Unknown topic '{topic}'", "available_topics": list(topics.keys())}
    else:
        return {"help": overview.strip(), "available_topics": list(topics.keys())}


def _execute_impl(client: AgentBridgeClient, tool_name: str, args: Dict[str, Any]) -> Any:
    """Implementation of tool execution."""

    if tool_name == "help":
        return _get_help_text(args.get("topic", ""))

    elif tool_name == "list_worlds":
        result = safe_call(client.list_worlds)
        if isinstance(result, dict) and "error" in result:
            return result
        return {
            "worlds": [
                {
                    "world_type": w.world_type,
                    "world_name": w.world_name,
                    "pie_instance": w.pie_instance,
                    "has_begun_play": w.has_begun_play,
                    "actor_count": w.actor_count,
                }
                for w in result.worlds
            ]
        }

    elif tool_name == "set_target_world":
        result = safe_call(client.set_target_world, args["world_identifier"])
        if isinstance(result, dict) and "error" in result:
            return result
        return {"success": True}

    elif tool_name == "query_actors":
        # Normalize Blueprint class names if filtering by class (auto-append _C suffix if needed)
        class_name = args.get("class_name", "")
        if class_name:
            class_name = _normalize_blueprint_class(class_name)
        result = safe_call(
            client.query_actors,
            class_name=class_name,
            name_pattern=args.get("name_pattern", ""),
            label_pattern=args.get("label_pattern", ""),
            tag=args.get("tag", ""),
            limit=args.get("limit", 100),
            include_hidden=args.get("include_hidden", False),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        actors = [client._parse_actor_descriptor(a) for a in result.actors]
        return {
            "count": len(actors),
            "actors": [_actor_to_dict(a) for a in actors],
        }

    elif tool_name == "get_actor":
        result = safe_call(
            client.get_actor,
            actor_id=args["actor_id"],
            include_properties=args.get("include_properties", False),
            include_components=args.get("include_components", False),
        )
        if isinstance(result, dict) and "error" in result:
            # Enhance error with suggestions for finding the actor
            actor_id = args["actor_id"]
            result["hint"] = "Use query_actors(label_pattern='...') to find actors by display name"
            suggestions = _find_similar_actors(client, actor_id, limit=5)
            if suggestions:
                result["similar_actors"] = suggestions
            return result
        if result.HasField("actor"):
            actor = client._parse_actor_descriptor(result.actor.actor_info)
            response = {"found": True, "actor": _actor_to_dict(actor)}

            # Include properties if requested and present
            if result.actor.properties:
                response["properties"] = {
                    kv.key: _property_value_to_dict(kv.value)
                    for kv in result.actor.properties
                }

            # Include components if requested and present
            if result.actor.components:
                response["components"] = [
                    {
                        "name": c.name,
                        "class_name": c.class_name,
                        "is_scene_component": c.is_scene_component,
                    }
                    for c in result.actor.components
                ]

            # Include tags if present
            if result.actor.tags:
                response["tags"] = list(result.actor.tags)

            # Include folder path if present
            if result.actor.folder_path:
                response["folder_path"] = result.actor.folder_path

            return response
        # Fallback - actor not found (shouldn't normally reach here since gRPC returns NOT_FOUND)
        return {"found": False, "error": f"Actor '{args['actor_id']}' not found"}

    elif tool_name == "spawn_actor":
        # Normalize Blueprint class names (auto-append _C suffix if needed)
        class_name = _normalize_blueprint_class(args["class_name"])
        result = safe_call(
            client.spawn_actor,
            class_name=class_name,
            location=tuple(args.get("location", [0, 0, 0])),
            rotation=tuple(args.get("rotation", [0, 0, 0])),
            scale=tuple(args.get("scale", [1, 1, 1])),
            label=args.get("label", ""),
            folder_path=args.get("folder_path", ""),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        if result.HasField("spawned_actor"):
            actor = client._parse_actor_descriptor(result.spawned_actor)
            return {"success": True, "actor": _actor_to_dict(actor)}
        return {
            "success": False,
            "error": f"Failed to spawn actor of class '{class_name}'",
            "hint": "Check that the class exists. Use list_classes(name_pattern='...') to search. For Blueprints, use format '/Game/Path/BP_Name.BP_Name' (the _C suffix is auto-added).",
            "common_classes": ["PointLight", "SpotLight", "StaticMeshActor", "CameraActor", "PlayerStart"],
        }

    elif tool_name == "delete_actor":
        result = safe_call(client.delete_actor, args["actor_id"])
        if isinstance(result, dict) and "error" in result:
            return result
        return {"success": True}

    elif tool_name == "duplicate_actor":
        result = safe_call(
            client.duplicate_actor,
            actor_id=args["actor_id"],
            location=tuple(args["location"]) if "location" in args else None,
            rotation=tuple(args["rotation"]) if "rotation" in args else None,
            scale=tuple(args["scale"]) if "scale" in args else None,
            new_label=args.get("new_label", ""),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        if result.HasField("duplicated_actor"):
            actor = client._parse_actor_descriptor(result.duplicated_actor)
            return {"success": True, "actor": _actor_to_dict(actor)}
        return {"success": False, "error": "Failed to duplicate actor"}

    elif tool_name == "set_actor_transform":
        result = safe_call(
            client.set_actor_transform,
            actor_id=args["actor_id"],
            location=tuple(args["location"]) if "location" in args else None,
            rotation=tuple(args["rotation"]) if "rotation" in args else None,
            scale=tuple(args["scale"]) if "scale" in args else None,
        )
        if isinstance(result, dict) and "error" in result:
            return result
        return {"success": True}

    elif tool_name == "get_property":
        # Normalize asset paths: /Game/Foo/Asset -> /Game/Foo/Asset.Asset
        actor_id = _normalize_asset_path(args["actor_id"])
        result = safe_call(client.get_property, actor_id, args["path"])
        if isinstance(result, dict) and "error" in result:
            # If normalized path failed, try original path as fallback
            if actor_id != args["actor_id"]:
                result = safe_call(client.get_property, args["actor_id"], args["path"])
                if not (isinstance(result, dict) and "error" in result):
                    value = _extract_property_value(result.value)
                    return {"path": args["path"], "value": value, "type": result.type_name}
            return _enhance_property_error(result, args["path"], args["actor_id"])
        # Extract typed value from PropertyValue proto (float, int, vector, etc.)
        value = _extract_property_value(result.value)
        return {"path": args["path"], "value": value, "type": result.type_name}

    elif tool_name == "set_property":
        # Normalize asset paths: /Game/Foo/Asset -> /Game/Foo/Asset.Asset
        actor_id = _normalize_asset_path(args["actor_id"])
        # Normalize the value to Unreal's expected string format
        # This allows flexible input like [1,0,0] for colors or {"x":1,"y":2,"z":3} for vectors
        normalized_value = _normalize_property_value(args["value"], args["path"])
        result = safe_call(client.set_property, actor_id, args["path"], normalized_value)
        if isinstance(result, dict) and "error" in result:
            # If normalized path failed, try original path as fallback
            if actor_id != args["actor_id"]:
                result = safe_call(client.set_property, args["actor_id"], args["path"], normalized_value)
                if not (isinstance(result, dict) and "error" in result):
                    return {"success": True}
            return _enhance_property_error(result, args["path"], args["actor_id"])
        return {"success": True}

    elif tool_name == "list_classes":
        # Normalize Blueprint base class names (auto-append _C suffix if needed)
        base_class_name = args.get("base_class_name", "Actor")
        if base_class_name and base_class_name != "Actor":
            base_class_name = _normalize_blueprint_class(base_class_name)
        result = safe_call(
            client.list_classes,
            base_class_name=base_class_name,
            name_pattern=args.get("name_pattern", ""),
            include_blueprint=args.get("include_blueprint", True),
            limit=args.get("limit", 50),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        return {
            "count": len(result.classes),
            "classes": [
                {
                    "class_name": c.class_name,
                    "display_name": c.display_name,
                    "class_path": c.class_path,
                    "parent_class_name": c.parent_class_name,
                    "is_blueprint": c.is_blueprint,
                    "is_abstract": c.is_abstract,
                }
                for c in result.classes
            ],
        }

    elif tool_name == "get_class_schema":
        # Normalize Blueprint class names (auto-append _C suffix if needed)
        class_name = _normalize_blueprint_class(args["class_name"])
        result = safe_call(
            client.get_class_schema,
            class_name=class_name,
            include_inherited=args.get("include_inherited", True),
            include_functions=args.get("include_functions", False),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        schema = result.schema
        ci = schema.class_info
        return {
            "class_name": ci.class_name,
            "display_name": ci.display_name,
            "class_path": ci.class_path,
            "parent_class_name": ci.parent_class_name,
            "is_blueprint": ci.is_blueprint,
            "is_abstract": ci.is_abstract,
            "properties": [
                {
                    "name": p.name,
                    "display_name": p.display_name,
                    "type_name": p.type_name,
                    "element_type": p.element_type if p.element_type else None,
                    "category": p.category,
                    "is_read_only": p.is_read_only,
                    "is_blueprint_visible": p.is_blueprint_visible,
                }
                for p in schema.properties
            ],
            "functions": [
                {
                    "name": f.function_name,
                    "description": f.description,
                    "is_static": f.is_static,
                    "is_pure": f.is_pure,
                    "parameters": [
                        {"name": p.name, "type_name": p.type_name}
                        for p in f.parameters
                    ],
                    "return_type": f.return_value.type_name if f.HasField("return_value") else None,
                }
                for f in schema.functions
            ],
        }

    # Static Function Invocation
    elif tool_name == "call_static_function":
        result = safe_call(
            client.call_static_function,
            class_name=args["class_name"],
            function_name=args["function_name"],
            parameters=args.get("parameters", {}),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        response = {"success": True}

        # Parse return value if present
        if result.HasField("return_value") and result.return_value.type != 0:
            response["return_value"] = _property_value_to_dict(result.return_value)

        # Parse out parameters if present
        if result.out_parameters:
            response["out_parameters"] = {
                kv.key: _property_value_to_dict(kv.value)
                for kv in result.out_parameters
            }

        return response

    # World Partition & Streaming
    elif tool_name == "is_world_partitioned":
        result = safe_call(client.is_world_partitioned)
        if isinstance(result, dict) and "error" in result:
            return result
        return {
            "is_partitioned": result.is_partitioned,
            "world_name": result.world_name,
        }

    elif tool_name == "query_all_actors":
        result = safe_call(
            client.query_all_actors,
            class_name=args.get("class_name", ""),
            name_pattern=args.get("name_pattern", ""),
            include_loaded=args.get("include_loaded", True),
            include_unloaded=args.get("include_unloaded", True),
            data_layer=args.get("data_layer", ""),
            limit=args.get("limit", 100),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        return {
            "total_loaded": result.total_loaded,
            "total_unloaded": result.total_unloaded,
            "actors": [
                {
                    "name": a.actor_info.name,
                    "label": a.actor_info.label,
                    "class_name": a.actor_info.class_name,
                    "guid": a.actor_info.guid,
                    "streaming_state": ["NOT_APPLICABLE", "LOADED", "UNLOADED", "INVALID"][a.streaming_state],
                    "is_spatially_loaded": a.is_spatially_loaded,
                    "data_layers": list(a.data_layers),
                    "location": [a.actor_info.transform.location.x,
                                 a.actor_info.transform.location.y,
                                 a.actor_info.transform.location.z] if a.actor_info.HasField("transform") else None,
                }
                for a in result.actors
            ],
        }

    elif tool_name == "get_streaming_state":
        result = safe_call(client.get_streaming_state, args["actor_guid"])
        if isinstance(result, dict) and "error" in result:
            return result
        state_names = ["NOT_APPLICABLE", "LOADED", "UNLOADED", "INVALID"]
        return {
            "state": state_names[result.state],
            "actor": {
                "name": result.actor.actor_info.name,
                "label": result.actor.actor_info.label,
                "class_name": result.actor.actor_info.class_name,
            } if result.HasField("actor") else None,
        }

    elif tool_name == "query_landscape":
        result = safe_call(client.query_landscape, args.get("include_unloaded", True))
        if isinstance(result, dict) and "error" in result:
            return result
        return {
            "total_count": result.total_count,
            "landscape_proxies": [
                {
                    "name": p.actor_info.name,
                    "label": p.actor_info.label,
                    "class_name": p.actor_info.class_name,
                    "guid": p.actor_info.guid,
                    "streaming_state": ["NOT_APPLICABLE", "LOADED", "UNLOADED", "INVALID"][p.streaming_state],
                }
                for p in result.landscape_proxies
            ],
        }

    elif tool_name == "get_landscape_bounds":
        result = safe_call(client.get_landscape_bounds)
        if isinstance(result, dict) and "error" in result:
            return result
        if not result.valid:
            return {"error": "No landscape found in world"}
        return {
            "valid": result.valid,
            "min": [result.min.x, result.min.y, result.min.z],
            "max": [result.max.x, result.max.y, result.max.z],
            "center": [result.center.x, result.center.y, result.center.z],
            "extent": [result.extent.x, result.extent.y, result.extent.z],
            "proxy_count": result.proxy_count,
            "landscape_name": result.landscape_name,
        }

    elif tool_name == "get_data_layers":
        result = safe_call(client.get_data_layers)
        if isinstance(result, dict) and "error" in result:
            return result
        return {
            "data_layers": list(result.data_layers),
        }

    elif tool_name == "get_actors_in_data_layer":
        result = safe_call(
            client.get_actors_in_data_layer,
            data_layer=args["data_layer"],
            include_unloaded=args.get("include_unloaded", True),
            limit=args.get("limit", 100),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        return {
            "total_count": result.total_count,
            "actors": [
                {
                    "name": a.actor_info.name,
                    "label": a.actor_info.label,
                    "class_name": a.actor_info.class_name,
                    "guid": a.actor_info.guid,
                    "streaming_state": ["NOT_APPLICABLE", "LOADED", "UNLOADED", "INVALID"][a.streaming_state],
                }
                for a in result.actors
            ],
        }

    elif tool_name == "execute_console_command":
        result = safe_call(client.execute_console_command, args["command"])
        if isinstance(result, dict) and "error" in result:
            return result
        return {
            "success": result.success,
            "output": result.output,
        }

    elif tool_name == "search_console_commands":
        limit = args.get("limit", 50)
        offset = args.get("offset", 0)
        result = safe_call(
            client.search_console_commands,
            args["keyword"],
            limit,
            offset,
            args.get("search_help", False),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        commands = []
        for cmd in result.commands:
            cmd_info = {
                "name": cmd.name,
                "help": cmd.help,
                "is_variable": cmd.is_variable,
            }
            if cmd.is_variable:
                cmd_info["value_type"] = cmd.value_type
                cmd_info["current_value"] = cmd.current_value
            commands.append(cmd_info)
        has_more = (offset + len(commands)) < result.total_matches
        return {
            "commands": commands,
            "count": len(commands),
            "total_matches": result.total_matches,
            "offset": offset,
            "has_more": has_more,
            "next_offset": offset + len(commands) if has_more else None,
        }

    # =========================================================================
    # Asset Operations (P0)
    # =========================================================================
    elif tool_name == "create_asset":
        result = safe_call(
            client.create_asset,
            args["asset_class"],
            args["package_path"],
            args["asset_name"],
            args.get("parent_asset_path", ""),
            args.get("properties"),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        return {
            "success": result.success,
            "error_message": result.error_message if not result.success else None,
            "asset_path": result.asset_path,
            "asset_class": result.asset_class,
        }

    elif tool_name == "save_asset":
        result = safe_call(
            client.save_asset,
            args["asset_path"],
            args.get("prompt_for_checkout", False),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        return {
            "success": result.success,
            "error_message": result.error_message if not result.success else None,
            "file_path": result.file_path,
        }

    elif tool_name == "save_actor_as_blueprint":
        result = safe_call(
            client.save_actor_as_blueprint,
            args["actor_id"],
            args["package_path"],
            args["blueprint_name"],
            args.get("replace_existing", False),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        return {
            "success": result.success,
            "error_message": result.error_message if not result.success else None,
            "blueprint_path": result.blueprint_path,
        }

    elif tool_name == "duplicate_asset":
        result = safe_call(
            client.duplicate_asset,
            args["source_path"],
            args["dest_package_path"],
            args["dest_asset_name"],
        )
        if isinstance(result, dict) and "error" in result:
            return result
        return {
            "success": result.success,
            "error_message": result.error_message if not result.success else None,
            "new_asset_path": result.new_asset_path,
        }

    # =========================================================================
    # Component Operations (P1)
    # =========================================================================
    elif tool_name == "get_component_transform":
        result = safe_call(
            client.get_component_transform,
            args["actor_id"],
            args["component_name"],
            args.get("world_space", True),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        if not result.success:
            return {"success": False, "error_message": result.error_message}
        t = result.transform
        return {
            "success": True,
            "location": [t.location.x, t.location.y, t.location.z],
            "rotation": [t.rotation.p, t.rotation.y, t.rotation.r],
            "scale": [t.scale.x, t.scale.y, t.scale.z],
        }

    elif tool_name == "set_component_transform":
        result = safe_call(
            client.set_component_transform,
            args["actor_id"],
            args["component_name"],
            args.get("location"),
            args.get("rotation"),
            args.get("scale"),
            args.get("world_space", True),
            args.get("sweep", False),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        return {"success": True}

    elif tool_name == "attach_actor":
        result = safe_call(
            client.attach_actor,
            args["child_actor_id"],
            args["parent_actor_id"],
            args.get("parent_component_name", ""),
            args.get("socket_name", ""),
            args.get("location_rule", "KeepWorld"),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        return {"success": True}

    elif tool_name == "detach_actor":
        result = safe_call(
            client.detach_actor,
            args["actor_id"],
            args.get("maintain_world_position", True),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        return {"success": True}

    # =========================================================================
    # File Operations (P1)
    # =========================================================================
    elif tool_name == "read_project_file":
        result = safe_call(
            client.read_project_file,
            args["relative_path"],
            args.get("as_base64", False),
            args.get("max_bytes", 0),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        return {
            "success": result.success,
            "error_message": result.error_message if not result.success else None,
            "content": result.content,
            "file_size": result.file_size,
            "is_binary": result.is_binary,
        }

    elif tool_name == "write_project_file":
        result = safe_call(
            client.write_project_file,
            args["relative_path"],
            args["content"],
            args.get("is_base64", False),
            args.get("create_directories", True),
            args.get("append", False),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        return {
            "success": result.success,
            "error_message": result.error_message if not result.success else None,
            "bytes_written": result.bytes_written,
        }

    elif tool_name == "list_project_directory":
        result = safe_call(
            client.list_project_directory,
            args.get("relative_path", ""),
            args.get("pattern", ""),
            args.get("recursive", False),
            args.get("limit", 100),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        files = []
        for f in result.files:
            files.append({
                "name": f.name,
                "relative_path": f.relative_path,
                "is_directory": f.is_directory,
                "size": f.size,
                "last_modified": f.last_modified,
            })
        return {
            "success": result.success,
            "error_message": result.error_message if not result.success else None,
            "files": files,
            "total_count": result.total_count,
        }

    elif tool_name == "copy_project_file":
        result = safe_call(
            client.copy_project_file,
            args["source_path"],
            args["dest_path"],
            args.get("overwrite", False),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        return {
            "success": result.success,
            "error_message": result.error_message if not result.success else None,
            "dest_absolute_path": result.dest_absolute_path,
        }

    # -------------------------------------------------------------------------
    # Component Operations
    # -------------------------------------------------------------------------

    elif tool_name == "attach_component":
        result = safe_call(
            client.attach_component,
            args["actor_id"],
            args["component_name"],
            args["parent_component_name"],
            args.get("socket_name", ""),
            args.get("location_rule", "keep_relative"),
            args.get("rotation_rule", "keep_relative"),
            args.get("scale_rule", "keep_relative"),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        return {"success": True}

    elif tool_name == "attach_actor":
        result = safe_call(
            client.attach_actor,
            args["child_actor_id"],
            args["parent_actor_id"],
            args.get("parent_component_name", ""),
            args.get("socket_name", ""),
            args.get("location_rule", "keep_world"),
            args.get("rotation_rule", "keep_world"),
            args.get("scale_rule", "keep_world"),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        return {"success": True}

    elif tool_name == "detach_component":
        result = safe_call(
            client.detach_component,
            args["actor_id"],
            args["component_name"],
            args.get("maintain_world_transform", True),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        return {"success": True}

    else:
        return {"error": f"Unknown tool: {tool_name}"}


# Register this service module
register_service(ServiceModule(
    name="agentbridge",
    description="AgentBridge - Unreal Engine world/actor manipulation",
    tools=TOOLS,
    execute=execute,
    connect=connect,
))
