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
