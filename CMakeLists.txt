cmake_minimum_required(VERSION 3.20)

project(BlackHoleSim LANGUAGES CXX)

add_executable(blackhole
    src/main.cpp
    src/greeting.cpp    
)

target_compile_features(blackhole PRIVATE cxx_std_20)

target_include_directories(blackhole PRIVATE include)