#pragma once
#include "Theme.h"

namespace pakku
{
/*  Segmented vertical meter with a dB scale.

    Alongside the instantaneous level it holds a peak marker: a thin line that
    jumps up at once and falls slowly. Without it, short transients flash and
    vanish before the eye registers how far they went.                       */
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    explicit LevelMeter (std::atomic<float>& src, bool scaleOnLeft)
        : source (src), scaleLeft (scaleOnLeft)
    {
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        auto scaleArea = scaleLeft ? r.removeFromLeft (26.0f) : r.removeFromRight (26.0f);
        r = r.reduced (1.0f, 0.0f);

        theme::well (g, r, 4.0f);

        const auto inner = r.reduced (3.5f);
        constexpr int segments = 28;
        const auto segH = inner.getHeight() / (float) segments;
        const auto lit = jmap (level, minDb, 0.0f, 0.0f, (float) segments);
        const auto peakSeg = jmap (peak, minDb, 0.0f, 0.0f, (float) segments);

        for (int i = 0; i < segments; ++i)
        {
            const Rectangle<float> seg (inner.getX(),
                                        inner.getBottom() - (i + 1) * segH + 0.9f,
                                        inner.getWidth(), segH - 1.8f);

            const auto frac = i / (float) segments;
            const auto on = (float) i < lit;

            auto c = theme::accent;
            if (frac > 0.93f)      c = theme::hot;
            else if (frac > 0.82f) c = theme::warn;

            if (! on)
            {
                // unlit: near black, with a trace of the colour it would take
                g.setColour (c.withAlpha (0.06f).overlaidWith (Colours::black.withAlpha (0.62f)));
                g.fillRoundedRectangle (seg, 1.2f);
                continue;
            }

            g.setGradientFill (ColourGradient (c.brighter (0.35f), seg.getX(), seg.getY(),
                                               c.darker (0.25f),   seg.getX(), seg.getBottom(), false));
            g.fillRoundedRectangle (seg, 1.2f);

            // the topmost lit segment spills a little light
            if ((float) (i + 1) >= lit)
            {
                g.setColour (c.withAlpha (0.28f));
                g.fillRoundedRectangle (seg.expanded (2.0f, 1.6f), 2.5f);
            }
        }

        // peak hold
        if (peak > minDb + 0.5f)
        {
            const auto y = inner.getBottom() - peakSeg * segH;
            auto c = peak > -3.0f ? theme::hot : (peak > -9.0f ? theme::warn : theme::accentHi);
            g.setColour (c);
            g.fillRect (inner.getX(), jlimit (inner.getY(), inner.getBottom() - 2.0f, y),
                        inner.getWidth(), 2.0f);
        }

        // scale
        g.setFont (theme::label (9.0f));
        for (int db : { 0, -6, -12, -18, -24, -36, -48, -60 })
        {
            const auto y = jmap ((float) db, minDb, 0.0f, inner.getBottom(), inner.getY());
            g.setColour (theme::textDim.withAlpha (db == 0 ? 0.95f : 0.7f));
            g.drawText (String (db), scaleArea.withY (y - 6.0f).withHeight (12.0f).reduced (3.0f, 0.0f),
                        scaleLeft ? Justification::centredRight : Justification::centredLeft);

            g.setColour (theme::strokeSoft.brighter (0.3f));
            const auto tx = scaleLeft ? r.getX() : r.getRight();
            g.drawLine (tx, y, tx + (scaleLeft ? -3.0f : 3.0f), y, 1.0f);
        }
    }

private:
    void timerCallback() override
    {
        const auto db = juce::Decibels::gainToDecibels (source.load(), minDb);

        // instant rise, gentle fall — a peak reading with decay
        level = db > level ? db : level + (db - level) * 0.25f;

        if (db >= peak) { peak = db; peakHold = 22; }        // ~0,7 s parado
        else if (--peakHold <= 0) peak = juce::jmax (minDb, peak - 1.1f);

        repaint();
    }

    std::atomic<float>& source;
    bool scaleLeft;
    float level = -60.0f, peak = -60.0f;
    int peakHold = 0;
    static constexpr float minDb = -60.0f;
};
} // namespace pakku
