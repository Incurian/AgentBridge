"""
MCP Service Modules

Each module exposes a Tempo or AgentBridge gRPC service as MCP tools.
Services are auto-discovered and registered with the MCP server.
"""

from typing import List, Dict, Any, Callable
from dataclasses import dataclass


@dataclass
class ServiceModule:
    """Represents a service module that can be registered with the MCP server."""
    name: str
    description: str
    tools: List[Dict[str, Any]]
    execute: Callable[[Any, str, Dict[str, Any]], str]
    connect: Callable[[str, int], Any]  # Returns a client


# Registry of available service modules
_registry: Dict[str, ServiceModule] = {}


def register_service(module: ServiceModule):
    """Register a service module."""
    _registry[module.name] = module


def get_all_services() -> Dict[str, ServiceModule]:
    """Get all registered service modules."""
    return _registry.copy()


def get_service(name: str) -> ServiceModule:
    """Get a service module by name."""
    return _registry.get(name)


# Auto-import all service modules to trigger registration
def _auto_register():
    """Import all service modules to register them."""
    from . import agentbridge
    from . import tempo_time
    from . import tempo_actor_control


_auto_register()
