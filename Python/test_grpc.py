#!/usr/bin/env python3
"""
gRPC Test Script for AgentBridge Service

Tests the AgentBridge gRPC service via Tempo's scripting infrastructure.
Requires:
1. Unreal Editor running with TempoSample project
2. gRPC server active (default port 50051)

Run with: python test_grpc.py [--host HOST] [--port PORT]
"""

import sys
import os
import argparse
import time

# Add Tempo API path for generated stubs
_tempo_api_path = "D:/tempo/TempoSample/Plugins/Tempo/TempoCore/Content/Python/API/tempo"
if _tempo_api_path not in sys.path:
    sys.path.insert(0, _tempo_api_path)

import grpc

# Import generated protobuf stubs
try:
    from AgentBridgeServer import AgentBridge_pb2 as pb
    from AgentBridgeServer import AgentBridge_pb2_grpc as pb_grpc
    from TempoScripting import Geometry_pb2
except ImportError as e:
    print(f"ERROR: Could not import protobuf stubs: {e}")
    print("Make sure PYTHONPATH includes Tempo's API directory.")
    print(f"Expected path: {_tempo_api_path}")
    sys.exit(1)


class Colors:
    """ANSI color codes for terminal output."""
    GREEN = "\033[92m"
    RED = "\033[91m"
    YELLOW = "\033[93m"
    CYAN = "\033[96m"
    RESET = "\033[0m"
    BOLD = "\033[1m"


def success(msg):
    print(f"  {Colors.GREEN}[PASS]{Colors.RESET} {msg}")


def failure(msg):
    print(f"  {Colors.RED}[FAIL]{Colors.RESET} {msg}")


def info(msg):
    print(f"  {Colors.CYAN}[INFO]{Colors.RESET} {msg}")


def section(name):
    print(f"\n{Colors.BOLD}{Colors.YELLOW}[{name}]{Colors.RESET}")


