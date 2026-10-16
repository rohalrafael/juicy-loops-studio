FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.16.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(Catch2)

add_executable(jls_tests
    metronome_tests.cpp
    model_tests.cpp
    theme_tests.cpp
    sequencer_tests.cpp
    transport_tests.cpp)

target_link_libraries(jls_tests PRIVATE jls_model jls_engine jls_theme Catch2::Catch2WithMain)

include(${catch2_SOURCE_DIR}/extras/Catch.cmake)
catch_discover_tests(jls_tests)
