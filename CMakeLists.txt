cmake_minimum_required(VERSION 3.20)

project(test LANGUAGES CXX)

add_executable(vec3test
    src/main.cpp
    include/vec3.h
    src/vec3.cpp
)

target_compile_features(vec3test PRIVATE cxx_std_20)

target_include_directories(vec3test PRIVATE include)