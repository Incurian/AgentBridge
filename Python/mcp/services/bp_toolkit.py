"""
bp_toolkit MCP Service - Blueprint/PCG/DataAsset Parsing & Modification

This service module conditionally exposes bp_toolkit tools when the optional
bp_toolkit submodule is present. bp_toolkit tools are LOCAL operations that
don't require gRPC/Unreal connectivity.

The submodule is detected by checking for key files in:
    ../../bp_toolkit/bp_builder.py

When not present, this module silently does nothing (no tools registered).
"""

import json
import logging
import sys
from pathlib import Path
from typing import Dict, Any, Optional

from . import register_service, ServiceModule

logger = logging.getLogger(__name__)

# =============================================================================
# SUBMODULE DETECTION
# =============================================================================

def _find_bp_toolkit() -> Optional[Path]:
    """
    Find the bp_toolkit submodule if present.

    Returns the path to bp_toolkit directory, or None if not available.
    Checks for bp_builder.py to ensure submodule is actually initialized.
    """
    # Path relative to this file: Python/mcp/services/ -> bp_toolkit/
    services_dir = Path(__file__).parent
    bp_toolkit_path = services_dir.parent.parent.parent / "bp_toolkit"

    # Check for key file to ensure submodule is initialized
    if (bp_toolkit_path / "scripts" / "bp_builder.py").exists():
        return bp_toolkit_path

    return None


BP_TOOLKIT_PATH = _find_bp_toolkit()

# =============================================================================
# TOOL DEFINITIONS (only if submodule present)
# =============================================================================

