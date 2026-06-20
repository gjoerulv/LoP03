cmake_minimum_required(VERSION 3.20)

project(CrystalDungeons
        VERSION 0.1.0
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

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(CRYSTAL_BUILD_TESTS         "Build Catch2 unit tests"            ON)
option(CRYSTAL_WARNINGS_AS_ERRORS  "Treat project warnings as errors"   OFF)
option(CRYSTAL_ENABLE_DEBUG_OVERLAY "Compile the runtime debug overlay" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Dependencies)

# ---------------------------------------------------------------------------
# Game logic library (everything except main(); lets tests link it)
# ---------------------------------------------------------------------------
add_library(crystal_core STATIC
    src/core/Application.cpp
    src/render/Viewport.cpp
    src/render/VirtualScreen.cpp
    src/input/InputMap.cpp
    src/input/RaylibInputQuery.cpp
    src/resource/ResourceManager.cpp
    src/platform/Paths.cpp
    src/content/Enums.cpp
    src/content/LoadReport.cpp
    src/content/JsonValidation.cpp
    src/content/ContentDatabase.cpp
    src/content/ContentLoader.cpp
    src/game/Party.cpp
    src/game/Inventory.cpp
    src/save/SaveSystem.cpp
    src/battle/Battle.cpp
    src/ui/Menu.cpp
    src/ui/TextInput.cpp
    src/ui/UiDraw.cpp
    src/town/Tilemap.cpp
    src/town/Movement.cpp
    src/town/TownData.cpp
    src/dungeon/DungeonModel.cpp
    src/dungeon/DungeonGenerator.cpp
    src/states/StateStack.cpp
    src/states/MainMenuState.cpp
    src/states/PartyCreationState.cpp
    src/states/SlotMenuState.cpp
    src/states/TownState.cpp
    src/states/TownMenuState.cpp
    src/states/InnState.cpp
    src/states/PlaceholderLocationState.cpp
    src/states/GuildState.cpp
    src/states/DungeonState.cpp
    src/states/DungeonMenuState.cpp
    src/states/BattleState.cpp
)

target_include_directories(crystal_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(crystal_core PUBLIC raylib nlohmann_json::nlohmann_json)
set_project_warnings(crystal_core)

if(CRYSTAL_ENABLE_DEBUG_OVERLAY)
    target_compile_definitions(crystal_core PUBLIC CRYSTAL_DEBUG_OVERLAY)
endif()

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(CrystalDungeons src/main.cpp)
target_link_libraries(CrystalDungeons PRIVATE crystal_core)
set_project_warnings(CrystalDungeons)

# On Windows, build a GUI app (no console) for Release; keep console in Debug.
if(WIN32)
    set_target_properties(CrystalDungeons PROPERTIES
        WIN32_EXECUTABLE $<CONFIG:Release>)
endif()

# Make JSON content available next to the executable at runtime.
add_custom_command(TARGET CrystalDungeons POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/data"
            "$<TARGET_FILE_DIR:CrystalDungeons>/data"
    COMMENT "Copying data/ next to CrystalDungeons.exe")

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
