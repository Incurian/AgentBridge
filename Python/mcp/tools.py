"""
MCP Tool Definitions for AgentBridge

Defines tools that can be used by Claude and other LLM agents to interact
with Unreal Engine via the AgentBridge gRPC service.
"""

from typing import Any, Dict, List
import json

# Tool definitions following MCP schema
TOOLS = [
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
        "description": "Search for actors in the current world. You can filter by class, name pattern, or tag. Returns a list of matching actors with their transforms.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "class_name": {
                    "type": "string",
                    "description": "Filter by class name (e.g., 'PointLight', 'StaticMeshActor', 'BP_MyActor')",
                },
                "name_pattern": {
                    "type": "string",
                    "description": "Wildcard pattern for actor name/label (e.g., 'Light*', '*Door*')",
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
        "description": "Get detailed information about a specific actor, including properties and components.",
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
                    "description": "Property path",
                },
                "value": {
                    "type": "string",
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
        "description": "List available actor/component classes. Useful for discovering what types of objects can be spawned.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "base_class_name": {
                    "type": "string",
                    "description": "Filter by base class (default: 'Actor')",
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
        "description": "Get the schema (properties and functions) for a class. Useful for understanding what properties can be set on an actor.",
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
]


def execute_tool(client, tool_name: str, arguments: Dict[str, Any]) -> str:
    """
    Execute an MCP tool and return the result as a string.

    Args:
        client: AgentBridgeGrpcClient instance
        tool_name: Name of the tool to execute
        arguments: Tool arguments

    Returns:
        Result as a JSON string
    """
    try:
        result = _execute_tool_impl(client, tool_name, arguments)
        return json.dumps(result, indent=2, default=str)
    except Exception as e:
        return json.dumps({"error": str(e)})


def _execute_tool_impl(client, tool_name: str, args: Dict[str, Any]) -> Any:
    """Implementation of tool execution."""

    # World Operations
    if tool_name == "list_worlds":
        worlds = client.list_worlds()
        return {
            "worlds": [
                {
                    "world_type": w.world_type,
                    "world_name": w.world_name,
                    "pie_instance": w.pie_instance,
                    "has_begun_play": w.has_begun_play,
                    "actor_count": w.actor_count,
                }
                for w in worlds
            ]
        }

    elif tool_name == "set_target_world":
        client.set_target_world(args["world_identifier"])
        return {"success": True}

    # Actor Discovery
    elif tool_name == "query_actors":
        actors = client.query_actors(
            class_name=args.get("class_name", ""),
            name_pattern=args.get("name_pattern", ""),
            tag=args.get("tag", ""),
            limit=args.get("limit", 100),
            include_hidden=args.get("include_hidden", False),
        )
        return {
            "count": len(actors),
            "actors": [_actor_to_dict(a) for a in actors],
        }

    elif tool_name == "get_actor":
        actor = client.get_actor(
            actor_id=args["actor_id"],
            include_properties=args.get("include_properties", False),
            include_components=args.get("include_components", False),
        )
        if actor:
            return {"found": True, "actor": _actor_to_dict(actor)}
        else:
            return {"found": False, "error": f"Actor '{args['actor_id']}' not found"}

    # Actor Manipulation
    elif tool_name == "spawn_actor":
        actor = client.spawn_actor(
            class_name=args["class_name"],
            location=tuple(args.get("location", [0, 0, 0])),
            rotation=tuple(args.get("rotation", [0, 0, 0])),
            scale=tuple(args.get("scale", [1, 1, 1])),
            label=args.get("label", ""),
            folder_path=args.get("folder_path", ""),
        )
        if actor:
            return {"success": True, "actor": _actor_to_dict(actor)}
        else:
            return {"success": False, "error": "Failed to spawn actor"}

    elif tool_name == "delete_actor":
        success = client.delete_actor(args["actor_id"])
        return {"success": success}

    elif tool_name == "set_actor_transform":
        success = client.set_actor_transform(
            actor_id=args["actor_id"],
            location=tuple(args["location"]) if "location" in args else None,
            rotation=tuple(args["rotation"]) if "rotation" in args else None,
            scale=tuple(args["scale"]) if "scale" in args else None,
        )
        return {"success": success}

    # Property Operations
    elif tool_name == "get_property":
        value = client.get_property(args["actor_id"], args["path"])
        if value is not None:
            return {"path": args["path"], "value": value}
        else:
            return {"error": f"Property '{args['path']}' not found"}

    elif tool_name == "set_property":
        success = client.set_property(args["actor_id"], args["path"], args["value"])
        return {"success": success}

    # Type Discovery
    elif tool_name == "list_classes":
        classes = client.list_classes(
            base_class_name=args.get("base_class_name", "Actor"),
            name_pattern=args.get("name_pattern", ""),
            include_blueprint=args.get("include_blueprint", True),
            limit=args.get("limit", 50),
        )
        return {"count": len(classes), "classes": classes}

    elif tool_name == "get_class_schema":
        schema = client.get_class_schema(
            class_name=args["class_name"],
            include_inherited=args.get("include_inherited", True),
            include_functions=args.get("include_functions", False),
        )
        if schema:
            return schema
        else:
            return {"error": f"Class '{args['class_name']}' not found"}

    else:
        return {"error": f"Unknown tool: {tool_name}"}


def _actor_to_dict(actor) -> Dict[str, Any]:
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
