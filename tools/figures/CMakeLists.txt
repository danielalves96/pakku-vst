juce_add_console_app(pakku_figures PRODUCT_NAME "pakku_figures")

target_sources(pakku_figures PRIVATE
    Main.cpp
    ../../src/PluginProcessor.cpp
    ../../src/PluginEditor.cpp)

target_compile_definitions(pakku_figures PRIVATE
    JUCE_STANDALONE_APPLICATION=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0
    JucePlugin_Name="Pakku" JucePlugin_Desc="Multiband transient shaper"
    JucePlugin_Manufacturer="Kyantech Labs"
    JucePlugin_VersionString="${PROJECT_VERSION}" JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0 JucePlugin_ProducesMidiOutput=0)

target_link_libraries(pakku_figures PRIVATE
    PakkuPresets PakkuFonts
    juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
