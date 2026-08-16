cmake_minimum_required(VERSION 3.20)

project(raytracer LANGUAGES CXX)

# Without this the default build has no optimization at all, which for a ray tracer
# is roughly a 10x penalty. Still overridable with -DCMAKE_BUILD_TYPE=Debug.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 31c1ad3
)

FetchContent_MakeAvailable(stb)

# GLFW brings its own tests, examples and docs; none of them are wanted here, and
# building them would dominate a clean configure.
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 7b6aead9fb88b3623e3b3725ebb42670cbe4c579  # 3.4
)

FetchContent_MakeAvailable(glfw)

# Everything except the entry points, so the offline renderer and the interactive
# viewer trace with byte-identical code. PUBLIC on the include directories and the
# language standard makes them usage requirements: anything linking this target
# inherits them instead of repeating them.
add_library(blackhole STATIC
    src/vec3.cpp
    src/ray.cpp
    src/camera.cpp
    src/image.cpp
    src/geodesic.cpp
    src/disk.cpp
    src/sky.cpp
    src/config.cpp
    src/renderer.cpp
    src/stb_image_write_implementation.cpp
    src/stb_image_implementation.cpp
)

target_compile_features(blackhole PUBLIC cxx_std_20)
target_include_directories(blackhole PUBLIC include ${stb_SOURCE_DIR})

# Offline renderer: writes a PNG. Also the ground-truth oracle for the viewer.
add_executable(raytracer src/main.cpp)
target_link_libraries(raytracer PRIVATE blackhole)

# Interactive viewer. Only this target depends on GLFW and OpenGL; macOS ships the
# OpenGL headers in a framework, so no loader library is needed.
add_executable(viewer src/viewer.cpp)
target_link_libraries(viewer PRIVATE blackhole glfw "-framework OpenGL")
