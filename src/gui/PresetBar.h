#pragma once
#include "Theme.h"
#include "Icons.h"
#include "../PresetManager.h"

namespace pakku
{
/** Icon-only button. The geometry comes from Phosphor; only the frame is here. */
class IconButton : public juce::Button
{
public:
    IconButton (icons::Id i, juce::String tip, bool framed = true, float fill = 0.52f)
        : juce::Button (tip), icon (i), hasFrame (framed), fraction (fill)
    {
        setTooltip (tip);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);

        if (hasFrame)
        {
            if (over && ! down)
            {
                g.setColour (theme::accentGlow.withMultipliedAlpha (0.35f));
                g.fillRoundedRectangle (r.expanded (1.5f), 6.5f);
            }
            theme::panel (g, r, 5.0f, down ? theme::bgWell : theme::bgPanel);
        }

        const auto side = juce::jmin (r.getWidth(), r.getHeight()) * fraction;
        icons::draw (g, icon, r.withSizeKeepingCentre (side, side),
                     down ? theme::accentHi : over ? theme::textBright : theme::text);
    }

private:
    icons::Id icon;
    bool hasFrame;
    float fraction;
};

/*  Preset bar at the top: arrow, clickable name, arrow.
    The name opens a menu with factory, user and the save/delete actions.  */
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (PresetManager& m) : manager (m)
    {
        addAndMakeVisible (prev);
        addAndMakeVisible (next);
        prev.onClick = [this] { manager.step (-1); };
        next.onClick = [this] { manager.step (+1); };
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (26.0f, 0.0f);
        theme::well (g, r, 5.0f);

        if (hovering)
        {
            g.setColour (theme::accent.withAlpha (0.22f));
            g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        }

        icons::draw (g, icons::Id::file,
                     r.removeFromLeft (28.0f).withSizeKeepingCentre (13.0f, 13.0f),
                     theme::textDim);

        auto caret = r.removeFromRight (20.0f);
        icons::draw (g, icons::Id::caretDown, caret.withSizeKeepingCentre (10.0f, 10.0f),
                     hovering ? theme::accent : theme::textDim);

        g.setFont (theme::label (12.5f, true));
        g.setColour (hovering ? theme::accentHi : theme::accent);
        theme::tracked (g, manager.getCurrent().toUpperCase(), r.reduced (4.0f, 0.0f), 1.0f);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        prev.setBounds (r.removeFromLeft (24));
        next.setBounds (r.removeFromRight (24));
    }

    void mouseEnter (const juce::MouseEvent&) override { hovering = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hovering = false; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.x < 26 || e.x > getWidth() - 26) return;   // the arrows handle themselves
        showMenu();
    }

    /** Repaints when the preset changes by some other route. */
    void refresh() { repaint(); }

    /** Public so the screenshot bench can capture the menu. */
    void showMenu()
    {
        juce::PopupMenu menu;
        const auto current = manager.getCurrent();

        menu.addSectionHeader ("Factory");
        for (const auto& n : manager.getFactoryNames())
            menu.addItem (n, true, n == current, [this, n] { manager.load (n); });

        const auto users = manager.getUserNames();
        if (! users.isEmpty())
        {
            menu.addSeparator();
            menu.addSectionHeader ("User");
            for (const auto& n : users)
                menu.addItem (n, true, n == current, [this, n] { manager.load (n); });
        }

        menu.addSeparator();

        auto action = [] (const juce::String& text, icons::Id id, bool enabled,
                          std::function<void()> fn)
        {
            juce::PopupMenu::Item item (text);
            item.setEnabled (enabled);
            item.setAction (std::move (fn));
            item.setImage (icons::drawable (id, theme::text));
            return item;
        };

        menu.addItem (action ("Save As...", icons::Id::floppyDisk, true,
                              [this] { promptSave(); }));
        menu.addItem (action ("Delete Preset", icons::Id::trash,
                              ! manager.isFactory (current),
                              [this, current] { manager.remove (current); }));
        menu.addItem (action ("Show Preset Folder", icons::Id::folderOpen, true,
                              [] { PresetManager::getRootDirectory().revealToUser(); }));

        menu.showMenuAsync (juce::PopupMenu::Options()
                                .withTargetComponent (this)
                                .withMinimumNumColumns (2)
                                .withMaximumNumColumns (3));
    }

private:
    void promptSave()
    {
        auto* win = new juce::AlertWindow ("Save Preset", "Preset name:",
                                           juce::MessageBoxIconType::NoIcon);
        win->addTextEditor ("name", manager.getCurrent());
        win->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
        win->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        win->enterModalState (true, juce::ModalCallbackFunction::create (
            [this, win] (int result)
            {
                if (result == 1)
                    manager.save (win->getTextEditorContents ("name").trim());
                delete win;
            }), false);
    }

    PresetManager& manager;
    bool hovering = false;
    IconButton prev { icons::Id::caretLeft,  "Previous preset", false, 0.40f };
    IconButton next { icons::Id::caretRight, "Next preset",     false, 0.40f };
};
} // namespace pakku
