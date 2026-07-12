cmake_minimum_required(VERSION 3.20)

project(raytracer LANGUAGES CXX)

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
)

target_compile_features(raytracer PRIVATE cxx_std_20)

target_include_directories(raytracer PRIVATE include ${stb_SOURCE_DIR})