#pragma once
#include "Theme.h"
#include "../dsp/ScopeBuffer.h"
#include "../dsp/SpectrumAnalyser.h"
#include "../Parameters.h"
#include <functional>

namespace pakku
{
/** Small round button — the M / S of each band. */
class RoundToggle : public juce::Component
{
public:
    RoundToggle (juce::AudioProcessorValueTreeState& s, const juce::String& id,
                 juce::String glyph, juce::Colour onColour)
        : letter (std::move (glyph)), on (onColour),
          attachment (*s.getParameter (id), [this] (float v) { state = v > 0.5f; repaint(); })
    {
        attachment.sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (state ? on : theme::bgWell);
        g.fillEllipse (r);
        g.setColour (state ? on.brighter (0.3f) : theme::stroke);
        g.drawEllipse (r, 1.0f);
        g.setColour (state ? theme::bgDeep : theme::textDim);
        g.setFont (theme::label (r.getHeight() * 0.62f, true));
        g.drawText (letter, getLocalBounds(), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        attachment.setValueAsCompleteGesture (state ? 0.0f : 1.0f);
    }

private:
    juce::String letter;
    juce::Colour on;
    bool state = false;
    juce::ParameterAttachment attachment;
};

/*  Central display, in two halves:

      top     — scrolling waveform with the threshold line
      middle  — M and S per band, centred horizontally in each one
      bottom  — spectrum with the draggable crossover dividers

    Clicking a region of the spectrum selects that band; the faders on either
    side then edit the selected one.                                       */
class Display : public juce::Component, private juce::Timer
{
public:
    using Buffer = ScopeBuffer<1 + numBands>;

    Display (juce::AudioProcessorValueTreeState& state, Buffer& b, SpectrumAnalyser& a,
             std::function<float()> thresholdDb, std::function<bool()> multi)
        : apvts (state), buffer (b), analyser (a),
          getThresholdDb (std::move (thresholdDb)), isMulti (std::move (multi))
    {
        for (int i = 0; i < numBands; ++i)
        {
            mutes[i] = std::make_unique<RoundToggle> (state, pid::mute (i), "M",
                                                      juce::Colour (0xffe8c04a));
            solos[i] = std::make_unique<RoundToggle> (state, pid::solo (i), "S", theme::accent);
            addAndMakeVisible (*mutes[i]);
            addAndMakeVisible (*solos[i]);
        }

        lowParam       = apvts.getParameter (pid::xoverLow);
        highParam      = apvts.getParameter (pid::xoverHigh);
        thresholdParam = apvts.getParameter (pid::threshold);

        startTimerHz (30);
    }

    /** Told when the user picks a different band in the spectrum. */
    std::function<void (int)> onBandSelected;
    int getSelectedBand() const { return selectedBand; }

    struct Areas { juce::Rectangle<float> wave, strip, spec; };

    /*  One source for the geometry — paint and resized need the same one.

        In single-band mode there is no crossover to show and no band to
        choose, so the spectrum steps aside and the waveform takes the whole
        height.                                                              */
    Areas computeAreas() const
    {
        const auto area = getLocalBounds().toFloat().reduced (8.0f);
        Areas a;

        if (! isMulti())
        {
            a.wave  = area;
            a.strip = area.withHeight (0.0f).withY (area.getBottom());
            a.spec  = a.strip;
            return a;
        }

        a.wave  = area.withHeight (area.getHeight() * 0.44f);
        a.strip = area.withY (a.wave.getBottom()).withHeight (22.0f);
        a.spec  = area.withY (a.strip.getBottom())
                      .withHeight (area.getBottom() - a.strip.getBottom());
        return a;
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        theme::well (g, r, 6.0f);

        const auto a = computeAreas();
        drawWaveform (g, a.wave);

        if (isMulti())
        {
            drawSpectrum (g, a.spec);
            g.setColour (theme::strokeSoft);
            g.drawHorizontalLine ((int) a.strip.getY(), a.wave.getX(), a.wave.getRight());
        }
    }

