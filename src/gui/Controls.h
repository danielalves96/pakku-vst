#pragma once
#include "Theme.h"

namespace pakku
{
/** Knob with its name above and value below, both drawn by the component. */
class LabeledKnob : public juce::Component
{
public:
    LabeledKnob (juce::AudioProcessorValueTreeState& state,
                 const juce::String& paramId, juce::String caption,
                 float knobScale = 1.0f)
        : name (std::move (caption)), scale (knobScale),
          param (state.getParameter (paramId))
    {
        slider.onValueChange = [this] { repaint(); };
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f,
                                    juce::MathConstants<float>::pi * 2.78f, true);
        slider.setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        addAndMakeVisible (slider);
        attachment = std::make_unique<
            juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();

        g.setColour (theme::textDim);
        g.setFont (theme::label (9.5f, true));
        theme::tracked (g, name, r.removeFromTop (14.0f), 1.1f);

        // the parameter knows how to format itself (unit, decimal places)
        const auto text = param != nullptr ? param->getCurrentValueAsText()
                                           : slider.getTextFromValue (slider.getValue());

        auto valueArea = r.removeFromBottom (16.0f);
        const auto hot = slider.isMouseOverOrDragging();

        g.setFont (theme::label (11.0f, true));
        const auto w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 16.0f;
        const auto pill = valueArea.withSizeKeepingCentre (juce::jmin (w, valueArea.getWidth()), 15.0f);

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (pill, 7.5f);
        if (hot)
        {
            g.setColour (theme::accentGlow.withMultipliedAlpha (0.5f));
            g.drawRoundedRectangle (pill.reduced (0.5f), 7.5f, 1.0f);
        }

        g.setColour (hot ? theme::accentHi : theme::accent);
        g.drawText (text, pill, juce::Justification::centred);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (0, 1);
        r.removeFromTop (14); r.removeFromBottom (16);
        const auto d = (int) (juce::jmin (r.getWidth(), r.getHeight()) * scale);
        slider.setBounds (r.withSizeKeepingCentre (d, d));
    }

    juce::Slider slider;

private:
    juce::String name;
    float scale;
    juce::AudioProcessorParameter* param = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** Bipolar vertical fader with a rotated side label and a readout on top. */
class VerticalShaper : public juce::Component
{
public:
    VerticalShaper (juce::AudioProcessorValueTreeState& state,
                    const juce::String& paramId, juce::String caption, bool labelRight)
        : name (std::move (caption)), onRight (labelRight), apvts (state)
    {
        slider.setSliderStyle (juce::Slider::LinearVertical);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);
        attachTo (paramId);
    }

    /** Rebinds the fader to another parameter, when the band selection changes. */
    void attachTo (const juce::String& paramId)
    {
        attachment.reset();   // release the old one before making the new one
        param = apvts.getParameter (paramId);
        attachment = std::make_unique<
            juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
        repaint();
    }

