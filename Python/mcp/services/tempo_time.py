"""
Tempo TimeService MCP Tools

Exposes simulation time control: play, pause, step, time mode.
"""

import json
from typing import Dict, Any
from . import register_service, ServiceModule
from .base import create_channel, safe_call

# Import Tempo's generated stubs
from TempoTime import Time_pb2 as pb
from TempoTime import Time_pb2_grpc as pb_grpc
from TempoScripting import Empty_pb2


TOOLS = [
    {
        "name": "tempo_play",
        "description": "Start or resume simulation playback in Unreal Engine.",
        "inputSchema": {
            "type": "object",
            "properties": {},
            "required": [],
        },
    },
    {
        "name": "tempo_pause",
        "description": "Pause simulation playback in Unreal Engine.",
        "inputSchema": {
            "type": "object",
            "properties": {},
            "required": [],
        },
    },
    {
        "name": "tempo_step",
        "description": "Advance simulation by one frame/step.",
        "inputSchema": {
            "type": "object",
            "properties": {},
            "required": [],
        },
    },
    {
        "name": "tempo_advance_steps",
        "description": "Advance simulation by a specific number of steps.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "steps": {
                    "type": "integer",
                    "description": "Number of simulation steps to advance",
                    "minimum": 1,
                },
            },
            "required": ["steps"],
        },
    },
    {
        "name": "tempo_set_time_mode",
        "description": "Set the simulation time mode. WALL_CLOCK runs in real-time, FIXED_STEP runs at a fixed rate.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "mode": {
                    "type": "string",
                    "enum": ["WALL_CLOCK", "FIXED_STEP"],
                    "description": "Time mode: WALL_CLOCK (real-time) or FIXED_STEP (deterministic)",
                },
            },
            "required": ["mode"],
        },
    },
    {
        "name": "tempo_set_sim_rate",
        "description": "Set the simulation steps per second when in FIXED_STEP mode.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "steps_per_second": {
                    "type": "integer",
                    "description": "Simulation steps per second (e.g., 30, 60, 120)",
                    "minimum": 1,
                },
            },
            "required": ["steps_per_second"],
        },
    },
]


class TempoTimeClient:
    """Client for Tempo's TimeService."""

    def __init__(self, host: str = "localhost", port: int = 50051):
        self.channel = create_channel(host, port)
        self.stub = pb_grpc.TimeServiceStub(self.channel)

    def play(self):
        return self.stub.Play(Empty_pb2.Empty())

    def pause(self):
        return self.stub.Pause(Empty_pb2.Empty())

    def step(self):
        return self.stub.Step(Empty_pb2.Empty())

    def advance_steps(self, steps: int):
        return self.stub.AdvanceSteps(pb.AdvanceStepsRequest(steps=steps))

    def set_time_mode(self, mode: str):
        time_mode = pb.WALL_CLOCK if mode == "WALL_CLOCK" else pb.FIXED_STEP
        return self.stub.SetTimeMode(pb.TimeModeRequest(time_mode=time_mode))

    def set_sim_steps_per_second(self, steps_per_second: int):
        return self.stub.SetSimStepsPerSecond(
            pb.SetSimStepsPerSecondRequest(sim_steps_per_second=steps_per_second)
        )


def connect(host: str, port: int) -> TempoTimeClient:
    """Create a TempoTimeClient."""
    return TempoTimeClient(host, port)


def execute(client: TempoTimeClient, tool_name: str, args: Dict[str, Any]) -> str:
    """Execute a tempo_time tool."""
    result = _execute_impl(client, tool_name, args)
    return json.dumps(result, indent=2)


def _execute_impl(client: TempoTimeClient, tool_name: str, args: Dict[str, Any]) -> Any:
    """Implementation of tool execution."""

    if tool_name == "tempo_play":
        safe_call(client.play)
        return {"success": True, "action": "play"}

    elif tool_name == "tempo_pause":
        safe_call(client.pause)
        return {"success": True, "action": "pause"}

    elif tool_name == "tempo_step":
        safe_call(client.step)
        return {"success": True, "action": "step"}

    elif tool_name == "tempo_advance_steps":
        steps = args["steps"]
        safe_call(client.advance_steps, steps)
        return {"success": True, "action": "advance_steps", "steps": steps}

    elif tool_name == "tempo_set_time_mode":
        mode = args["mode"]
        safe_call(client.set_time_mode, mode)
        return {"success": True, "action": "set_time_mode", "mode": mode}

    elif tool_name == "tempo_set_sim_rate":
        rate = args["steps_per_second"]
        safe_call(client.set_sim_steps_per_second, rate)
        return {"success": True, "action": "set_sim_rate", "steps_per_second": rate}

    else:
        return {"error": f"Unknown tool: {tool_name}"}


# Register this service module
register_service(ServiceModule(
    name="tempo_time",
    description="Tempo TimeService - simulation playback control",
    tools=TOOLS,
    execute=execute,
    connect=connect,
))