if BP_TOOLKIT_PATH:
    # Add bp_toolkit to path for imports
    scripts_path = BP_TOOLKIT_PATH / "scripts"
    if str(scripts_path) not in sys.path:
        sys.path.insert(0, str(scripts_path))

    TOOLS = [
        {"name": "bp_export_asset", "description": "Export a uasset file to JSON using UAssetGUI. Returns the JSON path on success.", "inputSchema": {"type": "object", "properties": {"uasset_path": {"type": "string"}, "ue_version": {"type": "string"}}, "required": ["uasset_path"]}},
        {"name": "bp_import_asset", "description": "Import modified JSON back to uasset format using UAssetGUI.", "inputSchema": {"type": "object", "properties": {"json_path": {"type": "string"}, "ue_version": {"type": "string"}}, "required": ["json_path"]}},
        {"name": "bp_detect_type", "description": "Detect the type of a UAssetAPI JSON export (Blueprint, PCGGraph, DataAsset, BehaviorTree, Material, etc.)", "inputSchema": {"type": "object", "properties": {"json_path": {"type": "string"}}, "required": ["json_path"]}},
        {"name": "bp_get_info", "description": "Get info about an asset JSON (type, exports count, graphs, namemap size)", "inputSchema": {"type": "object", "properties": {"json_path": {"type": "string"}}, "required": ["json_path"]}},
        {"name": "bp_list_properties", "description": "List all properties in an asset export with their types and values", "inputSchema": {"type": "object", "properties": {"json_path": {"type": "string"}, "export_index": {"type": "integer"}}, "required": ["json_path"]}},
        {"name": "bp_get_property", "description": "Get a property value by path (e.g., 'BiomeDefinition.BiomePriority', 'BiomeAssets[0].Generator')", "inputSchema": {"type": "object", "properties": {"json_path": {"type": "string"}, "property_path": {"type": "string"}, "export_index": {"type": "integer"}}, "required": ["json_path", "property_path"]}},
        {"name": "bp_set_property", "description": "Set a property value by path. Saves the modified JSON in-place or to output_path.", "inputSchema": {"type": "object", "properties": {"json_path": {"type": "string"}, "property_path": {"type": "string"}, "value": {}, "export_index": {"type": "integer"}, "output_path": {"type": "string"}}, "required": ["json_path", "property_path", "value"]}},
        {"name": "bp_clone_asset", "description": "Clone an asset JSON as a new asset with a different name. Creates a new JSON file.", "inputSchema": {"type": "object", "properties": {"json_path": {"type": "string"}, "new_name": {"type": "string"}, "new_folder_path": {"type": "string"}, "output_path": {"type": "string"}}, "required": ["json_path", "new_name"]}},
        {"name": "bp_list_graphs", "description": "List all graphs in a Blueprint or PCG asset", "inputSchema": {"type": "object", "properties": {"json_path": {"type": "string"}}, "required": ["json_path"]}},
        {"name": "bp_add_comment", "description": "Add a comment node to a Blueprint graph", "inputSchema": {"type": "object", "properties": {"json_path": {"type": "string"}, "graph_name": {"type": "string"}, "text": {"type": "string"}, "x": {"type": "integer"}, "y": {"type": "integer"}, "width": {"type": "integer"}, "height": {"type": "integer"}, "output_path": {"type": "string"}}, "required": ["json_path", "graph_name", "text"]}},
        {"name": "bp_clone_node", "description": "Clone an existing node in a Blueprint (preserves pins/connections)", "inputSchema": {"type": "object", "properties": {"json_path": {"type": "string"}, "node_name": {"type": "string"}, "offset_x": {"type": "integer"}, "offset_y": {"type": "integer"}, "output_path": {"type": "string"}}, "required": ["json_path", "node_name"]}},
        {"name": "bp_find", "description": "Search for a pattern in asset namemap and exports", "inputSchema": {"type": "object", "properties": {"json_path": {"type": "string"}, "pattern": {"type": "string"}}, "required": ["json_path", "pattern"]}},
        {"name": "bp_query", "description": "Run a type-specific query (list-events, list-tasks, textures, etc.)", "inputSchema": {"type": "object", "properties": {"json_path": {"type": "string"}, "query_type": {"type": "string"}, "pattern": {"type": "string"}}, "required": ["json_path", "query_type"]}},
        {"name": "bp_parse", "description": "Parse a Blueprint JSON into organized documentation with call graphs and Mermaid diagrams", "inputSchema": {"type": "object", "properties": {"json_path": {"type": "string"}, "output_dir": {"type": "string"}}, "required": ["json_path"]}},
    ]

    # =============================================================================
    # TOOL HANDLERS
    # =============================================================================

    def _handle_export_asset(args: Dict[str, Any]) -> Dict[str, Any]:
        """Export uasset to JSON."""
        from bp_export import export_uasset_to_json

        uasset_path = Path(args["uasset_path"])
        ue_version = args.get("ue_version", "VER_UE5_4")

        success, message = export_uasset_to_json(uasset_path, None, ue_version)
        return {
            "success": success,
            "message": message,
            "json_path": str(uasset_path.with_suffix('.json')) if success else None
        }

    def _handle_import_asset(args: Dict[str, Any]) -> Dict[str, Any]:
        """Import JSON back to uasset."""
        from bp_export import import_json_to_uasset

        json_path = Path(args["json_path"])
        ue_version = args.get("ue_version", "VER_UE5_4")

        success, message = import_json_to_uasset(json_path, ue_version=ue_version)
        return {
            "success": success,
            "message": message
        }

    def _handle_detect_type(args: Dict[str, Any]) -> Dict[str, Any]:
        """Detect asset type."""
        from bp_builder import AssetModifier

        asset = AssetModifier(args["json_path"])
        return {
            "success": True,
            "asset_type": asset.asset_type,
            "json_path": args["json_path"]
        }

    def _handle_get_info(args: Dict[str, Any]) -> Dict[str, Any]:
        """Get asset info."""
        from bp_builder import AssetModifier

        asset = AssetModifier(args["json_path"])
        graphs = asset.list_graphs()

        return {
            "success": True,
            "asset_type": asset.asset_type,
            "exports_count": len(asset.data.get("Exports", [])),
            "imports_count": len(asset.data.get("Imports", [])),
            "namemap_count": len(asset.data.get("NameMap", [])),
            "graphs": [{"index": idx, "name": name} for idx, name in graphs]
        }

    def _handle_list_properties(args: Dict[str, Any]) -> Dict[str, Any]:
        """List properties in an export."""
        from bp_builder import AssetModifier

        asset = AssetModifier(args["json_path"])
        export_idx = args.get("export_index", 0)
        props = asset.list_properties(export_idx)

        return {
            "success": True,
            "export_index": export_idx,
            "properties": [{"name": name, "type": ptype, "value": value} for name, ptype, value in props]
        }

    def _handle_get_property(args: Dict[str, Any]) -> Dict[str, Any]:
        """Get property by path."""
        from bp_builder import AssetModifier

        asset = AssetModifier(args["json_path"])
        export_idx = args.get("export_index", 0)
        value = asset.get_property(args["property_path"], export_idx)

        if value is None:
            return {"success": False, "error": f"Property not found: {args['property_path']}"}

        return {
            "success": True,
            "path": args["property_path"],
            "value": value
        }

    def _handle_set_property(args: Dict[str, Any]) -> Dict[str, Any]:
        """Set property by path."""
        from bp_builder import AssetModifier

        asset = AssetModifier(args["json_path"])
        export_idx = args.get("export_index", 0)

        success = asset.set_property(args["property_path"], args["value"], export_idx)
        if not success:
            return {"success": False, "error": f"Failed to set property: {args['property_path']}"}

        output_path = asset.save(args.get("output_path"))
        return {
            "success": True,
            "path": args["property_path"],
            "new_value": args["value"],
            "output_path": str(output_path)
        }

    def _handle_clone_asset(args: Dict[str, Any]) -> Dict[str, Any]:
        """Clone asset with new name."""
        from bp_builder import AssetModifier

        asset = AssetModifier(args["json_path"])
        new_asset = asset.clone_asset(args["new_name"], args.get("new_folder_path"))

        output_path = args.get("output_path")
        if output_path:
            saved_path = new_asset.save(output_path)
        else:
            # Auto-generate path next to original
            source = Path(args["json_path"])
            saved_path = new_asset.save(str(source.parent / f"{args['new_name']}.json"))

        return {
            "success": True,
            "new_name": args["new_name"],
            "output_path": str(saved_path)
        }

    def _handle_list_graphs(args: Dict[str, Any]) -> Dict[str, Any]:
        """List graphs in Blueprint/PCG."""
        from bp_builder import AssetModifier

        asset = AssetModifier(args["json_path"])
        graphs = asset.list_graphs()

        return {
            "success": True,
            "graphs": [{"index": idx, "name": name, "nodes": len(asset.get_graph_nodes(idx))} for idx, name in graphs]
        }

    def _handle_add_comment(args: Dict[str, Any]) -> Dict[str, Any]:
        """Add comment to graph."""
        from bp_builder import AssetModifier

        asset = AssetModifier(args["json_path"])
        new_idx = asset.add_comment(
            graph_name=args["graph_name"],
            text=args["text"],
            x=args.get("x", 0),
            y=args.get("y", 0),
            width=args.get("width", 400),
            height=args.get("height", 200)
        )
        output_path = asset.save(args.get("output_path"))

        return {
            "success": True,
            "new_export_index": new_idx,
            "output_path": str(output_path)
        }

    def _handle_clone_node(args: Dict[str, Any]) -> Dict[str, Any]:
        """Clone node in Blueprint."""
        from bp_builder import AssetModifier

        asset = AssetModifier(args["json_path"])
        new_idx = asset.clone_node(
            source_name=args["node_name"],
            offset_x=args.get("offset_x", 200),
            offset_y=args.get("offset_y", 100)
        )
        output_path = asset.save(args.get("output_path"))

        return {
            "success": True,
            "new_export_index": new_idx,
            "output_path": str(output_path)
        }

    def _handle_find(args: Dict[str, Any]) -> Dict[str, Any]:
        """Search for pattern in asset."""
        from asset_parser import find_in_asset
        import json

        json_path = Path(args["json_path"])
        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        results = find_in_asset(data, args["pattern"])
        return {
            "success": True,
            "pattern": args["pattern"],
            "matches": results
        }

    def _handle_query(args: Dict[str, Any]) -> Dict[str, Any]:
        """Run type-specific query."""
        from asset_parser import query_asset

        result = query_asset(
            args["json_path"],
            args["query_type"],
            args.get("pattern")
        )
        return {
            "success": True,
            "query_type": args["query_type"],
            "result": result
        }

    def _handle_parse(args: Dict[str, Any]) -> Dict[str, Any]:
        """Full Blueprint parsing."""
        from bp_parser import parse_blueprint

        json_path = Path(args["json_path"])
        output_dir = args.get("output_dir")

        if output_dir:
            output_dir = Path(output_dir)
        else:
            output_dir = json_path.parent / f"{json_path.stem}_parsed"

        summary = parse_blueprint(json_path, output_dir)
        return {
            "success": True,
            "output_dir": str(output_dir),
            "summary": summary
        }

    # Handler dispatch
    HANDLERS = {
        "bp_export_asset": _handle_export_asset,
        "bp_import_asset": _handle_import_asset,
        "bp_detect_type": _handle_detect_type,
        "bp_get_info": _handle_get_info,
        "bp_list_properties": _handle_list_properties,
        "bp_get_property": _handle_get_property,
        "bp_set_property": _handle_set_property,
        "bp_clone_asset": _handle_clone_asset,
        "bp_list_graphs": _handle_list_graphs,
        "bp_add_comment": _handle_add_comment,
        "bp_clone_node": _handle_clone_node,
        "bp_find": _handle_find,
        "bp_query": _handle_query,
        "bp_parse": _handle_parse,
    }

    # =============================================================================
    # SERVICE MODULE IMPLEMENTATION
    # =============================================================================

    class BpToolkitClient:
        """
        Dummy client for bp_toolkit - tools are local Python operations.
        No gRPC connection needed.
        """
        def __init__(self, host: str, port: int):
            # Store for interface compatibility, but we don't use gRPC
            self.host = host
            self.port = port
            self.bp_toolkit_path = BP_TOOLKIT_PATH

    def _connect(host: str, port: int) -> BpToolkitClient:
        """Create a 'client' - just a marker that bp_toolkit is available."""
        return BpToolkitClient(host, port)

    def _execute(client: BpToolkitClient, tool_name: str, args: Dict[str, Any]) -> str:
        """Execute a bp_toolkit tool."""
        handler = HANDLERS.get(tool_name)
        if not handler:
            return json.dumps({"error": f"Unknown bp_toolkit tool: {tool_name}"})

        try:
            result = handler(args)
            return json.dumps(result, indent=2, default=str)
        except Exception as e:
            logger.exception(f"bp_toolkit tool error: {tool_name}")
            return json.dumps({"error": str(e)})

    # =============================================================================
    # REGISTRATION
    # =============================================================================

    register_service(ServiceModule(
        name="bp_toolkit",
        description="Blueprint/PCG/DataAsset parsing and modification (local operations)",
        tools=TOOLS,
        execute=_execute,
        connect=_connect,
    ))

    logger.info(f"bp_toolkit service registered with {len(TOOLS)} tools (from {BP_TOOLKIT_PATH})")

else:
    # bp_toolkit not present - silently skip registration
    logger.debug("bp_toolkit submodule not found, skipping registration")
