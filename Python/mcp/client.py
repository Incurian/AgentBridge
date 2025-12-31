"""
gRPC Client for AgentBridge

Wraps the generated gRPC stubs with a Pythonic interface.
"""

import grpc
from typing import Optional, List, Tuple, Dict, Any
from dataclasses import dataclass

# Import generated protobuf types
# These are installed by Tempo's build process
try:
    from AgentBridgeServer import AgentBridge_pb2 as pb
    from AgentBridgeServer import AgentBridge_pb2_grpc as pb_grpc
    from TempoScripting import Empty_pb2
except ImportError:
    # Fallback for development - try relative import from Tempo's API
    import sys
    sys.path.insert(0, "D:/tempo/TempoSample/Plugins/Tempo/TempoCore/Content/Python/API/tempo")
    from AgentBridgeServer import AgentBridge_pb2 as pb
    from AgentBridgeServer import AgentBridge_pb2_grpc as pb_grpc
    from TempoScripting import Empty_pb2


@dataclass
class WorldInfo:
    """Information about an Unreal world context."""
    world_type: str
    world_name: str
    pie_instance: int
    has_begun_play: bool
    actor_count: int


@dataclass
class ActorInfo:
    """Information about an actor in the world."""
    guid: str
    path: str
    name: str
    label: str
    class_name: str
    location: Tuple[float, float, float]
    rotation: Tuple[float, float, float]  # pitch, yaw, roll
    scale: Tuple[float, float, float]
    is_hidden: bool
    parent_actor_id: str


