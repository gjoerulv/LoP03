include(Catch)

add_executable(crystal_tests
    test_viewport.cpp
    test_state_stack.cpp
    test_input_map.cpp
    test_input_frame.cpp
    test_remap.cpp
    test_prompt_labels.cpp
    test_settings.cpp
    test_asset_manifest.cpp
    test_animation.cpp
    test_audio.cpp
    test_tutorial.cpp
    test_accessibility.cpp
    test_presentation_lint.cpp
    test_paths.cpp
    test_atomic_file.cpp
    test_fatal_dialog.cpp
    test_content_validation.cpp
    test_content_loader.cpp
    test_learnset.cpp
    test_rest.cpp
    test_menu.cpp
    test_text_input.cpp
    test_text_layout.cpp
    test_scroll_window.cpp
    test_party.cpp
    test_save_system.cpp
    test_town.cpp
    test_town_ladder.cpp
    test_stakes_ladder.cpp
    test_black_market.cpp
    test_boss_drops.cpp
    test_dungeon.cpp
    test_room_layout.cpp
    test_inventory.cpp
    test_battle.cpp
    test_enmity.cpp
    test_battle_sequencer.cpp
    test_boss_mechanics.cpp
    test_danger.cpp
    test_scoring.cpp
    test_scoreboard.cpp
    test_status.cpp
    test_status_v2.cpp
    test_passives.cpp
    test_equipment.cpp
    test_equip_shop_filter.cpp
    test_fade.cpp
    test_balance.cpp
    test_economy.cpp
    test_composition.cpp
    test_events.cpp
    test_leveling.cpp
)

target_link_libraries(crystal_tests PRIVATE crystal_core Catch2::Catch2WithMain)
set_project_warnings(crystal_tests)

# Absolute paths to the shipped content/assets, so tests validate the real files.
target_compile_definitions(crystal_tests PRIVATE
    CRYSTAL_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/data"
    CRYSTAL_TEST_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")

# Registers each TEST_CASE with CTest.
catch_discover_tests(crystal_tests)
