add_executable(FSMgine_tests
    test_StringInterner.cpp
    test_Transition.cpp
    test_FSM.cpp
    test_Integration.cpp
)

target_link_libraries(FSMgine_tests
    FSMgine
    GTest::gtest
    GTest::gtest_main
)

if(FSMGINE_MULTI_THREADED)
    target_link_libraries(FSMgine_tests Threads::Threads)
endif()

# Register tests with CTest
add_test(NAME FSMgine_unit_tests COMMAND FSMgine_tests)