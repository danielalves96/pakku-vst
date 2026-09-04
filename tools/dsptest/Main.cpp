#include "../../src/PluginProcessor.h"
#include <cmath>
using namespace juce;

static void testPresets()
{
    PakkuAudioProcessor proc;
    auto& pm = proc.presets;
    auto val = [&] (const juce::String& id) { return proc.apvts.getRawParameterValue (id)->load(); };

    std::cout << "\n=== FACTORY (.pkku embedded in the binary) ===\n";
    const auto names = pm.getFactoryNames();

    int n = 0;
    for (const auto& name : names)
    {
        pm.load (name);
        if (n < 5 || n >= (int) names.size() - 2)
            std::cout << "  " << name.paddedRight (' ', 22)
                      << (val (pid::bandMode) < 0.5f ? "multi " : "single")
                      << " | xover " << String (val (pid::xoverLow), 1).paddedLeft (' ', 7)
                      << " /" << String (val (pid::xoverHigh), 1).paddedLeft (' ', 8)
                      << " | thr " << String (val (pid::threshold), 2).paddedLeft (' ', 7)
                      << " | " << (val (pid::clipMode) > 0.5f ? "soft clip" : "limit") << "\n";
        else if (n == 5) std::cout << "  ...\n";
        ++n;
    }
    std::cout << "  total: " << names.size() << " factory presets\n";

    //==========================================================================
    std::cout << "\n=== USER PRESET ROUND TRIP ===\n";

    pm.load (names[0]);

    // move a few parameters so there is something distinct to save
    struct { const char* id; float norm; } tweaks[] = {
        { pid::nyc,        0.42f }, { pid::air,      0.77f },
        { pid::threshold,  0.30f }, { pid::mix,      0.55f },
        { pid::xoverLow,   0.62f }, { pid::bandMode, 1.0f  },
    };
    for (const auto& t : tweaks)
        if (auto* p = proc.apvts.getParameter (t.id))
            p->setValueNotifyingHost (t.norm);

    std::map<String, float> before;
    for (auto* p : proc.getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
            before[rp->paramID] = proc.apvts.getRawParameterValue (rp->paramID)->load();

    const auto saved = pm.save ("Meu Preset de Teste");
    std::cout << "  written: " << (saved ? "OK" : "FAILED") << "\n";
    std::cout << "  shows up in the user list: "
              << (pm.getUserNames().contains ("Meu Preset de Teste") ? "OK" : "FAILED") << "\n";

    pm.load (names[1]);                       // leave the preset to force a reload
    pm.load ("Meu Preset de Teste");

    int diff = 0; float worst = 0.0f; String worstId;
    for (const auto& [id, v] : before)
    {
        const auto now = proc.apvts.getRawParameterValue (id)->load();
        const auto d = std::abs (now - v);
        if (d > worst) { worst = d; worstId = id; }
        if (d > std::max (0.02f, std::abs (v) * 0.001f)) ++diff;
    }
    std::cout << "  parameters that differ: " << diff << (diff == 0 ? "   OK" : "   FAILED") << "\n";
    std::cout << "  largest difference: " << String (worst, 6) << "  (" << worstId << ")\n";

    // must refuse to save over a factory name
    std::cout << "  blocks overwriting factory: "
              << (pm.save (names[0]) ? "FALHOU" : "OK") << "\n";

    pm.remove ("Meu Preset de Teste");
    std::cout << "  deleted: "
              << (pm.getUserNames().contains ("Meu Preset de Teste") ? "FALHOU" : "OK") << "\n";

    // a corrupt file must neither crash nor load garbage
    auto bad = pakku::PresetManager::getUserDirectory().getChildFile ("corrompido.pkku");
    bad.replaceWithText ("isto nao e um preset");
    pm.load ("corrompido");
    std::cout << "  invalid file ignored: "
              << (pm.getCurrent() != "corrompido" ? "OK" : "FALHOU") << "\n";
    bad.deleteFile();
}

int main()
{
    ScopedJuceInitialiser_GUI init;
    testPresets();
    constexpr int SR = 48000, N = 24000, B = 512;

    PakkuAudioProcessor proc;
    proc.setPlayConfigDetails (2, 2, SR, B);
    proc.prepareToPlay (SR, B);

    //==========================================================================
    /*  Everything the DSP does is derived from the sample rate at prepare
        time: filter coefficients, envelope constants, the ceiling's lookahead.
        A constant left in samples rather than in seconds would pass at 48 kHz
        and drift everywhere else, so each rate is checked on its own.       */
    std::cout << "\n=== SAMPLE RATE COVERAGE ===\n";

    for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        PakkuAudioProcessor p2;
        p2.prepareToPlay (rate, 512);

        auto setP = [&] (const String& id, float v)
        {
            if (auto* rp = dynamic_cast<RangedAudioParameter*> (p2.apvts.getParameter (id)))
                rp->setValueNotifyingHost (rp->convertTo0to1 (v));
        };
        for (auto* prm : p2.getParameters())
            if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
                rp->setValueNotifyingHost (rp->getDefaultValue());
        setP (pid::presence, 0.0f);
        setP (pid::bandMode, 0.0f);          // multiband
        setP (pid::xoverLow, 800.0f);
        setP (pid::xoverHigh, 8000.0f);

        // transfer function at the two crossover points, from noise
        const int n = 1 << 15;
        std::vector<float> noise ((size_t) n);
        Random rnd (11);
        for (auto& v : noise) v = 0.05f * (rnd.nextFloat() * 2.0f - 1.0f);

        auto sig = noise;
        {
            MidiBuffer midi;
            AudioBuffer<float> blk (2, 512);
            for (size_t pos = 0; pos < sig.size(); pos += 512)
            {
                const auto k = (int) jmin ((size_t) 512, sig.size() - pos);
                blk.setSize (2, k, false, false, true);
                for (int c = 0; c < 2; ++c) blk.copyFrom (c, 0, sig.data() + pos, k);
                p2.processBlock (blk, midi);
                for (int i = 0; i < k; ++i) sig[pos + (size_t) i] = blk.getSample (0, i);
            }
        }

        bool finite = true;
        for (auto v : sig) if (! std::isfinite (v)) { finite = false; break; }

        dsp::FFT fft (14);
        auto spec = [&] (const std::vector<float>& x, size_t off)
        {
            HeapBlock<float> fd (1 << 15, true);
            for (int i = 0; i < (1 << 14); ++i)
                fd[i] = x[off + (size_t) i]
                        * (0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * i / ((1 << 14) - 1)));
            fft.performFrequencyOnlyForwardTransform (fd);
            std::vector<float> m ((size_t) (1 << 13));
            for (size_t i = 0; i < m.size(); ++i) m[i] = fd[i];
            return m;
        };

        const auto latency = (size_t) p2.getLatencySamples();
        const auto ref = spec (noise, 0), out = spec (sig, latency);

        auto atHz = [&] (double hz)
        {
            const auto bin = (size_t) std::round (hz * (1 << 14) / rate);
            if (bin >= out.size()) return 0.0f;
            return (float) Decibels::gainToDecibels (out[bin] / jmax (1.0e-9f, ref[bin]), -90.0f);
        };

        const auto latencyMs = 1000.0 * p2.getLatencySamples() / rate;

        std::cout << "  " << String ((int) rate).paddedRight (' ', 8)
                  << "sum @800Hz " << String (atHz (800.0), 2).paddedLeft (' ', 6)
                  << " dB | @8kHz " << String (atHz (8000.0), 2).paddedLeft (' ', 6)
                  << " dB | @1kHz " << String (atHz (1000.0), 2).paddedLeft (' ', 6)
                  << " dB | latency " << String (latencyMs, 2) << " ms"
                  << (finite ? "" : "   NON-FINITE OUTPUT") << "\n";
    }

    std::cout << "reported latency ..... " << proc.getLatencySamples() << " samples\n";
    std::cout << "ceiling lookahead (float) ..... "
              << String (proc.getOversamplerLatency(), 6) << "\n";

    auto set = [&] (const char* id, float norm)
    {
        if (auto* p = proc.apvts.getParameter (id))
            p->setValueNotifyingHost (norm);
    };
    set (pid::bandMode, 1.0f);          // single
    set (pid::threshold, 1.0f);         // 0 dB
    set (pid::mix, 1.0f);

  for (float mode : { 0.0f, 1.0f })
  {
    set (pid::clipMode, mode);
    proc.prepareToPlay (SR, B);   // clean state between the two measurements

    std::cout << "\n--- Ceiling = "
              << proc.apvts.getParameter (pid::clipMode)->getCurrentValueAsText() << " ---\n";

    AudioBuffer<float> buf (2, N);
    for (int i = 0; i < N; ++i)
    {
        const auto s = 2.0f * std::sin (MathConstants<float>::twoPi * 15000.0f * i / SR);
        buf.setSample (0, i, s); buf.setSample (1, i, s);
    }

    MidiBuffer midi;
    for (int pos = 0; pos < N; pos += B)
    {
        const auto n = jmin (B, N - pos);
        AudioBuffer<float> slice (buf.getArrayOfWritePointers(), 2, pos, n);
        proc.processBlock (slice, midi);
    }

    const int start = 8000, len = 8192;
    dsp::FFT fft (13);
    HeapBlock<float> fd (len * 2, true);
    for (int i = 0; i < len; ++i)
        fd[i] = buf.getSample (0, start + i)
                * (0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * i / (len - 1)));
    fft.performFrequencyOnlyForwardTransform (fd);

    auto bin = [&] (double hz) { return (int) std::round (hz * len / SR); };
    const auto f15 = fd[bin (15000)];
    std::cout << "fundamental .......... "
              << String (20.0 * std::log10 (f15 / (len / 2.0) + 1e-12), 2) << " dB\n";
    std::cout << "alias 3 kHz .......... "
              << String (20.0 * std::log10 ((fd[bin (3000)] + 1e-12) / (f15 + 1e-12)), 2) << " dBc\n";
    std::cout << "alias 9 kHz .......... "
              << String (20.0 * std::log10 ((fd[bin (9000)] + 1e-12) / (f15 + 1e-12)), 2) << " dBc\n";
  }
    return 0;
}