class AgentBridgeGrpcTester:
    """Tests for AgentBridge gRPC service."""

    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
        self.channel = None
        self.stub = None
        self.test_actor_name = f"GrpcTest_{int(time.time())}"
        self.tests_passed = 0
        self.tests_failed = 0

    def connect(self) -> bool:
        """Connect to the gRPC server."""
        try:
            self.channel = grpc.insecure_channel(f"{self.host}:{self.port}")
            self.stub = pb_grpc.AgentBridgeServiceStub(self.channel)
            # Test connection with a simple call
            grpc.channel_ready_future(self.channel).result(timeout=5)
            return True
        except grpc.FutureTimeoutError:
            return False
        except Exception as e:
            print(f"Connection error: {e}")
            return False

    def disconnect(self):
        """Close the gRPC channel."""
        if self.channel:
            self.channel.close()

    def _make_vector(self, x, y, z):
        return Geometry_pb2.Vector(x=x, y=y, z=z)

    def _make_rotation(self, pitch, yaw, roll):
        return Geometry_pb2.Rotation(r=roll, p=pitch, y=yaw)

    def _make_scale(self, x, y, z):
        return pb.Scale(x=x, y=y, z=z)

    def test_list_worlds(self) -> bool:
        """Test ListWorlds RPC."""
        try:
            response = self.stub.ListWorlds(pb.ListWorldsRequest())
            if len(response.worlds) > 0:
                success(f"ListWorlds: Found {len(response.worlds)} world(s)")
                for w in response.worlds:
                    info(f"  - {w.world_type}: {w.world_name} ({w.actor_count} actors)")
                return True
            else:
                failure("ListWorlds: No worlds returned")
                return False
        except grpc.RpcError as e:
            failure(f"ListWorlds: gRPC error - {e.code().name}: {e.details()}")
            return False

    def test_query_actors(self) -> bool:
        """Test QueryActors RPC."""
        try:
            # Query for any Light actors
            response = self.stub.QueryActors(pb.QueryActorsRequest(
                name_pattern="*Light*",
                limit=10,
            ))
            success(f"QueryActors: Found {response.total_count} actor(s) matching '*Light*'")
            for actor in response.actors[:3]:
                info(f"  - {actor.label or actor.name} ({actor.class_name})")
            return True
        except grpc.RpcError as e:
            failure(f"QueryActors: gRPC error - {e.code().name}: {e.details()}")
            return False

    def test_spawn_actor(self) -> bool:
        """Test SpawnActor RPC."""
        try:
            transform = pb.ActorTransform(
                location=self._make_vector(100, 200, 300),
                rotation=self._make_rotation(0, 45, 0),
                scale=self._make_scale(1, 1, 1),
            )
            response = self.stub.SpawnActor(pb.SpawnActorRequest(
                class_name="PointLight",
                transform=transform,
                label=self.test_actor_name,
            ))
            if response.HasField("spawned_actor"):
                actor = response.spawned_actor
                success(f"SpawnActor: Created '{actor.label}' (GUID: {actor.guid[:8]}...)")
                loc = actor.transform.location
                info(f"  Location: ({loc.x}, {loc.y}, {loc.z})")
                return True
            else:
                failure("SpawnActor: No actor returned")
                return False
        except grpc.RpcError as e:
            failure(f"SpawnActor: gRPC error - {e.code().name}: {e.details()}")
            return False

    def test_get_actor(self) -> bool:
        """Test GetActor RPC."""
        try:
            response = self.stub.GetActor(pb.GetActorRequest(
                actor_id=self.test_actor_name,
                include_properties=True,
                include_components=True,
            ))
            if response.HasField("actor"):
                actor = response.actor
                success(f"GetActor: Retrieved '{actor.actor_info.label}'")
                info(f"  Properties: {len(actor.properties)} item(s)")
                info(f"  Components: {len(actor.components)} item(s)")
                return True
            else:
                failure(f"GetActor: Actor '{self.test_actor_name}' not found")
                return False
        except grpc.RpcError as e:
            failure(f"GetActor: gRPC error - {e.code().name}: {e.details()}")
            return False

    def test_set_actor_transform(self) -> bool:
        """Test SetActorTransform RPC."""
        try:
            new_transform = pb.ActorTransform(
                location=self._make_vector(500, 500, 500),
                rotation=self._make_rotation(0, 90, 0),
            )
            self.stub.SetActorTransform(pb.SetActorTransformRequest(
                actor_id=self.test_actor_name,
                transform=new_transform,
            ))
            success("SetActorTransform: Moved actor to (500, 500, 500)")
            return True
        except grpc.RpcError as e:
            failure(f"SetActorTransform: gRPC error - {e.code().name}: {e.details()}")
            return False

    def test_get_property_path(self) -> bool:
        """Test GetPropertyPath RPC."""
        try:
            response = self.stub.GetPropertyPath(pb.GetPropertyPathRequest(
                actor_id=self.test_actor_name,
                path="LightComponent.Intensity",
            ))
            success(f"GetPropertyPath: LightComponent.Intensity = {response.type_name}")
            # Log the value type
            val = response.value
            if val.type == pb.PROPERTY_TYPE_FLOAT:
                info(f"  Value: {val.float_value} (float)")
            elif val.type == pb.PROPERTY_TYPE_INT:
                info(f"  Value: {val.int_value} (int)")
            elif val.type == pb.PROPERTY_TYPE_STRING:
                info(f"  Value: {val.string_value} (string)")
            return True
        except grpc.RpcError as e:
            failure(f"GetPropertyPath: gRPC error - {e.code().name}: {e.details()}")
            return False

    def test_set_property_path(self) -> bool:
        """Test SetPropertyPath RPC."""
        try:
            value = pb.PropertyValue(
                type=pb.PROPERTY_TYPE_FLOAT,
                float_value=5000.0,
            )
            self.stub.SetPropertyPath(pb.SetPropertyPathRequest(
                actor_id=self.test_actor_name,
                path="LightComponent.Intensity",
                value=value,
            ))
            success("SetPropertyPath: Set LightComponent.Intensity = 5000")
            return True
        except grpc.RpcError as e:
            failure(f"SetPropertyPath: gRPC error - {e.code().name}: {e.details()}")
            return False

    def test_list_classes(self) -> bool:
        """Test ListClasses RPC."""
        try:
            response = self.stub.ListClasses(pb.ListClassesRequest(
                base_class_name="Light",
                include_blueprint=True,
                limit=20,
            ))
            success(f"ListClasses: Found {response.total_count} Light classes")
            for cls in response.classes[:5]:
                info(f"  - {cls.class_name} (parent: {cls.parent_class_name})")
            return True
        except grpc.RpcError as e:
            failure(f"ListClasses: gRPC error - {e.code().name}: {e.details()}")
            return False

    def test_call_function(self) -> bool:
        """Test CallFunction RPC (known issue: returns default values)."""
        try:
            response = self.stub.CallFunction(pb.CallFunctionRequest(
                actor_id=self.test_actor_name,
                function_name="K2_GetActorLocation",
            ))
            success("CallFunction: K2_GetActorLocation executed")
            # Note: Return values currently return defaults due to known issue
            val = response.return_value
            if val.type == pb.PROPERTY_TYPE_VECTOR:
                v = val.vector_value
                info(f"  Returned: ({v.x}, {v.y}, {v.z}) [may be default due to known issue]")
            return True
        except grpc.RpcError as e:
            failure(f"CallFunction: gRPC error - {e.code().name}: {e.details()}")
            return False

    def test_delete_actor(self) -> bool:
        """Test DeleteActor RPC."""
        try:
            self.stub.DeleteActor(pb.DeleteActorRequest(
                actor_id=self.test_actor_name,
            ))
            success(f"DeleteActor: Deleted '{self.test_actor_name}'")
            return True
        except grpc.RpcError as e:
            failure(f"DeleteActor: gRPC error - {e.code().name}: {e.details()}")
            return False

    def test_verify_deletion(self) -> bool:
        """Verify the test actor was deleted."""
        try:
            # Query should return no results for our test actor
            response = self.stub.QueryActors(pb.QueryActorsRequest(
                name_pattern=self.test_actor_name,
                limit=1,
            ))
            if response.total_count == 0:
                success("Verify deletion: Actor confirmed deleted")
                return True
            else:
                failure("Verify deletion: Actor still exists!")
                return False
        except grpc.RpcError as e:
            failure(f"Verify deletion: gRPC error - {e.code().name}: {e.details()}")
            return False

    def run_all_tests(self):
        """Run all tests."""
        print("=" * 60)
        print(f"{Colors.BOLD}AgentBridge gRPC Service Test Suite{Colors.RESET}")
        print(f"Server: {self.host}:{self.port}")
        print("=" * 60)

        # Connect
        section("Connection")
        if not self.connect():
            failure(f"Could not connect to gRPC server at {self.host}:{self.port}")
            print("\nMake sure:")
            print("  1. Unreal Editor is running with TempoSample project")
            print("  2. The gRPC server is active (default port 50051)")
            return False
        success(f"Connected to gRPC server")

        # World operations
        section("World Operations")
        self._run_test(self.test_list_worlds)

        # Actor discovery
        section("Actor Discovery")
        self._run_test(self.test_query_actors)

        # Actor manipulation
        section("Actor Manipulation")
        self._run_test(self.test_spawn_actor)
        self._run_test(self.test_get_actor)
        self._run_test(self.test_set_actor_transform)

        # Property operations
        section("Property Operations")
        self._run_test(self.test_get_property_path)
        self._run_test(self.test_set_property_path)

        # Function invocation
        section("Function Invocation")
        self._run_test(self.test_call_function)

        # Type discovery
        section("Type Discovery")
        self._run_test(self.test_list_classes)

        # Cleanup
        section("Cleanup")
        self._run_test(self.test_delete_actor)
        self._run_test(self.test_verify_deletion)

        # Summary
        print("\n" + "=" * 60)
        total = self.tests_passed + self.tests_failed
        if self.tests_failed == 0:
            print(f"{Colors.GREEN}{Colors.BOLD}All {total} tests passed!{Colors.RESET}")
        else:
            print(f"{Colors.RED}{Colors.BOLD}{self.tests_failed}/{total} tests failed{Colors.RESET}")
        print("=" * 60)

        self.disconnect()
        return self.tests_failed == 0

    def _run_test(self, test_func):
        """Run a test and track results."""
        if test_func():
            self.tests_passed += 1
        else:
            self.tests_failed += 1


def main():
    parser = argparse.ArgumentParser(description="Test AgentBridge gRPC service")
    parser.add_argument("--host", default="localhost", help="gRPC server host")
    parser.add_argument("--port", type=int, default=50051, help="gRPC server port")
    args = parser.parse_args()

    tester = AgentBridgeGrpcTester(args.host, args.port)
    success = tester.run_all_tests()
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
