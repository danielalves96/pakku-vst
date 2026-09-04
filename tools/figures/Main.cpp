/*  Generates the manual's figure data from the plugin itself.

    Nothing here is drawn by hand: every curve comes out of an offline render
    through PakkuAudioProcessor with known parameters. If the DSP changes,
    run this again and the manual's figures follow.                        */
#include "../../src/PluginProcessor.h"
#include <fstream>

using namespace juce;

namespace
{
constexpr double SR = 48000.0;
constexpr int    B  = 512;

struct Bench
{
    PakkuAudioProcessor proc;

    void reset() { proc.prepareToPlay (SR, B); }

    void set (const String& id, float realValue)
    {
        if (auto* p = dynamic_cast<RangedAudioParameter*> (proc.apvts.getParameter (id)))
            p->setValueNotifyingHost (p->convertTo0to1 (realValue));
    }

    /** Clears everything that would colour the result, leaving only what the
        test is meant to show. */
    void neutral()
    {
        for (auto* p : proc.getParameters())
            if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
            {
                rp->setValueNotifyingHost (rp->getDefaultValue());
            }
        set (pid::presence, 0.0f);
        set (pid::air, 0.0f);
        set (pid::nyc, 0.0f);
        set (pid::threshold, 0.0f);
    }

    void render (std::vector<float>& buf)
    {
        MidiBuffer midi;
        AudioBuffer<float> block (2, B);

        for (size_t pos = 0; pos < buf.size(); pos += B)
        {
            const auto n = (int) jmin ((size_t) B, buf.size() - pos);
            block.setSize (2, n, false, false, true);
            for (int c = 0; c < 2; ++c)
                block.copyFrom (c, 0, buf.data() + pos, n);

            proc.processBlock (block, midi);

            for (int i = 0; i < n; ++i)
                buf[pos + (size_t) i] = block.getSample (0, i);
        }
    }
};

/** A short hit: a fast transient over a decaying body. */
std::vector<float> drumHit (size_t n, float amplitude)
{
    std::vector<float> v (n, 0.0f);
    Random rnd (7);

    for (size_t i = 0; i < n; ++i)
    {
        const auto t = (float) i / (float) SR;
        const auto click = std::exp (-t * 900.0f) * (rnd.nextFloat() * 2.0f - 1.0f);
        const auto body  = std::exp (-t * 26.0f)
                           * std::sin (MathConstants<float>::twoPi * 180.0f * t);
        const auto tail  = std::exp (-t * 7.0f) * 0.35f
                           * std::sin (MathConstants<float>::twoPi * 90.0f * t);
        v[i] = amplitude * (0.55f * click + 0.7f * body + tail);
    }
    return v;
}

void writeCsv (const File& f, const String& header,
               const std::vector<std::vector<float>>& cols)
{
    std::ofstream out (f.getFullPathName().toStdString());
    out << header.toStdString() << "\n";

    const auto rows = cols.front().size();
    for (size_t i = 0; i < rows; ++i)
    {
        for (size_t c = 0; c < cols.size(); ++c)
            out << (c ? "," : "") << cols[c][i];
        out << "\n";
    }
    std::cout << "  " << f.getFileName() << "  (" << rows << " rows)\n";
}
} // namespace

