cmake_minimum_required(VERSION 3.20)

project(test LANGUAGES CXX)

add_executable(vec3test
    src/main.cpp
    src/vec3.cpp
    src/ray.cpp
)

target_compile_features(vec3test PRIVATE cxx_std_20)

target_include_directories(vec3test PRIVATE include)