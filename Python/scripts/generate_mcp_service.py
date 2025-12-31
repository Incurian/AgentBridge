#!/usr/bin/env python3
"""
Proto to MCP Service Module Generator

Parses .proto files and generates MCP service module stubs.

Usage:
    python generate_mcp_service.py <proto_file> [--output <output_dir>]

Example:
    python generate_mcp_service.py /path/to/MyService.proto --output ../mcp/services/
"""

import re
import sys
import argparse
from pathlib import Path
from typing import List, Dict, Any, Optional, Tuple


def parse_proto_file(proto_path: str) -> Dict[str, Any]:
    """Parse a .proto file and extract service definitions."""
    with open(proto_path, 'r') as f:
        content = f.read()

    result = {
        'package': None,
        'imports': [],
        'messages': [],
        'enums': [],
        'services': [],
    }

    # Extract package
    pkg_match = re.search(r'package\s+(\w+);', content)
    if pkg_match:
        result['package'] = pkg_match.group(1)

    # Extract imports
    for match in re.finditer(r'import\s+"([^"]+)"', content):
        result['imports'].append(match.group(1))

    # Extract enums
    for match in re.finditer(r'enum\s+(\w+)\s*\{([^}]+)\}', content):
        enum_name = match.group(1)
        enum_body = match.group(2)
        values = []
        for val_match in re.finditer(r'(\w+)\s*=\s*(\d+)', enum_body):
            values.append({'name': val_match.group(1), 'value': int(val_match.group(2))})
        result['enums'].append({'name': enum_name, 'values': values})

    # Extract messages (simplified - just names)
    for match in re.finditer(r'message\s+(\w+)\s*\{', content):
        result['messages'].append(match.group(1))

    # Extract services and RPCs
    service_pattern = r'service\s+(\w+)\s*\{([^}]+)\}'
    for service_match in re.finditer(service_pattern, content):
        service_name = service_match.group(1)
        service_body = service_match.group(2)

        rpcs = []
        rpc_pattern = r'rpc\s+(\w+)\s*\(\s*(stream\s+)?(\S+)\s*\)\s*returns\s*\(\s*(stream\s+)?(\S+)\s*\)'
        for rpc_match in re.finditer(rpc_pattern, service_body):
            rpcs.append({
                'name': rpc_match.group(1),
                'input_stream': bool(rpc_match.group(2)),
                'input_type': rpc_match.group(3),
                'output_stream': bool(rpc_match.group(4)),
                'output_type': rpc_match.group(5),
            })

        result['services'].append({
            'name': service_name,
            'rpcs': rpcs,
        })

    return result


def camel_to_snake(name: str) -> str:
    """Convert CamelCase to snake_case."""
    s1 = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
    return re.sub('([a-z0-9])([A-Z])', r'\1_\2', s1).lower()


def generate_tool_name(service_prefix: str, rpc_name: str) -> str:
    """Generate MCP tool name from service prefix and RPC name."""
    return f"{service_prefix}_{camel_to_snake(rpc_name)}"


def infer_module_path(import_path: str) -> Tuple[str, str]:
    """Infer Python module path from proto import path."""
    # Convert "TempoCore/TempoCore.proto" -> ("TempoCore", "TempoCore_pb2")
    parts = import_path.replace('.proto', '').split('/')
    if len(parts) >= 2:
        return parts[-2], f"{parts[-1]}_pb2"
    return parts[0], f"{parts[0]}_pb2"


