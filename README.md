# CUDA Fish Schooling - GPU Visualizer

cool fishes
## What's included
- `CMakeLists.txt` - Project build configuration
- `src/` - Core sources: `main.cpp`, `cuda_kernels.cu`, and build-time GL loader `glad.c` (if present)
- `build.bat` - Windows build script (Visual Studio 2022)

## Quick setup
1. Clone this cleaned folder into a fresh repo.
2. Install prerequisites: CUDA Toolkit, GLFW, GLM, GLAD, ImGui.
   - GLAD: generate and copy `glad.c`, `glad/glad.h`, `KHR/khrplatform.h` into `src/` (see original repo for config)
   - ImGui: clone `https://github.com/ocornut/imgui.git` into `external/imgui`
3. Run `build.bat` from the `Final Cleaned` folder.

## Notes
- This folder intentionally excludes large assets (UE5 content, compiled binaries) and workspace-specific files.
- The CMake expects `GLFW_DIR` and `GLM_DIR` set either via `CMake GUI` or `-D` flags on the command line.
- For zero-copy visualization with Direct3D12/UE5, refer to the main repo's `UE5_INTEGRATION_GUIDE.md`.

## Build example (powershell)
```powershell
cd "Final Cleaned"
./build.bat
```


