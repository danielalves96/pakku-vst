#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>

namespace pakku
{
/*  Peak ring for the display: written by audio, read by the interface.

    No locks. Audio only advances writePos, the interface only reads. The
    occasional one-pixel tear is invisible in the drawing and a fair price for
    never blocking the audio thread.

    Each column keeps the minimum and maximum of its slice, so the drawn
    waveform keeps its peaks even when the material is heavily decimated.  */
template <int NumLanes, int NumColumns = 1024>
class ScopeBuffer
{
public:
    static constexpr int numLanes   = NumLanes;
    static constexpr int numColumns = NumColumns;

    void prepare (double sampleRate, double secondsAcross = 2.0)
    {
        samplesPerColumn = juce::jmax (1, (int) (sampleRate * secondsAcross / numColumns));
        clear();
    }

    void clear()
    {
        for (auto& lane : lanes)
            for (int i = 0; i < numColumns; ++i)
            {
                lane.mins[i].store (0.0f, std::memory_order_relaxed);
                lane.maxs[i].store (0.0f, std::memory_order_relaxed);
            }

        counter = 0;
        for (auto& lane : lanes) { lane.runMin = 0.0f; lane.runMax = 0.0f; }
        writePos.store (0, std::memory_order_release);
    }

    /** One sample per lane. Call once per audio frame. */
    inline void push (const float* values) noexcept
    {
        for (int l = 0; l < numLanes; ++l)
        {
            lanes[l].runMin = juce::jmin (lanes[l].runMin, values[l]);
            lanes[l].runMax = juce::jmax (lanes[l].runMax, values[l]);
        }

        if (++counter < samplesPerColumn)
            return;

        const auto w = writePos.load (std::memory_order_relaxed);

        for (int l = 0; l < numLanes; ++l)
        {
            lanes[l].mins[w].store (lanes[l].runMin, std::memory_order_relaxed);
            lanes[l].maxs[w].store (lanes[l].runMax, std::memory_order_relaxed);
            lanes[l].runMin = 0.0f;
            lanes[l].runMax = 0.0f;
        }

        writePos.store ((w + 1) % numColumns, std::memory_order_release);
        counter = 0;
    }

    /** Reads a lane already unrolled: index 0 is the oldest column. */
    void read (int lane, float* outMin, float* outMax) const noexcept
    {
        const auto w = writePos.load (std::memory_order_acquire);

        for (int i = 0; i < numColumns; ++i)
        {
            const auto src = (w + i) % numColumns;
            outMin[i] = lanes[lane].mins[src].load (std::memory_order_relaxed);
            outMax[i] = lanes[lane].maxs[src].load (std::memory_order_relaxed);
        }
    }

private:
    struct Lane
    {
        std::array<std::atomic<float>, numColumns> mins {}, maxs {};
        float runMin = 0.0f, runMax = 0.0f;
    };

    std::array<Lane, numLanes> lanes;
    std::atomic<int> writePos { 0 };
    int counter = 0, samplesPerColumn = 64;
};
} // namespace pakku