    void resized() override
    {
        layoutBandButtons();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (hitThreshold (e.position))
        {
            draggingThreshold = true;
            thresholdParam->beginChangeGesture();
            return;
        }

        dragging = hitDivider (e.position.x);

        if (dragging < 0 && computeAreas().spec.contains (e.position))
        {
            const auto f = xToFreq (e.position.x);
            const auto band = f < lowValue() ? 0 : (f < highValue() ? 1 : 2);
            selectedBand = band;
            if (onBandSelected) onBandSelected (band);
            repaint();
        }

        if (dragging >= 0)
            (dragging == 0 ? lowParam : highParam)->beginChangeGesture();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (draggingThreshold)
        {
            setThresholdFromY (e.position.y);
            return;
        }

        if (dragging < 0) return;

        auto* p = dragging == 0 ? lowParam : highParam;
        const auto f = juce::jlimit (25.0f, 19000.0f, xToFreq (e.position.x));
        p->setValueNotifyingHost (p->convertTo0to1 (f));
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (draggingThreshold)
        {
            thresholdParam->endChangeGesture();
            draggingThreshold = false;
            repaint();
            return;
        }

        if (dragging >= 0)
            (dragging == 0 ? lowParam : highParam)->endChangeGesture();
        dragging = -1;
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto onThreshold = hitThreshold (e.position);
        if (onThreshold != hoverThreshold) { hoverThreshold = onThreshold; repaint(); }

        setMouseCursor (onThreshold        ? juce::MouseCursor::UpDownResizeCursor
                        : hitDivider (e.position.x) >= 0
                                           ? juce::MouseCursor::LeftRightResizeCursor
                                           : juce::MouseCursor::NormalCursor);
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hoverThreshold) { hoverThreshold = false; repaint(); }
    }

private:
    void timerCallback() override { analyser.update(); layoutBandButtons(); repaint(); }

    float lowValue()  const { return apvts.getRawParameterValue (pid::xoverLow)->load(); }
    float highValue() const { return apvts.getRawParameterValue (pid::xoverHigh)->load(); }

    float freqToX (float f) const
    {
        const auto a = computeAreas().spec;
        const auto t = std::log (juce::jmax (20.0f, f) / 20.0f) / std::log (1000.0f);
        return a.getX() + t * a.getWidth();
    }

    float xToFreq (float x) const
    {
        const auto a = computeAreas().spec;
        const auto t = (x - a.getX()) / juce::jmax (1.0f, a.getWidth());
        return 20.0f * std::pow (1000.0f, t);
    }

    /*  Geometry of the threshold line, shared by the drawing and the drag:
        the two have to agree on where the line is.                      */
    float thresholdY() const
    {
        const auto a = computeAreas().wave;
        const auto thr = juce::Decibels::decibelsToGain (getThresholdDb());
        return a.getCentreY() - thr * a.getHeight() * 0.44f;
    }

    bool hitThreshold (juce::Point<float> p) const
    {
        const auto a = computeAreas().wave;
        return a.contains (p) && std::abs (p.y - thresholdY()) < 7.0f;
    }

    void setThresholdFromY (float y)
    {
        const auto a = computeAreas().wave;
        const auto half = a.getHeight() * 0.44f;
        const auto gain = juce::jlimit (0.001f, 1.0f, (a.getCentreY() - y) / half);
        const auto db = juce::jlimit (-50.0f, 0.0f,
                                      juce::Decibels::gainToDecibels (gain));

        thresholdParam->setValueNotifyingHost (thresholdParam->convertTo0to1 (db));
        repaint();
    }

    int hitDivider (float x) const
    {
        if (! isMulti()) return -1;
        if (std::abs (x - freqToX (lowValue()))  < 7.0f) return 0;
        if (std::abs (x - freqToX (highValue())) < 7.0f) return 1;
        return -1;
    }

    void layoutBandButtons()
    {
        const auto strip = computeAreas().strip;
        const auto y = strip.getY();
        const auto show = isMulti();

        const float edges[numBands + 1] = { 20.0f, lowValue(), highValue(), 19500.0f };

        for (int b = 0; b < numBands; ++b)
        {
            mutes[b]->setVisible (show);
            solos[b]->setVisible (show);
            if (! show) continue;

            const auto cx = (freqToX (edges[b]) + freqToX (edges[b + 1])) * 0.5f;
            mutes[b]->setBounds ((int) cx - 20, (int) y + 3, 17, 17);
            solos[b]->setBounds ((int) cx +  3, (int) y + 3, 17, 17);
        }
    }

