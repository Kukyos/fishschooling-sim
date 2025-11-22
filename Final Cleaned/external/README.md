This folder should contain external dependencies required to build the project.

- ImGui: 'git clone https://github.com/ocornut/imgui.git external/imgui'
- GLFW: Install or set path in CMake (GLFW_DIR)
- GLM: Install or set path in CMake (GLM_DIR)
- GLAD: Generate a glad loader and copy `glad.c`, `glad/glad.h`, and `KHR/khrplatform.h` into `src/`

For convenience, include ImGui as a submodule in GitHub: 
`git submodule add https://github.com/ocornut/imgui.git external/imgui`
