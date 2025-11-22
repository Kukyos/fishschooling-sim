@echo off
REM Build script for CUDA Fish Visualizer

echo ========================================
echo Building CUDA Fish Visualizer
echo ========================================
echo.

REM Ensure we're in GpuSchool directory
if not exist CMakeLists.txt (
    echo [ERROR] CMakeLists.txt not found in current directory!
    echo Please run this script from the GpuSchool folder.
    echo.
    echo Current directory: %CD%
    echo Expected: C:\Users\Cleo\Desktop\SHITIBUILT\fishschooling-sim\Final Cleaned
    echo.
    pause
    exit /b 1
)

REM Clean previous build
if exist build (
    echo Cleaning previous build...
    rmdir /s /q build
)

REM Create build directory
mkdir build
cd build

echo.
echo Configuring with CMake...
cmake .. -G "Visual Studio 17 2022" -A x64

if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed!
    echo.
    echo Try specifying paths manually:
    echo cmake .. -DGLFW_DIR="C:/Program Files/GLFW" -DGLM_DIR="C:/Program Files/GLM"
    cd ..
    pause
    exit /b 1
)

echo.
echo Building Release configuration...
cmake --build . --config Release -j 8

if %errorlevel% neq 0 (
    echo [ERROR] Build failed!
    cd ..
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build Successful!
echo ========================================
echo.
echo Executable: build\Release\fish_vis.exe
echo.
echo Run with: cd build\Release ^&^& fish_vis.exe
echo.
cd ..
pause
