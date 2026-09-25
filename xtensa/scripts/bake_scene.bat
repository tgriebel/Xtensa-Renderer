@echo off
setlocal

REM Launch the AssetBaker against a scene folder.
REM Usage: bake_scene.bat <scene folder>
REM
REM Scene folder resolves relative to xtensa\scenes\ (where ScenePath = ".\scenes\"),
REM and is expected to contain a json file matching the folder's own name
REM (e.g. "chess" -> scenes\chess\chess.json).

if "%~1"=="" (
    echo Usage: bake_scene.bat ^<scene folder^>
    exit /b 1
)

set SCENE_FOLDER=%~1
if "%SCENE_FOLDER:~-1%"=="\" set SCENE_FOLDER=%SCENE_FOLDER:~0,-1%

for %%F in ("%SCENE_FOLDER%") do set SCENE_NAME=%%~nxF

set SCENE_JSON=%SCENE_FOLDER%\%SCENE_NAME%.json

set SCRIPT_DIR=%~dp0
set VK_RENDERER_DIR=%SCRIPT_DIR%..
set SOLUTION_OUT=%SCRIPT_DIR%..\..\x64

set BAKER_EXE=%SOLUTION_OUT%\Release\AssetBaker.exe

REM If the Release exe doesn't exist, kick off a Release build.
if not exist "%BAKER_EXE%" (
    call "%SCRIPT_DIR%buildTools.bat"
    if errorlevel 1 exit /b 1
)

if not exist "%BAKER_EXE%" (
    echo ERROR: AssetBaker still missing at "%BAKER_EXE%" after build attempt.
    exit /b 1
)

pushd "%VK_RENDERER_DIR%"
if not exist "scenes\%SCENE_JSON%" (
    echo ERROR: "scenes\%SCENE_JSON%" not found.
    popd
    exit /b 1
)

"%BAKER_EXE%" "%SCENE_JSON%"
set EXIT_CODE=%ERRORLEVEL%
popd

exit /b %EXIT_CODE%
