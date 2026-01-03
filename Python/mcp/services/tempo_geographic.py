"""
Tempo GeographicService MCP Tools

Date/time, geographic coordinates, and day cycle control.
"""

import json
from typing import Dict, Any
from . import register_service, ServiceModule
from .base import create_channel, safe_call

from TempoGeographic import Geographic_pb2 as pb
from TempoGeographic import Geographic_pb2_grpc as pb_grpc
from TempoScripting import Empty_pb2


TOOLS = [
    {"name": "tempo_set_date", "description": "Set the simulation date.", "inputSchema": {"type": "object", "properties": {"day": {"type": "integer"}, "month": {"type": "integer"}, "year": {"type": "integer"}}, "required": ["day", "month", "year"]}},
    {"name": "tempo_set_time_of_day", "description": "Set the simulation time of day.", "inputSchema": {"type": "object", "properties": {"hour": {"type": "integer"}, "minute": {"type": "integer"}, "second": {"type": "integer", "default": 0}}, "required": ["hour", "minute"]}},
    {"name": "tempo_set_day_cycle_rate", "description": "Set the day/night cycle speed relative to real time. 1.0 = real time, 60.0 = 1 hour per minute.", "inputSchema": {"type": "object", "properties": {"rate": {"type": "number"}}, "required": ["rate"]}},
    {"name": "tempo_get_datetime", "description": "Get the current simulation date and time.", "inputSchema": {"type": "object"}},
    {"name": "tempo_set_geographic_reference", "description": "Set the geographic reference point (lat/lon/alt) for the simulation world.", "inputSchema": {"type": "object", "properties": {"latitude": {"type": "number"}, "longitude": {"type": "number"}, "altitude": {"type": "number", "default": 0}}, "required": ["latitude", "longitude"]}},
]


class TempoGeographicClient:
    """Client for Tempo's GeographicService."""

    def __init__(self, host: str = "localhost", port: int = 50051):
        self.channel = create_channel(host, port)
        self.stub = pb_grpc.GeographicServiceStub(self.channel)

    def set_date(self, day: int, month: int, year: int):
        return self.stub.SetDate(pb.Date(day=day, month=month, year=year))

    def set_time_of_day(self, hour: int, minute: int, second: int = 0):
        return self.stub.SetTimeOfDay(pb.TimeOfDay(hour=hour, minute=minute, second=second))

    def set_day_cycle_relative_rate(self, rate: float):
        return self.stub.SetDayCycleRelativeRate(pb.DayCycleRateRequest(rate=rate))

    def get_datetime(self):
        return self.stub.GetDateTime(Empty_pb2.Empty())

    def set_geographic_reference(self, latitude: float, longitude: float, altitude: float = 0):
        return self.stub.SetGeographicReference(pb.GeographicCoordinate(
            latitude=latitude,
            longitude=longitude,
            altitude=altitude,
        ))


def connect(host: str, port: int) -> TempoGeographicClient:
    return TempoGeographicClient(host, port)


def execute(client: TempoGeographicClient, tool_name: str, args: Dict[str, Any]) -> str:
    result = _execute_impl(client, tool_name, args)
    return json.dumps(result, indent=2)


def _execute_impl(client: TempoGeographicClient, tool_name: str, args: Dict[str, Any]) -> Any:
    if tool_name == "tempo_set_date":
        safe_call(client.set_date, args["day"], args["month"], args["year"])
        return {
            "success": True,
            "action": "set_date",
            "date": f"{args['year']}-{args['month']:02d}-{args['day']:02d}",
        }

    elif tool_name == "tempo_set_time_of_day":
        safe_call(client.set_time_of_day, args["hour"], args["minute"], args.get("second", 0))
        return {
            "success": True,
            "action": "set_time_of_day",
            "time": f"{args['hour']:02d}:{args['minute']:02d}:{args.get('second', 0):02d}",
        }

    elif tool_name == "tempo_set_day_cycle_rate":
        safe_call(client.set_day_cycle_relative_rate, args["rate"])
        return {"success": True, "action": "set_day_cycle_rate", "rate": args["rate"]}

    elif tool_name == "tempo_get_datetime":
        result = safe_call(client.get_datetime)
        if isinstance(result, dict) and "error" in result:
            return result
        return {
            "date": {
                "day": result.date.day,
                "month": result.date.month,
                "year": result.date.year,
            },
            "time": {
                "hour": result.time.hour,
                "minute": result.time.minute,
                "second": result.time.second,
            },
        }

    elif tool_name == "tempo_set_geographic_reference":
        safe_call(
            client.set_geographic_reference,
            args["latitude"],
            args["longitude"],
            args.get("altitude", 0),
        )
        return {
            "success": True,
            "action": "set_geographic_reference",
            "latitude": args["latitude"],
            "longitude": args["longitude"],
            "altitude": args.get("altitude", 0),
        }

    else:
        return {"error": f"Unknown tool: {tool_name}"}


register_service(ServiceModule(
    name="tempo_geographic",
    description="Tempo GeographicService - date/time, geographic coordinates",
    tools=TOOLS,
    execute=execute,
    connect=connect,
))
