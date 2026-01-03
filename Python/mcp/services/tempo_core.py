"""
Tempo TempoCoreService MCP Tools

Level management, control mode, and engine operations.
"""

import json
from typing import Dict, Any
from . import register_service, ServiceModule
from .base import create_channel, safe_call

from TempoCore import TempoCore_pb2 as pb
from TempoCore import TempoCore_pb2_grpc as pb_grpc
from TempoScripting import Empty_pb2


TOOLS = [
    {"name": "tempo_load_level", "description": "Load a level/map in Unreal Engine. Can optionally defer loading and start paused.", "inputSchema": {"type": "object", "properties": {"level": {"type": "string"}, "deferred": {"type": "boolean", "default": False}, "start_paused": {"type": "boolean", "default": False}}, "required": ["level"]}},
    {"name": "tempo_finish_loading_level", "description": "Complete a deferred level load. Call after tempo_load_level with deferred=true.", "inputSchema": {"type": "object"}},
    {"name": "tempo_get_current_level", "description": "Get the name of the currently loaded level.", "inputSchema": {"type": "object"}},
    {"name": "tempo_quit", "description": "Quit the Unreal Engine application.", "inputSchema": {"type": "object"}},
    {"name": "tempo_set_viewport_render", "description": "Enable or disable main viewport rendering. Disabling can improve performance for headless simulation.", "inputSchema": {"type": "object", "properties": {"enabled": {"type": "boolean"}}, "required": ["enabled"]}},
    {"name": "tempo_set_control_mode", "description": "Set the simulation control mode: NONE (0), USER (1), OPEN_LOOP (2), CLOSED_LOOP (3).", "inputSchema": {"type": "object", "properties": {"mode": {"type": "string", "enum": ["NONE", "USER", "OPEN_LOOP", "CLOSED_LOOP"]}}, "required": ["mode"]}},
]


class TempoCoreClient:
    """Client for Tempo's TempoCoreService."""

    def __init__(self, host: str = "localhost", port: int = 50051):
        self.channel = create_channel(host, port)
        self.stub = pb_grpc.TempoCoreServiceStub(self.channel)

    def load_level(self, level: str, deferred: bool = False, start_paused: bool = False):
        return self.stub.LoadLevel(pb.LoadLevelRequest(
            level=level,
            deferred=deferred,
            start_paused=start_paused,
        ))

    def finish_loading_level(self):
        return self.stub.FinishLoadingLevel(Empty_pb2.Empty())

    def get_current_level_name(self):
        return self.stub.GetCurrentLevelName(Empty_pb2.Empty())

    def quit(self):
        return self.stub.Quit(Empty_pb2.Empty())

    def set_main_viewport_render_enabled(self, enabled: bool):
        return self.stub.SetMainViewportRenderEnabled(
            pb.SetMainViewportRenderEnabledRequest(enabled=enabled)
        )

    def set_control_mode(self, mode: int):
        return self.stub.SetControlMode(pb.SetControlModeRequest(mode=mode))


def connect(host: str, port: int) -> TempoCoreClient:
    return TempoCoreClient(host, port)


def execute(client: TempoCoreClient, tool_name: str, args: Dict[str, Any]) -> str:
    result = _execute_impl(client, tool_name, args)
    return json.dumps(result, indent=2)


def _execute_impl(client: TempoCoreClient, tool_name: str, args: Dict[str, Any]) -> Any:
    if tool_name == "tempo_load_level":
        safe_call(
            client.load_level,
            level=args["level"],
            deferred=args.get("deferred", False),
            start_paused=args.get("start_paused", False),
        )
        return {"success": True, "action": "load_level", "level": args["level"]}

    elif tool_name == "tempo_finish_loading_level":
        safe_call(client.finish_loading_level)
        return {"success": True, "action": "finish_loading_level"}

    elif tool_name == "tempo_get_current_level":
        result = safe_call(client.get_current_level_name)
        if isinstance(result, dict) and "error" in result:
            return result
        return {"level": result.level}

    elif tool_name == "tempo_quit":
        safe_call(client.quit)
        return {"success": True, "action": "quit"}

    elif tool_name == "tempo_set_viewport_render":
        safe_call(client.set_main_viewport_render_enabled, args["enabled"])
        return {"success": True, "action": "set_viewport_render", "enabled": args["enabled"]}

    elif tool_name == "tempo_set_control_mode":
        mode_map = {"NONE": 0, "USER": 1, "OPEN_LOOP": 2, "CLOSED_LOOP": 3}
        mode = mode_map.get(args["mode"], 0)
        safe_call(client.set_control_mode, mode)
        return {"success": True, "action": "set_control_mode", "mode": args["mode"]}

    else:
        return {"error": f"Unknown tool: {tool_name}"}


register_service(ServiceModule(
    name="tempo_core",
    description="Tempo TempoCoreService - level loading, control mode, engine ops",
    tools=TOOLS,
    execute=execute,
    connect=connect,
))