def generate_mcp_service(proto_data: Dict[str, Any], service_prefix: str) -> str:
    """Generate MCP service module Python code."""
    if not proto_data['services']:
        raise ValueError("No services found in proto file")

    service = proto_data['services'][0]  # Use first service
    service_name = service['name']
    snake_service = camel_to_snake(service_name)

    # Determine module imports based on package or infer from service name
    pkg = proto_data.get('package')
    if pkg:
        module_name = pkg.replace('.', '_')
    else:
        # Infer from service name (e.g., TempoCoreService -> TempoCore)
        module_name = service_name.replace('Service', '')
        pkg = module_name

    # Build imports section
    imports = [
        '"""',
        f'Tempo {service_name} MCP Tools',
        '',
        'Auto-generated from proto definition.',
        '"""',
        '',
        'import json',
        'from typing import Dict, Any',
        'from . import register_service, ServiceModule',
        'from .base import create_channel, safe_call',
        '',
        f'from {pkg} import {module_name}_pb2 as pb',
        f'from {pkg} import {module_name}_pb2_grpc as pb_grpc',
        'from TempoScripting import Empty_pb2',
        '',
    ]

    # Build tools list
    tools = ['TOOLS = [']
    for rpc in service['rpcs']:
        if rpc['output_stream']:
            # Skip streaming RPCs for now (add note)
            tools.append(f'    # Skipped: {rpc["name"]} (streaming RPC)')
            continue

        tool_name = generate_tool_name(service_prefix, rpc['name'])
        tools.append('    {')
        tools.append(f'        "name": "{tool_name}",')
        tools.append(f'        "description": "TODO: Add description for {rpc["name"]}",')
        tools.append('        "inputSchema": {')
        tools.append('            "type": "object",')
        tools.append('            "properties": {')
        tools.append('                # TODO: Add parameters based on request message')
        tools.append('            },')
        tools.append('            "required": [],')
        tools.append('        },')
        tools.append('    },')
    tools.append(']')

    # Build client class
    client_class = [
        '',
        '',
        f'class {service_name}Client:',
        f'    """Client for Tempo\'s {service_name}."""',
        '',
        '    def __init__(self, host: str = "localhost", port: int = 50051):',
        '        self.channel = create_channel(host, port)',
        f'        self.stub = pb_grpc.{service_name}Stub(self.channel)',
        '',
    ]

    # Add method stubs for each RPC
    for rpc in service['rpcs']:
        if rpc['output_stream']:
            continue
        method_name = camel_to_snake(rpc['name'])
        if rpc['input_type'] in ['TempoScripting.Empty', 'Empty']:
            client_class.append(f'    def {method_name}(self):')
            client_class.append(f'        return self.stub.{rpc["name"]}(Empty_pb2.Empty())')
        else:
            client_class.append(f'    def {method_name}(self, **kwargs):')
            client_class.append(f'        # TODO: Build request from kwargs')
            client_class.append(f'        return self.stub.{rpc["name"]}(pb.{rpc["input_type"].split(".")[-1]}(**kwargs))')
        client_class.append('')

    # Build connect and execute functions
    functions = [
        '',
        f'def connect(host: str, port: int) -> {service_name}Client:',
        f'    return {service_name}Client(host, port)',
        '',
        '',
        f'def execute(client: {service_name}Client, tool_name: str, args: Dict[str, Any]) -> str:',
        '    result = _execute_impl(client, tool_name, args)',
        '    return json.dumps(result, indent=2)',
        '',
        '',
        f'def _execute_impl(client: {service_name}Client, tool_name: str, args: Dict[str, Any]) -> Any:',
    ]

    # Add tool dispatch
    first = True
    for rpc in service['rpcs']:
        if rpc['output_stream']:
            continue
        tool_name = generate_tool_name(service_prefix, rpc['name'])
        method_name = camel_to_snake(rpc['name'])
        if first:
            functions.append(f'    if tool_name == "{tool_name}":')
            first = False
        else:
            functions.append(f'    elif tool_name == "{tool_name}":')
        functions.append(f'        result = safe_call(client.{method_name})')
        functions.append('        if isinstance(result, dict) and "error" in result:')
        functions.append('            return result')
        functions.append(f'        return {{"success": True, "action": "{method_name}"}}')
        functions.append('')

    functions.append('    else:')
    functions.append('        return {"error": f"Unknown tool: {tool_name}"}')

    # Build registration
    registration = [
        '',
        '',
        'register_service(ServiceModule(',
        f'    name="{service_prefix}_{snake_service}",',
        f'    description="Tempo {service_name} - TODO: add description",',
        '    tools=TOOLS,',
        '    execute=execute,',
        '    connect=connect,',
        '))',
    ]

    return '\n'.join(imports + tools + client_class + functions + registration)


def main():
    parser = argparse.ArgumentParser(description='Generate MCP service module from proto file')
    parser.add_argument('proto_file', help='Path to .proto file')
    parser.add_argument('--output', '-o', default='.', help='Output directory')
    parser.add_argument('--prefix', '-p', default='tempo', help='Tool name prefix (e.g., "tempo")')
    parser.add_argument('--dry-run', '-n', action='store_true', help='Print to stdout instead of writing file')

    args = parser.parse_args()

    proto_path = Path(args.proto_file)
    if not proto_path.exists():
        print(f"Error: File not found: {proto_path}", file=sys.stderr)
        sys.exit(1)

    print(f"Parsing: {proto_path}")
    proto_data = parse_proto_file(str(proto_path))

    if not proto_data['services']:
        print("Error: No services found in proto file", file=sys.stderr)
        sys.exit(1)

    service_name = proto_data['services'][0]['name']
    output_name = f"{args.prefix}_{camel_to_snake(service_name)}.py"

    print(f"Found service: {service_name}")
    print(f"RPCs: {len(proto_data['services'][0]['rpcs'])}")

    code = generate_mcp_service(proto_data, args.prefix)

    if args.dry_run:
        print("\n" + "=" * 60)
        print(code)
    else:
        output_path = Path(args.output) / output_name
        with open(output_path, 'w') as f:
            f.write(code)
        print(f"Generated: {output_path}")
        print("\nNOTE: The generated code requires manual editing:")
        print("  - Add proper parameter schemas to TOOLS")
        print("  - Implement request building in client methods")
        print("  - Add result parsing in execute handlers")


if __name__ == '__main__':
    main()
