add_library(jls_engine STATIC
    drum_synth.cpp
    effect_pool.cpp
    effects.cpp
    engine.cpp
    metronome.cpp
    preview_player.cpp
    sampler.cpp
    scope.cpp
    step_sequencer.cpp
    time_stretch.cpp
    transport.cpp)

target_include_directories(jls_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(jls_engine PRIVATE signalsmith-stretch)

if(MSVC)
    # C4324 "structure was padded due to alignment specifier" is the point of SpscQueue's
    # alignas(64): its indices sit on separate cache lines.
    target_compile_options(jls_engine PRIVATE /W4 /WX /wd4324)
else()
    target_compile_options(jls_engine PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
