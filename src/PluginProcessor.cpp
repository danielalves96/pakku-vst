#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

PakkuAudioProcessor::PakkuAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  AudioChannelSet::stereo(), true)
          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", pakku::createLayout())
{
}

bool PakkuAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void PakkuAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    const auto ch = (uint32) jmax (1, getTotalNumOutputChannels());

    dsp::ProcessSpec spec { sampleRate, (uint32) samplesPerBlock, ch };

    crossover.prepare (spec);
    tone.prepare (spec);
    nyc.prepare (spec);
    ceiling.prepare (spec);

    for (auto& s : shapers) s.prepare (sampleRate);
    singleShaper.prepare (sampleRate);

    scope.prepare (sampleRate, 2.0);
    analyser.prepare (sampleRate);

    dryBuffer .setSize ((int) ch, samplesPerBlock);
    bandBuffer.setSize ((int) ch, samplesPerBlock);

    for (auto* s : { &inGain, &outGain, &mixAmount })
        s->reset (sampleRate, 0.01);

    const auto latency = ceiling.getLookaheadSamples();

    dryDelay.prepare (spec);
    dryDelay.setMaximumDelayInSamples ((int) std::ceil (latency) + 4);
    dryDelay.setDelay (latency);
    dryDelay.reset();

    // One sample under the nominal 5 ms position: the detector already sees
    // the current sample before the delay line hands it over.
    activeLatency = juce::jmax (0, (int) std::ceil (latency) - 1);
    updateParameters();
    setLatencySamples (p.bypass ? 0 : activeLatency);
}

void PakkuAudioProcessor::releaseResources()
{
    crossover.reset(); tone.reset(); nyc.reset(); ceiling.reset(); dryDelay.reset();
    for (auto& s : shapers) s.reset();
    singleShaper.reset();
}

void PakkuAudioProcessor::updateParameters()
{
    auto val = [this] (const String& id) { return apvts.getRawParameterValue (id)->load(); };

    p.multi     = val (pid::bandMode)  < 0.5f;
    p.bypass    = val (pid::bypass)    < 0.5f;
    p.mode      = val (pid::clipMode)  > 0.5f ? pakku::Ceiling::Mode::softClip
                                              : pakku::Ceiling::Mode::limit;
    p.xLow      = val (pid::xoverLow);
    p.xHigh     = val (pid::xoverHigh);
    p.air       = val (pid::air);
    p.presence  = val (pid::presence);
    p.nyc       = val (pid::nyc);
    p.threshold = val (pid::threshold);

    p.transSingle = val (pid::transSingle);
    p.lenSingle   = val (pid::lenSingle);

    p.anySolo = false;
    for (int b = 0; b < numBands; ++b)
    {
        p.trans[b] = val (pid::trans (b));
        p.len[b]   = val (pid::len   (b));
        p.solo[b]  = val (pid::solo  (b)) > 0.5f;
        p.mute[b]  = val (pid::mute  (b)) > 0.5f;
        p.anySolo |= p.solo[b];
    }

    crossover.setCrossoverFrequencies (p.xLow, p.xHigh);
    tone.setAmounts (p.air, p.presence);
    ceiling.setParams (p.mode, p.threshold);

    for (int b = 0; b < numBands; ++b)
        shapers[b].setAmounts (p.trans[b], p.len[b]);
    singleShaper.setAmounts (p.transSingle, p.lenSingle);

    inGain   .setTargetValue (Decibels::decibelsToGain (val (pid::inputGain)));
    outGain  .setTargetValue (Decibels::decibelsToGain (val (pid::outputGain)));
    mixAmount.setTargetValue (val (pid::mix));
}

void PakkuAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const auto numCh = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    for (int c = getTotalNumInputChannels(); c < numCh; ++c)
        buffer.clear (c, 0, numSamples);

    updateParameters();

    const auto wantedLatency = p.bypass ? 0 : activeLatency;
    if (getLatencySamples() != wantedLatency)
        setLatencySamples (wantedLatency);

    inputLevel.store (buffer.getMagnitude (0, numSamples));

    if (p.bypass)
    {
        outputLevel.store (buffer.getMagnitude (0, numSamples));
        return;
    }

    // --- input gain ---
    for (int i = 0; i < numSamples; ++i)
    {
        const auto g = inGain.getNextValue();
        for (int c = 0; c < numCh; ++c)
            buffer.getWritePointer (c)[i] *= g;
    }

    // --- dry copy for the Mix ---
    dryBuffer.setSize (numCh, numSamples, false, false, true);
    for (int c = 0; c < numCh; ++c)
        dryBuffer.copyFrom (c, 0, buffer, c, 0, numSamples);

    // --- transient shaping ---
    if (p.multi)
    {
        // bandBuffer is already sized in prepareToPlay: nothing is allocated here
        bandBuffer.setSize (numCh, numSamples, false, false, true);
        bandBuffer.clear();
        auto& acc = bandBuffer;

        float bandGain[numBands];
        for (int b = 0; b < numBands; ++b)
            bandGain[b] = (p.mute[b] || (p.anySolo && ! p.solo[b])) ? 0.0f : 1.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float bands[numBands][2] {};

            for (int c = 0; c < numCh; ++c)
            {
                float lo, mid, hi;
                crossover.split (c, buffer.getReadPointer (c)[i], lo, mid, hi);
                bands[0][c] = lo; bands[1][c] = mid; bands[2][c] = hi;
            }

            float lanes[1 + numBands] { 0.0f };

            for (int b = 0; b < numBands; ++b)
            {
                if (bandGain[b] == 0.0f) continue;

                // sidechain = channel average, which keeps the stereo image steady
                float sc = 0.0f;
                for (int c = 0; c < numCh; ++c) sc += bands[b][c];
                sc /= (float) numCh;

                const auto g = shapers[b].computeGain (sc) * bandGain[b];
                lanes[b + 1] = sc * g;

                for (int c = 0; c < numCh; ++c)
                    acc.getWritePointer (c)[i] += bands[b][c] * g;
            }

            for (int b = 0; b < numBands; ++b) lanes[0] += lanes[b + 1];
            scope.push (lanes);
            analyser.push (lanes[0]);
        }

        for (int c = 0; c < numCh; ++c)
            buffer.copyFrom (c, 0, acc, c, 0, numSamples);
    }
    else
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float sc = 0.0f;
            for (int c = 0; c < numCh; ++c) sc += buffer.getReadPointer (c)[i];
            sc /= (float) numCh;

            const auto g = singleShaper.computeGain (sc);
            for (int c = 0; c < numCh; ++c)
                buffer.getWritePointer (c)[i] *= g;

            const float lanes[1 + numBands] { sc * g, 0.0f, 0.0f, 0.0f };
            scope.push (lanes);
            analyser.push (lanes[0]);
        }
    }

    // --- tone ---
    dsp::AudioBlock<float> block (buffer);
    tone.process (block);

    // --- parallel compression ---
    nyc.process (buffer, p.nyc);

    // --- ceiling, 5 ms lookahead ---
    ceiling.process (block);

    // --- dry/wet blend and output ---
    for (int i = 0; i < numSamples; ++i)
    {
        const auto m = mixAmount.getNextValue();
        const auto g = outGain.getNextValue();

        for (int c = 0; c < numCh; ++c)
        {
            dryDelay.pushSample (c, dryBuffer.getSample (c, i));
            const auto dry = dryDelay.popSample (c);

            auto* d = buffer.getWritePointer (c);
            d[i] = (d[i] * m + dry * (1.0f - m)) * g;
        }
    }

    outputLevel.store (buffer.getMagnitude (0, numSamples));
}

void PakkuAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("pakkuStateSchema", 2, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PakkuAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = ValueTree::fromXml (*xml);

            if ((int) state.getProperty ("pakkuStateSchema", 1) < 2)
            {
                auto migrate = [] (const String& id, float value)
                {
                    if (id == pid::mix || id == pid::nyc
                        || id == pid::transSingle || id == pid::lenSingle
                        || id.startsWith ("transBand") || id.startsWith ("lenBand"))
                        return value * 0.01f;
                    if (id == pid::bandMode || id == pid::bypass)
                        return 1.0f - value;
                    return value;
                };

                // APVTS uses PARAM children in the current serialisation. The
                // root-property path covers sessions written by older JUCE
                // versions of the project.
                for (auto child : state)
                    if (child.hasProperty ("id") && child.hasProperty ("value"))
                    {
                        const auto id = child.getProperty ("id").toString();
                        child.setProperty ("value", migrate (
                            id, (float) (double) child.getProperty ("value")), nullptr);
                    }

                for (const auto& id : { String (pid::mix), String (pid::nyc),
                                        String (pid::transSingle), String (pid::lenSingle),
                                        String (pid::bandMode), String (pid::bypass) })
                    if (state.hasProperty (id))
                        state.setProperty (id, migrate (
                            id, (float) (double) state.getProperty (id)), nullptr);

                for (int b = 0; b < numBands; ++b)
                    for (const auto& id : { pid::trans (b), pid::len (b) })
                        if (state.hasProperty (id))
                            state.setProperty (id, migrate (
                                id, (float) (double) state.getProperty (id)), nullptr);
            }

            state.setProperty ("pakkuStateSchema", 2, nullptr);
            apvts.replaceState (state);
        }
}

AudioProcessorEditor* PakkuAudioProcessor::createEditor()
{
    return new PakkuAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PakkuAudioProcessor();
}
