"""
AgentBridge MCP Server

Exposes Unreal Engine operations to Claude and other LLM agents via MCP.
"""

from .server import serve
from .client import AgentBridgeGrpcClient

__version__ = "0.1.0"
__all__ = ["serve", "AgentBridgeGrpcClient"]
