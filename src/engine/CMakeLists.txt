add_library(jls_engine STATIC
    drum_synth.cpp
    effect_pool.cpp
    effects.cpp
    engine.cpp
    metronome.cpp
    preview_player.cpp
    sampler.cpp
    step_sequencer.cpp
    transport.cpp)

target_include_directories(jls_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

if(MSVC)
    target_compile_options(jls_engine PRIVATE /W4 /WX)
else()
    target_compile_options(jls_engine PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
