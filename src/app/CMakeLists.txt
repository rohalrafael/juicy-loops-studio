juce_add_gui_app(JuicyLoopsStudio
    PRODUCT_NAME "Juicy Loops Studio"
    COMPANY_NAME "Juicy Loops Studio contributors"
    BUNDLE_ID io.github.rohalrafael.juicyloopsstudio
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

target_sources(JuicyLoopsStudio PRIVATE
    commands.cpp
    live_input.cpp
    native_focus.cpp
    stretch_cache.cpp
    main.cpp
    main_component.cpp
    ../io/audio_export.cpp
    ../io/sample_loader.cpp
    ../plugins/plugin_host.cpp
    ../ui/browser.cpp
    ../ui/channel_rack.cpp
    ../ui/controls.cpp
    ../ui/envelope_editor.cpp
    ../ui/floating_panel.cpp
    ../ui/theme.cpp
    ../ui/mixer.cpp
    ../ui/oscilloscope.cpp
    ../ui/plugin_menu.cpp
    ../ui/sampler_panel.cpp
    ../ui/settings_dialog.cpp
    ../ui/piano_roll.cpp
    ../ui/playlist.cpp
    ../ui/theme_dialog.cpp
    ../ui/title_bar.cpp)

target_include_directories(JuicyLoopsStudio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

juce_add_binary_data(JlsFonts SOURCES
    ${PROJECT_SOURCE_DIR}/resources/fonts/inter/Inter.ttf
    ${PROJECT_SOURCE_DIR}/resources/fonts/inter/Inter-Medium.ttf
    ${PROJECT_SOURCE_DIR}/resources/fonts/inter/Inter-SemiBold.ttf
    ${PROJECT_SOURCE_DIR}/resources/fonts/ibmplexmono/IBMPlexMono-Medium.ttf)

target_compile_definitions(JuicyLoopsStudio PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    $<$<PLATFORM_ID:Linux>:JUCE_JACK=1>
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_LV2=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:JuicyLoopsStudio,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:JuicyLoopsStudio,JUCE_VERSION>")

# Xlib for app/native_focus (giving the keyboard back from plugin windows). JUCE loads libX11 at
# run time anyway; its development package is already a build requirement.
if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_link_libraries(JuicyLoopsStudio PRIVATE X11::X11)
endif()

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
