@echo off
REM Quick setup script for CUDA Fish Visualizer

echo ========================================
echo CUDA Fish Visualizer - Setup
echo ========================================
echo.

REM Check for CUDA
where nvcc >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] CUDA Toolkit not found! Install from: https://developer.nvidia.com/cuda-downloads
    exit /b 1
)
echo [OK] CUDA Toolkit found

REM Check for CMake
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] CMake not found! Install from: https://cmake.org/download/
    exit /b 1
)
echo [OK] CMake found

echo.
echo ========================================
echo Downloading Dependencies
echo ========================================
echo.

REM Create external directory
if not exist external mkdir external

REM Clone ImGui
if not exist external\imgui (
    echo Downloading ImGui...
    git clone --depth 1 https://github.com/ocornut/imgui.git external\imgui
    if %errorlevel% neq 0 (
        echo [ERROR] Failed to clone ImGui
        exit /b 1
    )
    echo [OK] ImGui downloaded
) else (
    echo [OK] ImGui already exists
)

echo.
echo ========================================
echo Manual Steps Required
echo ========================================
echo.
echo 1. Download GLFW from: https://www.glfw.org/download.html
echo    Extract to: C:\Program Files\GLFW\
echo.
echo 2. Download GLM from: https://github.com/g-truc/glm/releases
echo    Extract to: C:\Program Files\GLM\
echo.
echo 3. Generate GLAD from: https://glad.dav1d.de/
echo    Settings: OpenGL 4.5+, Core Profile
echo    Extract glad.c to: src\
echo    Extract glad/ and KHR/ to: src\

echo After completing these steps, run: build.bat
echo.
pause
