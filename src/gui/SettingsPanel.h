#pragma once
#include "Theme.h"
#include "PresetBar.h"

namespace pakku
{
/** Square holding a proportional inner rectangle — the three window sizes. */
class SizeButton : public juce::Button
{
public:
    SizeButton (float fraction, juce::String tip) : juce::Button (tip), frac (fraction)
    {
        setTooltip (tip);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setSelected (bool s) { selected = s; repaint(); }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);

        if (selected)
        {
            g.setColour (theme::accentGlow.withMultipliedAlpha (0.55f));
            g.fillRoundedRectangle (r.expanded (2.0f), 6.0f);
        }

        theme::panel (g, r, 4.0f, down || selected ? theme::bgWell : theme::bgPanel);

        const auto inner = r.withSizeKeepingCentre (r.getWidth() * frac, r.getHeight() * frac);
        g.setColour (selected ? theme::accentHi : over ? theme::text : theme::textDim);
        g.fillRoundedRectangle (inner, 1.5f);

        if (selected)
        {
            g.setColour (theme::accent);
            g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.2f);
        }
    }

private:
    float frac;
    bool selected = false;
};

/** A name that opens an address in the browser when clicked. */
class LinkButton : public juce::Button
{
public:
    LinkButton (juce::String label, juce::String address, icons::Id glyph)
        : juce::Button (label), text (std::move (label)), url (std::move (address)), icon (glyph)
    {
        setTooltip (url);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        onClick = [this] { juce::URL (url).launchInDefaultBrowser(); };
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat();

        g.setFont (theme::label (11.0f, true));
        const auto textWidth = theme::trackedWidth (g.getCurrentFont(), text, 1.0f);
        const auto glyph = 13.0f;
        const auto total = glyph + 7.0f + textWidth;

        auto row = r.withSizeKeepingCentre (total, r.getHeight());
        icons::draw (g, icon, row.removeFromLeft (glyph).withSizeKeepingCentre (glyph, glyph),
                     over || down ? theme::accentHi : theme::accent);
        row.removeFromLeft (7.0f);

        g.setColour (over || down ? theme::accentHi : theme::accent);
        theme::tracked (g, text, row, 1.0f, juce::Justification::left);

        // underline on hover only: without it the name never reads as a link
        if (over)
        {
            g.setColour (theme::accentHi.withAlpha (0.6f));
            g.fillRect (row.getX(), row.getBottom() - 2.0f, textWidth, 1.0f);
        }
    }

private:
    juce::String text, url;
    icons::Id icon;
};

/*  Sponsorship invitation.

    The GitHub Sponsors card is a web page and cannot live inside a plugin.
    What can be done is to rebuild the invitation in the panel's own language
    and send the click to the same address.                                */
class SponsorButton : public juce::Button
{
public:
    SponsorButton() : juce::Button ("Sponsor")
    {
        setTooltip (url);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        onClick = [this] { juce::URL (url).launchInDefaultBrowser(); };
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat().reduced (1.0f);

        if (over && ! down)
        {
            g.setColour (heartColour.withAlpha (0.22f));
            g.fillRoundedRectangle (r.expanded (2.0f), 8.0f);
        }

        theme::panel (g, r, 6.0f,
                      down ? theme::bgWell
                           : heartColour.withAlpha (0.10f).overlaidWith (theme::bgPanel));
        g.setColour (heartColour.withAlpha (over ? 0.55f : 0.35f));
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

        auto body = r.reduced (13.0f, 9.0f);
        auto glyph = body.removeFromLeft (20.0f);
        body.removeFromLeft (11.0f);

        icons::draw (g, icons::Id::heart, glyph.withSizeKeepingCentre (18.0f, 18.0f),
                     over || down ? heartColour.brighter (0.3f) : heartColour);

        g.setFont (theme::label (11.0f, true));
        g.setColour (over || down ? theme::textBright : theme::text);
        theme::tracked (g, "SPONSOR THIS PLUGIN", body.removeFromTop (14.0f), 1.4f,
                        Justification::left);

        g.setFont (theme::label (10.5f));
        g.setColour (theme::textDim);
        g.drawFittedText ("Pakku is free and stays free. A little help keeps the next one coming.",
                          body.toNearestInt(), Justification::topLeft, 2);
    }

private:
    const juce::Colour heartColour { 0xffdb61a2 };   // GitHub Sponsors pink
    const juce::String url { "https://github.com/sponsors/danielalves96" };
};

