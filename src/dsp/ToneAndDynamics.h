#pragma once
#include <juce_dsp/juce_dsp.h>

namespace pakku
{
/*  Air and Presence.
    Air works almost entirely above 10 kHz; Presence is broader, taking in
    the upper mids as well as the top.                                   */
class ToneShaper
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sr = spec.sampleRate;
        air.prepare (spec); presence.prepare (spec);
        setAmounts (0.0f, 0.0f);
    }

    void reset() { air.reset(); presence.reset(); }

    void setAmounts (float airPct, float presPct)
    {
        /*  At zero the stage leaves the path entirely.

            A shelf at unity gain is flat in magnitude but it is still an IIR
            and it still shifts phase. That is inaudible on its own — except
            that partial Mix sums this signal against the dry one, and the
            phase difference turns into comb filtering. Skipping also saves
            CPU in the most common case, which is both at zero.             */
        bypassed = (airPct <= 1.0e-4f && presPct <= 1.0e-4f);
        if (bypassed) return;

        const auto airDb  = airPct  * 0.1204f;
        const auto presDb = presPct * 0.1204f;

        *air.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sr, 10000.0, 1.1f, juce::Decibels::decibelsToGain (airDb));
        *presence.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sr, 4000.0, 1.1f, juce::Decibels::decibelsToGain (presDb));
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        if (bypassed) return;

        juce::dsp::ProcessContextReplacing<float> ctx (block);
        presence.process (ctx);
        air.process (ctx);
    }

    bool isBypassed() const noexcept { return bypassed; }

private:
    using Shelf = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                 juce::dsp::IIR::Coefficients<float>>;
    double sr = 44100.0;
    bool bypassed = true;
    Shelf air, presence;
};

/*  NYC — New York style parallel compression: crush a copy and blend it back.
    The settings are fixed and aggressive; the knob only controls how much of
    the crushed copy is folded in.                                          */
class ParallelCompressor
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sr = spec.sampleRate;
        scratch.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);
        envelopeSquared.assign ((size_t) spec.numChannels, 0.0f);

        /*  RMS detection, roughly 6:1 from -40 dBFS up, and about 34.5 dB of
            makeup on the way back out. */
        threshold = juce::Decibels::decibelsToGain (-40.0f);
        makeup = juce::Decibels::decibelsToGain (34.5f);
        attackCoef  = timeCoefficient (2.0f);
        releaseCoef = timeCoefficient (140.0f);
    }

    void reset() { std::fill (envelopeSquared.begin(), envelopeSquared.end(), 0.0f); }

    /** amount runs 0..0.5, matching the exposed parameter. */
    void process (juce::AudioBuffer<float>& buf, float amount)
    {
        if (amount <= 1.0e-4f) return;

        const auto ch = buf.getNumChannels(), n = buf.getNumSamples();
        scratch.setSize (ch, n, false, false, true);

        for (int c = 0; c < ch; ++c)
            scratch.copyFrom (c, 0, buf, c, 0, n);

        for (int c = 0; c < ch; ++c)
        {
            auto* wet = scratch.getWritePointer (c);
            auto env = envelopeSquared[(size_t) c];

            for (int i = 0; i < n; ++i)
            {
                const auto square = wet[i] * wet[i];
                const auto coef = square > env ? attackCoef : releaseCoef;
                env = square + coef * (env - square);

                const auto rms = std::sqrt (juce::jmax (0.0f, env));
                const auto reduction = rms > threshold
                    ? std::pow (rms / threshold, (1.0f / 6.0f) - 1.0f)
                    : 1.0f;

                wet[i] *= reduction * makeup;
            }

            envelopeSquared[(size_t) c] = env;
            buf.addFrom (c, 0, scratch, c, 0, n, amount);
        }
    }

private:
    float timeCoefficient (float ms) const
    {
        return std::exp (-1.0f / (float) (0.001 * ms * sr));
    }

    juce::AudioBuffer<float> scratch;
    std::vector<float> envelopeSquared;
    double sr = 44100.0;
    float threshold = 0.01f, makeup = 1.0f;
    float attackCoef = 0.0f, releaseCoef = 0.0f;
};

/*  Peak limiter that holds the ceiling exactly at the threshold.

    Written by hand rather than using juce::dsp::Limiter, which folds in
    automatic makeup gain (it normalises to 0 dBFS): with that in place,
    moving the Threshold changed the level instead of catching peaks. Here a
    ceiling is a ceiling — it holds at the threshold and passes untouched
    below it.

    Detection is linked across channels so the stereo image does not shift. */
