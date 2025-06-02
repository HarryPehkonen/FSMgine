cmake_minimum_required(VERSION 3.20)
project(fsmgine)

# Set C++ standard to C++20 since we're using ranges
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create executable with both source files
add_executable(fsmgine 
    src/fsmgine.cpp
    src/main.cpp
)

target_include_directories(fsmgine PRIVATE include)