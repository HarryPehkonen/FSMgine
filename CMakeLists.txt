cmake_minimum_required(VERSION 3.20)
project(fsmgine)

# Set C++ standard to C++17 since we're using string_view
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add include directory
include_directories(include)

# Create executable with all source files
add_executable(fsmgine 
    src/main.cpp
    src/FSMgineCore.cpp
    src/DSLRules.cpp
    src/CodeGenerator.cpp
)

# Add compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(fsmgine PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(fsmgine PRIVATE /W4)
endif()

# Installation rules
install(TARGETS fsmgine
    RUNTIME DESTINATION bin
)