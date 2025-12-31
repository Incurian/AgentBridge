#!/usr/bin/env python3
"""
Test script for the AgentBridge Python client.

Run with: python test_client.py

Requires the Unreal Editor to be running with the AgentBridge plugin loaded.
"""

import sys
sys.path.insert(0, ".")

from agentbridge import AgentBridgeClient, AgentBridgeError


def main():
    print("=" * 60)
    print("AgentBridge Python Client Test")
    print("=" * 60)

    # Create client
    client = AgentBridgeClient()

    # Health check
    print("\n[1] Health Check...")
    if not client.health_check():
        print("ERROR: Server not running. Start Unreal Editor with AgentBridge plugin.")
        return 1

    print("    Server is healthy!")

    # List worlds
    print("\n[2] List Worlds...")
    worlds = client.list_worlds()
    for i, world in enumerate(worlds):
        print(f"    [{i}] {world.world_type}: {world.world_name} ({world.actor_count} actors)")

    # Query actors
    print("\n[3] Query Actors (Light*)...")
    actors = client.query_actors(name_pattern="Light", limit=5)
    for actor in actors:
        print(f"    - {actor.label} ({actor.class_name}) at {actor.location.to_tuple()}")

    # Spawn actor
    print("\n[4] Spawn PointLight...")
    try:
        new_actor = client.spawn_actor(
            "PointLight",
            location=(100, 200, 300),
            label="PythonTestLight",
        )
        print(f"    Spawned: {new_actor.label} (GUID: {new_actor.guid})")
    except AgentBridgeError as e:
        print(f"    ERROR: {e.message}")
        return 1

    # Get actor location via function call
    print("\n[5] Get Actor Location (via K2_GetActorLocation)...")
    location = client.get_actor_location("PythonTestLight")
    print(f"    Location: {location.to_tuple()}")

    # Move the actor
    print("\n[6] Move Actor...")
    client.set_actor_transform("PythonTestLight", location=(500, 500, 500))
    new_location = client.get_actor_location("PythonTestLight")
    print(f"    New Location: {new_location.to_tuple()}")

    # List light classes
    print("\n[7] List Light Classes...")
    classes = client.list_classes(base_class_name="Light", limit=10)
    for cls in classes:
        print(f"    - {cls.class_name} (parent: {cls.parent_class_name})")

    # Delete the test actor
    print("\n[8] Delete Test Actor...")
    client.delete_actor("PythonTestLight")
    print("    Deleted!")

    # Verify deletion
    print("\n[9] Verify Deletion...")
    found = client.find_actor("PythonTestLight")
    if found:
        print("    ERROR: Actor still exists!")
        return 1
    else:
        print("    Confirmed: Actor no longer exists.")

    print("\n" + "=" * 60)
    print("All tests passed!")
    print("=" * 60)
    return 0


if __name__ == "__main__":
    sys.exit(main())
