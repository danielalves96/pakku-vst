#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

/*  Pakku's parameter contract.

    The internal IDs are stable: a saved session or an old preset keeps
    loading even if a name, a range or a curve changes. That is why an ID
    never follows the label shown in the interface.

    The order parameters are registered in is the order the host lists them
    in, and changing it remaps existing automation — so that does not move
    either.                                                                */

namespace pid
{
    inline constexpr auto inputGain   = "inputGain";
    inline constexpr auto outputGain  = "outputGain";
    inline constexpr auto mix         = "mix";
    inline constexpr auto nyc         = "nyc";
    inline constexpr auto air         = "air";
    inline constexpr auto presence    = "presence";
    inline constexpr auto threshold   = "threshold";
    inline constexpr auto clipMode    = "clipMode";     // 0 = Limiter, 1 = Soft Clipper
    inline constexpr auto bandMode    = "bandMode";     // 0 = Multi, 1 = Single
    inline constexpr auto bypass      = "bypass";       // 0 = On, 1 = Off

    inline constexpr auto off1        = "offParam1";
    inline constexpr auto off2        = "offParam2";
    inline constexpr auto off3        = "offParam3";

    inline constexpr auto xoverLow    = "xoverLow";
    inline constexpr auto xoverHigh   = "xoverHigh";
    inline constexpr auto transSingle = "transSingle";
    inline constexpr auto lenSingle   = "lenSingle";

    inline juce::String trans (int b) { return "transBand" + juce::String (b + 1); }
    inline juce::String len   (int b) { return "lenBand"   + juce::String (b + 1); }
    inline juce::String solo  (int b) { return "soloBand"  + juce::String (b + 1); }
    inline juce::String mute  (int b) { return "muteBand"  + juce::String (b + 1); }
}

namespace pakku
{
    inline constexpr int numBands = 3;

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;

        const auto floatAttrs = [] (auto formatter, bool automatable = true)
        {
            return AudioParameterFloatAttributes()
                .withAutomatable (automatable)
                .withStringFromValueFunction (formatter);
        };

        auto gain = [&floatAttrs] (const String& id, const String& name)
        {
            NormalisableRange<float> r { -40.0f, 15.0f, 0.0f };
            r.skew = 2.2f;
            return std::make_unique<AudioParameterFloat> (
                ParameterID { id, 1 }, name, r, 0.0f,
                floatAttrs ([] (float v, int)
                {
                    if (v <= -39.999f) return String { "-inf" };
                    if (std::abs (v) < 0.005f) v = 0.0f;
                    return String (v, 2) + " dB";
                }));
        };

        auto shaper = [&floatAttrs] (const String& id, const String& name)
        {
            return std::make_unique<AudioParameterFloat> (
                ParameterID { id, 1 }, name,
                NormalisableRange<float> { -1.0f, 1.0f, 0.0f }, 0.0f,
                floatAttrs ([] (float v, int)
                {
                    if (std::abs (v) < 0.005f) return String { "-0.00" };
                    return String (v, 2);
                }));
        };

        auto amount = [&floatAttrs] (const String& id, const String& name,
                                     float hi, float def, int places)
        {
            return std::make_unique<AudioParameterFloat> (
                ParameterID { id, 1 }, name,
                NormalisableRange<float> { 0.0f, hi, 0.0f }, def,
                floatAttrs ([places] (float v, int)
                {
                    return places == 0 ? String (roundToInt (v)) : String (v, places);
                }));
        };

        auto freq = [&floatAttrs] (const String& id, const String& name, float def)
        {
            NormalisableRange<float> r { 50.0f, 15000.0f, 0.0f };
            r.skew = 0.199f;
            return std::make_unique<AudioParameterFloat> (
                ParameterID { id, 1 }, name, r, def,
                floatAttrs ([] (float v, int) { return String (v, 1); }, false));
        };

        const auto hiddenBool = [] (const String& id, const String& name)
        {
            return std::make_unique<AudioParameterBool> (
                ParameterID { id, 1 }, name, false,
                AudioParameterBoolAttributes().withAutomatable (false));
        };

        // Public ordering, 29 parameters.
        layout.add (amount (pid::air, "Air", 100.0f, 0.0f, 0));
        layout.add (amount (pid::mix, "Mix", 1.0f, 1.0f, 2));
        layout.add (amount (pid::nyc, "NYC", 0.5f, 0.0f, 2));
        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { pid::clipMode, 1 }, "L/SC",
            StringArray { "Limiter", "Soft Clipper" }, 1));

        layout.add (hiddenBool (pid::off1, "Off Param 1"));
        layout.add (hiddenBool (pid::off2, "Off Param 2"));
        layout.add (hiddenBool (pid::off3, "Off Param 3"));

        {
            NormalisableRange<float> r { -50.0f, 0.0f, 0.0f };
            r.setSkewForCentre (-14.64f);
            layout.add (std::make_unique<AudioParameterFloat> (
                ParameterID { pid::threshold, 1 }, "Threshold", r, 0.0f,
                floatAttrs ([] (float v, int)
                {
                    if (std::abs (v) < 0.005f) v = 0.0f;
                    return String (v, 2) + " dB";
                })));
        }

        layout.add (shaper (pid::transSingle, "Transient SB"));
        layout.add (freq (pid::xoverLow, "Freq Low", 800.0f));
        layout.add (gain (pid::outputGain, "Output Gain"));

        for (int b = 0; b < numBands; ++b)
            layout.add (hiddenBool (pid::solo (b), "Solo Button " + String (b + 1)));

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { pid::bandMode, 1 }, "Single/Multi",
            StringArray { "Multi Band", "Single Band" }, 1));

        layout.add (amount (pid::presence, "Presence", 100.0f, 0.1f, 0));

        for (int b = 0; b < numBands; ++b)
            layout.add (shaper (pid::len (b), "Tail MB " + String (b + 1)));
        for (int b = 0; b < numBands; ++b)
            layout.add (shaper (pid::trans (b), "Transient MB " + String (b + 1)));

        layout.add (gain (pid::inputGain, "Input Gain"));

        for (int b = 0; b < numBands; ++b)
            layout.add (hiddenBool (pid::mute (b), "Mute Button " + String (b + 1)));

        layout.add (freq (pid::xoverHigh, "Freq High", 8000.0f));
        layout.add (shaper (pid::lenSingle, "Tail SB"));
        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { pid::bypass, 1 }, "Bypass", true,
            AudioParameterBoolAttributes().withStringFromValueFunction (
                [] (bool off, int) { return off ? String { "Off" } : String { "On" }; })));

        return layout;
    }
}
