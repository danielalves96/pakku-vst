#pragma once
#include <juce_dsp/juce_dsp.h>

namespace pakku
{
/*  Linkwitz-Riley crossover, 4th order, three bands.

    The low band never passes through the second split, so it gets an allpass
    at the high crossover frequency to keep its phase aligned with the others.
    Without that the bands do not sum flat around the crossover region.      */
class Crossover
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        for (auto* f : { &lowSplit, &highSplit, &lowAllpass })
            f->prepare (spec);

        lowAllpass.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
        reset();
    }

    void reset()
    {
        lowSplit.reset(); highSplit.reset(); lowAllpass.reset();
    }

    void setCrossoverFrequencies (float lowHz, float highHz)
    {
        // guard rail: keeps the mid band at least half an octave wide
        highHz = juce::jmax (highHz, lowHz * 1.5f);

        lowSplit  .setCutoffFrequency (lowHz);
        highSplit .setCutoffFrequency (highHz);
        lowAllpass.setCutoffFrequency (highHz);
    }

    /** Separa uma amostra em 3 bandas coerentes em fase. */
    inline void split (int channel, float in, float& low, float& mid, float& high) noexcept
    {
        float rest;
        lowSplit.processSample (channel, in, low, rest);
        highSplit.processSample (channel, rest, mid, high);
        low = lowAllpass.processSample (channel, low);
    }

private:
    juce::dsp::LinkwitzRileyFilter<float> lowSplit, highSplit, lowAllpass;
};
} // namespace pakku
