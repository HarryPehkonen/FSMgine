# Try to find Google Benchmark
find_package(benchmark QUIET)

if(benchmark_FOUND)
    # Build Google Benchmark version
    add_executable(FSMgine_benchmarks
        bench_StringInterner.cpp
        bench_FSM.cpp
    )

    target_link_libraries(FSMgine_benchmarks
        FSMgine
        benchmark::benchmark
        benchmark::benchmark_main
    )

    if(FSMGINE_MULTI_THREADED)
        target_link_libraries(FSMgine_benchmarks Threads::Threads)
    endif()

    # Add benchmark target
    add_custom_target(benchmark
        COMMAND FSMgine_benchmarks
        DEPENDS FSMgine_benchmarks
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    
    message(STATUS "Google Benchmark found - building full benchmark suite")
else()
    message(STATUS "Google Benchmark not found - building simple timer benchmark")
endif()

# Always build simple timer benchmark (no external deps)
add_executable(FSMgine_simple_benchmark
    simple_timer_benchmark.cpp
)

target_link_libraries(FSMgine_simple_benchmark FSMgine)

if(FSMGINE_MULTI_THREADED)
    target_link_libraries(FSMgine_simple_benchmark Threads::Threads)
endif()

# Add simple benchmark target
add_custom_target(simple_benchmark
    COMMAND FSMgine_simple_benchmark
    DEPENDS FSMgine_simple_benchmark
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
) 