    void setCaption (juce::String c) { name = std::move (c); repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        theme::panel (g, r, 6.0f);

        // readout on top
        auto readout = r.reduced (5.0f).removeFromTop (17.0f);
        const auto text = param != nullptr ? param->getCurrentValueAsText() : juce::String {};
        const auto active = std::abs ((float) slider.getValue()) > 0.005;

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (readout, 4.0f);
        g.setFont (theme::label (11.0f, true));
        g.setColour (active ? theme::accent : theme::textDim);
        g.drawText (text, readout, juce::Justification::centred);

        // label rotated flush with the edge
        g.setColour (theme::textDim);
        g.setFont (theme::label (9.5f, true));

        juce::Graphics::ScopedSaveState save (g);
        const auto cx = onRight ? r.getRight() - 10.0f : r.getX() + 10.0f;
        const auto cy = r.getCentreY() + 8.0f;
        g.addTransform (juce::AffineTransform::rotation (
            onRight ? juce::MathConstants<float>::halfPi
                    : -juce::MathConstants<float>::halfPi, cx, cy));
        theme::tracked (g, name,
                        juce::Rectangle<float> (cx - 60.0f, cy - 7.0f, 120.0f, 14.0f), 1.4f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (5);
        r.removeFromTop (17 + 6);
        r.removeFromBottom (6);
        if (onRight) r.removeFromRight (15); else r.removeFromLeft (15);
        slider.setBounds (r);
    }

    juce::Slider slider;

private:
    juce::String name;
    bool onRight;
    juce::AudioProcessorValueTreeState& apvts;
    juce::AudioProcessorParameter* param = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** Two-state switch — MULTI/SINGLE, LIMIT/SOFT CLIP. */
class ToggleSwitch : public juce::Component
{
public:
    /** topValue says which parameter value the upper label stands for, so the
        switch follows the visual layout without inverting the state.     */
    ToggleSwitch (juce::RangedAudioParameter& parameter,
                  juce::String topLabel, juce::String bottomLabel, bool topIsOne)
        : param (parameter), top (std::move (topLabel)), bottom (std::move (bottomLabel)),
          topValue (topIsOne),
          attachment (parameter, [this] (float v) { value = v > 0.5f; repaint(); })
    {
        attachment.sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        const auto topActive = (value == topValue);

        auto drawLabel = [&] (const String& s, Rectangle<float> area, bool on)
        {
            g.setFont (theme::label (9.5f, true));
            g.setColour (on ? theme::accentHi : theme::textDim);
            theme::tracked (g, s, area, 1.2f);
        };

        drawLabel (top,    r.removeFromTop (14.0f),    topActive);
        drawLabel (bottom, r.removeFromBottom (14.0f), ! topActive);

        auto body = r.withSizeKeepingCentre (30.0f, 50.0f);
        theme::well (g, body, 15.0f);

        const auto d = 20.0f;
        const auto cy = topActive ? body.getY() + 15.0f : body.getBottom() - 15.0f;
        const Point<float> c (body.getCentreX(), cy);
        const Rectangle<float> knob (c.x - d * 0.5f, c.y - d * 0.5f, d, d);

        // halo behind the thumb
        theme::glowEllipse (g, c, d * 0.5f, theme::accentGlow, 3);

        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillEllipse (knob.translated (0.0f, 1.5f));

        g.setGradientFill (ColourGradient (Colour (0xff6d8a97), c.x, knob.getY(),
                                           Colour (0xff16242c), c.x, knob.getBottom(), false));
        g.fillEllipse (knob);
        g.setColour (Colours::white.withAlpha (0.18f));
        g.drawEllipse (knob.reduced (0.5f), 1.0f);

        // lit eye in the middle
        g.setColour (theme::accentHi);
        g.fillEllipse (knob.withSizeKeepingCentre (7.0f, 7.0f));
        g.setColour (theme::accentGlow);
        g.drawEllipse (knob.withSizeKeepingCentre (9.5f, 9.5f), 2.0f);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        attachment.setValueAsCompleteGesture (value ? 0.0f : 1.0f);
    }

private:
    juce::RangedAudioParameter& param;
    juce::String top, bottom;
    bool topValue = true;
    bool value = false;
    juce::ParameterAttachment attachment;
};

/** The BYPASS button: a capsule with an LED, lit when the effect is out. */
class BypassButton : public juce::Button
{
public:
    BypassButton() : juce::Button ("BYPASS")
    {
        setClickingTogglesState (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const auto on = getToggleState();

        if (on)
        {
            g.setColour (theme::warn.withAlpha (0.18f));
            g.fillRoundedRectangle (r.expanded (2.5f), r.getHeight() * 0.5f + 2.5f);
        }

        theme::panel (g, r, r.getHeight() * 0.5f,
                      on ? theme::warn.withAlpha (0.22f).overlaidWith (theme::bgPanel)
                         : (down ? theme::bgWell : theme::bgPanel));

        auto led = r.removeFromLeft (r.getHeight()).withSizeKeepingCentre (8.0f, 8.0f);
        if (on) theme::glowEllipse (g, led.getCentre(), 4.0f, theme::warn.withAlpha (0.5f), 3);
        g.setColour (on ? theme::warn : theme::strokeSoft.brighter (0.3f));
        g.fillEllipse (led);

        g.setFont (theme::label (10.5f, true));
        g.setColour (on ? theme::warn : (over ? theme::text : theme::textDim));
        theme::tracked (g, "BYPASS", r.withTrimmedRight (r.getHeight() * 0.4f), 1.6f);
    }
};
} // namespace pakku
