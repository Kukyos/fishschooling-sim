# Quick Start Guide - CUDA Fish Visualizer

## Fast Setup (15 minutes)

### Step 1: Download Dependencies (5 min)

1. **GLFW** (precompiled):
   - Go to: https://www.glfw.org/download.html
   - Download: "64-bit Windows binaries"
   - Extract to: `C:\Program Files\GLFW\`
   - Verify structure:
     ```
     C:\Program Files\GLFW\
     ├── include\GLFW\
     └── lib-vc2019\glfw3.lib
     ```

2. **GLM** (header-only):
   - Go to: https://github.com/g-truc/glm/releases/latest
   - Download: `glm-X.X.X.zip`
   - Extract to: `C:\Program Files\GLM\`
   - Verify: `C:\Program Files\GLM\glm\glm.hpp` exists

3. **GLAD** (generate):
   - Go to: https://glad.dav1d.de/
   - Settings:
     - Language: **C/C++**
     - Specification: **OpenGL**
     - gl: **Version 4.5** (or higher)
     - Profile: **Core**
     - Check: **Generate a loader**
   - Click **GENERATE**
   - Download `glad.zip`
   - Extract contents to `src/`:
     ```
     src/
     ├── glad.c
     ├── glad/
     │   └── glad.h
     └── KHR/
         └── khrplatform.h
     ```

### Step 2: Download ImGui (1 min)

```powershell
cd C:\Users\Cleo\Desktop\SHITIBUILT\fishschooling-sim\Final Cleaned
git clone --depth 1 https://github.com/ocornut/imgui.git external/imgui
```

### Step 3: Build (5 min)

```powershell
cd C:\Users\Cleo\Desktop\SHITIBUILT\fishschooling-sim\Final Cleaned

# Option 1: Use build script
./build.bat

# Option 2: Manual build
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release -j 8
```

If CMake can't find GLFW/GLM, specify paths:
```powershell
cmake .. -DGLFW_DIR="C:/Program Files/GLFW" -DGLM_DIR="C:/Program Files/GLM"
```

### Step 4: Run (instant)

```powershell
cd build\Release
./fish_vis.exe
```

You should see:
- Window with 10,000 fish swimming
- FPS counter (should be 200+ on RTX 4060)
- UI controls for parameters

## Controls

- **Right Mouse Button**: Drag to rotate camera
- **Mouse Wheel**: Zoom in/out
- **Sliders**: Adjust boid behavior in real-time
- **ESC**: Close application

## Troubleshooting

### "nvcc not found"
- Add CUDA to PATH: `C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6\bin`

### "Cannot find GLFW"
- Verify installation: `dir "C:\Program Files\GLFW\lib-vc2019\"`
- Set path manually: `cmake .. -DGLFW_DIR="C:/full/path/to/GLFW"`

### "Cannot find GLM"
- Verify: `dir "C:\Program Files\GLM\glm\glm.hpp"`
- Set path: `cmake .. -DGLM_DIR="C:/full/path/to/GLM"`

### "GLAD not found"
- Ensure you copied ALL files from glad.zip:
  - `glad.c` → `src/glad.c`
  - `glad/` folder → `src/glad/`
  - `KHR/` folder → `src/KHR/`

### "ImGui not found"
- Run: `git clone https://github.com/ocornut/imgui.git external/imgui`
- Verify: `dir external\imgui\imgui.cpp`

### Low FPS / Integrated GPU
- Right-click `fish_vis.exe` → Run with graphics processor → High-performance NVIDIA processor
- Or set in NVIDIA Control Panel globally

## Performance Test

Test different agent counts by editing `main.cpp`:
```cpp
int g_agentCount = 1000;   // Fast: 600+ FPS
int g_agentCount = 10000;  // Default: 200+ FPS
int g_agentCount = 50000;  // Stress: 60+ FPS
```

Rebuild and run after changes.

## Next Steps

1. **Adjust camera**: Change `g_cameraDistance` in `main.cpp` for different views
2. **Modify colors**: Edit fragment shader in `main.cpp`
3. **Add predator**: Add a large fast-moving agent in `cuda_kernels.cu`
4. **Record video**: Use OBS Studio or NVIDIA ShadowPlay at 1440p 60fps

## Support

If you hit issues:
1. Check all paths are correct
2. Verify CUDA 12.x is installed: `nvcc --version`
3. Verify Visual Studio 2019+ is installed
4. Try CMake GUI for visual path configuration
5. Check build output for specific error messages
