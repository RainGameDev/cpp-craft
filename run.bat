@echo off
REM Windows one-step: configure + build + run (run from cmd.exe).
REM Requires: CMake, a compiler (Visual Studio or MinGW), Vulkan SDK, GLFW.
REM GLFW via vcpkg example: vcpkg install glfw3, then pass
REM   -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
REM Usage:
REM   run.bat                 :: build (Release) and run
REM   run.bat --build-only    :: build only, don't launch the window
REM   run.bat --clean         :: wipe build\ first, then build+run
REM   run.bat --config Debug  :: Debug build (next arg is the config)
setlocal EnableDelayedExpansion

cd /d "%~dp0"

set "CONFIG=Release"
set "BUILD_ONLY=0"

:parse
if "%~1"=="" goto done_parse
if "%~1"=="--build-only" set "BUILD_ONLY=1"
if "%~1"=="--clean" (
  if exist build rmdir /s /q build
)
if "%~1"=="--config" (
  if not "%~2"=="" (
    set "CONFIG=%~2"
    shift
  )
)
if "%~1"=="--help" (
  echo Usage: run.bat [--clean] [--build-only] [--config Debug^^|Release]
  exit /b 0
)
shift
goto parse
:done_parse

cmake -S . -B build
if errorlevel 1 (
  echo CMake configure failed. Install CMake, Vulkan SDK, and GLFW, then retry.
  exit /b 1
)

cmake --build build --config %CONFIG%
if errorlevel 1 (
  echo Build failed.
  exit /b 1
)

if "%BUILD_ONLY%"=="1" (
  echo Build done.
  exit /b 0
)

REM Multi-config generators (Visual Studio): build\Release\minecraft.exe
REM Single-config generators (Ninja/MinGW):  build\minecraft.exe
if exist "build\%CONFIG%\minecraft.exe" (
  "build\%CONFIG%\minecraft.exe"
) else if exist "build\minecraft.exe" (
  "build\minecraft.exe"
) else if exist "build\Debug\minecraft.exe" (
  "build\Debug\minecraft.exe"
) else (
  echo Could not find minecraft.exe under build\.
  exit /b 1
)
