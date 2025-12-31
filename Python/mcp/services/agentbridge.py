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
                    "description": "Optional topic: 'actors', 'properties', 'classes', 'console', 'workflows', or leave empty for overview",
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

    def query_actors(self, class_name="", name_pattern="", tag="", limit=100, include_hidden=False):
        return self.stub.QueryActors(pb.QueryActorsRequest(
            class_name=class_name,
            name_pattern=name_pattern,
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
- Blueprint: /Game/BP_Name.BP_Name_C (note the _C suffix!)

UNITS:
- Location: centimeters (100 = 1 meter)
- Rotation: degrees [Pitch, Yaw, Roll]
- Scale: multiplier [X, Y, Z] where 1.0 = normal

TIPS:
- Use query_actors first to explore what's in the scene
- Use get_class_schema to see what properties a class has
- Use search_console_commands if you need to do something unusual
- execute_console_command is the escape hatch for anything not covered

Use help(topic='actors|properties|classes|console|workflows') for detailed help.
"""

    topics = {
        "actors": """
ACTOR OPERATIONS:

Finding actors:
- query_actors(name_pattern="*Door*") - Wildcard search
- query_actors(class_name="PointLight") - Filter by type
- query_actors(tag="Interactive") - Filter by tag
- get_actor(actor_id="MyLight", include_properties=True) - Full details

Creating actors:
- spawn_actor(class_name="PointLight", location=[0,0,500], label="MyLight")
- spawn_actor(class_name="/Game/BP_Enemy.BP_Enemy_C", location=[100,0,0])

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
- get_property_path(actor_id, path="RootComponent.RelativeLocation") - Specific path

Setting properties:
- set_property_path(actor_id, path="LightComponent.Intensity", value=5000)
- set_actor_properties(actor_id, properties=[{"key": "bHidden", "value": true}])

Property paths:
- Simple: "bHidden", "ActorLabel"
- Nested: "RootComponent.RelativeLocation.X"
- Array: "Materials[0]"
- Component: "LightComponent.Intensity"

Common light properties:
- LightComponent.Intensity (float, default ~5000 for point lights)
- LightComponent.LightColor (Color: R,G,B,A 0-255)
- LightComponent.AttenuationRadius (float, cm)

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

Blueprint classes (need full path + _C):
- /Game/Blueprints/BP_Enemy.BP_Enemy_C
- /Game/Characters/BP_Player.BP_Player_C

The _C suffix is required - it refers to the generated class, not the asset.
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
4. set_property_path("MainLight", "LightComponent.Intensity", 10000)

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
"""
    }

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
        result = safe_call(
            client.query_actors,
            class_name=args.get("class_name", ""),
            name_pattern=args.get("name_pattern", ""),
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
            return result
        if result.HasField("actor"):
            actor = client._parse_actor_descriptor(result.actor.actor_info)
            return {"found": True, "actor": _actor_to_dict(actor)}
        return {"found": False, "error": f"Actor '{args['actor_id']}' not found"}

    elif tool_name == "spawn_actor":
        result = safe_call(
            client.spawn_actor,
            class_name=args["class_name"],
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
        return {"success": False, "error": "Failed to spawn actor"}

    elif tool_name == "delete_actor":
        result = safe_call(client.delete_actor, args["actor_id"])
        if isinstance(result, dict) and "error" in result:
            return result
        return {"success": True}

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
        result = safe_call(client.get_property, args["actor_id"], args["path"])
        if isinstance(result, dict) and "error" in result:
            return result
        return {"path": args["path"], "value": result.value.string_value}

    elif tool_name == "set_property":
        result = safe_call(client.set_property, args["actor_id"], args["path"], args["value"])
        if isinstance(result, dict) and "error" in result:
            return result
        return {"success": True}

    elif tool_name == "list_classes":
        result = safe_call(
            client.list_classes,
            base_class_name=args.get("base_class_name", "Actor"),
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
        result = safe_call(
            client.get_class_schema,
            class_name=args["class_name"],
            include_inherited=args.get("include_inherited", True),
            include_functions=args.get("include_functions", False),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        schema = result.schema
        return {
            "class_name": schema.class_info.class_name,
            "properties": [
                {
                    "name": p.name,
                    "display_name": p.display_name,
                    "type_name": p.type_name,
                    "is_read_only": p.is_read_only,
                }
                for p in schema.properties
            ],
            "functions": [
                {
                    "name": f.function_name,
                    "description": f.description,
                    "is_static": f.is_static,
                }
                for f in schema.functions
            ],
        }

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
