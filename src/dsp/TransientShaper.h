#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace pakku
{
/** Envelope follower with independent attack and release constants. */
class EnvelopeFollower
{
public:
    void prepare (double sampleRate, float attackMs, float releaseMs)
    {
        sr = sampleRate;
        setTimes (attackMs, releaseMs);
        env = 0.0f;
    }

    void setTimes (float attackMs, float releaseMs)
    {
        aCoef = coef (attackMs);
        rCoef = coef (releaseMs);
    }

    void reset() noexcept { env = 0.0f; }

    inline float process (float rectified) noexcept
    {
        const auto c = (rectified > env) ? aCoef : rCoef;
        env = rectified + c * (env - rectified);
        return env;
    }

private:
    float coef (float ms) const
    {
        return ms <= 0.0f ? 0.0f
                          : std::exp (-1.0f / (float) (0.001 * ms * sr));
    }

    double sr = 44100.0;
    float aCoef = 0.0f, rCoef = 0.0f, env = 0.0f;
};

/*  Transient shaping from differential envelopes.

    ATTACK  — a fast envelope compared against a slow one. On an onset the
              fast one shoots ahead; the gap between them is the transient.
    LENGTH  — two envelopes with different release times. Through the decay
              the long release sits above the short one, and that difference
              is the tail.

    The two overlap without interfering because they act on different time
    windows of the same event.                                              */
class TransientShaper
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        /*  Tuned for a peak around 0.4 ms and a half-life near 86 ms.

            The half-life is set by the ATTACK of the slow envelope, not by
            its release: the lift lasts for as long as the slow one has not
            caught up with the fast one. Half-life = 0.693 * tau, so 86 ms
            asks for a tau of roughly 124 ms.                              */
        attFast.prepare (sampleRate,   0.1f, 110.0f);
        attSlow.prepare (sampleRate, 75.0f, 13.0f);
        susFast.prepare (sampleRate,   0.1f,  60.0f);
        susSlow.prepare (sampleRate,   0.1f, 450.0f);
        smoothed.reset (sampleRate, 0.0001);
        smoothed.setCurrentAndTargetValue (1.0f);
    }

    void reset()
    {
        for (auto* e : { &attFast, &attSlow, &susFast, &susSlow }) e->reset();
        smoothed.setCurrentAndTargetValue (1.0f);
    }

    /** amounts em -1..+1 */
    void setAmounts (float transient, float length) noexcept
    {
        kAtt = transient;
        kLen = length;
    }

    inline bool isNeutral() const noexcept
    {
        return std::abs (kAtt) < 1.0e-4f && std::abs (kLen) < 1.0e-4f;
    }

    /** Detection runs on the sidechain signal, the sum of the channels. */
    inline float computeGain (float sidechain) noexcept
    {
        const auto x = std::abs (sidechain);

        const auto aF = attFast.process (x);
        const auto aS = attSlow.process (x);
        const auto sF = susFast.process (x);
        const auto sS = susSlow.process (x);

        constexpr float eps = 1.0e-7f;
        // differences in dB, positive while the matching event is happening
        const auto attDb = 20.0f * std::log10 ((aF + eps) / (aS + eps));
        const auto lenDb = 20.0f * std::log10 ((sS + eps) / (sF + eps));

        // calibrated scaling: about +13.5 dB of peak and +18.8 dB of tail at full
        auto gainDb = kAtt * juce::jmax (0.0f, attDb) * 0.745f
                    + kLen * juce::jmax (0.0f, lenDb) * 0.850f;

        gainDb = juce::jlimit (-20.0f, 20.0f, gainDb);
        smoothed.setTargetValue (juce::Decibels::decibelsToGain (gainDb));
        return smoothed.getNextValue();
    }

private:
    double sr = 44100.0;
    float kAtt = 0.0f, kLen = 0.0f;
    EnvelopeFollower attFast, attSlow, susFast, susSlow;
    juce::SmoothedValue<float> smoothed;
};
} // namespace pakku
