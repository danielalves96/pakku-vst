#pragma once
#include "Theme.h"
#include "Icons.h"

namespace pakku
{
class PakkuLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PakkuLookAndFeel()
    {
        setColour (juce::Label::textColourId,               theme::text);
        setColour (juce::TextButton::buttonColourId,        theme::bgPanel);
        setColour (juce::TextButton::textColourOffId,       theme::textDim);
        setColour (juce::TextButton::textColourOnId,        theme::accent);
        setColour (juce::ComboBox::backgroundColourId,      theme::bgWell);
        setColour (juce::ComboBox::textColourId,            theme::accent);
        setColour (juce::ComboBox::outlineColourId,         theme::stroke);
        setColour (juce::PopupMenu::backgroundColourId,     theme::bgPanel);
        setColour (juce::PopupMenu::textColourId,           theme::textBright);
        setColour (juce::PopupMenu::headerTextColourId,     theme::accent);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::accentDim);
        setColour (juce::AlertWindow::backgroundColourId,   theme::bgPanel);
        setColour (juce::AlertWindow::textColourId,         theme::textBright);
        setColour (juce::AlertWindow::outlineColourId,      theme::stroke);
        setColour (juce::TextEditor::backgroundColourId,    theme::bgWell);
        setColour (juce::TextEditor::textColourId,          theme::textBright);
        setColour (juce::TextEditor::highlightColourId,     theme::accentDim);
        setColour (juce::TextEditor::outlineColourId,       theme::stroke);
        setColour (juce::TextEditor::focusedOutlineColourId, theme::accent);
        setColour (juce::CaretComponent::caretColourId,     theme::accent);
    }

    //==============================================================================
    //  Menus — the JUCE default sits apart from the rest of the panel, so the
    //  whole drawing is ours: background, item, section header, separator.

    juce::Font getPopupMenuFont() override { return theme::label (13.0f); }
    int  getPopupMenuBorderSize() override { return 8; }

    /*  Without this the menu opens at 1:1 while the interface sits at 0.85 or
        1.25, and the difference in type size is glaring.                    */
    bool shouldPopupMenuScaleWithTargetComponent (const juce::PopupMenu::Options&) override
    {
        return true;
    }

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        using namespace juce;
        const Rectangle<float> r (0.0f, 0.0f, (float) width, (float) height);

        g.setGradientFill (ColourGradient (theme::bgPanel.interpolatedWith (theme::bgLift, 0.28f),
                                           0.0f, 0.0f,
                                           theme::bgPanel.darker (0.45f), 0.0f, r.getBottom(), false));
        g.fillRect (r);

        g.setColour (theme::bevel);
        g.fillRect (r.withHeight (1.0f));
        g.setColour (theme::stroke);
        g.drawRect (r, 1.0f);
    }

    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                    int standardHeight, int& idealWidth,
                                    int& idealHeight) override
    {
        if (isSeparator)
        {
            idealWidth = 60;
            idealHeight = standardHeight > 0 ? standardHeight / 2 : 11;
            return;
        }

        const auto f = getPopupMenuFont();
        idealHeight = juce::jmax (25, (int) std::ceil (f.getHeight() * 1.85f));
        idealWidth = (int) juce::GlyphArrangement::getStringWidth (f, text) + 64;
    }

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override
    {
        using namespace juce;

        if (isSeparator)
        {
            auto r = area.toFloat().reduced (12.0f, 0.0f);
            g.setColour (theme::stroke.withAlpha (0.75f));
            g.fillRect (r.withHeight (1.0f).withY (r.getCentreY()));
            return;
        }

        auto r = area.toFloat().reduced (5.0f, 1.0f);

        if (isHighlighted && isActive)
        {
            g.setColour (theme::accent.withAlpha (0.15f));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (theme::accent.withAlpha (0.42f));
            g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        }

        const auto colour = textColour != nullptr
                              ? *textColour
                              : ! isActive ? theme::textDim.withAlpha (0.45f)
                              : isHighlighted ? theme::textBright : theme::text;

        auto content = r.reduced (7.0f, 0.0f);
        auto gutter = content.removeFromLeft (17.0f);
        content.removeFromLeft (7.0f);

        if (isTicked)
            icons::draw (g, icons::Id::check,
                         gutter.withSizeKeepingCentre (12.0f, 12.0f), theme::accent);
        else if (icon != nullptr)
            icon->drawWithin (g, gutter.withSizeKeepingCentre (13.0f, 13.0f),
                              RectanglePlacement::centred, 1.0f);

        if (hasSubMenu)
            icons::draw (g, icons::Id::caretRight,
                         content.removeFromRight (14.0f).withSizeKeepingCentre (8.0f, 8.0f), colour);
        else if (shortcutKeyText.isNotEmpty())
        {
            g.setFont (theme::label (11.0f));
            g.setColour (theme::textDim);
            g.drawText (shortcutKeyText, content.removeFromRight (56.0f),
                        Justification::centredRight, true);
        }

        g.setFont (getPopupMenuFont());
        g.setColour (isTicked ? theme::accent : colour);
        g.drawText (text, content, Justification::centredLeft, true);
    }

    void drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area,
                                     const juce::String& name) override
    {
        auto r = area.toFloat().reduced (12.0f, 0.0f);

        g.setFont (theme::label (9.5f, true));
        g.setColour (theme::accent);
        theme::tracked (g, name.toUpperCase(), r.withTrimmedBottom (3.0f), 1.8f,
                        juce::Justification::left);

        g.setColour (theme::stroke.withAlpha (0.65f));
        g.fillRect (r.removeFromBottom (1.0f));
    }

    int getPopupMenuColumnSeparatorWidthWithOptions (const juce::PopupMenu::Options&) override
    {
        return 11;
    }

    void drawPopupMenuColumnSeparatorWithOptions (juce::Graphics& g,
                                                  const juce::Rectangle<int>& bounds,
                                                  const juce::PopupMenu::Options&) override
    {
        g.setColour (theme::stroke.withAlpha (0.7f));
        g.fillRect (bounds.toFloat().withSizeKeepingCentre (1.0f,
                                                            (float) bounds.getHeight() - 16.0f));
    }

    //==============================================================================
    //  Dialog box (save preset) and its buttons.

    void drawAlertBox (juce::Graphics& g, juce::AlertWindow& w,
                       const juce::Rectangle<int>& textArea, juce::TextLayout& layout) override
    {
        auto r = w.getLocalBounds().toFloat();
        g.fillAll (theme::bgDeep);
        theme::panel (g, r.reduced (0.5f), 8.0f, theme::bgPanel.brighter (0.05f));
        layout.draw (g, textArea.toFloat());
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);

        if (over && ! down)
        {
            g.setColour (theme::accentGlow.withMultipliedAlpha (0.3f));
            g.fillRoundedRectangle (r.expanded (1.5f), 6.5f);
        }
        theme::panel (g, r, 5.0f, down ? theme::bgWell : theme::bgPanel);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool down) override
    {
        g.setFont (theme::label (12.0f, true));
        g.setColour (down || over ? theme::accentHi : theme::text);
        g.drawText (b.getButtonText(), b.getLocalBounds(),
                    juce::Justification::centred, false);
    }

    //==============================================================================
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle,
                           juce::Slider& s) override
    {
        using namespace juce;

        auto area = Rectangle<int> (x, y, w, h).toFloat();
        const auto side = jmin (area.getWidth(), area.getHeight());
        area = area.withSizeKeepingCentre (side, side).reduced (1.5f);

        const auto c = area.getCentre();
        const auto outer = area.getWidth() * 0.5f;
        const auto ringR = outer * 0.80f;
        const auto bodyR = ringR - 4.6f;
        const auto angle = startAngle + pos * (endAngle - startAngle);

        const auto lit = s.isEnabled() && (s.isMouseOverOrDragging() || s.isMouseButtonDown());
        const auto accent = lit ? theme::accentHi : theme::accent;

        // ---- outer tick marks ----
        constexpr int ticks = 11;
        for (int i = 0; i < ticks; ++i)
        {
            const auto t = i / (float) (ticks - 1);
            const auto a = startAngle + t * (endAngle - startAngle);
            const auto sn = std::sin (a), cs = std::cos (a);
            const auto r0 = outer * 0.90f, r1 = outer;
            g.setColour (theme::strokeSoft.brighter (0.15f));
            g.drawLine (c.x + sn * r0, c.y - cs * r0, c.x + sn * r1, c.y - cs * r1, 1.3f);
        }

        // ---- ring track, recessed ----
        Path track;
        track.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.strokePath (track, PathStrokeType (4.4f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (theme::strokeSoft.brighter (0.25f));
        g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

        // ---- fill up to the position ----
        const auto isBipolar = s.getMinimum() < -0.001 && s.getMaximum() > 0.001
                               && std::abs (s.getMinimum() + s.getMaximum()) < 0.001;
        const auto from = isBipolar ? (startAngle + endAngle) * 0.5f : startAngle;

        if (std::abs (angle - from) > 0.004f)
        {
            Path fill;
            fill.addCentredArc (c.x, c.y, ringR, ringR, 0.0f,
                                jmin (from, angle), jmax (from, angle), true);

            // halo: the same stroke, wide and translucent, under the crisp one
            g.setColour (theme::accentGlow.withMultipliedAlpha (lit ? 1.0f : 0.7f));
            g.strokePath (fill, PathStrokeType (7.5f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (accent);
            g.strokePath (fill, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        }

        // ---- body ----
        const Rectangle<float> body (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);

        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (body.translated (0.0f, 1.6f).expanded (1.2f));

        g.setGradientFill (ColourGradient (theme::bgLift.brighter (0.10f), c.x, body.getY(),
                                           theme::bgWell,                  c.x, body.getBottom(), false));
        g.fillEllipse (body);

        // rim: light on top, dark below
        g.setGradientFill (ColourGradient (juce::Colours::white.withAlpha (0.16f), c.x, body.getY(),
                                           juce::Colours::black.withAlpha (0.35f), c.x, body.getBottom(), false));
        g.drawEllipse (body.reduced (0.5f), 1.2f);

        // ---- pointer ----
        const auto sn = std::sin (angle), cs = std::cos (angle);
        const auto p0 = bodyR * 0.34f, p1 = bodyR * 0.86f;
        const Line<float> pointer (c.x + sn * p0, c.y - cs * p0,
                                   c.x + sn * p1, c.y - cs * p1);

        g.setColour (theme::accentGlow);
        g.drawLine (pointer, 4.2f);
        g.setColour (lit ? juce::Colours::white : theme::textBright);
        g.drawLine (pointer, 2.0f);
    }

    //==============================================================================
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                           float sliderPos, float, float,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        using namespace juce;
        if (style != Slider::LinearVertical)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, 0, 0, style, s);
            return;
        }

        auto bounds = Rectangle<int> (x, y, w, h).toFloat();
        const auto cx = bounds.getCentreX();
        const auto trackW = 9.0f;
        const auto lit = s.isMouseOverOrDragging() || s.isMouseButtonDown();

        const Rectangle<float> groove (cx - trackW * 0.5f, bounds.getY(),
                                       trackW, bounds.getHeight());
        theme::well (g, groove, trackW * 0.5f);

        // side tick marks
        for (int i = 0; i <= 8; ++i)
        {
            const auto ty = bounds.getY() + bounds.getHeight() * i / 8.0f;
            const auto major = (i % 4) == 0;
            g.setColour (major ? theme::stroke.brighter (0.35f) : theme::strokeSoft.brighter (0.2f));
            g.drawLine (cx - trackW * 0.5f - (major ? 8.0f : 5.0f), ty,
                        cx - trackW * 0.5f - 2.0f, ty, 1.0f);
            g.drawLine (cx + trackW * 0.5f + 2.0f, ty,
                        cx + trackW * 0.5f + (major ? 8.0f : 5.0f), ty, 1.0f);
        }

        // bipolar fills out from the centre
        const auto isBipolar = s.getMinimum() < -0.001 && s.getMaximum() > 0.001;
        const auto zeroY = isBipolar ? bounds.getCentreY() : bounds.getBottom();
        const auto top = jmin (zeroY, sliderPos), bot = jmax (zeroY, sliderPos);

        if (bot - top > 1.0f)
        {
            const Rectangle<float> f (cx - trackW * 0.5f + 2.0f, top, trackW - 4.0f, bot - top);
            g.setColour (theme::accentGlow);
            g.fillRoundedRectangle (f.expanded (2.5f, 0.0f), (trackW - 4.0f) * 0.5f + 2.5f);
            g.setGradientFill (ColourGradient (theme::accentHi, cx, top,
                                               theme::accent,   cx, bot, false));
            g.fillRoundedRectangle (f, (trackW - 4.0f) * 0.5f);
        }

        // zero mark
        if (isBipolar)
        {
            g.setColour (theme::textDim.withAlpha (0.7f));
            g.drawLine (cx - trackW * 0.5f - 10.0f, zeroY, cx + trackW * 0.5f + 10.0f, zeroY, 1.2f);
        }

        // ---- thumb ----
        const auto tw = jmin (bounds.getWidth(), 26.0f), th = 14.0f;
        const Rectangle<float> thumb (cx - tw * 0.5f, sliderPos - th * 0.5f, tw, th);

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (thumb.translated (0.0f, 2.0f), 4.0f);

        g.setGradientFill (ColourGradient (Colour (0xff5b7683), cx, thumb.getY(),
                                           Colour (0xff17252d), cx, thumb.getBottom(), false));
        g.fillRoundedRectangle (thumb, 4.0f);

        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawLine (thumb.getX() + 3.0f, thumb.getY() + 0.7f,
                    thumb.getRight() - 3.0f, thumb.getY() + 0.7f, 1.2f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawRoundedRectangle (thumb.reduced (0.5f), 4.0f, 1.0f);

        // reading line across the thumb
        const auto marker = thumb.withSizeKeepingCentre (tw - 9.0f, 2.0f);
        g.setColour (lit ? theme::accentHi : theme::accent);
        g.fillRoundedRectangle (marker, 1.0f);
        if (lit)
        {
            g.setColour (theme::accentGlow);
            g.fillRoundedRectangle (marker.expanded (1.5f), 2.0f);
        }
    }
};
} // namespace pakku
