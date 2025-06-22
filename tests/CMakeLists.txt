add_executable(FSMgine_tests
    test_StringInterner.cpp
    test_Transition.cpp
    test_FSM.cpp
    test_Integration.cpp
)

target_link_libraries(FSMgine_tests
    ${TEST_LIBRARY}
    GTest::gtest
    GTest::gtest_main
)

# Register tests with CTest
add_test(NAME FSMgine_unit_tests COMMAND FSMgine_tests)