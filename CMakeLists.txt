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
    src/states/StateStack.cpp
    src/states/TitleState.cpp
    src/states/MenuOverlayState.cpp
    src/input/InputMap.cpp
    src/input/RaylibInputQuery.cpp
    src/resource/ResourceManager.cpp
    src/platform/Paths.cpp
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
