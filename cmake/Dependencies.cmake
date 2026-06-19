# Third-party dependencies, pinned via FetchContent.
# Do NOT float to master/main. Bumping a tag requires human approval.

include(FetchContent)

# Show download/build progress on the first configure.
set(FETCHCONTENT_QUIET OFF)

# --- raylib 6.0 (tags have no "v" prefix) -----------------------------------
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)   # don't build raylib's examples
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        6.0
    GIT_SHALLOW    TRUE
)

# --- nlohmann/json v3.12.0 --------------------------------------------------
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.12.0
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(raylib nlohmann_json)

# --- Catch2 v3.15.1 (only when building tests) ------------------------------
if(CRYSTAL_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.15.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    # Make catch_discover_tests() available to tests/CMakeLists.txt.
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
endif()
