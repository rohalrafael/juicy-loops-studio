juce_add_gui_app(JuicyLoopsStudio
    PRODUCT_NAME "Juicy Loops Studio"
    COMPANY_NAME "Juicy Loops Studio contributors"
    BUNDLE_ID io.github.rohalrafael.juicyloopsstudio
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

target_sources(JuicyLoopsStudio PRIVATE
    main.cpp
    main_component.cpp
    ../io/sample_loader.cpp
    ../ui/channel_rack.cpp
    ../ui/theme.cpp)

target_include_directories(JuicyLoopsStudio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_compile_definitions(JuicyLoopsStudio PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_JACK=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:JuicyLoopsStudio,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:JuicyLoopsStudio,JUCE_VERSION>")

target_link_libraries(JuicyLoopsStudio
    PRIVATE
        jls_model
        jls_engine
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
