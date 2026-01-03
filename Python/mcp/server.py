"""
AgentBridge MCP Server

Implements the Model Context Protocol (MCP) server that exposes Unreal Engine
operations to Claude and other LLM agents.

Supports modular loading via profiles or explicit module lists:
    --profile standard   # Use predefined profile (core, standard, editor, simulation, full)
    --modules core,assets,simulation  # Load specific modules

Each service is auto-discovered and its tools are exposed to MCP clients.

Usage:
    python -m mcp.server [--host HOST] [--port PORT] [--profile PROFILE] [--modules MODULES]

The server communicates via stdio using JSON-RPC.
"""

import os
import sys
import json
import logging
import argparse
from typing import Dict, Any, Optional, List, Set

from .services import (
    get_all_services,
    get_filtered_services,
    get_profile_modules,
    get_enabled_tools,
    get_available_modules,
    get_available_profiles,
    ServiceModule,
    PROFILES,
    MODULES,
    DEFAULT_PROFILE,
)

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[logging.StreamHandler(sys.stderr)],
)
logger = logging.getLogger(__name__)


class MCPServer:
    """
    MCP Server for AgentBridge and Tempo services.

    Handles JSON-RPC messages over stdio and dispatches to the appropriate
    service module based on tool name.

    Supports modular loading via profiles or explicit module lists.
    """

    def __init__(
        self,
        host: str = "localhost",
        port: int = 50051,
        profile: Optional[str] = None,
        modules: Optional[List[str]] = None,
    ):
        """
        Initialize the MCP server.

        Args:
            host: gRPC server host
            port: gRPC server port
            profile: Profile name to load (core, standard, editor, simulation, full)
            modules: Explicit list of module names to load (overrides profile)
        """
        self.host = host
        self.port = port

        # Determine which modules to enable
        if modules:
            # Explicit module list provided
            self.enabled_modules = list(modules)
            # Always include core
            if "core" not in self.enabled_modules:
                self.enabled_modules.insert(0, "core")
        else:
            # Use profile (or default)
            profile_name = profile or os.environ.get("AGENTBRIDGE_PROFILE", DEFAULT_PROFILE)
            self.enabled_modules = get_profile_modules(profile_name)

        self.profile_name = profile or DEFAULT_PROFILE

        # Load filtered services based on enabled modules
        self.services: Dict[str, ServiceModule] = get_filtered_services(self.enabled_modules)

        # Create connections for each service (lazy)
        self.clients: Dict[str, Any] = {}

        # Build tool -> service mapping for routing
        self.tool_to_service: Dict[str, str] = {}
        for service_name, service in self.services.items():
            for tool in service.tools:
                self.tool_to_service[tool["name"]] = service_name

        # Add the meta-tool for dynamic loading
        self._load_modules_tool = {
            "name": "load_modules",
            "description": f"Load additional tool modules. Available: {', '.join(sorted(MODULES.keys()))}",
            "inputSchema": {
                "type": "object",
                "properties": {
                    "modules": {
                        "type": "array",
                        "items": {"type": "string"},
                    }
                },
                "required": ["modules"],
            },
        }

        logger.info(f"Profile: {self.profile_name} ({len(self.enabled_modules)} modules, {len(self.tool_to_service)} tools)")
        logger.info(f"Enabled modules: {', '.join(self.enabled_modules)}")
        for name, service in self.services.items():
            if service.tools:
                logger.info(f"  - {name}: {len(service.tools)} tools")

    def _get_client(self, service_name: str) -> Optional[Any]:
        """Get or create a client for a service."""
        if service_name not in self.clients:
            if service_name in self.services:
                try:
                    service = self.services[service_name]
                    self.clients[service_name] = service.connect(self.host, self.port)
                    logger.info(f"Connected to {service_name} at {self.host}:{self.port}")
                except Exception as e:
                    logger.error(f"Failed to connect to {service_name}: {e}")
                    return None
        return self.clients.get(service_name)

    def _get_all_tools(self) -> List[Dict[str, Any]]:
        """Get all tools from all enabled services, plus meta-tools."""
        all_tools = []
        for service in self.services.values():
            all_tools.extend(service.tools)
        # Add load_modules meta-tool
        all_tools.append(self._load_modules_tool)
        return all_tools

    def _handle_load_modules(self, requested_modules: List[str]) -> Dict[str, Any]:
        """
        Dynamically load additional modules.

        Args:
            requested_modules: List of module names to load

        Returns:
            Result dict with loaded modules and new tools
        """
        already_enabled = set(self.enabled_modules)
        newly_loaded = []
        new_tools = []

        for module_name in requested_modules:
            if module_name in already_enabled:
                continue
            if module_name not in MODULES:
                continue

            # Add to enabled modules
            self.enabled_modules.append(module_name)
            newly_loaded.append(module_name)
            new_tools.extend(MODULES[module_name]["tools"])

        if newly_loaded:
            # Rebuild services with new modules
            self.services = get_filtered_services(self.enabled_modules)

            # Rebuild tool mapping
            self.tool_to_service.clear()
            for service_name, service in self.services.items():
                for tool in service.tools:
                    self.tool_to_service[tool["name"]] = service_name

            logger.info(f"Loaded {len(newly_loaded)} modules: {', '.join(newly_loaded)}")
            logger.info(f"Total tools now: {len(self.tool_to_service)}")

        return {
            "loaded_modules": newly_loaded,
            "new_tools": new_tools,
            "total_modules": len(self.enabled_modules),
            "total_tools": len(self.tool_to_service) + 1,  # +1 for load_modules itself
        }

    def handle_message(self, message: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """
        Handle an incoming JSON-RPC message.

        Args:
            message: Parsed JSON-RPC message

        Returns:
            Response message or None for notifications
        """
        method = message.get("method", "")
        msg_id = message.get("id")
        params = message.get("params", {})

        logger.debug(f"Received: {method}")

        # Handle MCP protocol messages
        if method == "initialize":
            return self._handle_initialize(msg_id, params)
        elif method == "initialized":
            # Notification, no response
            return None
        elif method == "tools/list":
            return self._handle_tools_list(msg_id)
        elif method == "tools/call":
            return self._handle_tools_call(msg_id, params)
        elif method == "ping":
            return self._make_response(msg_id, {})
        elif method == "shutdown":
            return self._make_response(msg_id, {})
        else:
            return self._make_error(msg_id, -32601, f"Method not found: {method}")

    def _handle_initialize(self, msg_id: Any, params: Dict[str, Any]) -> Dict[str, Any]:
        """Handle the initialize request."""
        # Pre-connect to all services
        for service_name in self.services:
            self._get_client(service_name)

        return self._make_response(msg_id, {
            "protocolVersion": "2024-11-05",
            "capabilities": {
                "tools": {},
            },
            "serverInfo": {
                "name": "agentbridge-mcp",
                "version": "0.3.0",  # Bumped for modular loading
            },
        })

    def _handle_tools_list(self, msg_id: Any) -> Dict[str, Any]:
        """Handle the tools/list request."""
        return self._make_response(msg_id, {
            "tools": self._get_all_tools(),
        })

    def _handle_tools_call(self, msg_id: Any, params: Dict[str, Any]) -> Dict[str, Any]:
        """Handle the tools/call request."""
        tool_name = params.get("name", "")
        arguments = params.get("arguments", {})

        logger.info(f"Tool call: {tool_name}")

        # Handle meta-tool: load_modules
        if tool_name == "load_modules":
            modules_to_load = arguments.get("modules", [])
            result = self._handle_load_modules(modules_to_load)
            return self._make_response(msg_id, {
                "content": [
                    {
                        "type": "text",
                        "text": json.dumps(result, indent=2),
                    }
                ],
            })

        # Find which service handles this tool
        service_name = self.tool_to_service.get(tool_name)
        if not service_name:
            # Check if it's a valid tool that's not loaded
            all_tools = set()
            for module_info in MODULES.values():
                all_tools.update(module_info["tools"])

            if tool_name in all_tools:
                # Tool exists but module not loaded
                # Find which module it's in
                module_for_tool = None
                for mod_name, mod_info in MODULES.items():
                    if tool_name in mod_info["tools"]:
                        module_for_tool = mod_name
                        break

                return self._make_response(msg_id, {
                    "content": [
                        {
                            "type": "text",
                            "text": json.dumps({
                                "error": f"Tool '{tool_name}' is not loaded. "
                                         f"Load module '{module_for_tool}' first: load_modules(modules=[\"{module_for_tool}\"])"
                            }),
                        }
                    ],
                    "isError": True,
                })

            return self._make_response(msg_id, {
                "content": [
                    {
                        "type": "text",
                        "text": json.dumps({"error": f"Unknown tool: {tool_name}"}),
                    }
                ],
                "isError": True,
            })

        # Get the client for this service
        client = self._get_client(service_name)
        if client is None:
            return self._make_response(msg_id, {
                "content": [
                    {
                        "type": "text",
                        "text": json.dumps({
                            "error": f"Not connected to {service_name} at {self.host}:{self.port}. "
                                     "Make sure Unreal Editor is running with the appropriate plugin."
                        }),
                    }
                ],
                "isError": True,
            })

        # Execute the tool using the service's execute function
        try:
            service = self.services[service_name]
            result = service.execute(client, tool_name, arguments)
            return self._make_response(msg_id, {
                "content": [
                    {
                        "type": "text",
                        "text": result,
                    }
                ],
            })
        except Exception as e:
            logger.error(f"Tool error: {e}")
            return self._make_response(msg_id, {
                "content": [
                    {
                        "type": "text",
                        "text": json.dumps({"error": str(e)}),
                    }
                ],
                "isError": True,
            })

    def _make_response(self, msg_id: Any, result: Any) -> Dict[str, Any]:
        """Create a JSON-RPC response."""
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": result,
        }

    def _make_error(self, msg_id: Any, code: int, message: str) -> Dict[str, Any]:
        """Create a JSON-RPC error response."""
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "error": {
                "code": code,
                "message": message,
            },
        }

    def run(self):
        """
        Run the MCP server, reading from stdin and writing to stdout.
        """
        logger.info("AgentBridge MCP Server starting...")
        logger.info(f"Will connect to services at {self.host}:{self.port}")

        while True:
            try:
                # Read a line from stdin
                line = sys.stdin.readline()
                if not line:
                    logger.info("EOF received, shutting down")
                    break

                line = line.strip()
                if not line:
                    continue

                # Parse JSON-RPC message
                try:
                    message = json.loads(line)
                except json.JSONDecodeError as e:
                    logger.error(f"Invalid JSON: {e}")
                    continue

                # Handle the message
                response = self.handle_message(message)

                # Send response if any
                if response is not None:
                    response_str = json.dumps(response)
                    sys.stdout.write(response_str + "\n")
                    sys.stdout.flush()

            except KeyboardInterrupt:
                logger.info("Interrupted, shutting down")
                break
            except Exception as e:
                logger.error(f"Error: {e}")

        logger.info("Server stopped")


