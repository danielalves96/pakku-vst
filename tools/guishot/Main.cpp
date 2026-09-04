// Renders the plugin interface to PNG, with no DAW involved.
#include "../../src/PluginEditor.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::ArgumentList args (argc, argv);

    const auto outPath = args.containsOption ("--out")
        ? args.getValueForOption ("--out") : juce::String ("gui.png");
    const auto width  = args.containsOption ("--w") ? args.getValueForOption ("--w").getIntValue() : 1000;
    const auto height = args.containsOption ("--h") ? args.getValueForOption ("--h").getIntValue() : 580;
    const auto multi  = args.containsOption ("--multi");

    PakkuAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);

    if (args.containsOption ("--preset"))
        proc.presets.load (args.getValueForOption ("--preset"));
    else if (multi)
        if (auto* p = proc.apvts.getParameter (pid::bandMode))
            p->setValueNotifyingHost (0.0f);

    // a little signal so the display is not empty
    {
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        juce::Random rnd (1234);

        for (int block = 0; block < 200; ++block)
        {
            for (int i = 0; i < 512; ++i)
            {
                const auto n = block * 512 + i;
                const auto beat = (n % 12000) / 12000.0f;
                const auto env = std::exp (-beat * 9.0f);
                const auto s = env * (0.75f * std::sin (n * 0.05f)
                                      + 0.35f * rnd.nextFloat() - 0.17f);
                buf.setSample (0, i, s);
                buf.setSample (1, i, s);
            }
            proc.processBlock (buf, midi);
        }
    }

    for (int i = 0; i < 8; ++i)   // let the analyser accumulate frames
        proc.analyser.update();

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());

    // --scale uses the plugin's real size; without it, force an arbitrary one
    if (args.containsOption ("--scale"))
    {
        if (auto* pk = dynamic_cast<PakkuAudioProcessorEditor*> (editor.get()))
            pk->setScaleIndex (args.getValueForOption ("--scale").getIntValue());
    }
    else
    {
        editor->setSize (width, height);
    }

    // --settings opens the settings panel on top, to check its layout
    if (args.containsOption ("--settings"))
        if (auto* pk = dynamic_cast<PakkuAudioProcessorEditor*> (editor.get()))
        {
            pk->openSettings();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        }

    /*  --test-threshold drags the threshold line across the waveform area and
        checks that the parameter followed the mouse.                        */
    if (args.containsOption ("--test-threshold"))
    {
        auto* pk = dynamic_cast<PakkuAudioProcessorEditor*> (editor.get());
        if (pk == nullptr) { std::cerr << "unexpected editor type\n"; return 1; }

        auto& display = pk->getDisplay();
        auto* param = proc.apvts.getParameter (pid::threshold);

        const auto before = proc.getThresholdDb();

        // the line sits at centre minus the threshold gain
        const auto wave = display.getLocalBounds().toFloat().reduced (8.0f);
        const auto startY = wave.getCentreY() - juce::Decibels::decibelsToGain (before)
                                                    * wave.getHeight() * 0.44f;
        const auto targetY = startY + 60.0f;

        auto makeEvent = [&] (float y, bool dragged)
        {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(),
                                     { wave.getCentreX(), y }, {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                     &display, &display, juce::Time::getCurrentTime(),
                                     { wave.getCentreX(), startY }, juce::Time::getCurrentTime(),
                                     1, dragged);
        };

        display.mouseDown (makeEvent (startY, false));
        display.mouseDrag (makeEvent (targetY, true));
        display.mouseUp   (makeEvent (targetY, true));

        /*  getThresholdDb() reads the per-block cache, refreshed in
            processBlock. One block of silence is enough to carry it over. */
        {
            juce::AudioBuffer<float> silence (2, 64);
            silence.clear();
            juce::MidiBuffer midi;
            proc.processBlock (silence, midi);
        }

        const auto after = proc.getThresholdDb();
        const auto expected = juce::Decibels::gainToDecibels (
            juce::jlimit (0.001f, 1.0f,
                          (wave.getCentreY() - targetY) / (wave.getHeight() * 0.44f)));

        std::cout << "threshold before ..... " << juce::String (before, 2) << " dB\n"
                  << "threshold after ...... " << juce::String (after, 2) << " dB\n"
                  << "expected from pixel .. " << juce::String (expected, 2) << " dB\n";

        const auto ok = std::abs (after - expected) < 0.05f && std::abs (after - before) > 0.5f;
        std::cout << "threshold drag ....... " << (ok ? "OK" : "FAILED") << "\n";

        // and the knob has to follow: text formatted by the parameter itself
        std::cout << "knob readout ......... " << param->getCurrentValueAsText() << "\n";
        return ok ? 0 : 1;
    }

    /*  --menu draws a sample of the menu with our LookAndFeel.

        Capturing the real PopupMenu window does not work without an actual
        window on screen; since the whole look comes out of these four methods,
        calling them directly shows exactly what the menu will display.       */
    if (args.containsOption ("--menu"))
    {
        pakku::PakkuLookAndFeel lnf;
        auto& lf = static_cast<juce::LookAndFeel&> (lnf);

        struct Row { const char* text; bool ticked, enabled, highlighted, separator; pakku::icons::Id icon; bool hasIcon; };
        const Row rows[] = {
            { "Init",               true,  true,  false, false, {}, false },
            { "808 Squasher",       false, true,  false, false, {}, false },
            { "Beefy Snare",        false, true,  true,  false, {}, false },
            { "Clean & Controlled", false, true,  false, false, {}, false },
            { "",                   false, true,  false, true,  {}, false },
            { "Save As...",         false, true,  false, false, pakku::icons::Id::floppyDisk, true },
            { "Delete Preset",      false, false, false, false, pakku::icons::Id::trash,      true },
            { "Show Preset Folder", false, true,  false, false, pakku::icons::Id::folderOpen, true },
        };

        int itemW = 0, itemH = 0;
        lf.getIdealPopupMenuItemSize ("Clean & Controlled", false, 25, itemW, itemH);

        const int border = lf.getPopupMenuBorderSize();
        const int headerH = 22;
        const int w = itemW + border * 2;

        int h = border * 2 + headerH;
        for (const auto& r : rows)
        {
            int iw = 0, ih = 0;
            lf.getIdealPopupMenuItemSize (r.text, r.separator, 25, iw, ih);
            h += ih;
        }

        juce::Image img (juce::Image::ARGB, w * 2, h * 2, true);
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (2.0f));

        lf.drawPopupMenuBackground (g, w, h);

        int y = border;
        lf.drawPopupMenuSectionHeader (g, { border, y, w - border * 2, headerH }, "Factory");
        y += headerH;

        for (const auto& r : rows)
        {
            int iw = 0, ih = 0;
            lf.getIdealPopupMenuItemSize (r.text, r.separator, 25, iw, ih);

            std::unique_ptr<juce::Drawable> icon;
            if (r.hasIcon)
                icon = pakku::icons::drawable (r.icon, pakku::theme::text);

            lf.drawPopupMenuItem (g, { border, y, w - border * 2, ih },
                                  r.separator, r.enabled, r.highlighted, r.ticked,
                                  false, r.text, {}, icon.get(), nullptr);
            y += ih;
        }

        juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (outPath));
        out.deleteFile();
        juce::FileOutputStream stream (out);
        juce::PNGImageFormat().writeImageToStream (img, stream);
        std::cout << "menu: " << out.getFullPathName() << "  ("
                  << img.getWidth() << "x" << img.getHeight() << ")\n";
        return 0;
    }

    auto img = editor->createComponentSnapshot (editor->getLocalBounds(), false, 2.0f);

    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (outPath));
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat png;

    if (! png.writeImageToStream (img, stream))
    {
        std::cerr << "failed to write PNG\n";
        return 1;
    }

    std::cout << "written: " << out.getFullPathName()
              << "  (" << img.getWidth() << "x" << img.getHeight() << ")\n";
    return 0;
}
