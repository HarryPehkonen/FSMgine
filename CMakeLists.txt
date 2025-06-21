cmake_minimum_required(VERSION 3.20)
project(FSMgine VERSION 1.0.1 LANGUAGES CXX)

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
# FSMgine is now mostly header-only, but StringInterner has a .cpp file.
add_library(FSMgine
    src/StringInterner.cpp
)

# Set library properties
set_target_properties(FSMgine PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME FSMgine
    PUBLIC_HEADER "include/FSMgine/FSMgine.hpp"
)

target_include_directories(FSMgine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

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
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
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
endif()

# Benchmarking support
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
    find_package(benchmark QUIET)
    if(NOT benchmark_FOUND)
        message(STATUS "Google Benchmark not found. Simple timer benchmark will be built instead.")
        message(STATUS "For full benchmark suite, install: sudo apt-get install libbenchmark-dev (Ubuntu/Debian)")
        message(STATUS "Or build from source: https://github.com/google/benchmark")
    endif()
endif()

# Documentation support
option(BUILD_DOCUMENTATION "Build documentation" OFF)
if(BUILD_DOCUMENTATION)
    find_package(Doxygen QUIET)
    if(DOXYGEN_FOUND)
        set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
        set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
        
        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
        
        add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
        
        message(STATUS "Doxygen found. You can build documentation with 'make docs'")
    else()
        message(STATUS "Doxygen not found. Documentation target will not be available.")
    endif()
endif()

# Example executables (optional)
option(BUILD_EXAMPLES "Build example programs" OFF)

if(BUILD_EXAMPLES)
    add_executable(turnstile_example examples/turnstile_example.cpp)
    target_link_libraries(turnstile_example FSMgine)
    add_executable(other_class_instance examples/other_class_instance.cpp)
    target_link_libraries(other_class_instance FSMgine)
    add_executable(other_class_eventful examples/other_class_eventful.cpp)
    target_link_libraries(other_class_eventful FSMgine)
    add_executable(no_class_functional examples/no_class_functional.cpp)
    target_link_libraries(no_class_functional FSMgine)
    add_executable(parentheses_checker examples/parentheses_checker.cpp)
    target_link_libraries(parentheses_checker FSMgine)
    add_executable(calculator examples/calculator.cpp)
    target_link_libraries(calculator FSMgine)
    add_executable(protocol_parser examples/protocol_parser.cpp)
    target_link_libraries(protocol_parser FSMgine)
    add_executable(resource_pool_example examples/resource_pool_example.cpp)
    target_link_libraries(resource_pool_example FSMgine)
endif()

# Installation rules
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install library and headers
install(TARGETS FSMgine
    EXPORT FSMgineTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

# Install CMake package configuration files
install(EXPORT FSMgineTargets
    FILE FSMgineTargets.cmake
    NAMESPACE FSMgine::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FSMgine
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/FSMgineConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FSMgineConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/FSMgineConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FSMgine
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/FSMgineConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/FSMgineConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FSMgine
)