class PeakLimiter
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sr = spec.sampleRate;
        reset();
    }

    void reset() { env = 0.0f; gain = 1.0f; }

    void setThreshold (float linear) { thr = juce::jmax (1.0e-6f, linear); }

    void process (juce::dsp::AudioBlock<float>& block,
                  const juce::dsp::AudioBlock<float>& detector)
    {
        const auto numCh = block.getNumChannels();
        const auto numSamples = block.getNumSamples();

        for (size_t i = 0; i < numSamples; ++i)
        {
            float peak = 0.0f;
            for (size_t c = 0; c < numCh; ++c)
                peak = juce::jmax (peak, std::abs (detector.getSample ((int) c, (int) i)));

            /*  Recovery speeds up as the reduction deepens: about 4.2 ms at
                3 dB over, about 2.6 ms at 12 dB over. That dependency is what
                gives the limiter its distortion character. */
            const auto overDb = juce::jmax (0.0f,
                juce::Decibels::gainToDecibels (
                    juce::jmax (env / thr, 1.0e-12f), -120.0f));
            const auto releaseMs = juce::jmin (60.0f,
                2.0f + 6.7f / juce::jmax (overDb, 0.12f));
            const auto release = std::exp (-1.0f / (float) (0.001 * releaseMs * sr));

            // The audio arrives 5 ms later, so the reduction can come in
            // right away without clipping the front of the audible peak.
            env = juce::jmax (peak, env * release);
            const auto target = env > thr ? thr / env : 1.0f;
            gain = target < gain ? target : target + release * (gain - target);

            for (size_t ch = 0; ch < numCh; ++ch)
                block.getChannelPointer (ch)[i] *= gain;
        }
    }

private:
    double sr = 44100.0;
    float thr = 1.0f, env = 0.0f, gain = 1.0f;
};

/*  Output ceiling: limiter or soft clipper, both anchored to the Threshold.
    The stage is delayed by 5 ms so the detector can see a peak coming. */
class Ceiling
{
public:
    enum class Mode { limit, softClip };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sr = spec.sampleRate;
        lookahead = (float) (0.005 * sr);

        delay.prepare (spec);
        delay.setMaximumDelayInSamples ((int) std::ceil (lookahead) + 4);
        delay.setDelay (lookahead);
        detectorScratch.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);

        limiter.prepare (spec);
        reset();
    }

    void reset() { limiter.reset(); delay.reset(); }

    float getLookaheadSamples() const noexcept { return lookahead; }

    void setParams (Mode m, float thresholdDb)
    {
        mode = m;
        thrDb = thresholdDb;
        thrLin = juce::Decibels::decibelsToGain (thresholdDb);
        limiter.setThreshold (thrLin);
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        if (mode == Mode::limit)
        {
            // The detector sees the signal ahead of time; the delay line
            // carries the audio that actually gets limited.
            detectorScratch.setSize ((int) block.getNumChannels(),
                                     (int) block.getNumSamples(), false, false, true);

            for (size_t c = 0; c < block.getNumChannels(); ++c)
                detectorScratch.copyFrom ((int) c, 0, block.getChannelPointer (c),
                                          (int) block.getNumSamples());

            for (size_t i = 0; i < block.getNumSamples(); ++i)
                for (size_t c = 0; c < block.getNumChannels(); ++c)
                {
                    delay.pushSample ((int) c, block.getSample ((int) c, (int) i));
                    block.getChannelPointer (c)[i] = delay.popSample ((int) c);
                }

            juce::dsp::AudioBlock<float> detectorBlock (detectorScratch);
            limiter.process (block, detectorBlock);
            return;
        }

        /*  Soft clip with the knee opening below the threshold.

              |u| <= k  ->  passes untouched
              |u| >  k  ->  k + (c-k)*tanh((|u|-k)/(c-k))

            Continuous, with derivative 1 at |u| = k, so entering the knee
            brings neither a step nor a kink in slope; it saturates toward c.

            The knee opens about 4 dB below the threshold and the ceiling
            settles just above it. The curve is odd-symmetric, so it generates
            odd harmonics only and no second-harmonic buzz.                 */
        constexpr float knee = 0.60f;
        constexpr float span = 0.40f;

        for (size_t c = 0; c < block.getNumChannels(); ++c)
        {
            auto* d = block.getChannelPointer (c);

            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                delay.pushSample ((int) c, d[i]);
                const auto x = delay.popSample ((int) c);
                const auto a = std::abs (x);
                const auto kneeAbs = knee * thrLin;
                const auto spanAbs = span * thrLin;

                if (a > kneeAbs)
                    d[i] = std::copysign (
                        kneeAbs + spanAbs * std::tanh ((a - kneeAbs) / spanAbs), x);
                else
                    d[i] = x;
            }
        }
    }

private:
    PeakLimiter limiter;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> delay { 512 };
    juce::AudioBuffer<float> detectorScratch;
    Mode mode = Mode::softClip;
    double sr = 44100.0;
    float lookahead = 220.5f;
    float thrDb = 0.0f, thrLin = 1.0f;
};
} // namespace pakku
