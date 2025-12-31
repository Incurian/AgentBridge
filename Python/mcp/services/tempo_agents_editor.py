"""
Tempo TempoAgentsEditorService MCP Tools

Zone graph builder pipeline for AI navigation.
"""

import json
from typing import Dict, Any
from . import register_service, ServiceModule
from .base import create_channel, safe_call

from TempoAgentsEditor import TempoAgentsEditor_pb2 as pb
from TempoAgentsEditor import TempoAgentsEditor_pb2_grpc as pb_grpc
from TempoScripting import Empty_pb2


TOOLS = [
    {
        "name": "tempo_run_zone_graph_builder",
        "description": "Run the Tempo Zone Graph Builder pipeline to generate navigation zones for AI agents.",
        "inputSchema": {
            "type": "object",
            "properties": {},
            "required": [],
        },
    },
]


class TempoAgentsEditorClient:
    """Client for Tempo's TempoAgentsEditorService."""

    def __init__(self, host: str = "localhost", port: int = 50051):
        self.channel = create_channel(host, port)
        self.stub = pb_grpc.TempoAgentsEditorServiceStub(self.channel)

    def run_zone_graph_builder_pipeline(self):
        return self.stub.RunTempoZoneGraphBuilderPipeline(Empty_pb2.Empty())


def connect(host: str, port: int) -> TempoAgentsEditorClient:
    return TempoAgentsEditorClient(host, port)


def execute(client: TempoAgentsEditorClient, tool_name: str, args: Dict[str, Any]) -> str:
    result = _execute_impl(client, tool_name, args)
    return json.dumps(result, indent=2)


def _execute_impl(client: TempoAgentsEditorClient, tool_name: str, args: Dict[str, Any]) -> Any:
    if tool_name == "tempo_run_zone_graph_builder":
        result = safe_call(client.run_zone_graph_builder_pipeline)
        if isinstance(result, dict) and "error" in result:
            return result
        return {
            "success": result.success,
            "action": "run_zone_graph_builder",
        }

    else:
        return {"error": f"Unknown tool: {tool_name}"}


register_service(ServiceModule(
    name="tempo_agents_editor",
    description="Tempo TempoAgentsEditorService - zone graph builder for AI navigation",
    tools=TOOLS,
    execute=execute,
    connect=connect,
))
