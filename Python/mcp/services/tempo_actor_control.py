"""
Tempo ActorControlService MCP Tools

Exposes actor/component manipulation via Tempo's native service.
Provides typed property setters (float, vector, color, asset, etc.)
"""

import json
from typing import Dict, Any, List, Optional
from . import register_service, ServiceModule
from .base import create_channel, safe_call

# Import Tempo's generated stubs
from TempoWorld import ActorControl_pb2 as pb
from TempoWorld import ActorControl_pb2_grpc as pb_grpc
from TempoScripting import Empty_pb2
from TempoScripting import Geometry_pb2


TOOLS = [
    {"name": "tempo_get_all_actors", "description": "Get a list of all actors in the world. Fast native implementation.", "inputSchema": {"type": "object"}},
    {
        "name": "tempo_spawn_actor",
        "description": "Spawn an actor using Tempo's native spawning. Supports relative transforms.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "type": {"type": "string"},
                "location": {"type": "array", "items": {"type": "number"}},
                "rotation": {"type": "array", "items": {"type": "number"}},
                "relative_to": {"type": "string"}
            },
            "required": ["type"]
        }
    },
    {"name": "tempo_destroy_actor", "description": "Destroy an actor using Tempo's native destruction.", "inputSchema": {"type": "object", "properties": {"actor": {"type": "string"}}, "required": ["actor"]}},
    {"name": "tempo_get_components", "description": "Get all components on an actor.", "inputSchema": {"type": "object", "properties": {"actor": {"type": "string"}}, "required": ["actor"]}},
    {"name": "tempo_add_component", "description": "Add a component to an actor.", "inputSchema": {"type": "object", "properties": {"actor": {"type": "string"}, "type": {"type": "string"}, "name": {"type": "string"}}, "required": ["actor", "type"]}},
    {"name": "tempo_get_actor_properties", "description": "Get all properties of an actor with their current values.", "inputSchema": {"type": "object", "properties": {"actor": {"type": "string"}, "include_components": {"type": "boolean", "default": False}}, "required": ["actor"]}},
    {"name": "tempo_get_component_properties", "description": "Get all properties of a specific component.", "inputSchema": {"type": "object", "properties": {"actor": {"type": "string"}, "component": {"type": "string"}}, "required": ["actor", "component"]}},
    {"name": "tempo_set_float_property", "description": "Set a float property on an actor or component (e.g., Intensity, Radius).", "inputSchema": {"type": "object", "properties": {"actor": {"type": "string"}, "component": {"type": "string"}, "property": {"type": "string"}, "value": {"type": "number"}}, "required": ["actor", "property", "value"]}},
    {"name": "tempo_set_int_property", "description": "Set an integer property on an actor or component.", "inputSchema": {"type": "object", "properties": {"actor": {"type": "string"}, "component": {"type": "string"}, "property": {"type": "string"}, "value": {"type": "integer"}}, "required": ["actor", "property", "value"]}},
    {"name": "tempo_set_bool_property", "description": "Set a boolean property on an actor or component.", "inputSchema": {"type": "object", "properties": {"actor": {"type": "string"}, "component": {"type": "string"}, "property": {"type": "string"}, "value": {"type": "boolean"}}, "required": ["actor", "property", "value"]}},
    {"name": "tempo_set_string_property", "description": "Set a string property on an actor or component.", "inputSchema": {"type": "object", "properties": {"actor": {"type": "string"}, "component": {"type": "string"}, "property": {"type": "string"}, "value": {"type": "string"}}, "required": ["actor", "property", "value"]}},
    {"name": "tempo_set_vector_property", "description": "Set a vector property (e.g., RelativeLocation, RelativeScale3D).", "inputSchema": {"type": "object", "properties": {"actor": {"type": "string"}, "component": {"type": "string"}, "property": {"type": "string"}, "x": {"type": "number"}, "y": {"type": "number"}, "z": {"type": "number"}}, "required": ["actor", "property", "x", "y", "z"]}},
    {"name": "tempo_set_rotator_property", "description": "Set a rotator property (e.g., RelativeRotation).", "inputSchema": {"type": "object", "properties": {"actor": {"type": "string"}, "component": {"type": "string"}, "property": {"type": "string"}, "roll": {"type": "number"}, "pitch": {"type": "number"}, "yaw": {"type": "number"}}, "required": ["actor", "property", "roll", "pitch", "yaw"]}},
    {"name": "tempo_set_color_property", "description": "Set a color property (e.g., LightColor).", "inputSchema": {"type": "object", "properties": {"actor": {"type": "string"}, "component": {"type": "string"}, "property": {"type": "string"}, "r": {"type": "integer"}, "g": {"type": "integer"}, "b": {"type": "integer"}}, "required": ["actor", "property", "r", "g", "b"]}},
    {"name": "tempo_set_asset_property", "description": "Set an asset reference property (e.g., StaticMesh, Material).", "inputSchema": {"type": "object", "properties": {"actor": {"type": "string"}, "component": {"type": "string"}, "property": {"type": "string"}, "value": {"type": "string"}}, "required": ["actor", "property", "value"]}},
    {"name": "tempo_set_actor_transform", "description": "Set an actor's world transform using Tempo's native method.", "inputSchema": {"type": "object", "properties": {"actor": {"type": "string"}, "location": {"type": "array"}, "rotation": {"type": "array"}, "relative_to": {"type": "string"}}, "required": ["actor"]}},
    {"name": "tempo_call_function", "description": "Call a function on an actor or component.", "inputSchema": {"type": "object", "properties": {"actor": {"type": "string"}, "component": {"type": "string"}, "function": {"type": "string"}}, "required": ["actor", "function"]}},
]


