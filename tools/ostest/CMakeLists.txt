juce_add_console_app(pakku_ostest PRODUCT_NAME "pakku_ostest")
target_sources(pakku_ostest PRIVATE Main.cpp)
target_compile_definitions(pakku_ostest PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(pakku_ostest PRIVATE juce::juce_dsp juce::juce_recommended_config_flags)
