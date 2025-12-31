"""
Tempo SensorService MCP Tools

Sensor discovery. Note: Streaming image APIs are not exposed as MCP tools.
"""

import json
from typing import Dict, Any
from . import register_service, ServiceModule
from .base import create_channel, safe_call

from TempoSensors import Sensors_pb2 as pb
from TempoSensors import Sensors_pb2_grpc as pb_grpc


MEASUREMENT_TYPE_NAMES = {
    0: "COLOR_IMAGE",
    1: "DEPTH_IMAGE",
    2: "LABEL_IMAGE",
}


TOOLS = [
    {
        "name": "tempo_get_available_sensors",
        "description": "Get list of available sensors (cameras) with their capabilities.",
        "inputSchema": {
            "type": "object",
            "properties": {},
            "required": [],
        },
    },
]


class TempoSensorsClient:
    """Client for Tempo's SensorService."""

    def __init__(self, host: str = "localhost", port: int = 50051):
        self.channel = create_channel(host, port)
        self.stub = pb_grpc.SensorServiceStub(self.channel)

    def get_available_sensors(self):
        return self.stub.GetAvailableSensors(pb.AvailableSensorsRequest())


def connect(host: str, port: int) -> TempoSensorsClient:
    return TempoSensorsClient(host, port)


def execute(client: TempoSensorsClient, tool_name: str, args: Dict[str, Any]) -> str:
    result = _execute_impl(client, tool_name, args)
    return json.dumps(result, indent=2)


def _execute_impl(client: TempoSensorsClient, tool_name: str, args: Dict[str, Any]) -> Any:
    if tool_name == "tempo_get_available_sensors":
        result = safe_call(client.get_available_sensors)
        if isinstance(result, dict) and "error" in result:
            return result
        sensors = []
        for s in result.available_sensors:
            sensors.append({
                "owner": s.owner,
                "name": s.name,
                "rate": s.rate,
                "measurement_types": [
                    MEASUREMENT_TYPE_NAMES.get(t, f"UNKNOWN({t})")
                    for t in s.measurement_types
                ],
            })
        return {
            "count": len(sensors),
            "sensors": sensors,
        }

    else:
        return {"error": f"Unknown tool: {tool_name}"}


register_service(ServiceModule(
    name="tempo_sensors",
    description="Tempo SensorService - sensor/camera discovery",
    tools=TOOLS,
    execute=execute,
    connect=connect,
))
