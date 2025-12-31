"""
Tempo WorldStateService MCP Tools

Actor state queries (position, velocity, bounds).
Note: Streaming RPCs are exposed as single-shot queries.
"""

import json
from typing import Dict, Any
from . import register_service, ServiceModule
from .base import create_channel, safe_call

from TempoWorld import WorldState_pb2 as pb
from TempoWorld import WorldState_pb2_grpc as pb_grpc


TOOLS = [
    {
        "name": "tempo_get_actor_state",
        "description": "Get the current state (transform, velocity, bounds) of an actor.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "actor_name": {
                    "type": "string",
                    "description": "Name of the actor to query",
                },
                "include_hidden_components": {
                    "type": "boolean",
                    "description": "Include hidden components in bounds calculation",
                    "default": False,
                },
            },
            "required": ["actor_name"],
        },
    },
    {
        "name": "tempo_get_actors_near",
        "description": "Get states of all actors near a reference actor within a radius.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "near_actor_name": {
                    "type": "string",
                    "description": "Name of the reference actor",
                },
                "search_radius": {
                    "type": "number",
                    "description": "Search radius in Unreal units (cm)",
                },
                "include_static": {
                    "type": "boolean",
                    "description": "Include static actors",
                    "default": False,
                },
                "include_hidden_actors": {
                    "type": "boolean",
                    "description": "Include hidden actors",
                    "default": False,
                },
                "include_hidden_components": {
                    "type": "boolean",
                    "description": "Include hidden components in bounds",
                    "default": False,
                },
            },
            "required": ["near_actor_name", "search_radius"],
        },
    },
]


class TempoWorldStateClient:
    """Client for Tempo's WorldStateService."""

    def __init__(self, host: str = "localhost", port: int = 50051):
        self.channel = create_channel(host, port)
        self.stub = pb_grpc.WorldStateServiceStub(self.channel)

    def get_current_actor_state(self, actor_name: str, include_hidden_components: bool = False):
        return self.stub.GetCurrentActorState(pb.ActorStateRequest(
            actor_name=actor_name,
            include_hidden_components=include_hidden_components,
        ))

    def get_current_actor_states_near(
        self,
        near_actor_name: str,
        search_radius: float,
        include_static: bool = False,
        include_hidden_actors: bool = False,
        include_hidden_components: bool = False,
    ):
        return self.stub.GetCurrentActorStatesNear(pb.ActorStatesNearRequest(
            near_actor_name=near_actor_name,
            search_radius=search_radius,
            include_static=include_static,
            include_hidden_actors=include_hidden_actors,
            include_hidden_components=include_hidden_components,
        ))


def connect(host: str, port: int) -> TempoWorldStateClient:
    return TempoWorldStateClient(host, port)


def execute(client: TempoWorldStateClient, tool_name: str, args: Dict[str, Any]) -> str:
    result = _execute_impl(client, tool_name, args)
    return json.dumps(result, indent=2)


def _actor_state_to_dict(state) -> Dict[str, Any]:
    """Convert ActorState protobuf to dictionary."""
    return {
        "name": state.name,
        "timestamp": state.timestamp,
        "transform": {
            "location": [state.transform.location.x, state.transform.location.y, state.transform.location.z],
            "rotation": [state.transform.rotation.p, state.transform.rotation.y, state.transform.rotation.r],
            "scale": [state.transform.scale.x, state.transform.scale.y, state.transform.scale.z],
        },
        "linear_velocity": [state.linear_velocity.x, state.linear_velocity.y, state.linear_velocity.z],
        "angular_velocity": [state.angular_velocity.x, state.angular_velocity.y, state.angular_velocity.z],
        "bounds": {
            "origin": [state.bounds.origin.x, state.bounds.origin.y, state.bounds.origin.z],
            "extent": [state.bounds.extent.x, state.bounds.extent.y, state.bounds.extent.z],
        },
    }


def _execute_impl(client: TempoWorldStateClient, tool_name: str, args: Dict[str, Any]) -> Any:
    if tool_name == "tempo_get_actor_state":
        result = safe_call(
            client.get_current_actor_state,
            args["actor_name"],
            args.get("include_hidden_components", False),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        return _actor_state_to_dict(result)

    elif tool_name == "tempo_get_actors_near":
        result = safe_call(
            client.get_current_actor_states_near,
            args["near_actor_name"],
            args["search_radius"],
            args.get("include_static", False),
            args.get("include_hidden_actors", False),
            args.get("include_hidden_components", False),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        return {
            "count": len(result.actor_states),
            "actors": [_actor_state_to_dict(s) for s in result.actor_states],
        }

    else:
        return {"error": f"Unknown tool: {tool_name}"}


register_service(ServiceModule(
    name="tempo_world_state",
    description="Tempo WorldStateService - actor state queries (position, velocity, bounds)",
    tools=TOOLS,
    execute=execute,
    connect=connect,
))
