#include "PluginEditor.h"

using namespace juce;

namespace pakku
{
EditorContent::EditorContent (PakkuAudioProcessor& p, std::function<void (int)> chooser)
    : proc (p), chooseScale (std::move (chooser)),
      display (p.apvts, p.scope, p.analyser,
               [&p] { return p.getThresholdDb(); },
               [&p] { return p.isMultiband(); })
{
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);
    addAndMakeVisible (display);
    addAndMakeVisible (presetBar);
    addAndMakeVisible (gear);

    // trocar de preset repinta a barra e pode mudar o modo de banda
    proc.presets.onChange = [this]
    {
        presetBar.refresh();
        applyBandMode();
    };

    gear.onClick = [this] { openSettings(); };

    settings.onClose   = [this] { closeSettings(); };
    settings.onRestore = [this] { restoreDefaults(); };
    settings.onReveal  = [] { PresetManager::getRootDirectory().revealToUser(); };
    settings.onRescan  = [this] { proc.presets.rescan(); };
    settings.onSize = [this] (int i) { if (chooseScale) chooseScale (i); };
    settings.setVisible (false);
    addChildComponent (settings);

    auto& s = proc.apvts;

    transients = std::make_unique<VerticalShaper> (s, pid::transSingle, "TRANSIENTS", false);
    length     = std::make_unique<VerticalShaper> (s, pid::lenSingle,   "LENGTH",     true);
    addAndMakeVisible (*transients);
    addAndMakeVisible (*length);

    display.onBandSelected = [this] (int b) { selectedBand = b; bindShapers(); };

    inGain    = std::make_unique<LabeledKnob> (s, pid::inputGain,  "INPUT GAIN");
    outGain   = std::make_unique<LabeledKnob> (s, pid::outputGain, "OUTPUT GAIN");
    nyc       = std::make_unique<LabeledKnob> (s, pid::nyc,        "NYC");
    mix       = std::make_unique<LabeledKnob> (s, pid::mix,        "MIX", 1.0f);
    presence  = std::make_unique<LabeledKnob> (s, pid::presence,   "PRESENCE");
    air       = std::make_unique<LabeledKnob> (s, pid::air,        "AIR");
    threshold = std::make_unique<LabeledKnob> (s, pid::threshold,  "THRESHOLD");

    for (auto* k : { inGain.get(), outGain.get(), nyc.get(), mix.get(),
                     presence.get(), air.get(), threshold.get() })
        addAndMakeVisible (*k);

    bandMode = std::make_unique<ToggleSwitch> (
        *dynamic_cast<RangedAudioParameter*> (s.getParameter (pid::bandMode)),
        "MULTI", "SINGLE", false);
    ceilingMode = std::make_unique<ToggleSwitch> (
        *dynamic_cast<RangedAudioParameter*> (s.getParameter (pid::clipMode)),
        "LIMIT", "SOFT CLIP", false);
    addAndMakeVisible (*bandMode);
    addAndMakeVisible (*ceilingMode);

    addAndMakeVisible (bypass);
    auto* bypassParam = dynamic_cast<RangedAudioParameter*> (s.getParameter (pid::bypass));
    bypassAtt = std::make_unique<ParameterAttachment> (
        *bypassParam,
        [this] (float off) { bypass.setToggleState (off < 0.5f, dontSendNotification); });
    bypass.onClick = [this]
    {
        // The parameter reads 0 when the effect is bypassed.
        bypassAtt->setValueAsCompleteGesture (bypass.getToggleState() ? 0.0f : 1.0f);
    };
    bypassAtt->sendInitialUpdate();

    applyBandMode();
    startTimerHz (8);
    setSize (baseWidth, baseHeight);
}

EditorContent::~EditorContent()
{
    proc.presets.onChange = nullptr;
}

//==============================================================================
void EditorContent::openSettings()
{
    settings.setSizeIndex (scaleIndex);
    settings.setLatencyText (String (proc.getLatencySamples()) + " samples");
    settings.setVisible (true);
    settings.toFront (true);
    settings.grabKeyboardFocus();
}

void EditorContent::closeSettings()
{
    settings.setVisible (false);
}

void EditorContent::restoreDefaults()
{
    // returns every parameter to its default, without needing a preset to exist
    for (auto* p : proc.getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }
}

void EditorContent::timerCallback()
{
    if (proc.isMultiband() != lastMulti)
        applyBandMode();
}