/*  Settings and credits panel, laid over the interface.

    It gathers what used to be spread between the gear menu and a help
    button: window size, preset folder shortcuts, restore defaults and the
    technical details. The window only changes size from here.             */
class SettingsPanel : public juce::Component, private juce::Timer
{
public:
    SettingsPanel()
    {
        for (auto* b : { &close, &folder, &rescan, &restore })
            addAndMakeVisible (*b);

        for (auto* b : { &sizeS, &sizeM, &sizeL })
            addAndMakeVisible (*b);

        addAndMakeVisible (developer);
        addAndMakeVisible (sponsor);

        close.onClick   = [this] { if (onClose)   onClose(); };
        folder.onClick  = [this] { if (onReveal)  onReveal(); };
        rescan.onClick  = [this] { if (onRescan)  onRescan(); };
        restore.onClick = [this] { if (onRestore) onRestore(); };

        sizeS.onClick = [this] { choose (0); };
        sizeM.onClick = [this] { choose (1); };
        sizeL.onClick = [this] { choose (2); };

        setWantsKeyboardFocus (true);
    }

    /*  The opening animates on becoming visible, not on construction: the
        panel is built with the editor and waits until someone asks for it. */
    void visibilityChanged() override
    {
        if (! isVisible()) { stopTimer(); return; }
        fade = 0.0f;
        setAlpha (0.0f);
        startTimerHz (60);
    }

    std::function<void()>     onClose, onReveal, onRescan, onRestore;
    std::function<void (int)> onSize;

    /** The plugin's latency, shown as session information. */
    void setLatencyText (juce::String t) { latency = std::move (t); repaint(); }

    void setSizeIndex (int i)
    {
        sizeIndex = juce::jlimit (0, 2, i);
        sizeS.setSelected (sizeIndex == 0);
        sizeM.setSelected (sizeIndex == 1);
        sizeL.setSelected (sizeIndex == 2);
    }

