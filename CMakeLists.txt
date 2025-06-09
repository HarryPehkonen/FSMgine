cmake_minimum_required(VERSION 3.20)
project(FSMgine VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard to C++17 for string_view support
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add include directory
include_directories(include)

# Define threading option
option(FSMGINE_MULTI_THREADED "Enable multi-threaded support" OFF)

# Configure threading
if(FSMGINE_MULTI_THREADED)
    find_package(Threads REQUIRED)
    add_compile_definitions(FSMGINE_MULTI_THREADED)
endif()

# Create FSMgine library
add_library(FSMgine STATIC
    src/StringInterner.cpp
    src/Transition.cpp
    src/FSM.cpp
    src/FSMBuilder.cpp
)

target_include_directories(FSMgine PUBLIC include)

if(FSMGINE_MULTI_THREADED)
    target_link_libraries(FSMgine PUBLIC Threads::Threads)
endif()

# Add compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(FSMgine PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(FSMgine PRIVATE /W4)
endif()

# Testing support
enable_testing()
find_package(GTest QUIET)

if(GTest_FOUND)
    add_subdirectory(tests)
else()
    message(STATUS "Google Test not found. Building simple test runner instead.")
    # Simple test runner
    add_executable(FSMgine_simple_tests tests/simple_test_runner.cpp)
    target_link_libraries(FSMgine_simple_tests FSMgine)
    add_test(NAME FSMgine_simple_tests COMMAND FSMgine_simple_tests)
endif()

# Example executables (optional)
option(BUILD_EXAMPLES "Build example programs" OFF)

if(BUILD_EXAMPLES)
    add_executable(turnstile_example examples/turnstile_example.cpp)
    target_link_libraries(turnstile_example FSMgine)
endif()

# Installation rules
install(TARGETS FSMgine
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)