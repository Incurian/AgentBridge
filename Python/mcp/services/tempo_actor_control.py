"""
Tempo ActorControlService MCP Tools

Exposes actor/component manipulation via Tempo's native service.
Provides unified property setter with auto type detection.
"""

import json
from typing import Dict, Any, List, Optional, Union
from . import register_service, ServiceModule
from .base import create_channel, safe_call

# Import Tempo's generated stubs
from TempoWorld import ActorControl_pb2 as pb
from TempoWorld import ActorControl_pb2_grpc as pb_grpc
from TempoScripting import Empty_pb2
from TempoScripting import Geometry_pb2


# =============================================================================
# TOOLS - Consolidated list (9 typed setters → 1 unified setter)
# =============================================================================

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

    # Unified property setter (replaces 8 individual typed setters)
    {
        "name": "tempo_set_property",
        "description": "Set property on actor/component. Type auto-detected: float, int, bool, string, vector [x,y,z], rotator {roll,pitch,yaw}, color [r,g,b], asset path.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "actor": {"type": "string"},
                "property": {"type": "string"},
                "value": {},  # Any type - auto-detected
                "component": {"type": "string"},
                "type_hint": {"type": "string", "enum": ["float", "int", "bool", "string", "vector", "rotator", "color", "asset"]}
            },
            "required": ["actor", "property", "value"]
        }
    },

    {"name": "tempo_set_actor_transform", "description": "Set an actor's world transform using Tempo's native method.", "inputSchema": {"type": "object", "properties": {"actor": {"type": "string"}, "location": {"type": "array"}, "rotation": {"type": "array"}, "relative_to": {"type": "string"}}, "required": ["actor"]}},
    {"name": "tempo_call_function", "description": "Call a function on an actor or component.", "inputSchema": {"type": "object", "properties": {"actor": {"type": "string"}, "component": {"type": "string"}, "function": {"type": "string"}}, "required": ["actor", "function"]}},
]


# =============================================================================
# CLIENT
# =============================================================================

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


# =============================================================================
# TYPE DETECTION & UNIFIED SETTER
# =============================================================================

def _detect_and_set_property(
    client: TempoActorControlClient,
    actor: str,
    property: str,
    value: Any,
    component: str = "",
    type_hint: Optional[str] = None,
) -> Dict[str, Any]:
    """
    Detect value type and call appropriate Tempo setter method.

    Type detection order:
    1. Explicit type_hint if provided
    2. Python type (bool, int, float, str)
    3. Array/tuple structure (vector, rotator, color)
    4. Dict structure (vector, rotator, color with named keys)
    5. String patterns (asset paths)
    """

    # If type_hint provided, use it directly
    if type_hint:
        return _set_property_by_type(client, actor, property, value, component, type_hint)

    # Boolean (must check before numeric, since bool is subclass of int in Python)
    if isinstance(value, bool):
        client.set_bool_property(actor, property, value, component)
        return {"type": "bool", "value": value}

    # Numeric - default to float since it's more permissive in UE
    # JSON doesn't distinguish int from float, and most UE properties are float
    if isinstance(value, (int, float)):
        client.set_float_property(actor, property, float(value), component)
        return {"type": "float", "value": float(value)}

    # String - check if it's an asset path
    if isinstance(value, str):
        if value.startswith("/Game/") or value.startswith("/Script/") or value.startswith("/Engine/"):
            client.set_asset_property(actor, property, value, component)
            return {"type": "asset", "value": value}
        client.set_string_property(actor, property, value, component)
        return {"type": "string", "value": value}

    # Array/tuple - could be vector, rotator, or color
    if isinstance(value, (list, tuple)):
        if len(value) == 3:
            # Check property name for hints
            prop_lower = property.lower()
            if 'color' in prop_lower or 'colour' in prop_lower:
                # Color - values should be 0-255 integers
                r, g, b = int(value[0]), int(value[1]), int(value[2])
                client.set_color_property(actor, property, r, g, b, component)
                return {"type": "color", "value": [r, g, b]}
            elif 'rotation' in prop_lower or 'rotator' in prop_lower:
                # Rotator - roll, pitch, yaw
                client.set_rotator_property(actor, property, float(value[0]), float(value[1]), float(value[2]), component)
                return {"type": "rotator", "value": value}
            else:
                # Default to vector for 3-element arrays
                client.set_vector_property(actor, property, float(value[0]), float(value[1]), float(value[2]), component)
                return {"type": "vector", "value": value}
        elif len(value) == 4:
            # RGBA color - ignore alpha, use RGB
            r, g, b = int(value[0]), int(value[1]), int(value[2])
            client.set_color_property(actor, property, r, g, b, component)
            return {"type": "color", "value": [r, g, b]}

    # Dict - check keys for structure hints
    if isinstance(value, dict):
        # Vector: {x, y, z}
        if 'x' in value and 'y' in value and 'z' in value:
            client.set_vector_property(actor, property, float(value['x']), float(value['y']), float(value['z']), component)
            return {"type": "vector", "value": [value['x'], value['y'], value['z']]}
        # Color: {r, g, b}
        if 'r' in value and 'g' in value and 'b' in value:
            r, g, b = int(value['r']), int(value['g']), int(value['b'])
            client.set_color_property(actor, property, r, g, b, component)
            return {"type": "color", "value": [r, g, b]}
        # Rotator: {roll, pitch, yaw} or {r, p, y}
        if 'roll' in value or 'pitch' in value or 'yaw' in value:
            roll = float(value.get('roll', 0))
            pitch = float(value.get('pitch', 0))
            yaw = float(value.get('yaw', 0))
            client.set_rotator_property(actor, property, roll, pitch, yaw, component)
            return {"type": "rotator", "value": {"roll": roll, "pitch": pitch, "yaw": yaw}}

    raise ValueError(
        f"Cannot determine property type for value: {value!r}. "
        f"Use type_hint='float'|'int'|'bool'|'string'|'vector'|'rotator'|'color'|'asset'"
    )


