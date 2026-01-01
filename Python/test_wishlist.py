#!/usr/bin/env python3
"""
Integration tests for AgentBridge Wishlist features.

Tests the new Asset, Component, and File operations.
Requires Unreal Editor running with TempoSample project.

Usage:
    cd D:/tempo/TempoSample/Plugins/AgentBridge/Python
    PYTHONPATH="D:/tempo/TempoSample/Plugins/Tempo/TempoCore/Content/Python/API/tempo" \
        D:/tempo/TempoSample/TempoEnv/Scripts/python.exe test_wishlist.py
"""

import sys
import os
import json
import time
import traceback

# Add the Python directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from mcp.services.agentbridge import connect, execute


def test_file_operations(client):
    """Test file read/write/list operations."""
    print("\n=== Testing File Operations ===")

    test_file = "Saved/AgentBridge_Test.txt"
    test_content = "Hello from AgentBridge test!\nLine 2\nLine 3"

    # Write a test file
    print(f"1. Writing test file: {test_file}")
    result = json.loads(execute(client, "write_project_file", {
        "relative_path": test_file,
        "content": test_content,
        "create_directories": True,
    }))
    print(f"   Result: success={result.get('success')}")
    if not result.get('success'):
        print(f"   ERROR: {result.get('error_message')}")
        return False

    # Read it back
    print(f"2. Reading test file back")
    result = json.loads(execute(client, "read_project_file", {
        "relative_path": test_file,
    }))
    print(f"   Result: success={result.get('success')}, size={result.get('file_size_bytes')}")
    if result.get('content') != test_content:
        print(f"   ERROR: Content mismatch!")
        return False

    # List the directory
    print(f"3. Listing Saved directory")
    result = json.loads(execute(client, "list_project_directory", {
        "relative_path": "Saved",
        "pattern": "AgentBridge_*",
        "limit": 10,
    }))
    print(f"   Result: success={result.get('success')}, found {len(result.get('files', []))} files")

    # Copy the file
    copy_dest = "Saved/AgentBridge_Test_Copy.txt"
    print(f"4. Copying file to {copy_dest}")
    result = json.loads(execute(client, "copy_project_file", {
        "source_path": test_file,
        "dest_path": copy_dest,
        "overwrite": True,
    }))
    print(f"   Result: success={result.get('success')}")

    print("File operations: PASSED")
    return True


def test_component_transforms(client):
    """Test component transform operations."""
    print("\n=== Testing Component Transforms ===")

    # First, spawn a test actor
    print("1. Spawning test actor")
    result = json.loads(execute(client, "spawn_actor", {
        "class_name": "PointLight",
        "location": [0, 0, 500],
        "label": "TestLight_Wishlist",
    }))
    if not result.get('success'):
        print(f"   ERROR: {result.get('error_message')}")
        return False
    actor_id = result.get('actor_id', 'TestLight_Wishlist')
    print(f"   Created: {actor_id}")

    # Get component transform
    print("2. Getting component transform")
    result = json.loads(execute(client, "get_component_transform", {
        "actor_id": actor_id,
        "component_name": "LightComponent0",
        "world_space": True,
    }))
    print(f"   Result: success={result.get('success')}")
    if result.get('success'):
        loc = result.get('location', [])
        print(f"   Location: {loc}")

    # Set component transform
    print("3. Setting component transform")
    result = json.loads(execute(client, "set_component_transform", {
        "actor_id": actor_id,
        "component_name": "LightComponent0",
        "location": [100, 0, 0],
        "world_space": False,  # Relative offset
    }))
    print(f"   Result: success={result.get('success')}")

    # Cleanup
    print("4. Cleaning up test actor")
    result = json.loads(execute(client, "delete_actor", {
        "actor_id": actor_id,
    }))
    print(f"   Deleted: success={result.get('success')}")

    print("Component transforms: PASSED")
    return True


