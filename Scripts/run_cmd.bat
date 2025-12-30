@echo off
REM Run console commands in headless editor and show output
REM Usage: run_cmd.bat "Command1,Command2"

setlocal

set UE_EDITOR=D:\UE571\Engine\Binaries\Win64\UnrealEditor-Cmd.exe
set PROJECT=E:\UnrealProjects\VR_Project\VR_Project.uproject
set LOG=E:\UnrealProjects\VR_Project\Saved\Logs\VR_Project.log

if "%~1"=="" (
    echo Usage: run_cmd.bat "Command1,Command2"
    echo Example: run_cmd.bat "AgentBridge.ListWorlds"
    exit /b 1
)

echo Running: %~1
echo.

REM Clear log marker
echo === AgentBridge Test Run === >> "%LOG%"

"%UE_EDITOR%" "%PROJECT%" -ExecCmds="%~1,Quit" -unattended -NullRHI -nosplash -nosound -noloadstartuppackages

echo.
echo === Output ===
findstr /C:"LogAgentBridge" "%LOG%" | tail -20
