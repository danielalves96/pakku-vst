#pragma once
#include "PluginProcessor.h"
#include "gui/LookAndFeel.h"
#include "gui/Meter.h"
#include "gui/Display.h"
#include "gui/Controls.h"
#include "gui/PresetBar.h"
#include "gui/SettingsPanel.h"

namespace pakku
{
/*  The whole interface lives here, at a fixed logical size.

    The editor around it only applies the scale chosen in the settings panel,
    which keeps proportions, stroke weights and type size identical at every
    size — rather than reflowing, which stretches some and not others.     */
class EditorContent : public juce::Component, private juce::Timer
{
public:
    static constexpr int baseWidth  = 1000;
    static constexpr int baseHeight = 580;

    EditorContent (PakkuAudioProcessor&, std::function<void (int)> chooseScale);
    ~EditorContent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Reflects the size currently in use back into the panel. */
    void setScaleIndex (int i) { scaleIndex = i; settings.setSizeIndex (i); }

    void openSettings();
    void openPresetMenu() { presetBar.showMenu(); }
    Display& getDisplay() { return display; }

private:
    void timerCallback() override;
    void applyBandMode();
    void bindShapers();
    void closeSettings();
    void restoreDefaults();
    void drawHeader (juce::Graphics&, juce::Rectangle<int>);
    void drawBrandStrip (juce::Graphics&, juce::Rectangle<int>);
    void rebuildBackdrop();

    PakkuAudioProcessor& proc;
    std::function<void (int)> chooseScale;
    int scaleIndex = 1;

    juce::Image backdrop;

    LevelMeter inMeter { proc.inputLevel,  true  };
    LevelMeter outMeter{ proc.outputLevel, false };
    Display    display;
    PresetBar  presetBar { proc.presets };
    IconButton gear { icons::Id::gear, "Settings and credits" };
    SettingsPanel settings;

    // one pair of faders only, rebound to whichever band the spectrum selects
    std::unique_ptr<VerticalShaper> transients, length;

    std::unique_ptr<LabeledKnob> inGain, outGain, nyc, mix, presence, air, threshold;
    std::unique_ptr<ToggleSwitch> bandMode, ceilingMode;

    BypassButton bypass;
    std::unique_ptr<juce::ParameterAttachment> bypassAtt;

    bool lastMulti = false;
    int  selectedBand = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditorContent)
};
} // namespace pakku

//==============================================================================
class PakkuAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit PakkuAudioProcessorEditor (PakkuAudioProcessor&);
    ~PakkuAudioProcessorEditor() override;

    void resized() override;

    /** 0 = small, 1 = medium, 2 = large. Stored with the session state. */
    void setScaleIndex (int);

    /** Usados pela bancada de captura de tela. */
    void openSettings()   { content.openSettings(); }
    void openPresetMenu() { content.openPresetMenu(); }
    pakku::Display& getDisplay() { return content.getDisplay(); }

private:
    static constexpr float scales[3] = { 0.85f, 1.0f, 1.25f };

    PakkuAudioProcessor& proc;
    pakku::PakkuLookAndFeel lnf;
    juce::TooltipWindow tips { this, 700 };
    pakku::EditorContent content;
    int scaleIndex = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PakkuAudioProcessorEditor)
};
