"""
AgentBridge MCP Server

Implements the Model Context Protocol (MCP) server that exposes Unreal Engine
operations to Claude and other LLM agents.

Supports multiple service modules (agentbridge, tempo_time, tempo_actor_control, etc.)
Each service is auto-discovered and its tools are exposed to MCP clients.

Usage:
    python -m mcp.server [--host HOST] [--port PORT]

The server communicates via stdio using JSON-RPC.
"""

import sys
import json
import logging
import argparse
from typing import Dict, Any, Optional, List

from .services import get_all_services, ServiceModule

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
    """

    def __init__(self, host: str = "localhost", port: int = 50051):
        """
        Initialize the MCP server.

        Args:
            host: gRPC server host
            port: gRPC server port
        """
        self.host = host
        self.port = port

        # Load all service modules
        self.services: Dict[str, ServiceModule] = get_all_services()

        # Create connections for each service
        self.clients: Dict[str, Any] = {}

        # Build tool -> service mapping for routing
        self.tool_to_service: Dict[str, str] = {}
        for service_name, service in self.services.items():
            for tool in service.tools:
                self.tool_to_service[tool["name"]] = service_name

        logger.info(f"Loaded {len(self.services)} services with {len(self.tool_to_service)} tools")
        for name, service in self.services.items():
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
        """Get all tools from all services."""
        all_tools = []
        for service in self.services.values():
            all_tools.extend(service.tools)
        return all_tools

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
                "version": "0.2.0",
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

        # Find which service handles this tool
        service_name = self.tool_to_service.get(tool_name)
        if not service_name:
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


def serve(host: str = "localhost", port: int = 50051):
    """
    Start the MCP server.

    Args:
        host: gRPC server host
        port: gRPC server port
    """
    server = MCPServer(host=host, port=port)
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
        "--debug",
        action="store_true",
        help="Enable debug logging",
    )

    args = parser.parse_args()

    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)

    serve(host=args.host, port=args.port)


if __name__ == "__main__":
    main()