    void drawWaveform (juce::Graphics& g, juce::Rectangle<float> a)
    {
        using namespace juce;
        std::array<float, Buffer::numColumns> mins {}, maxs {};
        buffer.read (0, mins.data(), maxs.data());

        const auto thrDb = getThresholdDb();
        const auto thr = Decibels::decibelsToGain (thrDb);
        const auto mid = a.getCentreY(), half = a.getHeight() * 0.44f;
        const auto step = a.getWidth() / (float) Buffer::numColumns;

        g.setColour (theme::strokeSoft.withAlpha (0.5f));
        for (int i = 1; i < 8; ++i)
            g.drawVerticalLine ((int) (a.getX() + a.getWidth() * i / 8.0f), a.getY(), a.getBottom());

        for (int i = 0; i < Buffer::numColumns; ++i)
        {
            const auto x = a.getX() + i * step;
            const auto lo = jlimit (-1.0f, 1.0f, mins[i]);
            const auto hi = jlimit (-1.0f, 1.0f, maxs[i]);
            const auto w = jmax (1.0f, step);

            g.setColour (theme::waveform.withAlpha (0.9f));
            g.fillRect (x, mid - hi * half, w, jmax (1.0f, (hi - lo) * half));

            if (hi > thr)
            {
                g.setColour (theme::accent);
                g.fillRect (x, mid - hi * half, w, jmax (1.0f, (hi - thr) * half));
            }
            if (lo < -thr)
            {
                g.setColour (theme::accent);
                g.fillRect (x, mid + thr * half, w, jmax (1.0f, (-lo - thr) * half));
            }
        }

        /*  Threshold line, draggable.

            It stops short of the dB reading and the labels sit above it, so
            the dashes never cross the text. Under the cursor it thickens and
            grows two handles, which is what announces it can be grabbed.    */
        const auto yThr = thresholdY();
        const auto grabbed = hoverThreshold || draggingThreshold;

        const float dashes[] = { 3.0f, 4.0f };
        if (grabbed)
        {
            g.setColour (theme::accentGlow);
            g.fillRect (a.getX(), yThr - 1.5f, a.getWidth(), 3.0f);
        }
        g.setColour (grabbed ? theme::accentHi : theme::accent.withAlpha (0.9f));
        g.drawDashedLine (Line<float> (a.getX(), yThr, a.getRight() - 88.0f, yThr),
                          dashes, 2, grabbed ? 1.8f : 1.0f);

        if (grabbed)
            for (int side = 0; side < 2; ++side)
            {
                const auto x = side == 0 ? a.getX() + 3.0f : a.getRight() - 6.0f;
                g.setColour (theme::accentHi);
                g.fillRoundedRectangle (x, yThr - 4.0f, 3.0f, 8.0f, 1.5f);
            }

        // if the line is pinned to the top, the label goes underneath it
        const auto labelY = (yThr - a.getY() < 17.0f) ? yThr + 3.0f : yThr - 17.0f;

        /*  The labels land on the waveform, which is white and opaque. A dark
            plate behind them keeps them legible over a peak.              */
        g.setFont (theme::label (10.0f, true));

        auto plate = [&] (const String& text, Rectangle<float> box, Justification just)
        {
            const auto w = GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 10.0f;
            const auto back = just.testFlags (Justification::right)
                                ? box.removeFromRight (w) : box.removeFromLeft (w);
            g.setColour (theme::bgDeep.withAlpha (0.78f));
            g.fillRoundedRectangle (back.expanded (0.0f, 1.0f), 3.0f);
            g.setColour (theme::accent.withAlpha (0.9f));
            g.drawText (text, back, Justification::centred);
        };

        plate ("THRESHOLD", a.withY (labelY).withHeight (13.0f)
                             .withX (a.getX() + 4.0f).withWidth (100.0f),
               Justification::left);
        plate (String (thrDb, 2) + " dB",
               a.withY (labelY).withHeight (13.0f)
                .withX (a.getRight() - 94.0f).withWidth (90.0f),
               Justification::right);
    }

