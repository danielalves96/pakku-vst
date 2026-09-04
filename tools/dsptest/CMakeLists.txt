juce_add_console_app(pakku_dsptest PRODUCT_NAME "pakku_dsptest")
target_sources(pakku_dsptest PRIVATE Main.cpp ../../src/PluginProcessor.cpp ../../src/PluginEditor.cpp)
target_compile_definitions(pakku_dsptest PRIVATE
    JUCE_STANDALONE_APPLICATION=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0
    JucePlugin_Name="Pakku" JucePlugin_Desc="test" JucePlugin_Manufacturer="Kyantech"
    JucePlugin_VersionString="${PROJECT_VERSION}" JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0 JucePlugin_ProducesMidiOutput=0)
target_link_libraries(pakku_dsptest PRIVATE PakkuPresets PakkuFonts juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
