cmake_minimum_required(VERSION 3.24)

project(JuicyLoopsStudio VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(JLS_BUILD_APP "Build the GUI application (pulls in JUCE)" ON)
option(JLS_BUILD_TESTS "Build unit tests" ON)

include(FetchContent)

# Engine, model and theme: plain C++, no JUCE or GUI dependency, so they stay unit-testable.
add_subdirectory(src/engine)
add_subdirectory(src/model)
add_subdirectory(src/theme)

if(JLS_BUILD_APP)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
    add_subdirectory(src/app)
endif()

if(JLS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