void EditorContent::bindShapers()
{
    // em multibanda os sliders editam a banda escolhida no espectro
    if (lastMulti)
    {
        transients->attachTo (pid::trans (selectedBand));
        length    ->attachTo (pid::len   (selectedBand));
        const auto suffix = " " + String (selectedBand + 1);
        transients->setCaption ("TRANSIENTS" + suffix);
        length    ->setCaption ("LENGTH" + suffix);
    }
    else
    {
        transients->attachTo (pid::transSingle);
        length    ->attachTo (pid::lenSingle);
        transients->setCaption ("TRANSIENTS");
        length    ->setCaption ("LENGTH");
    }
}

void EditorContent::applyBandMode()
{
    lastMulti = proc.isMultiband();
    selectedBand = display.getSelectedBand();
    bindShapers();
    resized();
}

//==============================================================================
void EditorContent::rebuildBackdrop()
{
    if (getWidth() <= 0 || getHeight() <= 0) return;

    backdrop = Image (Image::ARGB, getWidth(), getHeight(), true);
    Graphics g (backdrop);
    const auto r = getLocalBounds().toFloat();

    // light from above, falling away to black at the foot
    g.setGradientFill (ColourGradient (theme::bgPanel.brighter (0.05f), r.getCentreX(), -60.0f,
                                       theme::bgDeep,                   r.getCentreX(), r.getBottom(),
                                       false));
    g.fillAll();

    // vinheta: os cantos recuam e o centro ganha o olho
    ColourGradient v (Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                      Colours::black.withAlpha (0.62f), r.getCentreX(), r.getBottom() + 90.0f, true);
    v.addColour (0.55, Colours::transparentBlack);
    g.setGradientFill (v);
    g.fillAll();

    // fine grain: takes the flat digital-gradient look off the background
    g.setTiledImageFill (theme::grain(), 0, 0, 0.035f);
    g.fillAll();
}

void EditorContent::drawHeader (Graphics& g, Rectangle<int> area)
{
    /*  Wave mark and wordmark, treated as one piece.

        The wave used to be drawn on its own terms — centred in the band, with
        rounded bars in saturated cyan. That is a different drawing family
        sitting next to Michroma. It now borrows everything that defines the
        piece from the type: cap height sets its height, it shares the
        baseline, bar thickness comes from the font's stem, and the colour is
        the same gradient. The centre bar is the one point of colour.       */
    const juce::String mark ("PAKKU");
    constexpr auto tracking = 2.2f;

    /*  The band ends where the preset selector starts: that one is centred
        on the plugin, so the space at the left is no longer the old 300. */
    const auto band = area.removeFromLeft (getWidth() / 2 - 200 - 12 - area.getX()).toFloat();
    const auto markGap = 0.85f;          // respiro entre onda e nome, em caixas altas
    constexpr int  bars = 9;
    constexpr auto gapRatio = 2.8f;      // gap between bars, in stem widths

    // corpo derivado do que sobra depois de reservar a onda e o respiro
    constexpr auto probe = 20.0f;
    const auto probeWidth = theme::trackedWidth (theme::display (probe), mark, tracking);
    const auto usable = band.getWidth() - 4.0f;
    auto height = juce::jmin (34.0f, probe * usable / probeWidth * 0.62f);
    auto font = theme::display (height);

    g.setFont (font);

    /*  Medidas de tinta, tiradas do contorno do glifo.

        GlyphArrangement::getBoundingBox returns the advance box — full font
        cheia da fonte e largura de passo —, que aqui daria uma onda alta e
        too wide. The outline gives the real cap height and stem.          */
    const auto ink = [] (const juce::Font& f, const juce::String& t)
    {
        GlyphArrangement ga;
        ga.addLineOfText (f, t, 0.0f, 0.0f);
        Path outline;
        ga.createPath (outline);
        return outline.getBounds();
    };

    const auto capHeight = -ink (font, "P").getY();

    /*  Bar thickness is the font's stem, measured on the "I" — that is what
        makes the wave weigh the same as the letters beside it.            */
    const auto stemOf = [&] (const juce::Font& f) { return ink (f, "I").getWidth(); };

    const auto waveOf = [&] (float stem)
    {
        return stem * ((float) bars + gapRatio * (float) (bars - 1));
    };

    const auto textWidth = theme::trackedWidth (font, mark, tracking);
    const auto total = waveOf (stemOf (font)) + capHeight * markGap + textWidth;

    // se o conjunto estourar a faixa, encolhe tudo junto
    if (total > usable)
    {
        height *= usable / total;
        font = theme::display (height);
        g.setFont (font);
    }

    const auto cap = -ink (font, "P").getY();
    const auto stem = stemOf (font);
    const auto wave = waveOf (stem);

    const auto baseline = band.getCentreY() + cap * 0.5f;
    const auto capTop = baseline - cap;

    // ---- onda ----
    {
        const juce::Rectangle<float> box (band.getX() + 2.0f, capTop, wave, cap);

        const float shape[bars] = { 0.38f, 0.62f, 0.88f, 0.70f, 1.00f,
                                    0.70f, 0.88f, 0.62f, 0.38f };
        const auto pitch = stem * (1.0f + gapRatio);

        for (int i = 0; i < bars; ++i)
        {
            const auto h = box.getHeight() * shape[i];
            const juce::Rectangle<float> bar (box.getX() + (float) i * pitch,
                                              box.getCentreY() - h * 0.5f, stem, h);

            if (shape[i] >= 1.0f)      // a barra mais alta carrega o acento
            {
                g.setColour (theme::accentGlow.withMultipliedAlpha (0.7f));
                g.fillRect (bar.expanded (1.0f, 0.8f));
                g.setColour (theme::accent);
                g.fillRect (bar);
            }
            else
            {
                g.setGradientFill (ColourGradient (theme::textBright, bar.getCentreX(), box.getY(),
                                                   Colour (0xff8fb2c0), bar.getCentreX(), box.getBottom(),
                                                   false));
                g.fillRect (bar);
            }
        }
    }

    // ---- nome ----
    {
        // tracked() centra o texto na caixa: a caixa vai onde a base tem de cair
        const juce::Rectangle<float> slot (band.getX() + 2.0f + wave + cap * markGap,
                                           baseline - font.getAscent(),
                                           textWidth + 4.0f, font.getHeight());

        // a short shadow underneath lands the text on the background
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        theme::tracked (g, mark, slot.translated (0.0f, 1.6f), tracking, Justification::left);

        /*  Flat white sits apart from the rest of the panel; a gradient into
            a cool tone puts the mark under the same light as the blocks.  */
        g.setGradientFill (ColourGradient (theme::textBright, slot.getX(), capTop,
                                           Colour (0xff8fb2c0), slot.getX(), baseline, false));
        theme::tracked (g, mark, slot, tracking, Justification::left);
    }
}

