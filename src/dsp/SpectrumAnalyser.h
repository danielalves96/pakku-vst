#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

namespace pakku
{
/*  Analyser for the display. Audio only drops samples into a ring and raises
    a flag; the FFT runs on the interface thread. Nothing expensive ever
    happens on the audio path.                                             */
class SpectrumAnalyser
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder;   // 2048
    static constexpr int numBins  = fftSize / 2;

    SpectrumAnalyser() : fft (fftOrder),
                         window (fftSize, juce::dsp::WindowingFunction<float>::hann) {}

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        fifo.fill (0.0f);
        fifoIndex = 0;
        ready.store (false);
        smoothed.fill (-100.0f);
    }

    /** Called from audio, one sample per frame. */
    inline void push (float sample) noexcept
    {
        if (fifoIndex == fftSize)
        {
            if (! ready.load (std::memory_order_acquire))
            {
                std::copy (fifo.begin(), fifo.end(), pending.begin());
                ready.store (true, std::memory_order_release);
            }
            fifoIndex = 0;
        }
        fifo[(size_t) fifoIndex++] = sample;
    }

    /** Called from the interface. Returns true if a new frame was ready. */
    bool update()
    {
        if (! ready.load (std::memory_order_acquire))
            return false;

        std::copy (pending.begin(), pending.end(), scratch.begin());
        std::fill (scratch.begin() + fftSize, scratch.end(), 0.0f);
        ready.store (false, std::memory_order_release);

        window.multiplyWithWindowingTable (scratch.data(), fftSize);
        fft.performFrequencyOnlyForwardTransform (scratch.data());

        for (int i = 0; i < numBins; ++i)
        {
            const auto mag = scratch[(size_t) i] / (float) numBins;
            const auto db  = juce::Decibels::gainToDecibels (mag, -100.0f);
            // gentle decay so the drawing does not flicker
            smoothed[(size_t) i] = db > smoothed[(size_t) i]
                                 ? db
                                 : smoothed[(size_t) i] + (db - smoothed[(size_t) i]) * 0.35f;
        }
        return true;
    }

    float binToHz (int bin) const { return (float) (bin * sr / fftSize); }
    float magnitudeDb (int bin) const { return smoothed[(size_t) bin]; }

private:
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    std::array<float, fftSize> fifo {}, pending {};
    std::array<float, fftSize * 2> scratch {};
    std::array<float, numBins> smoothed {};

    int fifoIndex = 0;
    std::atomic<bool> ready { false };
    double sr = 48000.0;
};
} // namespace pakku
