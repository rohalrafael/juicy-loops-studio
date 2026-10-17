cmake_minimum_required(VERSION 3.24)

project(JuicyLoopsStudio VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(JLS_BUILD_APP "Build the GUI application (pulls in JUCE)" ON)
option(JLS_BUILD_TESTS "Build unit tests" ON)

include(FetchContent)

# JSON for theme and project files (header-only).
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa)
FetchContent_MakeAvailable(nlohmann_json)

# Engine, model and theme: plain C++, no JUCE or GUI dependency, so they stay unit-testable.
add_subdirectory(src/engine)
add_subdirectory(src/model)
add_subdirectory(src/theme)

if(JLS_BUILD_APP)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.4
        GIT_SHALLOW TRUE
        # Our fixes to JUCE (see patches/README.md).
        PATCH_COMMAND ${CMAKE_COMMAND}
            -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/patches/juce-xembed-use-after-free.patch
            -DSOURCE_DIR=<SOURCE_DIR>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_patch.cmake)
    FetchContent_MakeAvailable(JUCE)
    add_subdirectory(src/app)
endif()

if(JLS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