void EditorContent::drawBrandStrip (Graphics& g, Rectangle<int> area)
{
    const auto text = String ("KYANTECH LABS");
    const auto font = theme::label (11.5f, true);
    const auto spacing = 4.2f;

    g.setFont (font);
    g.setColour (theme::accent.withAlpha (0.9f));
    theme::tracked (g, text, area.toFloat(), spacing);

    const auto tw = theme::trackedWidth (font, text, spacing);
    const auto cy = (float) area.getCentreY();

    // duas fileiras de pontos, esmaecendo para fora
    for (int side = 0; side < 2; ++side)
    {
        const auto dir = side == 0 ? -1.0f : 1.0f;
        const auto x0 = (float) area.getCentreX() + dir * (tw * 0.5f + 26.0f);

        for (int i = 0; i < 26; ++i)
        {
            const auto a = 0.42f * (1.0f - i / 26.0f);
            g.setColour (theme::accentDim.withAlpha (a));
            const auto x = x0 + dir * (float) i * 7.0f;
            for (int k = 0; k < 2; ++k)
                g.fillRect (x, cy - 4.0f + (float) k * 7.0f, 2.0f, 2.0f);
        }
    }
}

void EditorContent::paint (Graphics& g)
{
    if (backdrop.isNull() || backdrop.getWidth() != getWidth())
        rebuildBackdrop();

    g.drawImageAt (backdrop, 0, 0);

    auto r = getLocalBounds();

    // moldura externa
    const auto frame = r.toFloat().reduced (5.0f);
    g.setColour (theme::stroke.withAlpha (0.55f));
    g.drawRoundedRectangle (frame, 10.0f, 1.0f);
    g.setColour (theme::bevel);
    g.drawLine (frame.getX() + 12.0f, frame.getY() + 0.8f,
                frame.getRight() - 12.0f, frame.getY() + 0.8f, 1.0f);

    drawHeader (g, r.reduced (18, 12).removeFromTop (54));
    drawBrandStrip (g, r.withY (r.getY() + 74).withHeight (18));

    // hairline under the header
    {
        const auto y = (float) r.getY() + 98.0f;
        ColourGradient line (Colours::transparentBlack, (float) r.getX() + 20.0f, y,
                             Colours::transparentBlack, (float) r.getRight() - 20.0f, y, false);
        line.addColour (0.5, theme::stroke);
        g.setGradientFill (line);
        g.fillRect ((float) r.getX() + 20.0f, y, (float) r.getWidth() - 40.0f, 1.0f);
    }

    g.setFont (theme::label (9.5f, true));
    g.setColour (theme::textDim);
    theme::tracked (g, "INPUT",  Rectangle<float> (18.0f, 106.0f, 120.0f, 14.0f), 1.6f);
    theme::tracked (g, "OUTPUT", Rectangle<float> ((float) getWidth() - 138.0f, 106.0f,
                                                   120.0f, 14.0f), 1.6f);

    // hairline above the control strip
    {
        const auto y = (float) getHeight() - 122.0f;
        ColourGradient line (Colours::transparentBlack, 40.0f, y,
                             Colours::transparentBlack, (float) getWidth() - 40.0f, y, false);
        line.addColour (0.5, theme::stroke.withAlpha (0.8f));
        g.setGradientFill (line);
        g.fillRect (40.0f, y, (float) getWidth() - 80.0f, 1.0f);
    }
}