    //==============================================================================
    void paint (juce::Graphics& g) override
    {
        using namespace juce;

        // veil: darkens what sits behind it
        g.fillAll (theme::bgDeep.withAlpha (0.80f));

        const auto card = cardBounds();

        theme::dropShadow (g, card, 12.0f, 14);
        theme::panel (g, card, 12.0f, theme::bgPanel.brighter (0.06f));

        auto left  = card.withWidth (card.getWidth() * 0.52f);
        auto right = card.withTrimmedLeft (card.getWidth() * 0.52f);

        // vertical divider, faded at both ends
        {
            const auto dx = left.getRight();
            const auto y0 = card.getY() + 46.0f, y1 = card.getBottom() - 26.0f;
            ColourGradient line (Colours::transparentBlack, dx, y0,
                                 Colours::transparentBlack, dx, y1, false);
            line.addColour (0.5, theme::stroke);
            g.setGradientFill (line);
            g.fillRect (dx, y0, 1.0f, y1 - y0);
        }

        // ---- titles ----
        g.setFont (theme::display (12.5f));
        g.setColour (theme::textBright);
        theme::tracked (g, "SETTINGS", left.withY (card.getY() + 26.0f).withHeight (20.0f), 2.4f);
        g.setColour (theme::accent);
        theme::tracked (g, "PAKKU", right.withY (card.getY() + 26.0f).withHeight (20.0f), 2.4f);

        // ---- left-hand rows ----
        g.setFont (theme::label (10.5f, true));
        const auto labelX = left.getX() + 26.0f;
        for (int i = 0; i < 6; ++i)
        {
            static const char* names[] = { "INTERFACE SIZE", "SHOW PRESET FOLDER",
                                           "RESCAN PRESET FOLDER", "RESTORE DEFAULTS",
                                           "PAKKU VERSION", "LATENCY" };
            const auto y = rowY (i);
            g.setColour (theme::text);
            theme::tracked (g, names[i],
                            Rectangle<float> (labelX, y - 7.0f, 240.0f, 14.0f), 1.1f,
                            Justification::left);
        }

        // readouts, aligned with the controls column
        auto readout = [&] (const String& text, int row)
        {
            g.setFont (theme::label (11.5f, true));
            g.setColour (theme::accent);
            g.drawText (text,
                        Rectangle<float> (left.getRight() - 152.0f, rowY (row) - 8.0f, 126.0f, 16.0f),
                        Justification::centredRight);
        };
        readout ("v" + String (JucePlugin_VersionString), 4);
        readout (latency, 5);

        // ---- credits ----
        g.setFont (theme::label (12.5f, true));
        g.setColour (theme::textBright);
        theme::tracked (g, "PRODUCTION", right.withY (creditY (0)).withHeight (16.0f), 1.6f);

        g.setFont (theme::label (11.0f, true));
        g.setColour (theme::accent);
        theme::tracked (g, "KYANTECH LABS", right.withY (creditY (0) + 19.0f).withHeight (15.0f), 1.0f);

        g.setFont (theme::label (12.5f, true));
        g.setColour (theme::textBright);
        theme::tracked (g, "PLUGIN DEVELOPER", right.withY (creditY (1)).withHeight (16.0f), 1.6f);
        // the name itself is the developer button, positioned in resized()

        // ---- origin ----
        {
            const juce::String claim ("BRAZILIAN FREE PLUGIN");
            g.setFont (theme::label (10.0f, true));

            const auto flagW = 21.0f, flagH = 15.0f;
            const auto textW = theme::trackedWidth (g.getCurrentFont(), claim, 1.5f);
            auto row = right.withY (flagY()).withHeight (18.0f)
                            .withSizeKeepingCentre (flagW + 9.0f + textW, 18.0f);

            drawBrazilFlag (g, row.removeFromLeft (flagW).withSizeKeepingCentre (flagW, flagH));
            row.removeFromLeft (9.0f);

            g.setColour (theme::text);
            theme::tracked (g, claim, row, 1.5f, Justification::left);
        }

        // ---- format badges ----
        {
            const char* formats[] = { "AU", "VST3" };
            g.setFont (theme::label (10.5f, true));
            const auto badgeY = card.getBottom() - 66.0f;
            float total = 0.0f;
            float widths[2];
            for (int i = 0; i < 2; ++i)
            {
                widths[i] = theme::trackedWidth (g.getCurrentFont(), formats[i], 1.6f) + 22.0f;
                total += widths[i] + 8.0f;
            }
            auto x = right.getCentreX() - (total - 8.0f) * 0.5f;

            for (int i = 0; i < 2; ++i)
            {
                const Rectangle<float> b (x, badgeY, widths[i], 20.0f);
                theme::well (g, b, 10.0f);
                g.setColour (theme::textDim);
                g.setFont (theme::label (10.5f, true));
                theme::tracked (g, formats[i], b, 1.6f);
                x += widths[i] + 8.0f;
            }
        }

        g.setFont (theme::label (9.5f, true));
        g.setColour (theme::textDim.withAlpha (0.8f));
        theme::tracked (g, String::fromUTF8 ("KYANTECH LABS   \xc2\xa9   2026"),
                        right.withY (card.getBottom() - 40.0f).withHeight (14.0f), 1.4f);

        // ---- left-hand footer ----
        g.setFont (theme::label (9.5f, true));
        g.setColour (theme::textDim.withAlpha (0.7f));
        theme::tracked (g, "MULTIBAND TRANSIENT SHAPER",
                        Rectangle<float> (labelX, card.getBottom() - 40.0f, 300.0f, 14.0f), 1.2f,
                        Justification::left);
    }

    void resized() override
    {
        const auto card = cardBounds();
        const auto right = card.getWidth() * 0.52f + card.getX() - 26.0f;

        close.setBounds (juce::Rectangle<float> (card.getRight() - 40.0f, card.getY() + 14.0f,
                                                 26.0f, 26.0f).toNearestInt());

        // the three sizes, pushed to the right of the column
        const auto boxes = 26.0f, gap = 7.0f;
        auto x = right - boxes;
        for (auto* b : { &sizeL, &sizeM, &sizeS })
        {
            b->setBounds (juce::Rectangle<float> (x, rowY (0) - boxes * 0.5f, boxes, boxes)
                              .toNearestInt());
            x -= boxes + gap;
        }

        auto place = [&] (juce::Button& b, int row)
        {
            b.setBounds (juce::Rectangle<float> (right - 28.0f, rowY (row) - 13.0f, 28.0f, 26.0f)
                             .toNearestInt());
        };
        place (folder, 1);
        place (rescan, 2);
        place (restore, 3);

        const auto rightCol = card.withTrimmedLeft (card.getWidth() * 0.52f);
        developer.setBounds (rightCol.withY (creditY (1) + 18.0f).withHeight (18.0f)
                                     .toNearestInt());
        sponsor.setBounds (sponsorBounds().toNearestInt());
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! cardBounds().contains (e.position.toFloat()) && onClose)
            onClose();
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k.isKeyCode (juce::KeyPress::escapeKey) && onClose) { onClose(); return true; }
        return false;
    }

