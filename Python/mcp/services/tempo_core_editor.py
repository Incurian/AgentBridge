"""
Editor MCP Tools

Editor-specific operations: PIE, simulate, level management.
Note: Tool names no longer have tempo_ prefix - these are general editor operations.
"""

import json
from typing import Dict, Any
from . import register_service, ServiceModule
from .base import create_channel, safe_call

from TempoCoreEditor import TempoCoreEditor_pb2 as pb
from TempoCoreEditor import TempoCoreEditor_pb2_grpc as pb_grpc
from TempoCore import TempoCore_pb2 as core_pb
from TempoCore import TempoCore_pb2_grpc as core_pb_grpc
from TempoScripting import Empty_pb2


TOOLS = [
    {"name": "play_in_editor", "description": "Start Play-In-Editor (PIE) session.", "inputSchema": {"type": "object"}},
    {"name": "simulate", "description": "Start Simulate mode in the editor.", "inputSchema": {"type": "object"}},
    {"name": "stop", "description": "Stop the current PIE or Simulate session.", "inputSchema": {"type": "object"}},
    {"name": "save_level", "description": "Save the current level to a file.", "inputSchema": {"type": "object", "properties": {"path": {"type": "string"}, "overwrite": {"type": "boolean", "default": False}}, "required": ["path"]}},
    {"name": "open_level", "description": "Open a level in the editor.", "inputSchema": {"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}},
    {"name": "new_level", "description": "Create a new empty level in the editor.", "inputSchema": {"type": "object"}},
    {"name": "get_current_level", "description": "Get the name of the currently loaded level.", "inputSchema": {"type": "object"}},
]


class EditorClient:
    """Client for editor operations (PIE, level management)."""

    def __init__(self, host: str = "localhost", port: int = 50051):
        self.channel = create_channel(host, port)
        self.stub = pb_grpc.TempoCoreEditorServiceStub(self.channel)
        self.core_stub = core_pb_grpc.TempoCoreServiceStub(self.channel)

    def play_in_editor(self):
        return self.stub.PlayInEditor(Empty_pb2.Empty())

    def simulate(self):
        return self.stub.Simulate(Empty_pb2.Empty())

    def stop(self):
        return self.stub.Stop(Empty_pb2.Empty())

    def save_level(self, path: str, overwrite: bool = False):
        return self.stub.SaveLevel(pb.SaveLevelRequest(path=path, overwrite=overwrite))

    def open_level(self, path: str):
        return self.stub.OpenLevel(pb.OpenLevelRequest(path=path))

    def new_level(self):
        return self.stub.NewLevel(Empty_pb2.Empty())

    def get_current_level(self):
        return self.core_stub.GetCurrentLevelName(Empty_pb2.Empty())


def connect(host: str, port: int) -> EditorClient:
    return EditorClient(host, port)


def execute(client: EditorClient, tool_name: str, args: Dict[str, Any]) -> str:
    result = _execute_impl(client, tool_name, args)
    return json.dumps(result, indent=2)


def _execute_impl(client: EditorClient, tool_name: str, args: Dict[str, Any]) -> Any:
    if tool_name == "play_in_editor":
        safe_call(client.play_in_editor)
        return {"success": True, "action": "play_in_editor"}

    elif tool_name == "simulate":
        safe_call(client.simulate)
        return {"success": True, "action": "simulate"}

    elif tool_name == "stop":
        safe_call(client.stop)
        return {"success": True, "action": "stop"}

    elif tool_name == "save_level":
        safe_call(client.save_level, args["path"], args.get("overwrite", False))
        return {"success": True, "action": "save_level", "path": args["path"]}

    elif tool_name == "open_level":
        safe_call(client.open_level, args["path"])
        return {"success": True, "action": "open_level", "path": args["path"]}

    elif tool_name == "new_level":
        safe_call(client.new_level)
        return {"success": True, "action": "new_level"}

    elif tool_name == "get_current_level":
        result = safe_call(client.get_current_level)
        if isinstance(result, dict) and "error" in result:
            return result
        return {"level": result.level}

    else:
        return {"error": f"Unknown tool: {tool_name}"}


register_service(ServiceModule(
    name="editor",
    description="Editor operations - PIE, simulate, level management",
    tools=TOOLS,
    execute=execute,
    connect=connect,
))
