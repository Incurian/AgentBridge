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


@dataclass
class CoreCapabilities:
    """Core reflection capabilities (always available)."""
    can_iterate_properties: bool = True
    can_invoke_functions: bool = True
    can_spawn_actors: bool = True
    can_destroy_actors: bool = True
    can_modify_transforms: bool = True
    can_modify_properties: bool = True

    @classmethod
    def from_dict(cls, d: Optional[Dict]) -> "CoreCapabilities":
        if not d:
            return cls()
        return cls(
            can_iterate_properties=d.get("canIterateProperties", True),
            can_invoke_functions=d.get("canInvokeFunctions", True),
            can_spawn_actors=d.get("canSpawnActors", True),
            can_destroy_actors=d.get("canDestroyActors", True),
            can_modify_transforms=d.get("canModifyTransforms", True),
            can_modify_properties=d.get("canModifyProperties", True),
        )


@dataclass
class EditorCapabilities:
    """Editor-only capabilities (may be unavailable in PIE/packaged)."""
    can_set_actor_label: bool = False
    can_set_actor_folder: bool = False
    can_use_transactions: bool = False
    has_property_metadata: bool = False
    can_access_editor_world: bool = False

    @classmethod
    def from_dict(cls, d: Optional[Dict]) -> "EditorCapabilities":
        if not d:
            return cls()
        return cls(
            can_set_actor_label=d.get("canSetActorLabel", False),
            can_set_actor_folder=d.get("canSetActorFolder", False),
            can_use_transactions=d.get("canUseTransactions", False),
            has_property_metadata=d.get("hasPropertyMetadata", False),
            can_access_editor_world=d.get("canAccessEditorWorld", False),
        )


@dataclass
class ContextCapabilities:
    """
    Full capabilities of the current world context.

    Use this to understand what operations are available and why
    certain features may be unavailable in different contexts
    (Editor vs PIE vs packaged game).
    """
    # Context identification
    world_type: str = ""  # "Editor", "PIE", "Game", "EditorPreview", "None"
    world_name: str = ""
    is_gameplay_active: bool = False
    pie_instance: int = -1

    # Capabilities
    core: CoreCapabilities = field(default_factory=CoreCapabilities)
    editor: EditorCapabilities = field(default_factory=EditorCapabilities)

    # Explanations for unavailable features
    unavailable_reasons: Dict[str, str] = field(default_factory=dict)

    @classmethod
    def from_dict(cls, d: Dict) -> "ContextCapabilities":
        return cls(
            world_type=d.get("worldType", ""),
            world_name=d.get("worldName", ""),
            is_gameplay_active=d.get("isGameplayActive", False),
            pie_instance=d.get("pieInstance", -1),
            core=CoreCapabilities.from_dict(d.get("coreCapabilities")),
            editor=EditorCapabilities.from_dict(d.get("editorCapabilities")),
            unavailable_reasons=d.get("unavailableReasons", {}),
        )

    def is_editor(self) -> bool:
        """Check if running in Editor context (not playing)."""
        return self.world_type == "Editor"

    def is_pie(self) -> bool:
        """Check if running in Play-In-Editor context."""
        return self.world_type == "PIE"

    def is_game(self) -> bool:
        """Check if running in standalone game context."""
        return self.world_type == "Game"


@dataclass
class DataAssetInfo:
    """Information about a DataAsset."""
    asset_path: str = ""
    asset_name: str = ""
    class_name: str = ""
    is_data_table: bool = False
    is_primary_data_asset: bool = False
    row_count: int = 0

    @classmethod
    def from_dict(cls, d: Dict) -> "DataAssetInfo":
        return cls(
            asset_path=d.get("assetPath", ""),
            asset_name=d.get("assetName", ""),
            class_name=d.get("className", ""),
            is_data_table=d.get("isDataTable", False),
            is_primary_data_asset=d.get("isPrimaryDataAsset", False),
            row_count=d.get("rowCount", 0),
        )