def serve(
    host: str = "localhost",
    port: int = 50051,
    profile: Optional[str] = None,
    modules: Optional[List[str]] = None,
):
    """
    Start the MCP server.

    Args:
        host: gRPC server host
        port: gRPC server port
        profile: Profile name (core, standard, editor, simulation, full)
        modules: Explicit list of module names to load
    """
    server = MCPServer(host=host, port=port, profile=profile, modules=modules)
    server.run()


def main():
    """Command-line entry point."""
    parser = argparse.ArgumentParser(
        description="AgentBridge MCP Server - Expose Unreal Engine and Tempo to LLM agents"
    )
    parser.add_argument(
        "--host",
        default="localhost",
        help="gRPC server host (default: localhost)",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=50051,
        help="gRPC server port (default: 50051)",
    )
    parser.add_argument(
        "--profile",
        choices=list(PROFILES.keys()),
        default=None,
        help=f"Load predefined module set. Choices: {', '.join(PROFILES.keys())} (default: {DEFAULT_PROFILE})",
    )
    parser.add_argument(
        "--modules",
        type=str,
        default=None,
        help=f"Comma-separated list of modules to load. Available: {', '.join(sorted(MODULES.keys()))}",
    )
    parser.add_argument(
        "--list-profiles",
        action="store_true",
        help="List available profiles and exit",
    )
    parser.add_argument(
        "--list-modules",
        action="store_true",
        help="List available modules and exit",
    )
    parser.add_argument(
        "--debug",
        action="store_true",
        help="Enable debug logging",
    )

    args = parser.parse_args()

    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)

    # Handle info commands
    if args.list_profiles:
        print("Available profiles:")
        for name, modules in PROFILES.items():
            tool_count = len(get_enabled_tools(modules))
            print(f"  {name}: {tool_count} tools ({', '.join(modules)})")
        return

    if args.list_modules:
        print("Available modules:")
        for name, info in sorted(MODULES.items()):
            print(f"  {name}: {info['description']} ({len(info['tools'])} tools)")
        return

    # Parse modules if provided
    modules = None
    if args.modules:
        modules = [m.strip() for m in args.modules.split(",")]

    serve(host=args.host, port=args.port, profile=args.profile, modules=modules)


if __name__ == "__main__":
    main()
