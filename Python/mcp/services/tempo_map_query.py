"""
Tempo MapQueryService MCP Tools

Lane and zone data queries for navigation/planning.
"""

import json
from typing import Dict, Any, List
from . import register_service, ServiceModule
from .base import create_channel, safe_call

from TempoMapQuery import MapQueries_pb2 as pb
from TempoMapQuery import MapQueries_pb2_grpc as pb_grpc
from TempoScripting import Geometry_pb2


LANE_RELATIONSHIP_NAMES = {
    0: "UNKNOWN",
    1: "SUCCESSOR",
    2: "PREDECESSOR",
    3: "NEIGHBOR",
}

LANE_ACCESSIBILITY_NAMES = {
    0: "UNKNOWN",
    1: "GREEN",
    2: "YELLOW",
    3: "RED",
    4: "STOP_SIGN",
    5: "YIELD_SIGN",
    6: "NO_TRAFFIC_CONTROL",
}


TOOLS = [
    {"name": "tempo_get_lanes", "description": "Get lane data (center points, width, connections) within a radius of a point.", "inputSchema": {"type": "object", "properties": {"center": {"type": "array"}, "radius": {"type": "number"}, "any_tags": {"type": "array"}, "all_tags": {"type": "array"}, "none_tags": {"type": "array"}}, "required": ["center", "radius"]}},
    {"name": "tempo_get_lane_accessibility", "description": "Check accessibility between two lanes (traffic light status, signs).", "inputSchema": {"type": "object", "properties": {"from_id": {"type": "integer"}, "to_id": {"type": "integer"}}, "required": ["from_id", "to_id"]}},
    {"name": "tempo_get_zones", "description": "Get zone data (boundaries, connections) within a radius of a point.", "inputSchema": {"type": "object", "properties": {"center": {"type": "array"}, "radius": {"type": "number"}, "any_tags": {"type": "array"}, "all_tags": {"type": "array"}, "none_tags": {"type": "array"}}, "required": ["center", "radius"]}},
]


class TempoMapQueryClient:
    """Client for Tempo's MapQueryService."""

    def __init__(self, host: str = "localhost", port: int = 50051):
        self.channel = create_channel(host, port)
        self.stub = pb_grpc.MapQueryServiceStub(self.channel)

    def get_lanes(
        self,
        center: tuple,
        radius: float,
        any_tags: List[str] = None,
        all_tags: List[str] = None,
        none_tags: List[str] = None,
    ):
        tag_filter = pb.TagFilter(
            any_tags=any_tags or [],
            all_tags=all_tags or [],
            none_tags=none_tags or [],
        )
        return self.stub.GetLanes(pb.LaneDataRequest(
            tag_filter=tag_filter,
            center=Geometry_pb2.Vector2D(x=center[0], y=center[1]),
            radius=radius,
        ))

    def get_lane_accessibility(self, from_id: int, to_id: int):
        return self.stub.GetLaneAccessibility(pb.LaneAccessibilityRequest(
            from_id=from_id,
            to_id=to_id,
        ))

    def get_zones(
        self,
        center: tuple,
        radius: float,
        any_tags: List[str] = None,
        all_tags: List[str] = None,
        none_tags: List[str] = None,
    ):
        tag_filter = pb.TagFilter(
            any_tags=any_tags or [],
            all_tags=all_tags or [],
            none_tags=none_tags or [],
        )
        return self.stub.GetZones(pb.ZoneDataRequest(
            tag_filter=tag_filter,
            center=Geometry_pb2.Vector(x=center[0], y=center[1], z=center[2]),
            radius=radius,
        ))


def connect(host: str, port: int) -> TempoMapQueryClient:
    return TempoMapQueryClient(host, port)


def execute(client: TempoMapQueryClient, tool_name: str, args: Dict[str, Any]) -> str:
    result = _execute_impl(client, tool_name, args)
    return json.dumps(result, indent=2)


def _lane_to_dict(lane) -> Dict[str, Any]:
    """Convert LaneData protobuf to dictionary."""
    return {
        "id": lane.id,
        "tags": list(lane.tags),
        "width": lane.width,
        "center_points": [[p.x, p.y, p.z] for p in lane.center_points],
        "connections": [
            {
                "id": c.id,
                "relationship": LANE_RELATIONSHIP_NAMES.get(c.relationship, "UNKNOWN"),
            }
            for c in lane.connections
        ],
    }


def _zone_to_dict(zone) -> Dict[str, Any]:
    """Convert ZoneData protobuf to dictionary."""
    return {
        "id": zone.id,
        "tags": list(zone.tags),
        "boundary_points": [[p.x, p.y, p.z] for p in zone.boundary_points],
        "connections": [{"id": c.id} for c in zone.connections],
    }


def _execute_impl(client: TempoMapQueryClient, tool_name: str, args: Dict[str, Any]) -> Any:
    if tool_name == "tempo_get_lanes":
        result = safe_call(
            client.get_lanes,
            tuple(args["center"]),
            args["radius"],
            args.get("any_tags"),
            args.get("all_tags"),
            args.get("none_tags"),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        return {
            "count": len(result.lanes),
            "lanes": [_lane_to_dict(l) for l in result.lanes],
        }

    elif tool_name == "tempo_get_lane_accessibility":
        result = safe_call(client.get_lane_accessibility, args["from_id"], args["to_id"])
        if isinstance(result, dict) and "error" in result:
            return result
        return {
            "from_id": args["from_id"],
            "to_id": args["to_id"],
            "accessibility": LANE_ACCESSIBILITY_NAMES.get(result.accessibility, "UNKNOWN"),
        }

    elif tool_name == "tempo_get_zones":
        result = safe_call(
            client.get_zones,
            tuple(args["center"]),
            args["radius"],
            args.get("any_tags"),
            args.get("all_tags"),
            args.get("none_tags"),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        return {
            "count": len(result.zones),
            "zones": [_zone_to_dict(z) for z in result.zones],
        }

    else:
        return {"error": f"Unknown tool: {tool_name}"}


register_service(ServiceModule(
    name="tempo_map_query",
    description="Tempo MapQueryService - lane and zone queries for navigation",
    tools=TOOLS,
    execute=execute,
    connect=connect,
))
