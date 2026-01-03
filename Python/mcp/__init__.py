"""
AgentBridge MCP Server

Exposes Unreal Engine operations to Claude and other LLM agents via MCP.
"""

__version__ = "0.3.0"


def __getattr__(name):
    """Lazy imports to avoid circular import warnings when using python -m."""
    if name == "serve":
        from .server import serve
        return serve
    elif name == "AgentBridgeGrpcClient":
        from .client import AgentBridgeGrpcClient
        return AgentBridgeGrpcClient
    raise AttributeError(f"module {__name__!r} has no attribute {name!r}")


__all__ = ["serve", "AgentBridgeGrpcClient"]