class AgentBridgeGrpcClient:
    """
    gRPC client for the AgentBridge service.

    Connects to Unreal Engine's AgentBridge gRPC server (via TempoScripting).

    Usage:
        client = AgentBridgeGrpcClient()
        worlds = client.list_worlds()
        actors = client.query_actors(name_pattern="Light")
    """

    def __init__(self, host: str = "localhost", port: int = 50051):
        """
        Initialize the gRPC client.

        Args:
            host: gRPC server host (default: localhost)
            port: gRPC server port (default: 50051, Tempo's default)
        """
        self.address = f"{host}:{port}"
        self._channel: Optional[grpc.Channel] = None
        self._stub: Optional[pb_grpc.AgentBridgeServiceStub] = None

    def connect(self) -> bool:
        """
        Connect to the gRPC server.

        Returns:
            True if connected successfully
        """
        try:
            self._channel = grpc.insecure_channel(self.address)
            self._stub = pb_grpc.AgentBridgeServiceStub(self._channel)
            # Test connection with a simple call
            self._stub.ListWorlds(pb.ListWorldsRequest(), timeout=2.0)
            return True
        except grpc.RpcError:
            return False

    def disconnect(self):
        """Disconnect from the gRPC server."""
        if self._channel:
            self._channel.close()
            self._channel = None
            self._stub = None

    def _ensure_connected(self):
        """Ensure we're connected, attempt reconnect if not."""
        if self._stub is None:
            if not self.connect():
                raise ConnectionError(f"Cannot connect to AgentBridge at {self.address}")

    # =========================================================================
    # World Operations
    # =========================================================================

    def list_worlds(self) -> List[WorldInfo]:
        """
        List all available world contexts.

        Returns:
            List of WorldInfo objects
        """
        self._ensure_connected()
        response = self._stub.ListWorlds(pb.ListWorldsRequest())

        return [
            WorldInfo(
                world_type=w.world_type,
                world_name=w.world_name,
                pie_instance=w.pie_instance,
                has_begun_play=w.has_begun_play,
                actor_count=w.actor_count,
            )
            for w in response.worlds
        ]

    def set_target_world(self, world_identifier: str) -> None:
        """
        Set the target world for subsequent operations.

        Args:
            world_identifier: World index, name, or "editor"/"pie"
        """
        self._ensure_connected()
        request = pb.SetTargetWorldRequest(world_identifier=world_identifier)
        self._stub.SetTargetWorld(request)

    # =========================================================================
    # Actor Discovery
    # =========================================================================

    def query_actors(
        self,
        class_name: str = "",
        name_pattern: str = "",
        tag: str = "",
        limit: int = 100,
        include_hidden: bool = False
    ) -> List[ActorInfo]:
        """
        Query actors in the current world.

        Args:
            class_name: Filter by class (e.g., "PointLight", "StaticMeshActor")
            name_pattern: Wildcard pattern for name/label
            tag: Filter by actor tag
            limit: Maximum results to return
            include_hidden: Include hidden actors

        Returns:
            List of ActorInfo objects
        """
        self._ensure_connected()
        request = pb.QueryActorsRequest(
            class_name=class_name,
            name_pattern=name_pattern,
            tag=tag,
            limit=limit,
            include_hidden=include_hidden,
        )
        response = self._stub.QueryActors(request)

        return [self._parse_actor_descriptor(a) for a in response.actors]

    def get_actor(
        self,
        actor_id: str,
        include_properties: bool = False,
        include_components: bool = False,
        property_depth: int = 1
    ) -> Optional[ActorInfo]:
        """
        Get detailed information about a specific actor.

        Args:
            actor_id: Actor name, label, path, or GUID
            include_properties: Include property values
            include_components: Include component list
            property_depth: Max recursion for nested properties

        Returns:
            ActorInfo or None if not found
        """
        self._ensure_connected()
        request = pb.GetActorRequest(
            actor_id=actor_id,
            include_properties=include_properties,
            include_components=include_components,
            property_depth=property_depth,
        )
        try:
            response = self._stub.GetActor(request)
            if response.HasField("actor"):
                return self._parse_actor_descriptor(response.actor.actor_info)
        except grpc.RpcError as e:
            if e.code() == grpc.StatusCode.NOT_FOUND:
                return None
            raise
        return None

    # =========================================================================
    # Actor Manipulation
    # =========================================================================

    def spawn_actor(
        self,
        class_name: str,
        location: Tuple[float, float, float] = (0, 0, 0),
        rotation: Tuple[float, float, float] = (0, 0, 0),
        scale: Tuple[float, float, float] = (1, 1, 1),
        label: str = "",
        folder_path: str = "",
    ) -> Optional[ActorInfo]:
        """
        Spawn a new actor in the world.

        Args:
            class_name: Class to spawn (e.g., "PointLight", "StaticMeshActor")
            location: World location (X, Y, Z)
            rotation: Rotation (Pitch, Yaw, Roll) in degrees
            scale: Scale (X, Y, Z)
            label: Editor display name
            folder_path: World Outliner folder path

        Returns:
            ActorInfo for the spawned actor, or None on failure
        """
        self._ensure_connected()

        transform = pb.ActorTransform(
            location=self._make_vector(*location),
            rotation=self._make_rotation(*rotation),
            scale=pb.Scale(x=scale[0], y=scale[1], z=scale[2]),
        )

        request = pb.SpawnActorRequest(
            class_name=class_name,
            transform=transform,
            label=label,
            folder_path=folder_path,
        )

        try:
            response = self._stub.SpawnActor(request)
            if response.HasField("spawned_actor"):
                return self._parse_actor_descriptor(response.spawned_actor)
        except grpc.RpcError:
            pass
        return None

    def delete_actor(self, actor_id: str) -> bool:
        """
        Delete an actor from the world.

        Args:
            actor_id: Actor name, label, path, or GUID

        Returns:
            True if deleted successfully
        """
        self._ensure_connected()
        request = pb.DeleteActorRequest(actor_id=actor_id)
        try:
            self._stub.DeleteActor(request)
            return True
        except grpc.RpcError:
            return False

    def set_actor_transform(
        self,
        actor_id: str,
        location: Optional[Tuple[float, float, float]] = None,
        rotation: Optional[Tuple[float, float, float]] = None,
        scale: Optional[Tuple[float, float, float]] = None,
        sweep: bool = False,
    ) -> bool:
        """
        Set an actor's transform.

        Args:
            actor_id: Actor name, label, path, or GUID
            location: New location (X, Y, Z) or None to keep current
            rotation: New rotation (Pitch, Yaw, Roll) or None to keep current
            scale: New scale (X, Y, Z) or None to keep current
            sweep: Check for collision during move

        Returns:
            True if successful
        """
        self._ensure_connected()

        transform = pb.ActorTransform()
        if location:
            transform.location.CopyFrom(self._make_vector(*location))
        if rotation:
            transform.rotation.CopyFrom(self._make_rotation(*rotation))
        if scale:
            transform.scale.CopyFrom(pb.Scale(x=scale[0], y=scale[1], z=scale[2]))

        request = pb.SetActorTransformRequest(
            actor_id=actor_id,
            transform=transform,
            sweep=sweep,
        )

        try:
            self._stub.SetActorTransform(request)
            return True
        except grpc.RpcError:
            return False

    # =========================================================================
    # Property Operations
    # =========================================================================

    def get_property(self, actor_id: str, path: str) -> Optional[str]:
        """
        Get a property value from an actor.

        Args:
            actor_id: Actor name, label, path, or GUID
            path: Property path (e.g., "RootComponent.RelativeLocation.X")

        Returns:
            Property value as string, or None if not found
        """
        self._ensure_connected()
        request = pb.GetPropertyPathRequest(actor_id=actor_id, path=path)
        try:
            response = self._stub.GetPropertyPath(request)
            return response.value.string_value
        except grpc.RpcError:
            return None

    def set_property(self, actor_id: str, path: str, value: str) -> bool:
        """
        Set a property value on an actor.

        Args:
            actor_id: Actor name, label, path, or GUID
            path: Property path
            value: New value as string

        Returns:
            True if successful
        """
        self._ensure_connected()
        prop_value = pb.PropertyValue(string_value=value)
        request = pb.SetPropertyPathRequest(
            actor_id=actor_id,
            path=path,
            value=prop_value,
        )
        try:
            self._stub.SetPropertyPath(request)
            return True
        except grpc.RpcError:
            return False

    # =========================================================================
    # Type Discovery
    # =========================================================================

    def list_classes(
        self,
        base_class_name: str = "Actor",
        name_pattern: str = "",
        include_blueprint: bool = True,
        include_abstract: bool = False,
        limit: int = 100,
    ) -> List[Dict[str, Any]]:
        """
        List available classes.

        Args:
            base_class_name: Filter by base class
            name_pattern: Wildcard pattern
            include_blueprint: Include Blueprint classes
            include_abstract: Include abstract classes
            limit: Maximum results

        Returns:
            List of class info dictionaries
        """
        self._ensure_connected()
        request = pb.ListClassesRequest(
            base_class_name=base_class_name,
            name_pattern=name_pattern,
            include_blueprint=include_blueprint,
            include_abstract=include_abstract,
            limit=limit,
        )
        response = self._stub.ListClasses(request)

        return [
            {
                "class_name": c.class_name,
                "display_name": c.display_name,
                "class_path": c.class_path,
                "parent_class_name": c.parent_class_name,
                "is_blueprint": c.is_blueprint,
                "is_abstract": c.is_abstract,
            }
            for c in response.classes
        ]

    def get_class_schema(
        self,
        class_name: str,
        include_inherited: bool = True,
        include_functions: bool = False,
    ) -> Optional[Dict[str, Any]]:
        """
        Get the schema (properties, functions) for a class.

        Args:
            class_name: Class name or path
            include_inherited: Include inherited members
            include_functions: Include function signatures

        Returns:
            Schema dictionary or None if not found
        """
        self._ensure_connected()
        request = pb.GetClassSchemaRequest(
            class_name=class_name,
            include_inherited=include_inherited,
            include_functions=include_functions,
        )
        try:
            response = self._stub.GetClassSchema(request)
            schema = response.schema
            return {
                "class_name": schema.class_info.class_name,
                "properties": [
                    {
                        "name": p.name,
                        "display_name": p.display_name,
                        "type_name": p.type_name,
                        "is_read_only": p.is_read_only,
                    }
                    for p in schema.properties
                ],
                "functions": [
                    {
                        "name": f.function_name,
                        "description": f.description,
                        "is_static": f.is_static,
                    }
                    for f in schema.functions
                ],
            }
        except grpc.RpcError:
            return None

    # =========================================================================
    # Helper Methods
    # =========================================================================

    def _make_vector(self, x: float, y: float, z: float):
        """Create a TempoScripting Vector."""
        from TempoScripting import Geometry_pb2
        return Geometry_pb2.Vector(x=x, y=y, z=z)

    def _make_rotation(self, pitch: float, yaw: float, roll: float):
        """Create a TempoScripting Rotation (r=roll, p=pitch, y=yaw)."""
        from TempoScripting import Geometry_pb2
        return Geometry_pb2.Rotation(r=roll, p=pitch, y=yaw)

    def _parse_actor_descriptor(self, desc) -> ActorInfo:
        """Parse an ActorDescriptor protobuf into ActorInfo."""
        transform = desc.transform
        return ActorInfo(
            guid=desc.guid,
            path=desc.path,
            name=desc.name,
            label=desc.label,
            class_name=desc.class_name,
            location=(
                transform.location.x,
                transform.location.y,
                transform.location.z,
            ),
            rotation=(
                transform.rotation.p,  # pitch
                transform.rotation.y,  # yaw
                transform.rotation.r,  # roll
            ),
            scale=(
                transform.scale.x,
                transform.scale.y,
                transform.scale.z,
            ),
            is_hidden=desc.is_hidden,
            parent_actor_id=desc.parent_actor_id,
        )
