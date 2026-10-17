juce_add_gui_app(JuicyLoopsStudio
    PRODUCT_NAME "Juicy Loops Studio"
    COMPANY_NAME "Juicy Loops Studio contributors"
    BUNDLE_ID io.github.rohalrafael.juicyloopsstudio
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

target_sources(JuicyLoopsStudio PRIVATE
    commands.cpp
    live_input.cpp
    main.cpp
    main_component.cpp
    ../io/audio_export.cpp
    ../io/sample_loader.cpp
    ../plugins/plugin_host.cpp
    ../ui/browser.cpp
    ../ui/channel_rack.cpp
    ../ui/theme.cpp
    ../ui/mixer.cpp
    ../ui/plugin_menu.cpp
    ../ui/settings_dialog.cpp
    ../ui/piano_roll.cpp
    ../ui/playlist.cpp
    ../ui/theme_dialog.cpp)

target_include_directories(JuicyLoopsStudio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

juce_add_binary_data(JlsFonts SOURCES
    ${PROJECT_SOURCE_DIR}/resources/fonts/ibmplexsans/IBMPlexSans.ttf
    ${PROJECT_SOURCE_DIR}/resources/fonts/bricolagegrotesque/BricolageGrotesque.ttf
    ${PROJECT_SOURCE_DIR}/resources/fonts/sora/Sora.ttf
    ${PROJECT_SOURCE_DIR}/resources/fonts/outfit/Outfit.ttf)

target_compile_definitions(JuicyLoopsStudio PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_JACK=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_LV2=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:JuicyLoopsStudio,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:JuicyLoopsStudio,JUCE_VERSION>")

target_link_libraries(JuicyLoopsStudio
    PRIVATE
        JlsFonts
        jls_model
        jls_engine
        jls_theme
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
