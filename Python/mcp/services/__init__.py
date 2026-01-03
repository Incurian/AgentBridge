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
# MODULE DEFINITIONS
# =============================================================================

# Maps logical module names to tool names they provide
MODULES = {
    # Core (always loaded) - essential for any workflow
    "core": {
        "tools": [
            "help", "list_worlds", "set_target_world",
            "query_actors", "get_actor", "spawn_actor", "delete_actor",
            "get_property", "set_property",
            "list_classes", "get_class_schema",
        ],
        "description": "Essential actor and property operations",
    },

    # Standard modules - commonly needed for editor work
    "transforms": {
        "tools": ["set_actor_transform", "duplicate_actor"],
        "description": "Actor transform and duplication",
    },
    "assets": {
        "tools": ["create_asset", "save_asset", "duplicate_asset", "save_actor_as_blueprint"],
        "description": "Asset creation and management",
    },
    "console": {
        "tools": ["execute_console_command", "search_console_commands"],
        "description": "Console command execution",
    },
    "functions": {
        "tools": ["call_static_function", "call_asset_function"],
        "description": "Blueprint function calls",
    },
    "files": {
        "tools": ["read_project_file", "write_project_file", "list_project_directory", "copy_project_file"],
        "description": "Project file operations",
    },

    # Extended modules - specialized functionality
    "components": {
        "tools": [
            "get_component_transform", "set_component_transform",
            "attach_actor", "detach_actor",
            "attach_component", "detach_component",
        ],
        "description": "Component transforms and attachment",
    },
    "world_partition": {
        "tools": [
            "is_world_partitioned", "query_all_actors", "get_streaming_state",
            "query_landscape", "get_landscape_bounds",
            "get_data_layers", "get_actors_in_data_layer",
        ],
        "description": "Large world streaming queries",
    },
    "blueprints": {
        "tools": [
            "bp_create_node", "bp_connect_pins", "bp_disconnect_pins",
            "bp_delete_node", "bp_list_nodes", "bp_list_pins",
        ],
        "description": "Blueprint graph manipulation",
    },
    "pcg": {
        "tools": [
            "pcg_add_node", "pcg_connect", "pcg_disconnect",
            "pcg_delete_node", "pcg_list_nodes", "pcg_get_input_output_nodes",
        ],
        "description": "PCG graph manipulation",
    },

    # Tempo simulation modules
    "simulation": {
        "tools": [
            "tempo_play", "tempo_pause", "tempo_step",
            "tempo_advance_steps", "tempo_set_time_mode", "tempo_set_sim_rate",
        ],
        "description": "Simulation playback control",
    },
    "tempo_actors": {
        "tools": [
            "tempo_get_all_actors", "tempo_spawn_actor", "tempo_destroy_actor",
            "tempo_get_components", "tempo_add_component",
            "tempo_get_actor_properties", "tempo_get_component_properties",
            "tempo_set_float_property", "tempo_set_int_property",
            "tempo_set_bool_property", "tempo_set_string_property",
            "tempo_set_vector_property", "tempo_set_rotator_property",
            "tempo_set_color_property", "tempo_set_asset_property",
            "tempo_set_actor_transform", "tempo_call_function",
        ],
        "description": "Tempo native actor operations",
    },
    "tempo_editor": {
        "tools": [
            "tempo_play_in_editor", "tempo_simulate", "tempo_stop",
            "tempo_save_level", "tempo_open_level", "tempo_new_level",
        ],
        "description": "Editor PIE and level management",
    },
    "tempo_geographic": {
        "tools": [
            "tempo_set_date", "tempo_set_time_of_day",
            "tempo_set_day_cycle_rate", "tempo_get_datetime",
            "tempo_set_geographic_reference",
        ],
        "description": "Date, time, and geographic settings",
    },
    "tempo_movement": {
        "tools": [
            "tempo_get_commandable_vehicles", "tempo_command_vehicle",
            "tempo_get_commandable_pawns", "tempo_pawn_move_to",
            "tempo_rebuild_navigation",
        ],
        "description": "Vehicle and pawn movement",
    },
    "tempo_state": {
        "tools": ["tempo_get_actor_state", "tempo_get_actors_near"],
        "description": "Actor state queries",
    },
    "tempo_levels": {
        "tools": [
            "tempo_load_level", "tempo_finish_loading_level",
            "tempo_get_current_level", "tempo_quit",
            "tempo_set_viewport_render", "tempo_set_control_mode",
        ],
        "description": "Level loading and control",
    },
    "tempo_misc": {
        "tools": [
            "tempo_get_label_map", "tempo_get_available_sensors",
            "tempo_get_lanes", "tempo_get_lane_accessibility", "tempo_get_zones",
            "tempo_run_zone_graph_builder",
        ],
        "description": "Labels, sensors, and map queries",
    },

    # Optional bp_toolkit module (only if submodule present)
    "bp_toolkit": {
        "tools": [
            "bp_export_asset", "bp_import_asset", "bp_detect_type", "bp_get_info",
            "bp_list_properties", "bp_get_property", "bp_set_property",
            "bp_clone_asset", "bp_list_graphs", "bp_add_comment",
            "bp_clone_node", "bp_find", "bp_query", "bp_parse",
        ],
        "description": "Offline asset manipulation via UAssetGUI",
    },
}


# =============================================================================
# PROFILE DEFINITIONS
# =============================================================================

PROFILES = {
    # Absolute minimum - 11 tools
    "core": ["core"],

    # Level editing - 25 tools (DEFAULT)
    "standard": ["core", "transforms", "assets", "console", "functions", "files"],

    # Full editor work - 38 tools
    "editor": ["core", "transforms", "assets", "console", "functions", "files",
               "components", "world_partition"],

    # Blueprint/PCG editing - 37 tools
    "scripting": ["core", "transforms", "assets", "console", "functions", "files",
                  "blueprints", "pcg"],

    # Runtime/PIE testing - 48 tools
    "simulation": ["core", "simulation", "tempo_actors", "tempo_editor",
                   "tempo_state", "tempo_levels"],

    # Everything - all modules
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