private:
    void timerCallback() override
    {
        // short opening: the panel arrives in about 130 ms
        fade = juce::jmin (1.0f, fade + 0.13f);
        setAlpha (fade);
        if (fade >= 1.0f) stopTimer();
    }

    juce::Rectangle<float> cardBounds() const
    {
        return getLocalBounds().toFloat().withSizeKeepingCentre (660.0f, 408.0f);
    }

    float rowY (int row) const { return cardBounds().getY() + 86.0f + (float) row * 46.0f; }

    float creditY (int i) const { return cardBounds().getY() + 78.0f + (float) i * 58.0f; }
    float flagY()         const { return cardBounds().getY() + 196.0f; }

    juce::Rectangle<float> sponsorBounds() const
    {
        const auto card = cardBounds();
        const auto right = card.withTrimmedLeft (card.getWidth() * 0.52f);
        return juce::Rectangle<float> (0.0f, card.getY() + 232.0f, right.getWidth() - 52.0f, 52.0f)
                   .withCentre ({ right.getCentreX(), card.getY() + 232.0f + 26.0f });
    }

    /*  The Brazilian flag in four shapes: field, lozenge, disc and band.
        No stars — at this size they would only turn into noise.            */
    static void drawBrazilFlag (juce::Graphics& g, juce::Rectangle<float> r)
    {
        using namespace juce;

        g.setColour (Colour (0xff009b3a));
        g.fillRoundedRectangle (r, 1.5f);

        const auto c = r.getCentre();
        const auto dx = r.getWidth() * 0.42f, dy = r.getHeight() * 0.40f;
        Path lozenge;
        lozenge.addQuadrilateral (c.x, c.y - dy, c.x + dx, c.y, c.x, c.y + dy, c.x - dx, c.y);
        g.setColour (Colour (0xfffedf00));
        g.fillPath (lozenge);

        const auto disc = r.getHeight() * 0.24f;
        g.setColour (Colour (0xff002776));
        g.fillEllipse (c.x - disc, c.y - disc, disc * 2.0f, disc * 2.0f);

        // white band: a curved chord across the disc
        Path band;
        band.startNewSubPath (c.x - disc * 0.95f, c.y + disc * 0.42f);
        band.quadraticTo (c.x, c.y - disc * 0.35f, c.x + disc * 0.95f, c.y - disc * 0.25f);
        g.setColour (Colours::white.withAlpha (0.92f));
        g.strokePath (band, PathStrokeType (juce::jmax (1.0f, disc * 0.30f)));

        g.setColour (Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (r.reduced (0.5f), 1.5f, 1.0f);
    }

    void choose (int i) { setSizeIndex (i); if (onSize) onSize (i); }

    int sizeIndex = 1;
    float fade = 0.0f;
    juce::String latency;

    IconButton close   { icons::Id::x,                     "Close", false, 0.46f };
    IconButton folder  { icons::Id::folderOpen,            "Open the preset folder" };
    IconButton rescan  { icons::Id::arrowsClockwise,       "Re-read the preset folder" };
    IconButton restore { icons::Id::arrowCounterClockwise, "Reset every parameter" };

    LinkButton developer { "DANIEL LUIZ ALVES", "https://github.com/danielalves96",
                           icons::Id::githubLogo };
    SponsorButton sponsor;

    SizeButton sizeS { 0.42f, "Small interface" };
    SizeButton sizeM { 0.62f, "Medium interface" };
    SizeButton sizeL { 0.82f, "Large interface" };
};
} // namespace pakku