void EditorContent::resized()
{
    backdrop = {};
    settings.setBounds (getLocalBounds());

    {
        auto header = getLocalBounds().reduced (18, 12).removeFromTop (54);
        gear.setBounds (header.removeFromRight (34).withSizeKeepingCentre (32, 30));

        /*  O seletor fica no eixo central do plugin, o mesmo da marca logo
            below. Centring it in the space left between the logo and the
            gear would throw it right, because the two sides are not the same
            largura.                                                        */
        presetBar.setBounds (Rectangle<int> (0, header.getY(), 400, 34)
                                 .withCentre ({ getWidth() / 2, header.getCentreY() }));
    }

    auto r = getLocalBounds().reduced (18);
    r.removeFromTop (78);              // header + brand strip

    auto bottom = r.removeFromBottom (108);
    r.removeFromTop (24);              // INPUT / OUTPUT labels

    // --- colunas laterais ---
    auto leftCol  = r.removeFromLeft (150);
    auto rightCol = r.removeFromRight (150);

    auto inKnob  = leftCol .removeFromBottom (78);
    auto outKnob = rightCol.removeFromBottom (78);
    inGain ->setBounds (inKnob .withSizeKeepingCentre (88, 76));
    outGain->setBounds (outKnob.withSizeKeepingCentre (88, 76));
    leftCol .removeFromBottom (8);
    rightCol.removeFromBottom (8);

    inMeter .setBounds (leftCol .removeFromLeft (66));
    outMeter.setBounds (rightCol.removeFromRight (66));
    leftCol .removeFromLeft (8);
    rightCol.removeFromRight (8);

    transients->setBounds (leftCol);
    length    ->setBounds (rightCol);

    r.reduce (10, 0);
    display.setBounds (r);

    // --- bottom strip ---
    bottom.removeFromTop (10);
    auto bar = bottom;

    const auto n = 9;
    const auto slotW = bar.getWidth() / n;
    auto slot = [&] (int i, int span = 1)
    {
        return Rectangle<int> (bar.getX() + i * slotW, bar.getY(),
                               slotW * span, bar.getHeight());
    };

    bandMode   ->setBounds (slot (0).withSizeKeepingCentre (78, 92));
    ceilingMode->setBounds (slot (1).withSizeKeepingCentre (88, 92));
    nyc        ->setBounds (slot (2).withSizeKeepingCentre (86, 94));
    mix        ->setBounds (slot (3, 2).withSizeKeepingCentre (120, 100));
    presence   ->setBounds (slot (5).withSizeKeepingCentre (86, 94));
    air        ->setBounds (slot (6).withSizeKeepingCentre (86, 94));

    threshold->setBounds (slot (7).withSizeKeepingCentre (86, 94));
    bypass.setBounds (slot (8).withSizeKeepingCentre (94, 30));
}
} // namespace pakku

//==============================================================================
PakkuAudioProcessorEditor::PakkuAudioProcessorEditor (PakkuAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      content (p, [this] (int i) { setScaleIndex (i); })
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    setResizable (false, false);

    setScaleIndex ((int) proc.apvts.state.getProperty ("guiScale", 1));
}

PakkuAudioProcessorEditor::~PakkuAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void PakkuAudioProcessorEditor::setScaleIndex (int i)
{
    scaleIndex = jlimit (0, 2, i);
    proc.apvts.state.setProperty ("guiScale", scaleIndex, nullptr);

    content.setScaleIndex (scaleIndex);
    content.setTransform (AffineTransform::scale (scales[scaleIndex]));

    setSize (roundToInt (pakku::EditorContent::baseWidth  * scales[scaleIndex]),
             roundToInt (pakku::EditorContent::baseHeight * scales[scaleIndex]));
}

void PakkuAudioProcessorEditor::resized()
{
    content.setBounds (0, 0, pakku::EditorContent::baseWidth,
                             pakku::EditorContent::baseHeight);
}