    void drawSpectrum (juce::Graphics& g, juce::Rectangle<float> a)
    {
        using namespace juce;
        const auto multi = isMulti();
        const float edges[numBands + 1] = { 20.0f, lowValue(), highValue(), 19500.0f };

        // highlight for the selected band
        if (multi)
        {
            const auto x0 = freqToX (edges[selectedBand]);
            const auto x1 = freqToX (edges[selectedBand + 1]);
            const juce::Rectangle<float> band (x0, a.getY(), x1 - x0, a.getHeight());

            /*  The chosen band has to stand out: the other two retreat under
                a dark veil while it gains a glow falling from the top.      */
            g.setColour (theme::bgDeep.withAlpha (0.42f));
            g.fillRect (a.withWidth (x0 - a.getX()));
            g.fillRect (a.withTrimmedLeft (x1 - a.getX()));

            g.setGradientFill (juce::ColourGradient (
                theme::accent.withAlpha (0.16f), band.getCentreX(), band.getY(),
                theme::accent.withAlpha (0.02f), band.getCentreX(), band.getBottom(), false));
            g.fillRect (band);

            g.setColour (theme::accent.withAlpha (0.75f));
            g.fillRect (band.withHeight (2.0f));
            g.setColour (theme::accent.withAlpha (0.35f));
            g.drawRect (band, 1.0f);
        }

        // frequency and level grid, under the curve
        {
            const float lines[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f,
                                    2000.0f, 5000.0f, 10000.0f };
            for (auto f : lines)
            {
                const auto x = freqToX (f);
                if (x < a.getX() + 2.0f || x > a.getRight() - 2.0f) continue;
                const auto major = (f == 100.0f || f == 1000.0f || f == 10000.0f);
                g.setColour (theme::strokeSoft.withAlpha (major ? 0.95f : 0.5f));
                g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
            }

            for (int db : { -24, -48, -72 })
            {
                const auto y = jmap ((float) db, -96.0f, 0.0f, a.getBottom(), a.getY() + 2.0f);
                g.setColour (theme::strokeSoft.withAlpha (0.45f));
                g.drawHorizontalLine ((int) y, a.getX(), a.getRight());
            }

            g.setFont (theme::label (8.5f));
            g.setColour (theme::textDim.withAlpha (0.6f));
            const std::pair<float, const char*> marks[] = {
                { 100.0f, "100" }, { 1000.0f, "1k" }, { 10000.0f, "10k" } };
            for (const auto& m : marks)
                g.drawText (m.second,
                            Rectangle<float> (freqToX (m.first) + 3.0f, a.getBottom() - 12.0f,
                                              28.0f, 11.0f),
                            Justification::centredLeft);
        }

        // curve
        /*  One point per pixel column, taking the peak of the bins that fall
            into it. Walking the bins directly would leave the low end angular
            (few bins spread over many pixels) and the top end jagged (many
            bins crowded into one pixel).                                   */
        Path curve;
        bool started = false;
        const auto minDb = -96.0f, maxDb = 0.0f;
        const auto cols = juce::jmax (1, (int) a.getWidth());

        for (int col = 0; col <= cols; ++col)
        {
            const auto x  = a.getX() + (float) col;
            const auto f0 = xToFreq (x - 0.5f), f1 = xToFreq (x + 0.5f);

            auto peak = -120.0f;
            const auto b0 = juce::jmax (1, (int) std::floor (f0 * SpectrumAnalyser::fftSize / 48000.0f));
            const auto b1 = juce::jmin (SpectrumAnalyser::numBins - 1,
                                        (int) std::ceil (f1 * SpectrumAnalyser::fftSize / 48000.0f));

            for (int bin = b0; bin <= b1; ++bin)
                peak = jmax (peak, analyser.magnitudeDb (bin));

            if (peak < -119.0f) continue;

            const auto y = jmap (jlimit (minDb, maxDb, peak), minDb, maxDb,
                                 a.getBottom(), a.getY() + 2.0f);
            if (! started) { curve.startNewSubPath (x, y); started = true; }
            else            curve.lineTo (x, y);
        }

        if (started)
        {
            auto filled = curve;
            filled.lineTo (a.getRight(), a.getBottom());
            filled.lineTo (a.getX(), a.getBottom());
            filled.closeSubPath();

            /*  The fill fades downward: at constant alpha the spectrum turns
                into a block and swallows the grid.                       */
            g.setGradientFill (ColourGradient (theme::accent.withAlpha (0.34f),
                                               a.getCentreX(), a.getY(),
                                               theme::accent.withAlpha (0.015f),
                                               a.getCentreX(), a.getBottom(), false));
            g.fillPath (filled);

            g.setColour (theme::accentGlow.withMultipliedAlpha (0.5f));
            g.strokePath (curve, PathStrokeType (3.0f));
            g.setColour (theme::accentHi);
            g.strokePath (curve, PathStrokeType (1.3f));
        }

        // draggable dividers
        if (multi)
        {
            for (int d = 0; d < 2; ++d)
            {
                const auto f = d == 0 ? lowValue() : highValue();
                const auto x = freqToX (f);

                g.setColour (theme::textBright.withAlpha (0.55f));
                g.drawVerticalLine ((int) x, a.getY(), a.getBottom());

                auto grip = Rectangle<float> (x - 3.5f, a.getCentreY() - 13.0f, 7.0f, 26.0f);
                g.setColour (theme::textDim);
                g.fillRoundedRectangle (grip, 2.0f);

                g.setFont (theme::label (9.5f, true));
                g.setColour (theme::textBright);

                // rotate -90 around the foot of the divider: the text runs up it
                Graphics::ScopedSaveState save (g);
                g.addTransform (AffineTransform::rotation (-MathConstants<float>::halfPi,
                                                           x, a.getBottom() - 4.0f));
                g.drawText (String (f, 1) + " Hz",
                            Rectangle<float> (x + 4.0f, a.getBottom() - 17.0f, 66.0f, 13.0f),
                            Justification::centredLeft);
            }
        }
    }

    juce::AudioProcessorValueTreeState& apvts;
    Buffer& buffer;
    SpectrumAnalyser& analyser;
    std::function<float()> getThresholdDb;
    std::function<bool()>  isMulti;

    juce::RangedAudioParameter *lowParam = nullptr, *highParam = nullptr;
    juce::RangedAudioParameter *thresholdParam = nullptr;
    bool draggingThreshold = false, hoverThreshold = false;
    std::unique_ptr<RoundToggle> mutes[numBands], solos[numBands];

    int dragging = -1, selectedBand = 0;
};
} // namespace pakku