class TempoActorControlClient:
    """Client for Tempo's ActorControlService."""

    def __init__(self, host: str = "localhost", port: int = 50051):
        self.channel = create_channel(host, port)
        self.stub = pb_grpc.ActorControlServiceStub(self.channel)

    def get_all_actors(self):
        return self.stub.GetAllActors(pb.GetAllActorsRequest())

    def spawn_actor(self, type: str, location=None, rotation=None, relative_to: str = ""):
        transform = None
        if location or rotation:
            loc = location or [0, 0, 0]
            rot = rotation or [0, 0, 0]
            transform = Geometry_pb2.Transform(
                location=Geometry_pb2.Vector(x=loc[0], y=loc[1], z=loc[2]),
                rotation=Geometry_pb2.Rotation(r=rot[0], p=rot[1], y=rot[2]),
            )
        return self.stub.SpawnActor(pb.SpawnActorRequest(
            type=type,
            transform=transform,
            relative_to_actor=relative_to,
        ))

    def destroy_actor(self, actor: str):
        return self.stub.DestroyActor(pb.DestroyActorRequest(actor=actor))

    def get_all_components(self, actor: str):
        return self.stub.GetAllComponents(pb.GetAllComponentsRequest(actor=actor))

    def add_component(self, actor: str, type: str, name: str = ""):
        return self.stub.AddComponent(pb.AddComponentRequest(
            actor=actor, type=type, name=name
        ))

    def get_actor_properties(self, actor: str, include_components: bool = False):
        return self.stub.GetActorProperties(pb.GetActorPropertiesRequest(
            actor=actor, include_components=include_components
        ))

    def get_component_properties(self, actor: str, component: str):
        return self.stub.GetComponentProperties(pb.GetComponentPropertiesRequest(
            actor=actor, component=component
        ))

    def set_float_property(self, actor: str, property: str, value: float, component: str = ""):
        return self.stub.SetFloatProperty(pb.SetFloatPropertyRequest(
            actor=actor, component=component, property=property, value=value
        ))

    def set_int_property(self, actor: str, property: str, value: int, component: str = ""):
        return self.stub.SetIntProperty(pb.SetIntPropertyRequest(
            actor=actor, component=component, property=property, value=value
        ))

    def set_bool_property(self, actor: str, property: str, value: bool, component: str = ""):
        return self.stub.SetBoolProperty(pb.SetBoolPropertyRequest(
            actor=actor, component=component, property=property, value=value
        ))

    def set_string_property(self, actor: str, property: str, value: str, component: str = ""):
        return self.stub.SetStringProperty(pb.SetStringPropertyRequest(
            actor=actor, component=component, property=property, value=value
        ))

    def set_vector_property(self, actor: str, property: str, x: float, y: float, z: float, component: str = ""):
        return self.stub.SetVectorProperty(pb.SetVectorPropertyRequest(
            actor=actor, component=component, property=property, x=x, y=y, z=z
        ))

    def set_rotator_property(self, actor: str, property: str, r: float, p: float, y: float, component: str = ""):
        return self.stub.SetRotatorProperty(pb.SetRotatorPropertyRequest(
            actor=actor, component=component, property=property, r=r, p=p, y=y
        ))

    def set_color_property(self, actor: str, property: str, r: int, g: int, b: int, component: str = ""):
        return self.stub.SetColorProperty(pb.SetColorPropertyRequest(
            actor=actor, component=component, property=property, r=r, g=g, b=b
        ))

    def set_asset_property(self, actor: str, property: str, value: str, component: str = ""):
        return self.stub.SetAssetProperty(pb.SetAssetPropertyRequest(
            actor=actor, component=component, property=property, value=value
        ))

    def set_actor_transform(self, actor: str, location=None, rotation=None, relative_to: str = ""):
        transform = Geometry_pb2.Transform()
        if location:
            transform.location.CopyFrom(Geometry_pb2.Vector(x=location[0], y=location[1], z=location[2]))
        if rotation:
            transform.rotation.CopyFrom(Geometry_pb2.Rotation(r=rotation[0], p=rotation[1], y=rotation[2]))
        return self.stub.SetActorTransform(pb.SetActorTransformRequest(
            actor=actor, transform=transform, relative_to_actor=relative_to
        ))

    def call_function(self, actor: str, function: str, component: str = ""):
        return self.stub.CallFunction(pb.CallFunctionRequest(
            actor=actor, component=component, function=function
        ))