def _set_property_by_type(
    client: TempoActorControlClient,
    actor: str,
    property: str,
    value: Any,
    component: str,
    type_hint: str,
) -> Dict[str, Any]:
    """Set property using explicit type hint."""

    if type_hint == "float":
        client.set_float_property(actor, property, float(value), component)
        return {"type": "float", "value": float(value)}

    elif type_hint == "int":
        client.set_int_property(actor, property, int(value), component)
        return {"type": "int", "value": int(value)}

    elif type_hint == "bool":
        client.set_bool_property(actor, property, bool(value), component)
        return {"type": "bool", "value": bool(value)}

    elif type_hint == "string":
        client.set_string_property(actor, property, str(value), component)
        return {"type": "string", "value": str(value)}

    elif type_hint == "asset":
        client.set_asset_property(actor, property, str(value), component)
        return {"type": "asset", "value": str(value)}

    elif type_hint == "vector":
        if isinstance(value, dict):
            x, y, z = float(value['x']), float(value['y']), float(value['z'])
        else:
            x, y, z = float(value[0]), float(value[1]), float(value[2])
        client.set_vector_property(actor, property, x, y, z, component)
        return {"type": "vector", "value": [x, y, z]}

    elif type_hint == "rotator":
        if isinstance(value, dict):
            r = float(value.get('roll', value.get('r', 0)))
            p = float(value.get('pitch', value.get('p', 0)))
            y = float(value.get('yaw', value.get('y', 0)))
        else:
            r, p, y = float(value[0]), float(value[1]), float(value[2])
        client.set_rotator_property(actor, property, r, p, y, component)
        return {"type": "rotator", "value": {"roll": r, "pitch": p, "yaw": y}}

    elif type_hint == "color":
        if isinstance(value, dict):
            r, g, b = int(value['r']), int(value['g']), int(value['b'])
        else:
            r, g, b = int(value[0]), int(value[1]), int(value[2])
        client.set_color_property(actor, property, r, g, b, component)
        return {"type": "color", "value": [r, g, b]}

    else:
        raise ValueError(f"Unknown type_hint: {type_hint}")


# =============================================================================
# TOOL EXECUTION
# =============================================================================

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

    # ==========================================================================
    # Unified property setter
    # ==========================================================================
    elif tool_name == "tempo_set_property":
        result = _detect_and_set_property(
            client,
            actor=args["actor"],
            property=args["property"],
            value=args["value"],
            component=args.get("component", ""),
            type_hint=args.get("type_hint"),
        )
        return {
            "success": True,
            "actor": args["actor"],
            "property": args["property"],
            "detected_type": result["type"],
            "value": result["value"],
        }

    # ==========================================================================
    # Legacy typed setters (for backward compatibility)
    # ==========================================================================
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

    # ==========================================================================
    # Other tools
    # ==========================================================================
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
    description="Tempo ActorControlService - actor/component manipulation with unified property setter",
    tools=TOOLS,
    execute=execute,
    connect=connect,
))
