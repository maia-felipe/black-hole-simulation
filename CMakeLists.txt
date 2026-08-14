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

add_executable(raytracer
    src/main.cpp
    src/vec3.cpp
    src/ray.cpp
    src/stb_image_write_implementation.cpp
    src/image.cpp
    src/camera.cpp
    src/geodesic.cpp
    src/disk.cpp
)

target_compile_features(raytracer PRIVATE cxx_std_20)

target_include_directories(raytracer PRIVATE include ${stb_SOURCE_DIR})