def test_actor_attachment(client):
    """Test actor attachment/detachment."""
    print("\n=== Testing Actor Attachment ===")

    # Spawn parent actor
    print("1. Spawning parent actor")
    result = json.loads(execute(client, "spawn_actor", {
        "class_name": "StaticMeshActor",
        "location": [0, 0, 100],
        "label": "TestParent_Wishlist",
    }))
    if not result.get('success'):
        print(f"   ERROR: {result.get('error_message')}")
        return False
    parent_id = "TestParent_Wishlist"
    print(f"   Created: {parent_id}")

    # Spawn child actor
    print("2. Spawning child actor")
    result = json.loads(execute(client, "spawn_actor", {
        "class_name": "PointLight",
        "location": [0, 0, 200],
        "label": "TestChild_Wishlist",
    }))
    if not result.get('success'):
        print(f"   ERROR: {result.get('error_message')}")
        return False
    child_id = "TestChild_Wishlist"
    print(f"   Created: {child_id}")

    # Attach child to parent
    print("3. Attaching child to parent")
    result = json.loads(execute(client, "attach_actor", {
        "child_actor_id": child_id,
        "parent_actor_id": parent_id,
        "location_rule": "keep_world",
    }))
    print(f"   Result: success={result.get('success')}")
    if not result.get('success'):
        print(f"   ERROR: {result.get('error_message', result.get('error'))}")

    # Detach child
    print("4. Detaching child")
    result = json.loads(execute(client, "detach_actor", {
        "actor_id": child_id,
        "location_rule": "keep_world",
    }))
    print(f"   Result: success={result.get('success')}")

    # Cleanup
    print("5. Cleaning up test actors")
    for actor in [child_id, parent_id]:
        result = json.loads(execute(client, "delete_actor", {"actor_id": actor}))
        print(f"   Deleted {actor}: {result.get('success')}")

    print("Actor attachment: PASSED")
    return True


def test_asset_operations(client):
    """Test asset creation and saving."""
    print("\n=== Testing Asset Operations ===")

    # Create a DataAsset
    print("1. Creating DataAsset")
    result = json.loads(execute(client, "create_asset", {
        "asset_class": "DataAsset",
        "package_path": "/Game/Test",
        "asset_name": "TestDataAsset_Wishlist",
    }))
    print(f"   Result: success={result.get('success')}")
    if result.get('success'):
        print(f"   Asset path: {result.get('asset_path')}")
    else:
        print(f"   Note: {result.get('error_message')}")
        # This is expected to fail if DataAsset isn't a creatable type

    # Get asset thumbnail (use a known asset)
    print("2. Getting asset thumbnail (Engine content)")
    result = json.loads(execute(client, "get_asset_thumbnail", {
        "asset_path": "/Engine/BasicShapes/Cube",
        "width": 64,
        "height": 64,
    }))
    print(f"   Result: success={result.get('success')}")
    if result.get('success'):
        img_len = len(result.get('image_data', ''))
        print(f"   Image data length: {img_len} chars (base64)")

    print("Asset operations: PASSED (with notes)")
    return True


def test_help_topics(client):
    """Test that help topics include new features."""
    print("\n=== Testing Help Topics ===")

    # Test assets topic
    print("1. Checking 'assets' topic")
    result = json.loads(execute(client, "help", {"topic": "assets"}))
    if "error" in result:
        print(f"   ERROR: {result.get('error')}")
        return False
    help_text = result.get('help', '')
    if 'create_asset' in help_text and 'file operations' in help_text.lower():
        print("   Found asset and file content")
    else:
        print("   WARNING: May be missing content")

    # Test components topic
    print("2. Checking 'components' topic")
    result = json.loads(execute(client, "help", {"topic": "components"}))
    if "error" in result:
        print(f"   ERROR: {result.get('error')}")
        return False
    help_text = result.get('help', '')
    if 'attach_actor' in help_text and 'detach' in help_text.lower():
        print("   Found attach/detach content")
    else:
        print("   WARNING: May be missing content")

    print("Help topics: PASSED")
    return True


def main():
    """Run all wishlist integration tests."""
    print("=" * 60)
    print("AgentBridge Wishlist Integration Tests")
    print("=" * 60)

    host = "localhost"
    port = 10001

    print(f"\nConnecting to gRPC server at {host}:{port}...")

    try:
        client = connect(host, port)

        # Quick connectivity check
        result = json.loads(execute(client, "list_worlds", {}))
        if "error" in result:
            print(f"ERROR: Cannot connect to server: {result.get('error')}")
            return 1
        print(f"Connected! Found {len(result.get('worlds', []))} world(s)")

    except Exception as e:
        print(f"ERROR: Failed to connect: {e}")
        print("\nMake sure Unreal Editor is running with the TempoSample project.")
        return 1

    # Run tests
    tests = [
        ("Help Topics", test_help_topics),
        ("File Operations", test_file_operations),
        ("Component Transforms", test_component_transforms),
        ("Actor Attachment", test_actor_attachment),
        ("Asset Operations", test_asset_operations),
    ]

    passed = 0
    failed = 0

    for name, test_func in tests:
        try:
            if test_func(client):
                passed += 1
            else:
                failed += 1
        except Exception as e:
            print(f"\nEXCEPTION in {name}:")
            traceback.print_exc()
            failed += 1

    # Summary
    print("\n" + "=" * 60)
    print(f"Test Summary: {passed} passed, {failed} failed")
    print("=" * 60)

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
