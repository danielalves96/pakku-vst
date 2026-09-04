juce_add_binary_data(PakkuPresets
    HEADER_NAME PresetData.h
    NAMESPACE   PresetData
    SOURCES
        ../resources/presets/808-squasher.pkku
        ../resources/presets/add-body.pkku
        ../resources/presets/add-drag.pkku
        ../resources/presets/add-punch-flavour.pkku
        ../resources/presets/beefy-snare.pkku
        ../resources/presets/boom-low-snap-top.pkku
        ../resources/presets/brightness-slap.pkku
        ../resources/presets/clean-controlled.pkku
        ../resources/presets/clean-lows.pkku
        ../resources/presets/clean-mud.pkku
        ../resources/presets/crispy-crunchy.pkku
        ../resources/presets/drum-ambience.pkku
        ../resources/presets/drum-flow.pkku
        ../resources/presets/elongate-hats.pkku
        ../resources/presets/hat-snap.pkku
        ../resources/presets/hihat-clarity.pkku
        ../resources/presets/increase-body.pkku
        ../resources/presets/increase-dynamics.pkku
        ../resources/presets/knock-dynamic.pkku
        ../resources/presets/loud-long-bright.pkku
        ../resources/presets/lowend-enhancer.pkku
        ../resources/presets/lows-go-boom.pkku
        ../resources/presets/make-me-a-hihat.pkku
        ../resources/presets/multislap.pkku
        ../resources/presets/sausage-of-the-year.pkku
        ../resources/presets/shaker-maker.pkku
        ../resources/presets/snappy-snare.pkku
        ../resources/presets/soft-to-trappy.pkku
        ../resources/presets/squash.pkku
        ../resources/presets/top-enhancement.pkku
    )

juce_add_binary_data(PakkuFonts
    HEADER_NAME FontData.h
    NAMESPACE   FontData
    SOURCES
        ../resources/fonts/Michroma-Regular.ttf
    )

juce_add_plugin(Pakku
    COMPANY_NAME              "Kyantech Labs"
    BUNDLE_ID                 "com.kyantechlabs.pakku"
    PLUGIN_MANUFACTURER_CODE  Kytl
    PLUGIN_CODE               Pkku
    FORMATS                   AU VST3
    PRODUCT_NAME              "Pakku"
    DESCRIPTION               "Multiband transient shaper"
    VST3_CATEGORIES           Fx Dynamics
    AU_MAIN_TYPE              kAudioUnitType_Effect
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   ${PAKKU_COPY_AFTER_BUILD})

juce_generate_juce_header(Pakku)

target_sources(Pakku PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp)

target_compile_definitions(Pakku PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_FORCE_USE_LEGACY_PARAM_IDS=1
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Pakku PRIVATE
    PakkuPresets
    PakkuFonts
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)
