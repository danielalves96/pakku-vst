#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "FontData.h"

namespace pakku::theme
{
    // ---- Kyantech Labs palette ------------------------------------------------
    const juce::Colour bgDeep      { 0xff050b11 };   // window background
    const juce::Colour bgPanel     { 0xff0b1922 };   // body of the blocks
    const juce::Colour bgLift      { 0xff122933 };   // top of a block gradient
    const juce::Colour bgWell      { 0xff03090e };   // floor of a recess
    const juce::Colour stroke      { 0xff1c3d4b };
    const juce::Colour strokeSoft  { 0xff0f2530 };
    const juce::Colour bevel       { 0x18ffffff };   // 1px highlight along the top
    const juce::Colour shadow      { 0xcc000000 };

    const juce::Colour accent      { 0xff35d2e6 };
    const juce::Colour accentHi    { 0xff9df5ff };
    const juce::Colour accentDim   { 0xff12707f };
    const juce::Colour accentGlow  { 0x6635d2e6 };

    const juce::Colour warn        { 0xffe9c15a };
    const juce::Colour hot         { 0xffff6a5a };

    const juce::Colour textBright  { 0xffe8f4f9 };
    const juce::Colour text        { 0xffaac3ce };
    const juce::Colour textDim     { 0xff6b8794 };
    const juce::Colour waveform    { 0xfff2f8fa };

    inline juce::Font label (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions()
                               .withHeight (h)
                               .withStyle (bold ? "Bold" : "Regular"));
    }

    /*  Michroma (SIL Open Font License 1.1) — the brand typeface.

        Wide, geometric and square-shouldered, from the same ground as
        Ethnocentric but openly licensed. Embedded in the binary so the name
        does not change face on a machine that has not installed it.

        Caps only: Michroma has lowercase, but the squared drawing asks for
        capitals — in lowercase the curves get cramped.
        The licence is in resources/fonts/OFL-michroma.txt.               */
    inline juce::Font display (float h)
    {
        static const juce::Typeface::Ptr face =
            juce::Typeface::createSystemTypefaceFor (FontData::MichromaRegular_ttf,
                                                     FontData::MichromaRegular_ttfSize);

        return face != nullptr
                 ? juce::Font (juce::FontOptions().withTypeface (face).withHeight (h))
                 : label (h, true);
    }

    /*  Text with letter spacing. Short all-caps labels breathe badly in the
        system font; opening the tracking gives them the screen-printed panel
        look the interface is after.                                          */
    inline void tracked (juce::Graphics& g, const juce::String& s,
                         juce::Rectangle<float> area, float spacing,
                         juce::Justification just = juce::Justification::centred)
    {
        const auto f = g.getCurrentFont();
        float total = 0.0f;
        for (int i = 0; i < s.length(); ++i)
            total += juce::GlyphArrangement::getStringWidth (f, s.substring (i, i + 1)) + spacing;
        total -= spacing;

        auto x = area.getX();
        if (just.testFlags (juce::Justification::horizontallyCentred))
            x = area.getCentreX() - total * 0.5f;
        else if (just.testFlags (juce::Justification::right))
            x = area.getRight() - total;

        for (int i = 0; i < s.length(); ++i)
        {
            const auto ch = s.substring (i, i + 1);
            const auto w = juce::GlyphArrangement::getStringWidth (f, ch);
            g.drawText (ch, juce::Rectangle<float> (x, area.getY(), w + 2.0f, area.getHeight()),
                        juce::Justification::centredLeft);
            x += w + spacing;
        }
    }

    inline float trackedWidth (const juce::Font& f, const juce::String& s, float spacing)
    {
        float total = 0.0f;
        for (int i = 0; i < s.length(); ++i)
            total += juce::GlyphArrangement::getStringWidth (f, s.substring (i, i + 1)) + spacing;
        return juce::jmax (0.0f, total - spacing);
    }

    // ---- relief primitives ----------------------------------------------------

    /*  Recess: dark gradient at the top, a short inner shadow and a cool
        border. This is what reads as a hole — tracks, meters, fields.        */
    inline void well (juce::Graphics& g, juce::Rectangle<float> r,
                      float radius = 5.0f, juce::Colour fill = bgWell)
    {
        g.setGradientFill (juce::ColourGradient (fill.darker (0.5f), r.getX(), r.getY(),
                                                 fill.brighter (0.08f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);

        {
            juce::Graphics::ScopedSaveState save (g);
            juce::Path clip;
            clip.addRoundedRectangle (r, radius);
            g.reduceClipRegion (clip, {});
            g.setGradientFill (juce::ColourGradient (
                juce::Colours::black.withAlpha (0.55f), r.getX(), r.getY(),
                juce::Colours::transparentBlack,        r.getX(), r.getY() + 7.0f, false));
            g.fillRect (r.withHeight (8.0f));
        }

        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawLine (r.getX() + radius, r.getBottom() - 0.5f,
                    r.getRight() - radius, r.getBottom() - 0.5f, 1.0f);
        g.setColour (stroke);
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    }

    /*  Raised block: light-to-dark gradient, a 1px bevel across the top half
        only, and a border. The opposite of the recess.                       */
    inline void panel (juce::Graphics& g, juce::Rectangle<float> r,
                       float radius = 6.0f, juce::Colour fill = bgPanel)
    {
        g.setGradientFill (juce::ColourGradient (fill.interpolatedWith (bgLift, 0.30f),
                                                 r.getX(), r.getY(),
                                                 fill.darker (0.42f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);

        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (r.withHeight (r.getHeight() * 0.5f).toNearestInt());
            juce::Path edge;
            edge.addRoundedRectangle (r.reduced (0.5f), radius);
            g.setColour (bevel);
            g.strokePath (edge, juce::PathStrokeType (1.0f));
        }

        g.setColour (stroke);
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    }

    /** Layered halo — cheaper than a blur and enough here. */
    inline void glowRect (juce::Graphics& g, juce::Rectangle<float> r, float radius,
                          juce::Colour c, int layers = 3, float step = 1.6f)
    {
        for (int i = layers; i >= 1; --i)
        {
            g.setColour (c.withMultipliedAlpha (0.16f * (float) i / (float) layers));
            g.drawRoundedRectangle (r.expanded ((float) i * step),
                                    radius + (float) i * step, step);
        }
    }

    inline void glowEllipse (juce::Graphics& g, juce::Point<float> c, float radius,
                             juce::Colour col, int layers = 4)
    {
        for (int i = layers; i >= 1; --i)
        {
            const auto rr = radius + (float) i * 1.6f;
            g.setColour (col.withMultipliedAlpha (0.14f * (float) i / (float) layers));
            g.fillEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f);
        }
    }

    inline void dropShadow (juce::Graphics& g, juce::Rectangle<float> r,
                            float radius, int spread = 8)
    {
        for (int i = spread; i >= 1; --i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.16f * (1.0f - (float) i / (float) spread)));
            g.drawRoundedRectangle (r.expanded ((float) i).translated (0.0f, 1.5f),
                                    radius + (float) i, 1.6f);
        }
    }

    /** Fine 128x128 grain, generated once and used as a tiled fill. */
    inline const juce::Image& grain()
    {
        static const juce::Image img = []
        {
            juce::Image i (juce::Image::ARGB, 128, 128, true);
            juce::Random r (0x9E3779B9);
            for (int y = 0; y < 128; ++y)
                for (int x = 0; x < 128; ++x)
                {
                    const auto v = (juce::uint8) r.nextInt (256);
                    i.setPixelAt (x, y, juce::Colour (v, v, v).withAlpha (1.0f));
                }
            return i;
        }();
        return img;
    }
}
