cmake_minimum_required(VERSION 3.20)

# Release-candidate version (owner decision 2026-07-20): 0.9.0 until the
# M23 external playtests pass; then 1.0.0. This is the single source —
# Version.hpp and the Windows .rc are configured from it.
project(CrystalDungeons
        VERSION 0.9.0
        DESCRIPTION "16-bit-inspired turn-based JRPG roguelite"
        LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Standard / global settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to a Debug build for single-config generators.
get_property(_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _is_multi_config AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# Put all binaries in predictable locations.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Version header generated from project(VERSION) — include as
# "core/Version.hpp" (the generated dir is on crystal_core's include path).
configure_file(packaging/Version.hpp.in
               "${CMAKE_BINARY_DIR}/generated/core/Version.hpp" @ONLY)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(CRYSTAL_BUILD_TESTS         "Build Catch2 unit tests"            ON)
option(CRYSTAL_WARNINGS_AS_ERRORS  "Treat project warnings as errors"   OFF)
option(CRYSTAL_ENABLE_DEBUG_OVERLAY "Compile the runtime debug overlay" ON)
# M23 capture CLI (--capture): dev builds only; Release never compiles it
# even when the option is ON.
option(CRYSTAL_ENABLE_CAPTURE      "Compile the deterministic capture CLI" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Dependencies)

# ---------------------------------------------------------------------------
# Game logic library (everything except main(); lets tests link it)
# ---------------------------------------------------------------------------
add_library(crystal_core STATIC
    src/core/Application.cpp
    src/core/Log.cpp
    src/render/Viewport.cpp
    src/render/VirtualScreen.cpp
    src/input/Input.cpp
    src/input/InputMap.cpp
    src/input/PromptLabels.cpp
    src/input/RaylibInputQuery.cpp
    src/input/Remap.cpp
    src/assets/AssetManifest.cpp
    src/settings/Settings.cpp
    src/states/RemapState.cpp
    src/states/SettingsState.cpp
    src/resource/ResourceManager.cpp
    src/platform/Paths.cpp
    src/platform/AtomicFile.cpp
    src/platform/FatalDialog.cpp
    src/content/Enums.cpp
    src/content/LoadReport.cpp
    src/content/JsonValidation.cpp
    src/content/ContentDatabase.cpp
    src/content/ContentLoader.cpp
    src/game/Party.cpp
    src/game/Inventory.cpp
    src/game/BossDrops.cpp
    src/game/Castle.cpp
    src/save/SaveSystem.cpp
    src/battle/Battle.cpp
    src/battle/Simulator.cpp
    src/audio/AudioManager.cpp
    src/ui/Menu.cpp
    src/ui/TextInput.cpp
    src/ui/TextLayout.cpp
    src/ui/UiDraw.cpp
    src/ui/UiStyle.cpp
    src/tutorial/Tutorial.cpp
    src/capture/CaptureRunner.cpp
    src/town/Tilemap.cpp
    src/town/Movement.cpp
    src/town/TownData.cpp
    src/dungeon/DungeonModel.cpp
    src/dungeon/DungeonGenerator.cpp
    src/dungeon/RoomLayout.cpp
    src/render/SpriteDraw.cpp
    src/render/BattleSequencer.cpp
    src/danger/DangerRating.cpp
    src/score/Scoring.cpp
    src/score/Scoreboard.cpp
    src/states/StateStack.cpp
    src/states/MainMenuState.cpp
    src/states/PartyCreationState.cpp
    src/states/SlotMenuState.cpp
    src/states/TownState.cpp
    src/states/TownMenuState.cpp
    src/states/InnState.cpp
    src/states/ItemShopState.cpp
    src/states/TrainingHallState.cpp
    src/states/BlackMarketState.cpp
    src/states/CastleState.cpp
    src/states/CastleChallengeState.cpp
    src/states/GuildState.cpp
    src/states/DungeonState.cpp
    src/states/DungeonMenuState.cpp
    src/states/BattleState.cpp
    src/states/DungeonResultState.cpp
    src/states/ScoreboardState.cpp
    src/states/StoryDialogState.cpp
    src/states/EquipShopState.cpp
    src/states/HelpState.cpp
    src/states/TutorialPromptState.cpp
    src/states/DetailsOverlayState.cpp
)

target_include_directories(crystal_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src"
                                               "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(crystal_core PUBLIC raylib nlohmann_json::nlohmann_json)
set_project_warnings(crystal_core)

# NOTE: do not add an explicit `user32` link here or on CrystalDungeons.
# MessageBox (src/platform/FatalDialog.cpp) resolves from user32.lib in MSVC's
# default standard libraries, which the linker places *after* raylib.lib.
# Naming user32 explicitly moves it ahead of raylib and breaks the link with
# "LNK2005: CloseWindow already defined in user32.lib" -- raylib's rcore.c
# defines its own CloseWindow.

# Configuration-aware guards are mandatory: CMAKE_BUILD_TYPE is empty for
# multi-configuration generators such as Visual Studio.
target_compile_definitions(crystal_core PUBLIC
    $<$<CONFIG:Release>:CRYSTAL_SHIPPING_BUILD>
    $<$<AND:$<BOOL:${CRYSTAL_ENABLE_DEBUG_OVERLAY}>,$<NOT:$<CONFIG:Release>>>:CRYSTAL_DEBUG_OVERLAY>
    $<$<AND:$<BOOL:${CRYSTAL_ENABLE_CAPTURE}>,$<NOT:$<CONFIG:Release>>>:CRYSTAL_CAPTURE>
)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(CrystalDungeons src/main.cpp)
target_link_libraries(CrystalDungeons PRIVATE crystal_core)
set_project_warnings(CrystalDungeons)

# Windows icon + VERSIONINFO (M24): the .rc is configured from the project
# version; the .ico is generated by tools/asset_gen/generate_icon.ps1.
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/crystal.ico")
    set(CRYSTAL_ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/packaging/crystal.ico")
    configure_file(packaging/CrystalDungeons.rc.in
                   "${CMAKE_BINARY_DIR}/generated/CrystalDungeons.rc" @ONLY)
    target_sources(CrystalDungeons PRIVATE
                   "${CMAKE_BINARY_DIR}/generated/CrystalDungeons.rc")
endif()

# On Windows, build a GUI app (no console window) for Release; keep the console
# in Debug. The GUI subsystem normally expects WinMain, so point the entry point
# back at the standard main() we provide.
if(WIN32)
    set_target_properties(CrystalDungeons PROPERTIES
        WIN32_EXECUTABLE $<CONFIG:Release>)
    if(MSVC)
        target_link_options(CrystalDungeons PRIVATE
            $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>)
    endif()
endif()

# Make JSON content and the asset tree available next to the executable.
add_custom_command(TARGET CrystalDungeons POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/data"
            "$<TARGET_FILE_DIR:CrystalDungeons>/data"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:CrystalDungeons>/assets"
    COMMENT "Copying data/ and assets/ next to CrystalDungeons.exe")

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(CRYSTAL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "CrystalDungeons configured: tests=${CRYSTAL_BUILD_TESTS} "
               "warnings_as_errors=${CRYSTAL_WARNINGS_AS_ERRORS} "
               "debug_overlay=${CRYSTAL_ENABLE_DEBUG_OVERLAY}")
