"""
MCP Service Modules

Each module exposes a Tempo or AgentBridge gRPC service as MCP tools.
Services are auto-discovered and registered with the MCP server.

Supports modular loading via profiles or explicit module lists.
"""

import os
import logging
from typing import List, Dict, Any, Callable, Optional, Set
from dataclasses import dataclass

logger = logging.getLogger(__name__)


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

# Track which modules are loaded
_loaded_modules: Set[str] = set()


# =============================================================================
# MODULE DEFINITIONS (v2 - Consolidated Structure)
# =============================================================================

# Maps logical module names to tool names they provide
# 8 modules total: core, classes, editor, world_partition, files, bp_toolkit, tempo_sim
MODULES = {
    # =========================================================================
    # Core (6 tools) - Always loaded, essential operations
    # =========================================================================
    "core": {
        "tools": [
            "help", "list_worlds", "set_target_world", "quit",
            "execute_console_command", "search_console_commands",
        ],
        "description": "Essential operations and console commands",
    },

    # =========================================================================
    # Classes (17 tools) - Actors, components, transforms, assets, functions
    # Phase 2 consolidated: 9 tools -> 4 unified tools
    # =========================================================================
    "classes": {
        "tools": [
            # Actor operations
            "query_actors", "get_actor", "spawn_actor", "delete_actor", "duplicate_actor",
            # Properties
            "get_property", "set_property",
            # Transforms (Phase 2 unified - works on actors AND components)
            "set_transform", "get_transform",
            # Attachment (Phase 2 unified - works on actors AND components)
            "attach", "detach",
            # Components
            "add_component",
            # Functions
            "call_function",
            # Type discovery
            "list_classes", "get_class_schema",
            # Assets
            "create_asset", "save_asset", "duplicate_asset", "save_actor_as_blueprint",
        ],
        "description": "Actors, components, transforms, assets, and functions",
    },

    # =========================================================================
    # Editor (7 tools) - PIE, simulate, level management
    # Note: Tool names no longer have tempo_ prefix
    # =========================================================================
    "editor": {
        "tools": [
            "play_in_editor", "simulate", "stop",
            "save_level", "open_level", "new_level", "get_current_level",
        ],
        "description": "Editor PIE and level management",
    },

    # =========================================================================
    # World Partition (7 tools) - Streaming, landscape queries
    # Note: Will consolidate to 5 tools in Phase 2 (query_all_actors, get_actors_in_data_layer -> query_actors)
    # =========================================================================
    "world_partition": {
        "tools": [
            "is_world_partitioned", "query_all_actors", "get_streaming_state",
            "query_landscape", "get_landscape_bounds",
            "get_data_layers", "get_actors_in_data_layer",
        ],
        "description": "Large world streaming queries",
    },

    # =========================================================================
    # Files (4 tools) - Project file operations
    # Note: Phase 3 will add move_project_file, create_project_directory
    # =========================================================================
    "files": {
        "tools": [
            "read_project_file", "write_project_file",
            "list_project_directory", "copy_project_file",
        ],
        "description": "Project file operations",
    },

    # =========================================================================
    # bp_toolkit (26 tools) - Blueprint, PCG, and offline asset manipulation
    # =========================================================================
    "bp_toolkit": {
        "tools": [
            # Live Blueprint graph editing (6)
            "bp_create_node", "bp_connect_pins", "bp_disconnect_pins",
            "bp_delete_node", "bp_list_nodes", "bp_list_pins",
            # Live PCG graph editing (6)
            "pcg_add_node", "pcg_connect", "pcg_disconnect",
            "pcg_delete_node", "pcg_list_nodes", "pcg_get_input_output_nodes",
            # Offline asset manipulation (14)
            "bp_export_asset", "bp_import_asset", "bp_detect_type", "bp_get_info",
            "bp_list_properties", "bp_get_property", "bp_set_property",
            "bp_clone_asset", "bp_list_graphs", "bp_add_comment",
            "bp_clone_node", "bp_find", "bp_query", "bp_parse",
        ],
        "description": "Blueprint and PCG graphs, offline asset manipulation",
    },

    # =========================================================================
    # tempo_sim (28 tools) - All Tempo simulation features
    # =========================================================================
    "tempo_sim": {
        "tools": [
            # Simulation control (10)
            "tempo_play", "tempo_pause", "tempo_step",
            "tempo_advance_steps", "tempo_set_time_mode", "tempo_set_sim_rate",
            "tempo_set_control_mode",
            "tempo_load_level", "tempo_finish_loading_level",
            "tempo_set_viewport_render",
            # Time/Geographic (5)
            "tempo_set_date", "tempo_set_time_of_day",
            "tempo_set_day_cycle_rate", "tempo_get_datetime",
            "tempo_set_geographic_reference",
            # State (2)
            "tempo_get_actor_state", "tempo_get_actors_near",
            # AI/Movement (6)
            "tempo_get_commandable_vehicles", "tempo_command_vehicle",
            "tempo_get_commandable_pawns", "tempo_pawn_move_to",
            "tempo_rebuild_navigation", "tempo_run_zone_graph_builder",
            # Sensors/Labels (2)
            "tempo_get_available_sensors", "tempo_get_label_map",
            # Map (3)
            "tempo_get_lanes", "tempo_get_lane_accessibility", "tempo_get_zones",
        ],
        "description": "Tempo simulation, time, AI, sensors, and map queries",
    },
}