@dataclass
class DataAssetDetails:
    """Detailed information about a DataAsset including properties."""
    asset_path: str = ""
    asset_name: str = ""
    class_name: str = ""
    is_data_table: bool = False
    is_primary_data_asset: bool = False
    row_count: int = 0
    properties: Dict[str, str] = field(default_factory=dict)

    @classmethod
    def from_dict(cls, d: Dict) -> "DataAssetDetails":
        return cls(
            asset_path=d.get("assetPath", ""),
            asset_name=d.get("assetName", ""),
            class_name=d.get("className", ""),
            is_data_table=d.get("isDataTable", False),
            is_primary_data_asset=d.get("isPrimaryDataAsset", False),
            row_count=d.get("rowCount", 0),
            properties=d.get("properties", {}),
        )


@dataclass
class DataTableRowInfo:
    """Information about a DataTable row."""
    row_name: str = ""
    data: Dict[str, str] = field(default_factory=dict)

    @classmethod
    def from_dict(cls, d: Dict) -> "DataTableRowInfo":
        return cls(
            row_name=d.get("rowName", ""),
            data=d.get("data", {}),
        )


@dataclass
class CaptureResult:
    """Result of a capture operation."""
    file_path: str = ""
    image_data: str = ""  # Base64-encoded PNG
    format: str = "PNG"
    width: int = 0
    height: int = 0
    size_bytes: int = 0

    @classmethod
    def from_dict(cls, d: Dict) -> "CaptureResult":
        return cls(
            file_path=d.get("filePath", ""),
            image_data=d.get("imageData", ""),
            format=d.get("format", "PNG"),
            width=d.get("width", 0),
            height=d.get("height", 0),
            size_bytes=d.get("sizeBytes", 0),
        )


@dataclass
class SceneCaptureResult(CaptureResult):
    """Result of a scene capture operation with camera info."""
    camera_location: Vector = field(default_factory=Vector)
    camera_rotation: Rotator = field(default_factory=Rotator)

    @classmethod
    def from_dict(cls, d: Dict) -> "SceneCaptureResult":
        return cls(
            file_path=d.get("filePath", ""),
            image_data=d.get("imageData", ""),
            format=d.get("format", "PNG"),
            width=d.get("width", 0),
            height=d.get("height", 0),
            size_bytes=d.get("sizeBytes", 0),
            camera_location=Vector.from_dict(d.get("cameraLocation")),
            camera_rotation=Rotator.from_dict(d.get("cameraRotation")),
        )


@dataclass
class AudioAnalysisResult:
    """Result of audio analysis."""
    rms_volume: float = 0.0
    peak_volume: float = 0.0
    frequency_bands: List[float] = field(default_factory=list)
    beat_detected: bool = False
    current_time: float = 0.0

    @classmethod
    def from_dict(cls, d: Dict) -> "AudioAnalysisResult":
        return cls(
            rms_volume=d.get("rmsVolume", 0.0),
            peak_volume=d.get("peakVolume", 0.0),
            frequency_bands=d.get("frequencyBands", []),
            beat_detected=d.get("beatDetected", False),
            current_time=d.get("currentTime", 0.0),
        )


@dataclass
class AudioCaptureResult:
    """Result of audio capture operation."""
    capture_id: str = ""
    file_path: str = ""
    audio_data: str = ""  # Base64-encoded WAV
    format: str = "WAV"
    sample_rate: int = 44100
    channels: int = 2
    duration: float = 0.0

    @classmethod
    def from_dict(cls, d: Dict) -> "AudioCaptureResult":
        return cls(
            capture_id=d.get("captureId", ""),
            file_path=d.get("filePath", ""),
            audio_data=d.get("audioData", ""),
            format=d.get("format", "WAV"),
            sample_rate=d.get("sampleRate", 44100),
            channels=d.get("channels", 2),
            duration=d.get("duration", 0.0),
        )


