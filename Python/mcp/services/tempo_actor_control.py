"""
Tempo ActorControlService MCP Tools

Exposes Tempo-specific actor operations that AgentBridge doesn't provide:
- tempo_spawn_actor: Spawn with relative_to (spawn relative to another actor)
- tempo_add_component: Dynamically add components to actors
- tempo_call_function: Call instance methods on actors/components

NOTE: Most actor operations should use AgentBridge tools instead:
- Use spawn_actor, delete_actor, query_actors, get_actor (AgentBridge)
- Use set_property, get_property (AgentBridge - handles structs, arrays, nested paths)
- Use set_actor_transform (AgentBridge)
"""

import json
from typing import Dict, Any
from . import register_service, ServiceModule
from .base import create_channel, safe_call

# Import Tempo's generated stubs
from TempoWorld import ActorControl_pb2 as pb
from TempoWorld import ActorControl_pb2_grpc as pb_grpc
from TempoScripting import Empty_pb2
from TempoScripting import Geometry_pb2


# =============================================================================
# TOOLS - Only Tempo-specific functionality (AgentBridge handles the rest)
# =============================================================================

# All tools consolidated into agentbridge.py:
# - tempo_spawn_actor -> spawn_actor(relative_to=...)
# - tempo_add_component -> add_component()
# - tempo_call_function -> call_function("Actor.Function") syntax
#
# The client methods below are still used by agentbridge.py for routing.
TOOLS = []


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
# TOOL EXECUTION
# =============================================================================

def execute(client: TempoActorControlClient, tool_name: str, args: Dict[str, Any]) -> str:
    """Execute a tempo_actor_control tool."""
    result = _execute_impl(client, tool_name, args)
    return json.dumps(result, indent=2, default=str)


def _execute_impl(client: TempoActorControlClient, tool_name: str, args: Dict[str, Any]) -> Any:
    """Implementation of tool execution.

    NOTE: All tempo_actor_control tools are now consolidated into agentbridge.py:
    - spawn_actor(relative_to=...) for relative spawning
    - add_component() for adding components
    - call_function("Actor.Function") for instance methods

    The client methods are still used by agentbridge.py for routing.
    """
    return {"error": f"Unknown tool: {tool_name}. All tools consolidated into agentbridge.py."}


# Register this service module (no tools exposed - all consolidated into agentbridge.py)
register_service(ServiceModule(
    name="tempo_actor_control",
    description="Internal: Tempo client for actor operations (tools consolidated into agentbridge.py)",
    tools=TOOLS,
    execute=execute,
    connect=connect,
))
