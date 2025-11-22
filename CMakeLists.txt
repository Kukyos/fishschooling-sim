cmake_minimum_required(VERSION 3.18)
project(FishCUDAVisualizer LANGUAGES C CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 14)
set(CMAKE_CUDA_ARCHITECTURES 75 86 89)

# Find packages
find_package(OpenGL REQUIRED)
find_package(CUDA REQUIRED)

# GLFW - adjust path if needed
set(GLFW_DIR "C:/Program Files/GLFW" CACHE PATH "GLFW installation directory")
find_library(GLFW_LIB NAMES glfw3 glfw PATHS "${GLFW_DIR}/lib-vc2022" "${GLFW_DIR}/lib" REQUIRED)
find_path(GLFW_INCLUDE NAMES GLFW/glfw3.h PATHS "${GLFW_DIR}/include" REQUIRED)

# GLM - header-only
set(GLM_DIR "C:/Program Files/GLM" CACHE PATH "GLM installation directory")
find_path(GLM_INCLUDE NAMES glm/glm.hpp PATHS "${GLM_DIR}/include" "${GLM_DIR}" REQUIRED)

# ImGui - we'll include source files directly
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui" CACHE PATH "ImGui source directory")

# GLAD - we'll include it in src/
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GLFW_INCLUDE}
    ${GLM_INCLUDE}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CUDA_INCLUDE_DIRS}
)

# ImGui sources
set(IMGUI_SOURCES "")
if(EXISTS ${IMGUI_DIR})
    set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )
endif()
if(NOT EXISTS ${IMGUI_DIR})
    message(WARNING "ImGui not found under ${IMGUI_DIR}. Please run setup.bat or clone ImGui into external/imgui.")
endif()

# Main executable
set(SRC_FILES
    src/main.cpp
    src/cuda_kernels.cu
)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/glad.c)
    list(APPEND SRC_FILES src/glad.c)
endif()

add_executable(fish_vis
    ${SRC_FILES}
    ${IMGUI_SOURCES}
)

# Explicitly set language for glad.c
set_source_files_properties(src/glad.c PROPERTIES LANGUAGE C)

# Set CUDA flags
set_target_properties(fish_vis PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_RESOLVE_DEVICE_SYMBOLS ON
)

target_compile_options(fish_vis PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/wd4819>
)

# Link libraries
target_link_libraries(fish_vis PRIVATE
    ${GLFW_LIB}
    OpenGL::GL
    ${CUDA_LIBRARIES}
    ${CUDA_CUDA_LIBRARY}
)

# Copy shaders to build directory if they exist
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

message(STATUS "CUDA Version: ${CUDA_VERSION}")
message(STATUS "GLFW Include: ${GLFW_INCLUDE}")
message(STATUS "GLM Include: ${GLM_INCLUDE}")
