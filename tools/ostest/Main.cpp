#include <juce_dsp/juce_dsp.h>
#include <cmath>
using namespace juce;

static double aliasLevel (bool useOversampling, int factorExponent,
                          dsp::Oversampling<float>::FilterType type)
{
    constexpr int SR = 48000, N = 24000, B = 512;
    AudioBuffer<float> buf (1, N);
    for (int i = 0; i < N; ++i)
        buf.setSample (0, i, 2.0f * std::sin (MathConstants<float>::twoPi * 15000.0f * i / SR));

    std::unique_ptr<dsp::Oversampling<float>> os;
    if (useOversampling)
    {
        os = std::make_unique<dsp::Oversampling<float>> (1, factorExponent, type, true, true);
        os->initProcessing ((size_t) B);
        os->reset();
    }

    auto clip = [] (dsp::AudioBlock<float>& b)
    {
        for (size_t c = 0; c < b.getNumChannels(); ++c)
        {
            auto* d = b.getChannelPointer (c);
            for (size_t i = 0; i < b.getNumSamples(); ++i)
            {
                const auto a = std::abs (d[i]);
                if (a > 1.0f) d[i] = std::copysign (1.0f + std::tanh (a - 1.0f), d[i]);
            }
        }
    };

    for (int pos = 0; pos < N; pos += B)
    {
        const auto n = jmin (B, N - pos);
        AudioBuffer<float> slice (buf.getArrayOfWritePointers(), 1, pos, n);
        dsp::AudioBlock<float> blk (slice);

        if (os)
        {
            auto up = os->processSamplesUp (blk);
            clip (up);
            os->processSamplesDown (blk);
        }
        else clip (blk);
    }

    // energy at 3 kHz relative to the fundamental
    const int start = 8000, len = 8192;
    dsp::FFT fft (13);
    HeapBlock<float> fd (len * 2, true);
    for (int i = 0; i < len; ++i)
        fd[i] = buf.getSample (0, start + i) * (0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * i / (len - 1)));
    fft.performFrequencyOnlyForwardTransform (fd);

    auto bin = [&] (double hz) { return (int) std::round (hz * len / SR); };
    const auto f15 = fd[bin (15000)], f3 = fd[bin (3000)];
    return 20.0 * std::log10 ((f3 + 1e-12) / (f15 + 1e-12));
}

int main()
{
    using OS = dsp::Oversampling<float>;
    std::cout << "aliasing at 3 kHz, relative to the 15 kHz fundamental:\n\n";
    std::cout << "  sem oversampling ............... "
              << String (aliasLevel (false, 0, OS::filterHalfBandPolyphaseIIR), 2) << " dBc\n";

    for (auto [name, type] : { std::pair { "polyphase IIR", OS::filterHalfBandPolyphaseIIR },
                               std::pair { "FIR equiripple", OS::filterHalfBandFIREquiripple } })
        for (int e : { 1, 2, 3 })
            std::cout << "  " << name << "  2^" << e << " = " << (1 << e) << "x ......... "
                      << String (aliasLevel (true, e, type), 2) << " dBc\n";
    return 0;
}