int main (int argc, char* argv[])
{
    ScopedJuceInitialiser_GUI juceInit;
    ArgumentList args (argc, argv);

    const File out (args.containsOption ("--out")
                        ? args.getValueForOption ("--out")
                        : String ("docs/manual/data"));
    out.createDirectory();

    std::cout << "writing figure data to " << out.getFullPathName() << "\n";

    // -- 1. transient shaping: attack and sustain, up and down --
    {
        constexpr size_t n = 16384;
        const auto amp = 0.06f;                 // -24 dBFS: clear of the ceiling

        auto run = [&] (const String& id, float value)
        {
            Bench b;
            b.reset();
            b.neutral();
            b.set (pid::bandMode, 1.0f);        // full range
            if (id.isNotEmpty()) b.set (id, value);

            auto sig = drumHit (n, amp);
            b.render (sig);
            return sig;
        };

        const auto dry   = drumHit (n, amp);
        const auto flat  = run ({}, 0.0f);
        const auto attUp = run (pid::transSingle,  0.85f);
        const auto attDn = run (pid::transSingle, -0.85f);
        const auto lenUp = run (pid::lenSingle,    0.85f);
        const auto lenDn = run (pid::lenSingle,   -0.85f);

        std::vector<float> t (n);
        for (size_t i = 0; i < n; ++i) t[i] = (float) i / (float) SR * 1000.0f;

        writeCsv (out.getChildFile ("transients.csv"),
                  "ms,dry,attack_up,attack_down,length_up,length_down",
                  { t, flat, attUp, attDn, lenUp, lenDn });
        ignoreUnused (dry);
    }

    // -- 2. crossover response, band by band --
    {
        /*  Measured as a noise transfer function, not from an impulse.

            The band sum passes through an allpass, which spreads an impulse
            over time without removing energy: the peak collapses and the
            reading lies. The ratio between the averaged output and input
            spectra cares about neither phase nor latency.                  */
        constexpr int order = 13;
        constexpr int fftSize = 1 << order;         // 8192
        constexpr int blocks = 24;
        constexpr size_t n = (size_t) fftSize * (blocks + 1);

        dsp::FFT fft (order);

        std::vector<float> noise (n);
        Random rnd (2024);
        for (size_t i = 0; i < n; ++i)
            noise[i] = 0.05f * (rnd.nextFloat() * 2.0f - 1.0f);

        std::vector<float> window (fftSize);
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi
                                                         * (float) i / (float) (fftSize - 1));

        auto spectrum = [&] (const std::vector<float>& sig, size_t offset)
        {
            std::vector<double> acc ((size_t) fftSize / 2, 0.0);

            for (int b = 0; b < blocks; ++b)
            {
                HeapBlock<float> fd ((size_t) fftSize * 2, true);
                const auto base = offset + (size_t) b * (size_t) fftSize;

                for (int i = 0; i < fftSize; ++i)
                    fd[i] = sig[base + (size_t) i] * window[(size_t) i];

                fft.performFrequencyOnlyForwardTransform (fd);

                for (int i = 0; i < fftSize / 2; ++i)
                    acc[(size_t) i] += (double) fd[i] * fd[i];
            }

            for (auto& v : acc) v = std::sqrt (v / blocks);
            return acc;
        };

        // the latency is constant and known; aligning the windows avoids
        // comparing different stretches of the noise
        const auto latency = (size_t) 240;
        const auto reference = spectrum (noise, 0);

        auto band = [&] (int solo)
        {
            Bench b;
            b.reset();
            b.neutral();
            b.set (pid::bandMode, 0.0f);        // multiband
            b.set (pid::xoverLow, 800.0f);
            b.set (pid::xoverHigh, 8000.0f);
            for (int i = 0; i < pakku::numBands; ++i)
                b.set (pid::solo (i), solo == i ? 1.0f : 0.0f);

            auto sig = noise;
            b.render (sig);

            const auto out = spectrum (sig, latency);

            std::vector<float> mag (out.size());
            for (size_t i = 0; i < out.size(); ++i)
                mag[i] = (float) Decibels::gainToDecibels (out[i] / jmax (1e-12, reference[i]), -90.0);
            return mag;
        };

        const auto low = band (0), mid = band (1), high = band (2), sum = band (-1);

        std::vector<float> hz (low.size());
        for (size_t i = 0; i < hz.size(); ++i)
            hz[i] = (float) ((double) i * SR / fftSize);

        writeCsv (out.getChildFile ("crossover.csv"), "hz,low,mid,high,sum",
                  { hz, low, mid, high, sum });
    }

    // -- 3. ceiling curve: input against output, in both modes --
    {
        std::vector<float> in, limit, clip;

        for (float db = -36.0f; db <= 12.01f; db += 0.5f)
        {
            const auto amp = Decibels::decibelsToGain (db);

            auto peak = [&] (float mode)
            {
                Bench b;
                b.reset();
                b.neutral();
                b.set (pid::bandMode, 1.0f);
                b.set (pid::clipMode, mode);

                constexpr size_t n = 12000;
                std::vector<float> sig (n);
                for (size_t i = 0; i < n; ++i)
                    sig[i] = amp * std::sin (MathConstants<float>::twoPi * 220.0f
                                             * (float) i / (float) SR);
                b.render (sig);

                float p = 0.0f;
                for (size_t i = n / 2; i < n; ++i) p = jmax (p, std::abs (sig[i]));
                return Decibels::gainToDecibels (p, -120.0f);
            };

            in.push_back (db);
            limit.push_back (peak (0.0f));
            clip.push_back (peak (1.0f));
        }

        writeCsv (out.getChildFile ("ceiling.csv"), "in_db,limiter_db,softclip_db",
                  { in, limit, clip });
    }

    std::cout << "done\n";
    return 0;
}
