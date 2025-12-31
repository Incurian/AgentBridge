"""
Base utilities for service modules.
"""

import grpc
from typing import Optional

# Common PYTHONPATH setup for Tempo's generated stubs
import sys
import os

_tempo_api_path = "D:/tempo/TempoSample/Plugins/Tempo/TempoCore/Content/Python/API/tempo"
if _tempo_api_path not in sys.path:
    sys.path.insert(0, _tempo_api_path)


def create_channel(host: str = "localhost", port: int = 50051) -> grpc.Channel:
    """Create a gRPC channel to the Tempo server."""
    return grpc.insecure_channel(f"{host}:{port}")


def safe_call(func, *args, **kwargs):
    """Wrap a gRPC call with error handling."""
    try:
        return func(*args, **kwargs)
    except grpc.RpcError as e:
        return {"error": f"gRPC error: {e.code().name} - {e.details()}"}
    except Exception as e:
        return {"error": str(e)}