# =============================================================================
# PROFILE DEFINITIONS (v2)
# =============================================================================

PROFILES = {
    # Absolute minimum - 6 tools
    "core": ["core"],

    # Level editing - 35 tools (DEFAULT for editor work)
    "standard": ["core", "classes", "editor", "files"],

    # Full editor work - 42 tools
    "editor": ["core", "classes", "editor", "world_partition", "files"],

    # Blueprint/PCG editing - 61 tools
    "scripting": ["core", "classes", "editor", "files", "bp_toolkit"],

    # Runtime/PIE testing - 34 tools
    "simulation": ["core", "classes", "tempo_sim"],

    # Everything - all modules (~100 tools)
    "full": list(MODULES.keys()),
}

DEFAULT_PROFILE = os.environ.get("AGENTBRIDGE_PROFILE", "full")


# =============================================================================
# REGISTRATION FUNCTIONS
# =============================================================================

def register_service(module: ServiceModule):
    """Register a service module."""
    _registry[module.name] = module


def get_all_services() -> Dict[str, ServiceModule]:
    """Get all registered service modules."""
    return _registry.copy()


def get_service(name: str) -> ServiceModule:
    """Get a service module by name."""
    return _registry.get(name)


# =============================================================================
# MODULE LOADING
# =============================================================================

def _import_all_services():
    """Import all service modules to populate the registry."""
    # AgentBridge service
    from . import agentbridge

    # Tempo services
    from . import tempo_time
    from . import tempo_actor_control
    from . import tempo_core
    from . import tempo_core_editor
    from . import tempo_geographic
    from . import tempo_movement
    from . import tempo_world_state
    from . import tempo_labels
    from . import tempo_sensors
    from . import tempo_map_query
    from . import tempo_agents_editor

    # Optional: bp_toolkit (only if submodule present)
    from . import bp_toolkit


def get_profile_modules(profile: str) -> List[str]:
    """Get the list of modules for a profile."""
    return PROFILES.get(profile, PROFILES[DEFAULT_PROFILE])


def get_enabled_tools(modules: List[str]) -> Set[str]:
    """Get the set of tool names enabled by a list of modules."""
    enabled = set()
    for module_name in modules:
        if module_name in MODULES:
            enabled.update(MODULES[module_name]["tools"])
    return enabled


def get_available_modules() -> Dict[str, str]:
    """Get all available modules with descriptions."""
    return {name: info["description"] for name, info in MODULES.items()}


def get_available_profiles() -> Dict[str, int]:
    """Get all profiles with their tool counts."""
    result = {}
    for profile_name, module_list in PROFILES.items():
        tools = get_enabled_tools(module_list)
        result[profile_name] = len(tools)
    return result


def count_tools_in_profile(profile: str) -> int:
    """Count total tools in a profile."""
    modules = get_profile_modules(profile)
    return len(get_enabled_tools(modules))


# =============================================================================
# FILTERED SERVICE ACCESS
# =============================================================================

class FilteredServiceModule:
    """A service module with tools filtered by enabled modules."""

    def __init__(self, base: ServiceModule, enabled_tools: Set[str]):
        self.name = base.name
        self.description = base.description
        self.execute = base.execute
        self.connect = base.connect
        # Filter tools to only include enabled ones
        self.tools = [t for t in base.tools if t["name"] in enabled_tools]


def get_filtered_services(enabled_modules: List[str]) -> Dict[str, ServiceModule]:
    """
    Get services with tools filtered to only those in enabled modules.

    Args:
        enabled_modules: List of module names to enable

    Returns:
        Dict of service name -> filtered ServiceModule
    """
    enabled_tools = get_enabled_tools(enabled_modules)

    filtered = {}
    for name, service in _registry.items():
        filtered_service = FilteredServiceModule(service, enabled_tools)
        if filtered_service.tools:  # Only include if it has enabled tools
            filtered[name] = filtered_service

    return filtered


# =============================================================================
# INITIALIZATION
# =============================================================================

# Import all services at module load time to populate registry
_import_all_services()

# Log module info
_total_tools = sum(len(s.tools) for s in _registry.values())
logger.info(f"Loaded {len(_registry)} services with {_total_tools} total tools")
logger.info(f"Available profiles: {list(PROFILES.keys())}")
logger.info(f"Default profile: {DEFAULT_PROFILE}")
