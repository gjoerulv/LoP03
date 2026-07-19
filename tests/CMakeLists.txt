include(Catch)

add_executable(crystal_tests
    test_viewport.cpp
    test_state_stack.cpp
    test_input_map.cpp
    test_paths.cpp
    test_content_validation.cpp
    test_content_loader.cpp
    test_menu.cpp
    test_text_input.cpp
    test_text_layout.cpp
    test_scroll_window.cpp
    test_party.cpp
    test_save_system.cpp
    test_town.cpp
    test_dungeon.cpp
    test_inventory.cpp
    test_battle.cpp
    test_danger.cpp
    test_scoring.cpp
    test_scoreboard.cpp
    test_status.cpp
    test_equipment.cpp
    test_fade.cpp
    test_balance.cpp
    test_leveling.cpp
)

target_link_libraries(crystal_tests PRIVATE crystal_core Catch2::Catch2WithMain)
set_project_warnings(crystal_tests)

# Absolute path to the shipped content, so tests can validate the real JSON.
target_compile_definitions(crystal_tests PRIVATE
    CRYSTAL_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/data")

# Registers each TEST_CASE with CTest.
catch_discover_tests(crystal_tests)