@dataclass
class MaterialParameterInfo:
    """Information about a material parameter."""
    name: str = ""
    type: str = ""  # "Scalar", "Vector", "Texture"
    value: str = ""
    group: str = ""

    @classmethod
    def from_dict(cls, d: Dict) -> "MaterialParameterInfo":
        return cls(
            name=d.get("name", ""),
            type=d.get("type", ""),
            value=d.get("value", ""),
            group=d.get("group", ""),
        )


@dataclass
class MaterialInfo:
    """Information about a material."""
    asset_path: str = ""
    name: str = ""
    is_material_instance: bool = False
    parent_path: str = ""
    two_sided: bool = False
    blend_mode: str = ""

    @classmethod
    def from_dict(cls, d: Dict) -> "MaterialInfo":
        return cls(
            asset_path=d.get("assetPath", ""),
            name=d.get("name", ""),
            is_material_instance=d.get("isMaterialInstance", False),
            parent_path=d.get("parentPath", ""),
            two_sided=d.get("twoSided", False),
            blend_mode=d.get("blendMode", ""),
        )


@dataclass
class MaterialDetails:
    """Detailed information about a material including parameters."""
    asset_path: str = ""
    name: str = ""
    is_material_instance: bool = False
    parent_path: str = ""
    two_sided: bool = False
    blend_mode: str = ""
    parameters: List[MaterialParameterInfo] = field(default_factory=list)

    @classmethod
    def from_dict(cls, d: Dict) -> "MaterialDetails":
        params = [MaterialParameterInfo.from_dict(p) for p in d.get("parameters", [])]
        mat = d.get("material", d)  # Handle both nested and flat responses
        return cls(
            asset_path=mat.get("assetPath", ""),
            name=mat.get("name", ""),
            is_material_instance=mat.get("isMaterialInstance", False),
            parent_path=mat.get("parentPath", ""),
            two_sided=mat.get("twoSided", False),
            blend_mode=mat.get("blendMode", ""),
            parameters=params,
        )


@dataclass
class MaterialInstanceResult:
    """Result of creating a material instance."""
    instance_name: str = ""
    applied_to_owner: bool = False

    @classmethod
    def from_dict(cls, d: Dict) -> "MaterialInstanceResult":
        return cls(
            instance_name=d.get("instanceName", ""),
            applied_to_owner=d.get("appliedToOwner", False),
        )


@dataclass
class PCGActorInfo:
    """Information about a PCG actor."""
    guid: str = ""
    name: str = ""
    label: str = ""
    graph_name: str = ""
    is_generated: bool = False
    status: str = ""

    @classmethod
    def from_dict(cls, d: Dict) -> "PCGActorInfo":
        return cls(
            guid=d.get("guid", ""),
            name=d.get("name", ""),
            label=d.get("label", ""),
            graph_name=d.get("graphName", ""),
            is_generated=d.get("isGenerated", False),
            status=d.get("status", ""),
        )


@dataclass
class PCGRegenerateResult:
    """Result of PCG regeneration."""
    generated_count: int = 0
    generation_time_ms: float = 0.0

    @classmethod
    def from_dict(cls, d: Dict) -> "PCGRegenerateResult":
        return cls(
            generated_count=d.get("generatedCount", 0),
            generation_time_ms=d.get("generationTimeMs", 0.0),
        )


@dataclass
class CVarInfo:
    """Information about a console variable."""
    name: str = ""
    value: str = ""
    type: str = ""  # "Int", "Float", "String"
    is_read_only: bool = False
    is_cheat: bool = False
    help_text: str = ""

    @classmethod
    def from_dict(cls, d: Dict) -> "CVarInfo":
        return cls(
            name=d.get("name", ""),
            value=d.get("value", ""),
            type=d.get("type", "String"),
            is_read_only=d.get("isReadOnly", False),
            is_cheat=d.get("isCheat", False),
            help_text=d.get("helpText", ""),
        )


class AgentBridgeError(Exception):
    """Exception raised when an AgentBridge command fails."""

    def __init__(self, message: str, command_id: str = ""):
        super().__init__(message)
        self.message = message
        self.command_id = command_id
