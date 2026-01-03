"""
Tempo MovementControlService MCP Tools

Vehicle and pawn movement commands, navigation.
"""

import json
from typing import Dict, Any
from . import register_service, ServiceModule
from .base import create_channel, safe_call

from TempoMovement import MovementControlService_pb2 as pb
from TempoMovement import MovementControlService_pb2_grpc as pb_grpc
from TempoScripting import Empty_pb2
from TempoScripting import Geometry_pb2


TOOLS = [
    {"name": "tempo_get_commandable_vehicles", "description": "Get list of vehicles that can be commanded.", "inputSchema": {"type": "object"}},
    {"name": "tempo_command_vehicle", "description": "Send acceleration and steering commands to a vehicle.", "inputSchema": {"type": "object", "properties": {"vehicle_name": {"type": "string"}, "acceleration": {"type": "number"}, "steering": {"type": "number"}}, "required": ["vehicle_name", "acceleration", "steering"]}},
    {"name": "tempo_get_commandable_pawns", "description": "Get list of pawns that can be commanded to move.", "inputSchema": {"type": "object"}},
    {"name": "tempo_pawn_move_to", "description": "Command a pawn to move to a location using navigation.", "inputSchema": {"type": "object", "properties": {"pawn_name": {"type": "string"}, "location": {"type": "array"}, "relative": {"type": "boolean", "default": False}}, "required": ["pawn_name", "location"]}},
    {"name": "tempo_rebuild_navigation", "description": "Rebuild the navigation mesh. Useful after spawning obstacles.", "inputSchema": {"type": "object"}},
]


class TempoMovementClient:
    """Client for Tempo's MovementControlService."""

    def __init__(self, host: str = "localhost", port: int = 50051):
        self.channel = create_channel(host, port)
        self.stub = pb_grpc.MovementControlServiceStub(self.channel)

    def get_commandable_vehicles(self):
        return self.stub.GetCommandableVehicles(Empty_pb2.Empty())

    def command_vehicle(self, vehicle_name: str, acceleration: float, steering: float):
        return self.stub.CommandVehicle(pb.VehicleCommandRequest(
            vehicle_name=vehicle_name,
            acceleration=acceleration,
            steering=steering,
        ))

    def get_commandable_pawns(self):
        return self.stub.GetCommandablePawns(Empty_pb2.Empty())

    def pawn_move_to_location(self, name: str, location: tuple, relative: bool = False):
        return self.stub.PawnMoveToLocation(pb.PawnMoveToLocationRequest(
            name=name,
            location=Geometry_pb2.Vector(x=location[0], y=location[1], z=location[2]),
            relative=relative,
        ))

    def rebuild_navigation(self):
        return self.stub.RebuildNavigation(Empty_pb2.Empty())


def connect(host: str, port: int) -> TempoMovementClient:
    return TempoMovementClient(host, port)


def execute(client: TempoMovementClient, tool_name: str, args: Dict[str, Any]) -> str:
    result = _execute_impl(client, tool_name, args)
    return json.dumps(result, indent=2)


MOVE_RESULT_NAMES = {
    0: "UNKNOWN",
    1: "SUCCESS",
    2: "BLOCKED",
    3: "OFF_PATH",
    4: "ABORTED",
    5: "INVALID",
}


def _execute_impl(client: TempoMovementClient, tool_name: str, args: Dict[str, Any]) -> Any:
    if tool_name == "tempo_get_commandable_vehicles":
        result = safe_call(client.get_commandable_vehicles)
        if isinstance(result, dict) and "error" in result:
            return result
        return {"vehicles": list(result.vehicle_name)}

    elif tool_name == "tempo_command_vehicle":
        safe_call(
            client.command_vehicle,
            args["vehicle_name"],
            args["acceleration"],
            args["steering"],
        )
        return {
            "success": True,
            "action": "command_vehicle",
            "vehicle": args["vehicle_name"],
        }

    elif tool_name == "tempo_get_commandable_pawns":
        result = safe_call(client.get_commandable_pawns)
        if isinstance(result, dict) and "error" in result:
            return result
        return {"pawns": list(result.pawn_name)}

    elif tool_name == "tempo_pawn_move_to":
        result = safe_call(
            client.pawn_move_to_location,
            args["pawn_name"],
            tuple(args["location"]),
            args.get("relative", False),
        )
        if isinstance(result, dict) and "error" in result:
            return result
        return {
            "success": result.result == 1,  # SUCCESS
            "result": MOVE_RESULT_NAMES.get(result.result, "UNKNOWN"),
            "pawn": args["pawn_name"],
        }

    elif tool_name == "tempo_rebuild_navigation":
        safe_call(client.rebuild_navigation)
        return {"success": True, "action": "rebuild_navigation"}

    else:
        return {"error": f"Unknown tool: {tool_name}"}


register_service(ServiceModule(
    name="tempo_movement",
    description="Tempo MovementControlService - vehicle/pawn commands, navigation",
    tools=TOOLS,
    execute=execute,
    connect=connect,
))
