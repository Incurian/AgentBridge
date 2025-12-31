"""
AgentBridge type definitions.

These dataclasses mirror the response structures from the UE plugin.
"""

from dataclasses import dataclass, field
from typing import Dict, List, Optional, Any, Tuple


@dataclass
class Vector:
    """3D vector (X, Y, Z)."""
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0

    @classmethod
    def from_dict(cls, d: Optional[Dict]) -> "Vector":
        if not d:
            return cls()
        return cls(
            x=d.get("x", 0.0),
            y=d.get("y", 0.0),
            z=d.get("z", 0.0),
        )

    def to_tuple(self) -> Tuple[float, float, float]:
        return (self.x, self.y, self.z)

    def to_dict(self) -> Dict[str, float]:
        return {"x": self.x, "y": self.y, "z": self.z}


@dataclass
class Rotator:
    """3D rotation (Pitch, Yaw, Roll) in degrees."""
    pitch: float = 0.0
    yaw: float = 0.0
    roll: float = 0.0

    @classmethod
    def from_dict(cls, d: Optional[Dict]) -> "Rotator":
        if not d:
            return cls()
        return cls(
            pitch=d.get("pitch", 0.0),
            yaw=d.get("yaw", 0.0),
            roll=d.get("roll", 0.0),
        )

    def to_tuple(self) -> Tuple[float, float, float]:
        return (self.pitch, self.yaw, self.roll)

    def to_dict(self) -> Dict[str, float]:
        return {"pitch": self.pitch, "yaw": self.yaw, "roll": self.roll}


@dataclass
class Transform:
    """Full transform (location, rotation, scale)."""
    location: Vector = field(default_factory=Vector)
    rotation: Rotator = field(default_factory=Rotator)
    scale: Vector = field(default_factory=lambda: Vector(1, 1, 1))

    @classmethod
    def from_dict(cls, d: Optional[Dict]) -> "Transform":
        if not d:
            return cls()
        return cls(
            location=Vector.from_dict(d.get("location")),
            rotation=Rotator.from_dict(d.get("rotation")),
            scale=Vector.from_dict(d.get("scale")) if d.get("scale") else Vector(1, 1, 1),
        )


@dataclass
class WorldInfo:
    """Information about a world context."""
    world_type: str = ""
    world_name: str = ""
    pie_instance: int = -1
    has_begun_play: bool = False
    actor_count: int = 0

    @classmethod
    def from_dict(cls, d: Dict) -> "WorldInfo":
        return cls(
            world_type=d.get("worldType", ""),
            world_name=d.get("worldName", ""),
            pie_instance=d.get("pieInstance", -1),
            has_begun_play=d.get("hasBegunPlay", False),
            actor_count=d.get("actorCount", 0),
        )


@dataclass
class ActorInfo:
    """Summary information about an actor."""
    guid: str = ""
    name: str = ""
    label: str = ""
    class_name: str = ""
    location: Vector = field(default_factory=Vector)
    hidden: bool = False

    @classmethod
    def from_dict(cls, d: Dict) -> "ActorInfo":
        return cls(
            guid=d.get("guid", ""),
            name=d.get("name", ""),
            label=d.get("label", ""),
            class_name=d.get("className", ""),
            location=Vector.from_dict(d.get("location")),
            hidden=d.get("hidden", False),
        )


@dataclass
class ActorDetails:
    """Detailed information about an actor."""
    guid: str = ""
    path: str = ""
    name: str = ""
    label: str = ""
    class_name: str = ""
    location: Vector = field(default_factory=Vector)
    rotation: Rotator = field(default_factory=Rotator)
    scale: Vector = field(default_factory=lambda: Vector(1, 1, 1))
    hidden: bool = False
    parent_actor_id: str = ""
    components: Dict[str, str] = field(default_factory=dict)
    properties: Dict[str, str] = field(default_factory=dict)

    @classmethod
    def from_dict(cls, d: Dict) -> "ActorDetails":
        return cls(
            guid=d.get("guid", ""),
            path=d.get("path", ""),
            name=d.get("name", ""),
            label=d.get("label", ""),
            class_name=d.get("className", ""),
            location=Vector.from_dict(d.get("location")),
            rotation=Rotator.from_dict(d.get("rotation")),
            scale=Vector.from_dict(d.get("scale")) if d.get("scale") else Vector(1, 1, 1),
            hidden=d.get("hidden", False),
            parent_actor_id=d.get("parentActorId", ""),
            components=d.get("components", {}),
            properties=d.get("properties", {}),
        )


@dataclass
class ClassInfo:
    """Information about a UClass."""
    class_name: str = ""
    display_name: str = ""
    class_path: str = ""
    parent_class_name: str = ""
    is_blueprint: bool = False
    is_abstract: bool = False

    @classmethod
    def from_dict(cls, d: Dict) -> "ClassInfo":
        return cls(
            class_name=d.get("className", ""),
            display_name=d.get("displayName", ""),
            class_path=d.get("classPath", ""),
            parent_class_name=d.get("parentClassName", ""),
            is_blueprint=d.get("isBlueprint", False),
            is_abstract=d.get("isAbstract", False),
        )


@dataclass
class PropertyValue:
    """A property value with type information."""
    value: Any = None
    type_name: str = ""

    @classmethod
    def from_dict(cls, d: Dict) -> "PropertyValue":
        return cls(
            value=d.get("value"),
            type_name=d.get("typeName", ""),
        )


@dataclass
class FunctionResult:
    """Result of a function call."""
    return_value: Any = None
    out_parameters: Dict[str, Any] = field(default_factory=dict)

    @classmethod
    def from_dict(cls, d: Dict) -> "FunctionResult":
        return cls(
            return_value=d.get("returnValue"),
            out_parameters=d.get("outParameters", {}),
        )


class AgentBridgeError(Exception):
    """Exception raised when an AgentBridge command fails."""

    def __init__(self, message: str, command_id: str = ""):
        super().__init__(message)
        self.message = message
        self.command_id = command_id
