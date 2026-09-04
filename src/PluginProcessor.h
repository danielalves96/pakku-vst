#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Parameters.h"
#include "PresetManager.h"
#include "dsp/Crossover.h"
#include "dsp/TransientShaper.h"
#include "dsp/ToneAndDynamics.h"
#include "dsp/ScopeBuffer.h"
#include "dsp/SpectrumAnalyser.h"

class PakkuAudioProcessor : public juce::AudioProcessor
{
public:
    PakkuAudioProcessor();
    ~PakkuAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Pakku"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    pakku::PresetManager presets { apvts };

    // meters for the interface: lock-free, written only by audio
    std::atomic<float> inputLevel { 0.0f }, outputLevel { 0.0f };

    // lane 0 = soma, lanes 1..3 = bandas do crossover
    pakku::ScopeBuffer<1 + pakku::numBands> scope;
    pakku::SpectrumAnalyser analyser;

    /** Exposed for the measurement bench. */
    float getOversamplerLatency() const
    {
        return ceiling.getLookaheadSamples();
    }

    float getThresholdDb() const { return p.threshold; }
    bool  isMultiband()    const { return p.multi; }

private:
    void updateParameters();

    template <typename T>
    T* raw (const juce::String& id) const
    {
        return dynamic_cast<T*> (apvts.getParameter (id));
    }

    static constexpr int numBands = pakku::numBands;

    pakku::Crossover        crossover;
    pakku::TransientShaper  shapers[numBands];   // multibanda
    pakku::TransientShaper  singleShaper;        // faixa cheia
    pakku::ToneShaper       tone;
    pakku::ParallelCompressor nyc;
    pakku::Ceiling          ceiling;

    juce::AudioBuffer<float> dryBuffer, bandBuffer;

    /*  The dry path has to be delayed as much as the wet one, otherwise Mix
        below 100% sums two misaligned copies and turns into a comb filter. */
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 512 };
    juce::SmoothedValue<float> inGain, outGain, mixAmount;

    // parameter cache, read once per block
    struct Cache
    {
        float trans[numBands] {}, len[numBands] {};
        bool  solo[numBands] {},  mute[numBands] {};
        float transSingle = 0.0f, lenSingle = 0.0f;
        float xLow = 800.0f, xHigh = 8000.0f;
        float air = 0.0f, presence = 0.0f, nyc = 0.0f, threshold = 0.0f;
        bool  multi = false, bypass = false, anySolo = false;
        pakku::Ceiling::Mode mode = pakku::Ceiling::Mode::softClip;
    } p;

    double currentSampleRate = 44100.0;
    int activeLatency = 220;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PakkuAudioProcessor)
};
