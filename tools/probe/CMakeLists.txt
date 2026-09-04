juce_add_console_app(pakku_probe
    PRODUCT_NAME "pakku_probe"
    BUNDLE_ID "com.pakku.probe")

target_sources(pakku_probe PRIVATE Main.cpp)

target_compile_definitions(pakku_probe PRIVATE
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_VST=0
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_STANDALONE_APPLICATION=1)

target_link_libraries(pakku_probe PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_core
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)
