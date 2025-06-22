cmake_minimum_required(VERSION 3.20)
project(FSMgine VERSION 1.0.1 LANGUAGES CXX)

# Set C++ standard to C++17 for string_view support
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add include directory
include_directories(include)

# Define threading option
option(FSMGINE_BUILD_MULTITHREADED "Build multi-threaded version of the library" ON)
option(FSMGINE_BUILD_SINGLETHREADED "Build single-threaded version of the library" ON)

# Ensure at least one version is built
if(NOT FSMGINE_BUILD_MULTITHREADED AND NOT FSMGINE_BUILD_SINGLETHREADED)
    message(FATAL_ERROR "At least one version of the library must be built. Enable either FSMGINE_BUILD_MULTITHREADED or FSMGINE_BUILD_SINGLETHREADED")
endif()

# Find threads if needed
if(FSMGINE_BUILD_MULTITHREADED)
    find_package(Threads REQUIRED)
endif()

# Function to create FSMgine library target
function(create_fsmgine_target TARGET_NAME MULTI_THREADED)
    add_library(${TARGET_NAME}
        src/StringInterner.cpp
    )
    
    # Set library properties
    set_target_properties(${TARGET_NAME} PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        EXPORT_NAME ${TARGET_NAME}
    )
    
    target_include_directories(${TARGET_NAME}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    
    if(${MULTI_THREADED})
        target_compile_definitions(${TARGET_NAME} PUBLIC FSMGINE_MULTI_THREADED)
        target_link_libraries(${TARGET_NAME} PUBLIC Threads::Threads)
    endif()
    
    # Add compiler warnings
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic)
    elseif(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /W4)
    endif()
endfunction()

# Create single-threaded library
if(FSMGINE_BUILD_SINGLETHREADED)
    create_fsmgine_target(FSMgine FALSE)
endif()

# Create multi-threaded library
if(FSMGINE_BUILD_MULTITHREADED)
    create_fsmgine_target(FSMgineMT TRUE)
endif()

# Testing support
option(BUILD_TESTING "Build tests" ON)
option(TEST_MULTITHREADED "Run tests with multi-threaded library" ${FSMGINE_BUILD_MULTITHREADED})

if(BUILD_TESTING)
    enable_testing()
    find_package(GTest QUIET)
    
    # Determine which library to test
    if(TEST_MULTITHREADED AND TARGET FSMgineMT)
        set(TEST_LIBRARY FSMgineMT)
    elseif(TARGET FSMgine)
        set(TEST_LIBRARY FSMgine)
    else()
        message(FATAL_ERROR "No suitable library target for testing")
    endif()

    if(GTest_FOUND)
        add_subdirectory(tests)
    else()
        message(STATUS "Google Test not found. Building simple test runner instead.")
        # Simple test runner
        add_executable(FSMgine_simple_tests tests/simple_test_runner.cpp)
        target_link_libraries(FSMgine_simple_tests ${TEST_LIBRARY})
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
option(EXAMPLES_USE_MULTITHREADED "Build examples with multi-threaded library" ${FSMGINE_BUILD_MULTITHREADED})

if(BUILD_EXAMPLES)
    # Determine which library to use for examples
    if(EXAMPLES_USE_MULTITHREADED AND TARGET FSMgineMT)
        set(EXAMPLE_LIBRARY FSMgineMT)
    elseif(TARGET FSMgine)
        set(EXAMPLE_LIBRARY FSMgine)
    else()
        message(FATAL_ERROR "No suitable library target for examples")
    endif()

    add_executable(turnstile_example examples/turnstile_example.cpp)
    target_link_libraries(turnstile_example ${EXAMPLE_LIBRARY})
    add_executable(other_class_instance examples/other_class_instance.cpp)
    target_link_libraries(other_class_instance ${EXAMPLE_LIBRARY})
    add_executable(other_class_eventful examples/other_class_eventful.cpp)
    target_link_libraries(other_class_eventful ${EXAMPLE_LIBRARY})
    add_executable(no_class_functional examples/no_class_functional.cpp)
    target_link_libraries(no_class_functional ${EXAMPLE_LIBRARY})
    add_executable(parentheses_checker examples/parentheses_checker.cpp)
    target_link_libraries(parentheses_checker ${EXAMPLE_LIBRARY})
    add_executable(calculator examples/calculator.cpp)
    target_link_libraries(calculator ${EXAMPLE_LIBRARY})
    add_executable(protocol_parser examples/protocol_parser.cpp)
    target_link_libraries(protocol_parser ${EXAMPLE_LIBRARY})
    add_executable(resource_pool_example examples/resource_pool_example.cpp)
    target_link_libraries(resource_pool_example ${EXAMPLE_LIBRARY})
endif()

# Installation rules
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install headers (shared by both libraries)
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

# Function to install a library target with its config
function(install_fsmgine_target TARGET_NAME)
    # Install library
    install(TARGETS ${TARGET_NAME}
        EXPORT ${TARGET_NAME}Targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    
    # Install CMake package configuration files
    install(EXPORT ${TARGET_NAME}Targets
        FILE ${TARGET_NAME}Targets.cmake
        NAMESPACE FSMgine::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_NAME}
    )
    
    # Create version file
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}ConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    
    # Configure config file
    if(${TARGET_NAME} STREQUAL "FSMgineMT")
        set(FSMGINE_IS_MULTITHREADED TRUE)
    else()
        set(FSMGINE_IS_MULTITHREADED FALSE)
    endif()
    
    # Set the target name for use in the template
    set(FSMGINE_TARGET_NAME ${TARGET_NAME})
    
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FSMgineConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}Config.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_NAME}
    )
    
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}ConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_NAME}
    )
endfunction()

# Install each library if it was built
if(TARGET FSMgine)
    install_fsmgine_target(FSMgine)
endif()

if(TARGET FSMgineMT)
    install_fsmgine_target(FSMgineMT)
endif()
