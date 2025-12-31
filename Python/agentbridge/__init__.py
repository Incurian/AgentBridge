"""
AgentBridge Python Client

HTTP client for communicating with the AgentBridge Unreal Engine plugin.
Provides a clean Python API for querying and manipulating actors in UE.

Usage:
    from agentbridge import AgentBridgeClient

    client = AgentBridgeClient()  # Connects to localhost:8080

    # List all worlds
    worlds = client.list_worlds()

    # Query actors
    actors = client.query_actors(name_pattern="Light", limit=10)

    # Spawn an actor
    actor = client.spawn_actor("PointLight", location=(0, 0, 200), label="MyLight")

    # Delete an actor
    client.delete_actor("MyLight")

Phase 2 Note:
    This client uses HTTP/JSON transport. In Phase 2, the transport layer
    will be swapped to gRPC while keeping the same API surface.
"""

from .client import AgentBridgeClient
from .types import (
    WorldInfo,
    ActorInfo,
    ActorDetails,
    ClassInfo,
    PropertyValue,
    FunctionResult,
    ContextCapabilities,
    CoreCapabilities,
    EditorCapabilities,
    DataAssetInfo,
    DataAssetDetails,
    DataTableRowInfo,
    CaptureResult,
    SceneCaptureResult,
    AudioAnalysisResult,
    AudioCaptureResult,
    MaterialInfo,
    MaterialDetails,
    MaterialParameterInfo,
    MaterialInstanceResult,
    PCGActorInfo,
    PCGRegenerateResult,
    CVarInfo,
    AgentBridgeError,
)

__version__ = "1.5.0"
__all__ = [
    "AgentBridgeClient",
    "AgentBridgeError",
    "WorldInfo",
    "ActorInfo",
    "ActorDetails",
    "ClassInfo",
    "PropertyValue",
    "FunctionResult",
    "ContextCapabilities",
    "CoreCapabilities",
    "EditorCapabilities",
    "DataAssetInfo",
    "DataAssetDetails",
    "DataTableRowInfo",
    "CaptureResult",
    "SceneCaptureResult",
    "AudioAnalysisResult",
    "AudioCaptureResult",
    "MaterialInfo",
    "MaterialDetails",
    "MaterialParameterInfo",
    "MaterialInstanceResult",
    "PCGActorInfo",
    "PCGRegenerateResult",
    "CVarInfo",
]