def connect(host: str, port: int) -> TempoActorControlClient:
    """Create a TempoActorControlClient."""
    return TempoActorControlClient(host, port)


def execute(client: TempoActorControlClient, tool_name: str, args: Dict[str, Any]) -> str:
    """Execute a tempo_actor_control tool."""
    result = _execute_impl(client, tool_name, args)
    return json.dumps(result, indent=2, default=str)


def _execute_impl(client: TempoActorControlClient, tool_name: str, args: Dict[str, Any]) -> Any:
    """Implementation of tool execution."""

    if tool_name == "tempo_get_all_actors":
        response = client.get_all_actors()
        return {
            "count": len(response.actors),
            "actors": [{"name": a.name, "type": a.type} for a in response.actors],
        }

    elif tool_name == "tempo_spawn_actor":
        response = client.spawn_actor(
            type=args["type"],
            location=args.get("location"),
            rotation=args.get("rotation"),
            relative_to=args.get("relative_to", ""),
        )
        return {
            "success": True,
            "spawned_name": response.spawned_name,
            "transform": {
                "location": [response.spawned_transform.location.x,
                            response.spawned_transform.location.y,
                            response.spawned_transform.location.z],
            },
        }

    elif tool_name == "tempo_destroy_actor":
        client.destroy_actor(args["actor"])
        return {"success": True, "destroyed": args["actor"]}

    elif tool_name == "tempo_get_components":
        response = client.get_all_components(args["actor"])
        return {
            "actor": args["actor"],
            "components": [
                {"name": c.name, "type": c.type}
                for c in response.components
            ],
        }

    elif tool_name == "tempo_add_component":
        response = client.add_component(
            actor=args["actor"],
            type=args["type"],
            name=args.get("name", ""),
        )
        return {"success": True, "component_name": response.name}

    elif tool_name == "tempo_get_actor_properties":
        response = client.get_actor_properties(
            actor=args["actor"],
            include_components=args.get("include_components", False),
        )
        return {
            "actor": args["actor"],
            "properties": [
                {"name": p.name, "type": p.type, "value": p.value, "component": p.component}
                for p in response.properties
            ],
        }

    elif tool_name == "tempo_get_component_properties":
        response = client.get_component_properties(
            actor=args["actor"],
            component=args["component"],
        )
        return {
            "actor": args["actor"],
            "component": args["component"],
            "properties": [
                {"name": p.name, "type": p.type, "value": p.value}
                for p in response.properties
            ],
        }

    elif tool_name == "tempo_set_float_property":
        client.set_float_property(
            actor=args["actor"],
            property=args["property"],
            value=args["value"],
            component=args.get("component", ""),
        )
        return {"success": True, "property": args["property"], "value": args["value"]}

    elif tool_name == "tempo_set_int_property":
        client.set_int_property(
            actor=args["actor"],
            property=args["property"],
            value=args["value"],
            component=args.get("component", ""),
        )
        return {"success": True, "property": args["property"], "value": args["value"]}

    elif tool_name == "tempo_set_bool_property":
        client.set_bool_property(
            actor=args["actor"],
            property=args["property"],
            value=args["value"],
            component=args.get("component", ""),
        )
        return {"success": True, "property": args["property"], "value": args["value"]}

    elif tool_name == "tempo_set_string_property":
        client.set_string_property(
            actor=args["actor"],
            property=args["property"],
            value=args["value"],
            component=args.get("component", ""),
        )
        return {"success": True, "property": args["property"], "value": args["value"]}

    elif tool_name == "tempo_set_vector_property":
        client.set_vector_property(
            actor=args["actor"],
            property=args["property"],
            x=args["x"],
            y=args["y"],
            z=args["z"],
            component=args.get("component", ""),
        )
        return {"success": True, "property": args["property"], "value": [args["x"], args["y"], args["z"]]}

    elif tool_name == "tempo_set_rotator_property":
        client.set_rotator_property(
            actor=args["actor"],
            property=args["property"],
            r=args["roll"],
            p=args["pitch"],
            y=args["yaw"],
            component=args.get("component", ""),
        )
        return {"success": True, "property": args["property"]}

    elif tool_name == "tempo_set_color_property":
        client.set_color_property(
            actor=args["actor"],
            property=args["property"],
            r=args["r"],
            g=args["g"],
            b=args["b"],
            component=args.get("component", ""),
        )
        return {"success": True, "property": args["property"], "color": [args["r"], args["g"], args["b"]]}

    elif tool_name == "tempo_set_asset_property":
        client.set_asset_property(
            actor=args["actor"],
            property=args["property"],
            value=args["value"],
            component=args.get("component", ""),
        )
        return {"success": True, "property": args["property"], "asset": args["value"]}

    elif tool_name == "tempo_set_actor_transform":
        client.set_actor_transform(
            actor=args["actor"],
            location=args.get("location"),
            rotation=args.get("rotation"),
            relative_to=args.get("relative_to", ""),
        )
        return {"success": True, "actor": args["actor"]}

    elif tool_name == "tempo_call_function":
        client.call_function(
            actor=args["actor"],
            function=args["function"],
            component=args.get("component", ""),
        )
        return {"success": True, "function": args["function"]}

    else:
        return {"error": f"Unknown tool: {tool_name}"}


# Register this service module
register_service(ServiceModule(
    name="tempo_actor_control",
    description="Tempo ActorControlService - actor/component manipulation with typed properties",
    tools=TOOLS,
    execute=execute,
    connect=connect,
))
