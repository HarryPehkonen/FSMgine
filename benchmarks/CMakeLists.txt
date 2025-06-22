# Determine which library to benchmark
if(NOT DEFINED BENCHMARK_LIBRARY)
    if(TEST_MULTITHREADED AND TARGET FSMgineMT)
        set(BENCHMARK_LIBRARY FSMgineMT)
    elseif(TARGET FSMgine)
        set(BENCHMARK_LIBRARY FSMgine)
    else()
        message(FATAL_ERROR "No suitable library target for benchmarking")
    endif()
endif()

# Try to find Google Benchmark
find_package(benchmark QUIET)

if(benchmark_FOUND)
    # Build Google Benchmark version
    add_executable(FSMgine_benchmarks
        bench_StringInterner.cpp
        bench_FSM.cpp
    )

    target_link_libraries(FSMgine_benchmarks
        ${BENCHMARK_LIBRARY}
        benchmark::benchmark
        benchmark::benchmark_main
    )

    # Add benchmark target
    add_custom_target(benchmark
        COMMAND FSMgine_benchmarks
        DEPENDS FSMgine_benchmarks
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    
    message(STATUS "Google Benchmark found - building full benchmark suite with ${BENCHMARK_LIBRARY}")
else()
    message(STATUS "Google Benchmark not found - building simple timer benchmark")
endif()

# Always build simple timer benchmark (no external deps)
add_executable(FSMgine_simple_benchmark
    simple_timer_benchmark.cpp
)

target_link_libraries(FSMgine_simple_benchmark ${BENCHMARK_LIBRARY})

# Add simple benchmark target
add_custom_target(simple_benchmark
    COMMAND FSMgine_simple_benchmark
    DEPENDS FSMgine_simple_benchmark
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
) 