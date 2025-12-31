"""
AgentBridge HTTP Client.

Provides a clean Python API for communicating with the AgentBridge UE plugin.
"""

import json
import urllib.request
import urllib.error
from typing import Dict, List, Optional, Tuple, Any, Union

from .types import (
    Vector,
    Rotator,
    WorldInfo,
    ActorInfo,
    ActorDetails,
    ClassInfo,
    PropertyValue,
    FunctionResult,
    ContextCapabilities,
    DataAssetInfo,
    DataAssetDetails,
    DataTableRowInfo,
    CaptureResult,
    SceneCaptureResult,
    AudioAnalysisResult,
    AudioCaptureResult,
    MaterialInfo,
    MaterialDetails,
    MaterialInstanceResult,
    PCGActorInfo,
    PCGRegenerateResult,
    CVarInfo,
    AgentBridgeError,
)


class AgentBridgeClient:
    """
    HTTP client for the AgentBridge Unreal Engine plugin.

    Usage:
        client = AgentBridgeClient()  # Default: localhost:8080
        client = AgentBridgeClient(host="192.168.1.100", port=8080)

        # Check connection
        if client.health_check():
            worlds = client.list_worlds()

    Thread Safety:
        This client is thread-safe for concurrent requests.
    """

    def __init__(self, host: str = "localhost", port: int = 8080, timeout: float = 30.0):
        """
        Initialize the client.

        Args:
            host: Server hostname (default: localhost)
            port: Server port (default: 8080)
            timeout: Request timeout in seconds (default: 30)
        """
        self.base_url = f"http://{host}:{port}/agentbridge"
        self.timeout = timeout

    def _execute(self, command: Dict) -> Dict:
        """
        Execute a command and return the response.

        Args:
            command: Command dictionary with 'type' and parameters

        Returns:
            Response dictionary

        Raises:
            AgentBridgeError: If the command fails
        """
        url = f"{self.base_url}/execute"
        data = json.dumps(command).encode("utf-8")

        req = urllib.request.Request(
            url,
            data=data,
            headers={"Content-Type": "application/json"},
            method="POST",
        )

        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as response:
                result = json.loads(response.read().decode("utf-8"))
        except urllib.error.URLError as e:
            raise AgentBridgeError(f"Connection failed: {e}")
        except json.JSONDecodeError as e:
            raise AgentBridgeError(f"Invalid JSON response: {e}")

        if not result.get("success", False):
            raise AgentBridgeError(
                result.get("error", "Unknown error"),
                command_id=result.get("commandId", ""),
            )

        return result

    # =========================================================================
    # Health & Info
    # =========================================================================

    def health_check(self) -> bool:
        """
        Check if the server is running.

        Returns:
            True if server is healthy
        """
        url = f"{self.base_url}/health"
        try:
            with urllib.request.urlopen(url, timeout=5.0) as response:
                result = json.loads(response.read().decode("utf-8"))
                return result.get("status") == "ok"
        except Exception:
            return False

    def get_schema(self) -> Dict:
        """
        Get the API schema.

        Returns:
            Schema dictionary
        """
        url = f"{self.base_url}/schema"
        with urllib.request.urlopen(url, timeout=self.timeout) as response:
            return json.loads(response.read().decode("utf-8"))

    # =========================================================================
    # World Operations
    # =========================================================================

    def list_worlds(self) -> List[WorldInfo]:
        """
        List all available world contexts.

        Returns:
            List of WorldInfo objects
        """
        result = self._execute({"type": "ListWorlds"})
        return [WorldInfo.from_dict(w) for w in result.get("worlds", [])]

    def set_target_world(self, world_identifier: str) -> None:
        """
        Set the target world for subsequent operations.

        Args:
            world_identifier: World index (as string), name, "editor", or "pie"
        """
        self._execute({
            "type": "SetTargetWorld",
            "worldIdentifier": world_identifier,
        })

    def get_capabilities(self) -> ContextCapabilities:
        """
        Get the capabilities available in the current world context.

        This is useful for understanding what operations are available
        based on whether you're in Editor, PIE, or a packaged game.

        Returns:
            ContextCapabilities object describing available features

        Example:
            caps = client.get_capabilities()
            if caps.is_pie():
                print(f"Running in PIE instance {caps.pie_instance}")
            if not caps.editor.can_use_transactions:
                print(f"Undo not available: {caps.unavailable_reasons.get('transaction', 'unknown')}")
        """
        result = self._execute({"type": "GetCapabilities"})
        return ContextCapabilities.from_dict(result)

    # =========================================================================
    # Actor Discovery
    # =========================================================================

    def query_actors(
        self,
        class_name: str = "",
        name_pattern: str = "",
        tag: str = "",
        limit: int = 100,
        include_hidden: bool = False,
    ) -> List[ActorInfo]:
        """
        Query actors matching criteria.

        Args:
            class_name: Filter by class (empty = all)
            name_pattern: Wildcard pattern for name/label
            tag: Filter by actor tag
            limit: Maximum results
            include_hidden: Include hidden actors

        Returns:
            List of ActorInfo objects
        """
        result = self._execute({
            "type": "QueryActors",
            "className": class_name,
            "namePattern": name_pattern,
            "tag": tag,
            "limit": limit,
            "includeHidden": include_hidden,
        })
        return [ActorInfo.from_dict(a) for a in result.get("actors", [])]

    def get_actor(
        self,
        actor_id: str,
        include_properties: bool = True,
        include_components: bool = True,
        property_depth: int = 2,
    ) -> ActorDetails:
        """
        Get detailed information about an actor.

        Args:
            actor_id: Actor name, label, path, or GUID
            include_properties: Include property values
            include_components: Include component list
            property_depth: Max recursion for nested properties

        Returns:
            ActorDetails object
        """
        result = self._execute({
            "type": "GetActor",
            "actorId": actor_id,
            "includeProperties": include_properties,
            "includeComponents": include_components,
            "propertyDepth": property_depth,
        })
        return ActorDetails.from_dict(result.get("actor", {}))

    # =========================================================================
    # Actor Manipulation
    # =========================================================================

    def spawn_actor(
        self,
        class_name: str,
        location: Union[Tuple[float, float, float], Vector] = (0, 0, 0),
        rotation: Union[Tuple[float, float, float], Rotator] = (0, 0, 0),
        scale: Union[Tuple[float, float, float], Vector] = (1, 1, 1),
        label: str = "",
        folder_path: str = "",
        properties: Optional[Dict[str, Any]] = None,
    ) -> ActorInfo:
        """
        Spawn a new actor.

        Args:
            class_name: Class to spawn (e.g., "PointLight", "StaticMeshActor")
            location: Spawn location (x, y, z)
            rotation: Spawn rotation (pitch, yaw, roll) in degrees
            scale: Spawn scale (x, y, z)
            label: Editor display name
            folder_path: World Outliner folder
            properties: Initial property values

        Returns:
            ActorInfo of spawned actor
        """
        # Convert tuples to dicts
        if isinstance(location, tuple):
            location = {"x": location[0], "y": location[1], "z": location[2]}
        elif isinstance(location, Vector):
            location = location.to_dict()

        if isinstance(rotation, tuple):
            rotation = {"pitch": rotation[0], "yaw": rotation[1], "roll": rotation[2]}
        elif isinstance(rotation, Rotator):
            rotation = rotation.to_dict()

        if isinstance(scale, tuple):
            scale = {"x": scale[0], "y": scale[1], "z": scale[2]}
        elif isinstance(scale, Vector):
            scale = scale.to_dict()

        result = self._execute({
            "type": "SpawnActor",
            "className": class_name,
            "location": location,
            "rotation": rotation,
            "scale": scale,
            "label": label,
            "folderPath": folder_path,
            "properties": properties or {},
        })
        return ActorInfo.from_dict(result.get("actor", {}))

    def delete_actor(self, actor_id: str) -> None:
        """
        Delete an actor.

        Args:
            actor_id: Actor name, label, path, or GUID
        """
        self._execute({
            "type": "DeleteActor",
            "actorId": actor_id,
        })

    def set_actor_transform(
        self,
        actor_id: str,
        location: Optional[Union[Tuple[float, float, float], Vector]] = None,
        rotation: Optional[Union[Tuple[float, float, float], Rotator]] = None,
        scale: Optional[Union[Tuple[float, float, float], Vector]] = None,
        sweep: bool = False,
    ) -> None:
        """
        Set an actor's transform.

        Args:
            actor_id: Actor name, label, path, or GUID
            location: New location (optional)
            rotation: New rotation (optional)
            scale: New scale (optional)
            sweep: Check for collision
        """
        cmd: Dict[str, Any] = {
            "type": "SetActorTransform",
            "actorId": actor_id,
            "sweep": sweep,
        }

        if location is not None:
            if isinstance(location, tuple):
                cmd["location"] = {"x": location[0], "y": location[1], "z": location[2]}
            else:
                cmd["location"] = location.to_dict()

        if rotation is not None:
            if isinstance(rotation, tuple):
                cmd["rotation"] = {"pitch": rotation[0], "yaw": rotation[1], "roll": rotation[2]}
            else:
                cmd["rotation"] = rotation.to_dict()

        if scale is not None:
            if isinstance(scale, tuple):
                cmd["scale"] = {"x": scale[0], "y": scale[1], "z": scale[2]}
            else:
                cmd["scale"] = scale.to_dict()

        self._execute(cmd)

    # =========================================================================
    # Property Path Operations
    # =========================================================================

    def get_property(self, actor_id: str, path: str) -> PropertyValue:
        """
        Get a property value at a path.

        Args:
            actor_id: Actor name, label, path, or GUID
            path: Property path (e.g., "RootComponent.RelativeLocation.X")

        Returns:
            PropertyValue with value and type
        """
        result = self._execute({
            "type": "GetPropertyPath",
            "actorId": actor_id,
            "path": path,
        })
        return PropertyValue.from_dict(result)

    def set_property(self, actor_id: str, path: str, value: Any) -> None:
        """
        Set a property value at a path.

        Args:
            actor_id: Actor name, label, path, or GUID
            path: Property path
            value: Value to set (will be JSON encoded)
        """
        self._execute({
            "type": "SetPropertyPath",
            "actorId": actor_id,
            "path": path,
            "value": json.dumps(value) if not isinstance(value, str) else value,
        })

    # =========================================================================
    # Function Calls
    # =========================================================================

    def call_function(
        self,
        function_name: str,
        actor_id: str = "",
        class_name: str = "",
        parameters: Optional[Dict[str, Any]] = None,
    ) -> FunctionResult:
        """
        Call a function on an actor or class.

        Args:
            function_name: Name of the function
            actor_id: Target actor (for instance methods)
            class_name: Target class (for static functions)
            parameters: Function parameters

        Returns:
            FunctionResult with return value and out parameters
        """
        result = self._execute({
            "type": "CallFunction",
            "actorId": actor_id,
            "className": class_name,
            "functionName": function_name,
            "parameters": parameters or {},
        })
        return FunctionResult.from_dict(result)

    # =========================================================================
    # Type Discovery
    # =========================================================================

    def list_classes(
        self,
        base_class_name: str = "",
        name_pattern: str = "",
        include_blueprint: bool = True,
        include_abstract: bool = False,
        limit: int = 100,
    ) -> List[ClassInfo]:
        """
        List classes matching criteria.

        Args:
            base_class_name: Filter by base class (empty = AActor)
            name_pattern: Wildcard pattern for name
            include_blueprint: Include Blueprint classes
            include_abstract: Include abstract classes
            limit: Maximum results

        Returns:
            List of ClassInfo objects
        """
        result = self._execute({
            "type": "ListClasses",
            "baseClassName": base_class_name,
            "namePattern": name_pattern,
            "includeBlueprint": include_blueprint,
            "includeAbstract": include_abstract,
            "limit": limit,
        })
        return [ClassInfo.from_dict(c) for c in result.get("classes", [])]

    # =========================================================================
    # Convenience Methods
    # =========================================================================

    def find_actor(self, name_or_label: str) -> Optional[ActorInfo]:
        """
        Find a single actor by name or label.

        Args:
            name_or_label: Actor name or label to find

        Returns:
            ActorInfo if found, None otherwise
        """
        actors = self.query_actors(name_pattern=name_or_label, limit=1)
        return actors[0] if actors else None

    def get_actor_location(self, actor_id: str) -> Vector:
        """
        Get an actor's location.

        Args:
            actor_id: Actor identifier

        Returns:
            Vector with location
        """
        # Use QueryActors instead of CallFunction due to return value handling issue
        actors = self.query_actors(name_pattern=actor_id, limit=1)
        if actors:
            return actors[0].location
        return Vector()

    def set_actor_location(self, actor_id: str, x: float, y: float, z: float) -> None:
        """
        Set an actor's location.

        Args:
            actor_id: Actor identifier
            x, y, z: New location coordinates
        """
        self.set_actor_transform(actor_id, location=(x, y, z))

    # =========================================================================
    # DataAsset Operations
    # =========================================================================

    def list_data_assets(
        self,
        base_class_name: str = "",
        path_filter: str = "",
        limit: int = 100,
    ) -> List[DataAssetInfo]:
        """
        List DataAssets in the project.

        Args:
            base_class_name: Filter by class (e.g., "DataTable", specific DataAsset type)
            path_filter: Wildcard filter for asset paths (e.g., "/Game/Data/*")
            limit: Maximum results

        Returns:
            List of DataAssetInfo objects

        Example:
            # List all DataTables
            tables = client.list_data_assets(base_class_name="DataTable")

            # List DataAssets in a specific folder
            assets = client.list_data_assets(path_filter="/Game/Items/*")
        """
        result = self._execute({
            "type": "ListDataAssets",
            "baseClassName": base_class_name,
            "pathFilter": path_filter,
            "limit": limit,
        })
        return [DataAssetInfo.from_dict(a) for a in result.get("assets", [])]

    def get_data_asset(
        self,
        asset_path: str,
        property_depth: int = 3,
    ) -> DataAssetDetails:
        """
        Get detailed information about a DataAsset.

        Args:
            asset_path: Full asset path (e.g., "/Game/Data/MyData.MyData")
            property_depth: Max recursion depth for nested properties

        Returns:
            DataAssetDetails with all properties

        Example:
            asset = client.get_data_asset("/Game/Data/ItemDatabase.ItemDatabase")
            for name, value in asset.properties.items():
                print(f"{name}: {value}")
        """
        result = self._execute({
            "type": "GetDataAsset",
            "assetPath": asset_path,
            "propertyDepth": property_depth,
        })
        return DataAssetDetails.from_dict(result.get("asset", {}))

    def get_data_table_rows(
        self,
        table_path: str,
        row_name: str = "",
        limit: int = 100,
    ) -> Tuple[str, int, List[DataTableRowInfo]]:
        """
        Get rows from a DataTable.

        Args:
            table_path: Full path to the DataTable asset
            row_name: Specific row to get (empty = all rows up to limit)
            limit: Maximum rows to return when row_name is empty

        Returns:
            Tuple of (row_struct_name, total_row_count, list of DataTableRowInfo)

        Example:
            # Get all rows
            struct_name, total, rows = client.get_data_table_rows("/Game/Data/Items.Items")
            print(f"Table uses {struct_name} with {total} rows")
            for row in rows:
                print(f"{row.row_name}: {row.data}")

            # Get specific row
            _, _, rows = client.get_data_table_rows("/Game/Data/Items.Items", row_name="Sword")
            if rows:
                print(rows[0].data)
        """
        result = self._execute({
            "type": "GetDataTableRow",
            "tablePath": table_path,
            "rowName": row_name,
            "limit": limit,
        })
        rows = [DataTableRowInfo.from_dict(r) for r in result.get("rows", [])]
        return (
            result.get("rowStructName", ""),
            result.get("totalRowCount", 0),
            rows,
        )

    def get_data_table_row(
        self,
        table_path: str,
        row_name: str,
    ) -> Optional[DataTableRowInfo]:
        """
        Get a single row from a DataTable by name.

        Args:
            table_path: Full path to the DataTable asset
            row_name: Name of the row to retrieve

        Returns:
            DataTableRowInfo if found, None otherwise

        Example:
            row = client.get_data_table_row("/Game/Data/Items.Items", "Sword")
            if row:
                print(f"Found: {row.data}")
        """
        _, _, rows = self.get_data_table_rows(table_path, row_name=row_name)
        return rows[0] if rows else None

    # =========================================================================
    # Capture Operations
    # =========================================================================

    def capture_viewport(
        self,
        output_path: str = "",
        width: int = 0,
        height: int = 0,
        show_ui: bool = False,
        format: str = "PNG",
    ) -> CaptureResult:
        """
        Capture the current viewport.

        Only available in Editor or PIE contexts where a viewport exists.
        For captures without a viewport, use capture_scene().

        Args:
            output_path: File path to save (empty = return base64 in result)
            width: Width override (0 = current viewport width)
            height: Height override (0 = current viewport height)
            show_ui: Include UI elements in capture
            format: Image format (PNG, JPG, EXR)

        Returns:
            CaptureResult with image data or file path

        Example:
            # Save to file
            result = client.capture_viewport(output_path="/tmp/screenshot.png")

            # Get as base64
            result = client.capture_viewport()
            if result.image_data:
                import base64
                png_bytes = base64.b64decode(result.image_data)
        """
        result = self._execute({
            "type": "CaptureViewport",
            "outputPath": output_path,
            "width": width,
            "height": height,
            "showUI": show_ui,
            "format": format,
        })
        return CaptureResult.from_dict(result)

    def capture_scene(
        self,
        location: Union[Tuple[float, float, float], Vector] = (0, 0, 0),
        rotation: Union[Tuple[float, float, float], Rotator] = (0, 0, 0),
        width: int = 1280,
        height: int = 720,
        fov: float = 90.0,
        actor_id: str = "",
        component_name: str = "",
        output_path: str = "",
        format: str = "PNG",
    ) -> SceneCaptureResult:
        """
        Capture the scene from a specified camera position.

        Works in all contexts (Editor, PIE, packaged). Either specify
        a camera position or use an existing SceneCaptureComponent2D.

        Args:
            location: Camera location (if not using existing actor)
            rotation: Camera rotation (pitch, yaw, roll) (if not using existing actor)
            width: Capture width in pixels
            height: Capture height in pixels
            fov: Field of view in degrees
            actor_id: Actor with SceneCaptureComponent2D (optional)
            component_name: Specific component name if actor has multiple
            output_path: File path to save (empty = return base64)
            format: Image format (PNG, JPG)

        Returns:
            SceneCaptureResult with image data and camera info

        Example:
            # Capture from a specific position
            result = client.capture_scene(
                location=(1000, 500, 300),
                rotation=(0, 45, 0),
                width=1920,
                height=1080
            )

            # Use existing SceneCapture actor
            result = client.capture_scene(actor_id="MySceneCapture")
        """
        # Convert tuples to dicts
        if isinstance(location, tuple):
            location = {"x": location[0], "y": location[1], "z": location[2]}
        elif isinstance(location, Vector):
            location = location.to_dict()

        if isinstance(rotation, tuple):
            rotation = {"pitch": rotation[0], "yaw": rotation[1], "roll": rotation[2]}
        elif isinstance(rotation, Rotator):
            rotation = rotation.to_dict()

        result = self._execute({
            "type": "CaptureScene",
            "location": location,
            "rotation": rotation,
            "width": width,
            "height": height,
            "fov": fov,
            "actorId": actor_id,
            "componentName": component_name,
            "outputPath": output_path,
            "format": format,
        })
        return SceneCaptureResult.from_dict(result)

    # =========================================================================
    # Audio Operations
    # =========================================================================

    def get_audio_analysis(
        self,
        source: str = "WorldAudio",
        actor_id: str = "",
        frequency_bands: int = 8,
    ) -> AudioAnalysisResult:
        """
        Get real-time audio analysis data.

        Note: This feature requires proper audio setup in the UE project.
        Currently provides basic analysis of world audio.

        Args:
            source: Audio source ("WorldAudio", "PlayerMic", "Actor")
            actor_id: Actor ID if source is "Actor"
            frequency_bands: Number of frequency bands to analyze

        Returns:
            AudioAnalysisResult with volume and frequency data

        Example:
            # Get audio levels
            analysis = client.get_audio_analysis()
            print(f"Volume: {analysis.rms_volume:.2f}")
            print(f"Bands: {analysis.frequency_bands}")
        """
        result = self._execute({
            "type": "GetAudioAnalysis",
            "source": source,
            "actorId": actor_id,
            "frequencyBands": frequency_bands,
        })
        return AudioAnalysisResult.from_dict(result)

    def start_audio_capture(
        self,
        source: str = "WorldAudio",
        actor_id: str = "",
        max_duration: float = 30.0,
        sample_rate: int = 44100,
        channels: int = 2,
    ) -> AudioCaptureResult:
        """
        Start recording audio.

        Note: Full audio capture requires AudioCapture module setup.
        This is a placeholder that indicates the feature structure.

        Args:
            source: Audio source ("WorldAudio", "PlayerMic", "Actor")
            actor_id: Actor ID if source is "Actor"
            max_duration: Maximum capture duration in seconds
            sample_rate: Sample rate (44100, 48000, etc.)
            channels: Number of audio channels (1=mono, 2=stereo)

        Returns:
            AudioCaptureResult with capture ID

        Example:
            # Start capture
            result = client.start_audio_capture(max_duration=10.0)
            capture_id = result.capture_id
            # ... wait ...
            # Stop and get audio
            audio = client.stop_audio_capture(capture_id)
        """
        result = self._execute({
            "type": "StartAudioCapture",
            "source": source,
            "actorId": actor_id,
            "maxDuration": max_duration,
            "sampleRate": sample_rate,
            "channels": channels,
        })
        return AudioCaptureResult.from_dict(result)

    def stop_audio_capture(
        self,
        capture_id: str,
        output_path: str = "",
    ) -> AudioCaptureResult:
        """
        Stop recording and retrieve audio data.

        Note: Full audio capture requires AudioCapture module setup.
        This is a placeholder that indicates the feature structure.

        Args:
            capture_id: Capture ID returned by start_audio_capture()
            output_path: File path to save (empty = return base64 WAV)

        Returns:
            AudioCaptureResult with audio data or file path

        Example:
            # Stop capture and get as base64
            audio = client.stop_audio_capture(capture_id)
            if audio.audio_data:
                import base64
                wav_bytes = base64.b64decode(audio.audio_data)
                with open("recording.wav", "wb") as f:
                    f.write(wav_bytes)

            # Stop capture and save to file
            audio = client.stop_audio_capture(capture_id, output_path="/tmp/audio.wav")
        """
        result = self._execute({
            "type": "StopAudioCapture",
            "captureId": capture_id,
            "outputPath": output_path,
        })
        return AudioCaptureResult.from_dict(result)

    # =========================================================================
    # Material Operations
    # =========================================================================

    def list_materials(
        self,
        path_filter: str = "",
        instances_only: bool = False,
        limit: int = 100,
    ) -> List[MaterialInfo]:
        """
        List materials in the project.

        Args:
            path_filter: Wildcard filter for asset paths (e.g., "/Game/Materials/*")
            instances_only: Only return material instances
            limit: Maximum results

        Returns:
            List of MaterialInfo objects

        Example:
            # List all materials
            materials = client.list_materials()

            # List materials in a folder
            materials = client.list_materials(path_filter="/Game/Materials/*")

            # List only material instances
            instances = client.list_materials(instances_only=True)
        """
        result = self._execute({
            "type": "ListMaterials",
            "pathFilter": path_filter,
            "instancesOnly": instances_only,
            "limit": limit,
        })
        return [MaterialInfo.from_dict(m) for m in result.get("materials", [])]

    def get_material_info(
        self,
        material_path: str,
        include_parameters: bool = True,
    ) -> MaterialDetails:
        """
        Get detailed information about a material.

        Args:
            material_path: Full asset path to the material
            include_parameters: Include parameter values

        Returns:
            MaterialDetails with material info and parameters

        Example:
            mat = client.get_material_info("/Game/Materials/M_Basic.M_Basic")
            print(f"Material: {mat.name}")
            for param in mat.parameters:
                print(f"  {param.name} ({param.type}): {param.value}")
        """
        result = self._execute({
            "type": "GetMaterialInfo",
            "materialPath": material_path,
            "includeParameters": include_parameters,
        })
        return MaterialDetails.from_dict(result)

    def create_material_instance(
        self,
        parent_material_path: str,
        instance_name: str = "",
        owner_actor_id: str = "",
        scalar_parameters: Optional[Dict[str, float]] = None,
        vector_parameters: Optional[Dict[str, Dict[str, float]]] = None,
    ) -> MaterialInstanceResult:
        """
        Create a dynamic material instance.

        Note: Creates UMaterialInstanceDynamic which persists only in current session.

        Args:
            parent_material_path: Path to parent material
            instance_name: Name for the instance (for later lookup)
            owner_actor_id: Actor to own the instance (for lifecycle)
            scalar_parameters: Initial scalar parameter values
            vector_parameters: Initial vector parameters as {name: {r, g, b, a}}

        Returns:
            MaterialInstanceResult with instance name

        Example:
            # Create red material instance
            result = client.create_material_instance(
                "/Game/Materials/M_Basic.M_Basic",
                instance_name="RedMaterial",
                owner_actor_id="MyCube",
                vector_parameters={"BaseColor": {"r": 1, "g": 0, "b": 0, "a": 1}}
            )
        """
        result = self._execute({
            "type": "CreateMaterialInstance",
            "parentMaterialPath": parent_material_path,
            "instanceName": instance_name,
            "ownerActorId": owner_actor_id,
            "scalarParameters": scalar_parameters or {},
            "vectorParameters": vector_parameters or {},
        })
        return MaterialInstanceResult.from_dict(result)

    def set_material_parameter(
        self,
        target_id: str,
        parameter_name: str,
        value: Any,
        parameter_type: str = "Scalar",
        component_name: str = "",
        slot_index: int = 0,
    ) -> None:
        """
        Set a parameter on an actor's material.

        Automatically creates a dynamic material instance if needed.

        Args:
            target_id: Actor identifier
            parameter_name: Name of the parameter
            value: Value to set (float for Scalar, dict for Vector, path for Texture)
            parameter_type: "Scalar", "Vector", or "Texture"
            component_name: Specific component (optional)
            slot_index: Material slot index

        Example:
            # Set roughness
            client.set_material_parameter("MyCube", "Roughness", 0.5)

            # Set color
            client.set_material_parameter(
                "MyCube", "BaseColor",
                {"r": 1, "g": 0, "b": 0, "a": 1},
                parameter_type="Vector"
            )

            # Set texture
            client.set_material_parameter(
                "MyCube", "DiffuseTexture",
                "/Game/Textures/T_Wood.T_Wood",
                parameter_type="Texture"
            )
        """
        # Convert value to string for Vector type
        if parameter_type == "Vector" and isinstance(value, dict):
            value = json.dumps(value)
        elif parameter_type == "Scalar":
            value = str(value)

        self._execute({
            "type": "SetMaterialParameter",
            "targetId": target_id,
            "parameterName": parameter_name,
            "value": value,
            "parameterType": parameter_type,
            "componentName": component_name,
            "slotIndex": slot_index,
        })

    def apply_material_to_actor(
        self,
        actor_id: str,
        material_path: str,
        component_name: str = "",
        slot_index: int = -1,
    ) -> None:
        """
        Apply a material to an actor's mesh.

        Args:
            actor_id: Target actor
            material_path: Material asset path
            component_name: Specific component (uses first mesh if empty)
            slot_index: Material slot (-1 for all slots)

        Example:
            # Apply to all slots
            client.apply_material_to_actor("MyCube", "/Game/Materials/M_Red.M_Red")

            # Apply to specific slot
            client.apply_material_to_actor(
                "MyMesh",
                "/Game/Materials/M_Metal.M_Metal",
                slot_index=0
            )
        """
        self._execute({
            "type": "ApplyMaterialToActor",
            "actorId": actor_id,
            "materialPath": material_path,
            "componentName": component_name,
            "slotIndex": slot_index,
        })

    # =========================================================================
    # PCG Operations
    # =========================================================================

    def list_pcg_actors(
        self,
        name_pattern: str = "",
        include_graph_info: bool = True,
        limit: int = 100,
    ) -> List[PCGActorInfo]:
        """
        List PCG actors in the world.

        Args:
            name_pattern: Wildcard filter for actor names
            include_graph_info: Include PCG graph information
            limit: Maximum results

        Returns:
            List of PCGActorInfo objects

        Example:
            pcg_actors = client.list_pcg_actors()
            for actor in pcg_actors:
                print(f"{actor.label}: {actor.status}")
        """
        result = self._execute({
            "type": "ListPCGActors",
            "namePattern": name_pattern,
            "includeGraphInfo": include_graph_info,
            "limit": limit,
        })
        return [PCGActorInfo.from_dict(a) for a in result.get("actors", [])]

    def regenerate_pcg(
        self,
        actor_id: str,
        component_name: str = "",
        force_refresh: bool = False,
    ) -> PCGRegenerateResult:
        """
        Trigger PCG regeneration.

        Note: Requires PCG plugin. Currently returns a helpful error message.

        Args:
            actor_id: PCG actor identifier
            component_name: Specific component if multiple
            force_refresh: Force full regeneration

        Returns:
            PCGRegenerateResult with generation stats

        Example:
            result = client.regenerate_pcg("MyPCGActor")
            print(f"Generated {result.generated_count} instances")
        """
        result = self._execute({
            "type": "RegeneratePCG",
            "actorId": actor_id,
            "componentName": component_name,
            "forceRefresh": force_refresh,
        })
        return PCGRegenerateResult.from_dict(result)

    def set_pcg_parameter(
        self,
        actor_id: str,
        parameter_name: str,
        value: Any,
        auto_regenerate: bool = True,
    ) -> None:
        """
        Set a PCG graph parameter.

        Note: Requires PCG plugin. Currently returns a helpful error message.

        Args:
            actor_id: PCG actor identifier
            parameter_name: Parameter name
            value: Value (will be JSON encoded)
            auto_regenerate: Regenerate after setting

        Example:
            client.set_pcg_parameter("MyPCGActor", "Density", 0.5)
        """
        self._execute({
            "type": "SetPCGParameter",
            "actorId": actor_id,
            "parameterName": parameter_name,
            "value": json.dumps(value) if not isinstance(value, str) else value,
            "autoRegenerate": auto_regenerate,
        })

    # =========================================================================
    # Console Variable (CVar) Operations
    # =========================================================================

    def get_cvar(self, name: str) -> CVarInfo:
        """
        Get a console variable value.

        Args:
            name: CVar name (e.g., "r.ScreenPercentage")

        Returns:
            CVarInfo with name, value, type, and help text

        Example:
            cvar = client.get_cvar("r.ScreenPercentage")
            print(f"{cvar.name} = {cvar.value} ({cvar.type})")
        """
        result = self._execute({
            "type": "GetCVar",
            "name": name,
        })
        return CVarInfo.from_dict(result)

    def set_cvar(self, name: str, value: Any) -> CVarInfo:
        """
        Set a console variable value.

        Args:
            name: CVar name
            value: New value (will be converted to string)

        Returns:
            CVarInfo with the new value

        Raises:
            AgentBridgeError: If CVar is read-only or not found

        Example:
            # Set an integer CVar
            client.set_cvar("r.ScreenPercentage", 100)

            # Set a float CVar
            client.set_cvar("r.Streaming.PoolSize", 2048)

            # Set a string CVar
            client.set_cvar("r.DefaultFeature.Bloom", "0")
        """
        result = self._execute({
            "type": "SetCVar",
            "name": name,
            "value": str(value),
        })
        return CVarInfo.from_dict(result)

    def list_cvars(
        self,
        pattern: str = "",
        limit: int = 100,
    ) -> List[CVarInfo]:
        """
        List console variables.

        Args:
            pattern: Substring filter for CVar names
            limit: Maximum results

        Returns:
            List of CVarInfo objects

        Example:
            # List all rendering CVars
            cvars = client.list_cvars(pattern="r.")
            for cvar in cvars:
                flags = []
                if cvar.is_read_only:
                    flags.append("ReadOnly")
                if cvar.is_cheat:
                    flags.append("Cheat")
                print(f"{cvar.name} = {cvar.value} {flags}")

            # List shadow-related CVars
            shadows = client.list_cvars(pattern="Shadow")
        """
        result = self._execute({
            "type": "ListCVars",
            "pattern": pattern,
            "limit": limit,
        })
        return [CVarInfo.from_dict(c) for c in result.get("cvars", [])]
