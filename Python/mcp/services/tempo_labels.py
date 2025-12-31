"""
Tempo LabelService MCP Tools

Semantic labeling for instance segmentation.
"""

import json
from typing import Dict, Any
from . import register_service, ServiceModule
from .base import create_channel, safe_call

from TempoLabels import Labels_pb2 as pb
from TempoLabels import Labels_pb2_grpc as pb_grpc
from TempoScripting import Empty_pb2


TOOLS = [
    {
        "name": "tempo_get_label_map",
        "description": "Get the mapping from instance IDs to semantic label IDs. Useful for interpreting segmentation images.",
        "inputSchema": {
            "type": "object",
            "properties": {},
            "required": [],
        },
    },
]


class TempoLabelsClient:
    """Client for Tempo's LabelService."""

    def __init__(self, host: str = "localhost", port: int = 50051):
        self.channel = create_channel(host, port)
        self.stub = pb_grpc.LabelServiceStub(self.channel)

    def get_instance_to_semantic_id_map(self):
        return self.stub.GetInstanceToSemanticIdMap(Empty_pb2.Empty())


def connect(host: str, port: int) -> TempoLabelsClient:
    return TempoLabelsClient(host, port)


def execute(client: TempoLabelsClient, tool_name: str, args: Dict[str, Any]) -> str:
    result = _execute_impl(client, tool_name, args)
    return json.dumps(result, indent=2)


def _execute_impl(client: TempoLabelsClient, tool_name: str, args: Dict[str, Any]) -> Any:
    if tool_name == "tempo_get_label_map":
        result = safe_call(client.get_instance_to_semantic_id_map)
        if isinstance(result, dict) and "error" in result:
            return result
        # Convert to a simple dictionary mapping
        label_map = {}
        for pair in result.instance_semantic_id_pairs:
            label_map[pair.InstanceId] = pair.SemanticId
        return {
            "count": len(label_map),
            "instance_to_semantic": label_map,
        }

    else:
        return {"error": f"Unknown tool: {tool_name}"}


register_service(ServiceModule(
    name="tempo_labels",
    description="Tempo LabelService - semantic label mapping for segmentation",
    tools=TOOLS,
    execute=execute,
    connect=connect,
))
