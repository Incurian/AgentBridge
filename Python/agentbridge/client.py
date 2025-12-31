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
