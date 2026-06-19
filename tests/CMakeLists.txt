include(Catch)

add_executable(crystal_tests
    test_viewport.cpp
    test_state_stack.cpp
    test_input_map.cpp
    test_paths.cpp
)

target_link_libraries(crystal_tests PRIVATE crystal_core Catch2::Catch2WithMain)
set_project_warnings(crystal_tests)

# Registers each TEST_CASE with CTest.
catch_discover_tests(crystal_tests)
