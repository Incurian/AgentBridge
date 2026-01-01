AgentBridge MCP Testing Summary

  Session Goal

  Test whether I can programmatically capture a screenshot using a SceneCaptureComponent2D and save/view it.

  ---
  What Worked ✅

  1. Basic Connectivity

  mcp__agentbridge__list_worlds
  Result: Successfully returned Editor world "TestingMap" with 147 actors.

  ---
  2. Spawning Actors

  mcp__agentbridge__spawn_actor
    class_name: "SceneCapture2D"
    label: "TestCapture"
    location: [0, 0, 200]
  Result: Success - created actor with GUID E294E1554F2B7FD956926CAC36E43D57

  ---
  3. Getting Components (via Tempo)

  mcp__agentbridge__tempo_get_components
    actor: "TestCapture"
  Result: Success - returned 4 components:
  - SceneComponent
  - NewSceneCaptureComponent2D (type: SceneCaptureComponent2D)
  - StaticMeshComponent_0
  - DrawFrustumComponent_0

  ---
  4. Getting Component Properties (via Tempo)

  mcp__agentbridge__tempo_get_component_properties
    actor: "TestCapture"
    component: "NewSceneCaptureComponent2D"
  Result: Success - returned full property list including TextureTarget: null, FOVAngle: 90.0, CaptureSource, etc.

  ---
  5. Calling Functions on Components (via Tempo)

  mcp__agentbridge__tempo_call_function
    actor: "TestCapture"
    component: "NewSceneCaptureComponent2D"
    function: "CaptureScene"
  Result: {"success": true, "function": "CaptureScene"}

  This was a key discovery - function calling works!

  ---
  What Didn't Work ❌

  1. Tempo Sensors Service

  mcp__agentbridge__tempo_get_available_sensors
  Error: gRPC error: UNAVAILABLE - Service is not active

  Expected: List of available camera/sensor components
  Issue: Tempo service not running or not configured

  ---
  2. Class Schema Reflection for SceneCaptureComponent2D

  mcp__agentbridge__get_class_schema
    class_name: "SceneCaptureComponent2D"
    include_functions: true
    include_inherited: true
  Result: {"class_name": "SceneCaptureComponent2D", "properties": [], "functions": []}

  Expected: List of properties and callable functions
  Issue: Schema reflection returned empty despite the class existing and being usable

  ---
  3. Listing SceneCapture Classes

  mcp__agentbridge__list_classes
    name_pattern: "*SceneCapture*"
  Result: {"count": 0, "classes": []}

  Expected: Find SceneCapture2D, SceneCaptureComponent2D, etc.
  Issue: Class discovery didn't find these classes (possibly not registered as spawnable Actors?)

  ---
  4. Get Actor with Properties/Components

  mcp__agentbridge__get_actor
    actor_id: "TestCapture"
    include_components: true
    include_properties: true
  Result: Returned basic actor info but did not include components or properties despite flags being set

  Expected: Full actor details with component list and property values
  Issue: include_components and include_properties flags appear to be ignored

  ---
  5. Creating a Render Target via Function Call

  mcp__agentbridge__tempo_call_function
    actor: "TestCapture"
    component: "NewSceneCaptureComponent2D"
    function: "CreateTextureRenderTarget2D"
  Error: Function not found

  Expected: Create a render target dynamically
  Issue: This isn't a function on the component (it's in KismetRenderingLibrary)

  ---
  6. Setting TextureTarget Property

  mcp__agentbridge__set_property
    actor_id: "TestCapture"
    path: "CaptureComponent2D.TextureTarget"
    value: "NewObject"
  Error: Failed to set path 'CaptureComponent2D.TextureTarget'

  Expected: Either create or assign a render target
  Issue: Can't set object reference properties this way, and can't create assets

  ---
  The Core Blocker

  The capture pipeline requires:
  1. ✅ SceneCapture2D actor - can spawn
  2. ✅ CaptureScene() function - can call
  3. ❌ TextureRenderTarget2D asset - cannot create
  4. ❌ Export to disk - no function exposed

  ---
  Suggested Improvements

  Specific Fixes

  | Issue                                                   | Suggested Fix                                                          |
  |---------------------------------------------------------|------------------------------------------------------------------------|
  | get_actor ignores include_components/include_properties | Fix the AgentBridge implementation to honor these flags                |
  | get_class_schema returns empty for valid classes        | Expand reflection to include component classes, not just Actor classes |
  | list_classes misses component types                     | Add option to list UObject/UActorComponent classes, not just AActor    |
  | Can't create TextureRenderTarget2D                      | Add asset creation tool (see below)                                    |
  | Can't call static library functions                     | Add tool to call static Blueprint library functions                    |

  General Improvements

  1. Asset Creation Tool

  mcp__agentbridge__create_asset
    class_name: "TextureRenderTarget2D"
    asset_name: "MyRenderTarget"
    properties: {"SizeX": 1024, "SizeY": 1024, "RenderTargetFormat": "RTF_RGBA8"}
  This would enable creating render targets, materials, data assets, etc.

  2. Static Function Calls

  mcp__agentbridge__call_static_function
    class_name: "KismetRenderingLibrary"
    function: "CreateRenderTarget2D"
    params: {"Width": 1024, "Height": 1024}
  Would unlock Blueprint library functions like:
  - UKismetRenderingLibrary::CreateRenderTarget2D
  - UKismetRenderingLibrary::ExportRenderTarget
  - UKismetSystemLibrary::ExecuteConsoleCommand

  3. Function Discovery

  mcp__agentbridge__list_functions
    class_name: "SceneCaptureComponent2D"
    include_inherited: true
  Would help discover what functions are actually callable, since get_class_schema returns empty.

  4. Image Capture Shortcut

  A high-level convenience tool:
  mcp__agentbridge__capture_viewport
    location: [0, 0, 500]
    rotation: [0, -45, 0]
    resolution: [1920, 1080]
    output_path: "C:/Screenshots/capture.png"
  This would handle the entire pipeline internally.

  ---
  Console Commands Found (Potentially Useful)

  | Command                          | Purpose                                                  |
  |----------------------------------|----------------------------------------------------------|
  | HighResShot 1920x1080            | Take screenshot of viewport (saves to Saved/Screenshots) |
  | r.DumpRenderTargetPoolMemory     | Debug render target allocation                           |
  | Trace.Screenshot [Name] [ShowUI] | Take screenshot and save to trace                        |

  ---
  Summary

  The AgentBridge tools are solid for actor manipulation (spawn, transform, properties, function calls). The gap is in asset creation and static library function access. Adding these two capabilities would unlock significantly more workflows, including the screenshot capture pipeline we tested.