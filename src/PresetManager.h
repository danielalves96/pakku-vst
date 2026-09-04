#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "PresetFormat.h"
#include "PresetData.h"

namespace pakku
{
/*  Presets in Pakku's own .pkku format.

    Em disco, dentro de ~/Library/Audio/Presets/Kyantech Labs/Pakku/ :

        Factory/   the shipped ones, written on first run
        User/      the user's — saved from the plugin, or brought in

    The factory presets also live compiled into the binary. That embedded
    copy is the safety net: if a file in Factory goes missing or comes back
    unreadable, it is rewritten from there. The folder is what counts while it
    is readable, otherwise having the folder would serve no purpose.

    Factory and user share one format: a preset the user saves is
    indistinguishable from a shipped one, it just lives somewhere else.    */
class PresetManager
{
public:
    static constexpr const char* extension = preset::extension;

    explicit PresetManager (juce::AudioProcessorValueTreeState& state) : apvts (state)
    {
        ensureFolders();
        migrateLoosePresets();
        materialiseFactory();
        loadFactory();

        current = initName;
    }

    /*  ~/Library/Audio/Presets/Kyantech Labs/Pakku on macOS, which is where
        plugins keep presets there; %APPDATA%\Kyantech Labs\Pakku on Windows,
        which is the convention there. Same Factory/User tree on both.      */
    static juce::File getRootDirectory()
    {
        auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);

       #if JUCE_MAC
        base = base.getChildFile ("Audio").getChildFile ("Presets");
       #endif

        return base.getChildFile ("Kyantech Labs").getChildFile ("Pakku");
    }

    static juce::File getFactoryDirectory() { return getRootDirectory().getChildFile ("Factory"); }
    static juce::File getUserDirectory()    { return getRootDirectory().getChildFile ("User"); }

    /** Re-reads both folders, for when a .pkku was dropped in from outside. */
    void rescan()
    {
        ensureFolders();
        materialiseFactory();

        factoryTrees.clear();
        factoryNames.clear();
        loadFactory();

        if (onChange) onChange();
    }

    //==============================================================================
    const juce::StringArray& getFactoryNames() const { return factoryNames; }

    juce::StringArray getUserNames() const
    {
        juce::StringArray names;
        for (const auto& f : getUserDirectory().findChildFiles (
                 juce::File::findFiles, false, juce::String ("*.") + extension))
        {
            const auto n = f.getFileNameWithoutExtension();
            if (! factoryNames.contains (n))
                names.add (n);
        }
        names.sort (true);
        return names;
    }

    bool isFactory (const juce::String& name) const
    {
        return factoryNames.contains (name);
    }
    juce::String getCurrent() const { return current; }

    //==============================================================================
    void load (const juce::String& name)
    {
        if (const auto it = factoryTrees.find (name); it != factoryTrees.end())
        {
            apply (it->second);
            finish (name);
            return;
        }

        const auto file = getUserDirectory().getChildFile (name + "." + extension);
        if (const auto tree = preset::read (file); tree.isValid())
        {
            apply (tree);
            finish (name);
        }
    }

    /** Saves the current state. Overwrites if the name already exists. */
    bool save (const juce::String& rawName)
    {
        const auto name = rawName.trim();
        if (name.isEmpty() || isFactory (name)) return false;

        std::vector<std::pair<juce::String, float>> values;
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                values.emplace_back (rp->paramID,
                                     apvts.getRawParameterValue (rp->paramID)->load());

        const auto tree = preset::makeTree (name, JucePlugin_VersionString, values);
        const auto file = getUserDirectory().getChildFile (name + "." + extension);

        if (! preset::write (file, tree)) return false;

        finish (name);
        return true;
    }

    void remove (const juce::String& name)
    {
        if (isFactory (name)) return;

        getUserDirectory().getChildFile (name + "." + extension).deleteFile();

        if (current == name && ! factoryNames.isEmpty())
            current = factoryNames[0];

        if (onChange) onChange();
    }

    void step (int delta)
    {
        auto all = factoryNames;
        all.addArray (getUserNames());
        if (all.isEmpty()) return;

        auto i = all.indexOf (current);
        if (i < 0)
        {
            load (delta >= 0 ? all[0] : all[all.size() - 1]);
            return;
        }
        load (all[(i + delta + all.size()) % all.size()]);
    }

    std::function<void()> onChange;

private:
    static constexpr const char* initName = "Init";

    /*  Init is built from the parameter defaults, not from a file.

        Without it the list has no way back: once you left Init for any other
        preset, returning to a neutral state meant reloading the plugin.     */
    juce::ValueTree makeInitTree() const
    {
        std::vector<std::pair<juce::String, float>> values;

        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                values.emplace_back (rp->paramID,
                                     rp->convertFrom0to1 (rp->getDefaultValue()));

        return preset::makeTree (initName, JucePlugin_VersionString, values);
    }

    struct Embedded { juce::String filename; const char* data; int size; };

    /** The factory .pkku files compiled into the binary. */
    static const std::vector<Embedded>& embedded()
    {
        static const std::vector<Embedded> list = []
        {
            std::vector<Embedded> v;
            for (int i = 0; i < PresetData::namedResourceListSize; ++i)
            {
                int size = 0;
                const auto* data = PresetData::getNamedResource (
                    PresetData::namedResourceList[i], size);

                if (data != nullptr && size > 0)
                    v.push_back ({ PresetData::getNamedResourceOriginalFilename (
                                       PresetData::namedResourceList[i]),
                                   data, size });
            }
            return v;
        }();
        return list;
    }

    static const Embedded* findEmbedded (const juce::String& filename)
    {
        for (const auto& e : embedded())
            if (e.filename == filename)
                return &e;
        return nullptr;
    }

    static void ensureFolders()
    {
        for (auto d : { getRootDirectory(), getFactoryDirectory(), getUserDirectory() })
            if (! d.isDirectory())
                d.createDirectory();
    }

    /*  Before the subfolders existed, user presets went straight into the
        root. Anyone who already had some does not lose them in the move.   */
    static void migrateLoosePresets()
    {
        const auto pattern = juce::String ("*.") + extension;
        for (const auto& f : getRootDirectory().findChildFiles (
                 juce::File::findFiles, false, pattern))
            f.moveFileTo (getUserDirectory().getChildFile (f.getFileName()));
    }

    /*  Keeps the Factory folder matching what shipped in the binary.

        Writes what is missing and rewrites what differs. Mirroring is what
        lets an update deliver a new or corrected preset: if the folder won
        over the binary, the previous version's copy would stay forever. Your
        own presets live in User/, where nothing is touched.                */
    static void materialiseFactory()
    {
        for (const auto& e : embedded())
        {
            const auto file = getFactoryDirectory().getChildFile (e.filename);

            if (file.existsAsFile() && file.getSize() == (juce::int64) e.size)
            {
                juce::MemoryBlock onDisk;
                if (file.loadFileAsData (onDisk)
                    && onDisk.getSize() == (size_t) e.size
                    && std::memcmp (onDisk.getData(), e.data, (size_t) e.size) == 0)
                    continue;
            }

            file.replaceWithData (e.data, (size_t) e.size);
        }
    }

    void loadFactory()
    {
        const auto pattern = juce::String ("*.") + extension;

        for (const auto& f : getFactoryDirectory().findChildFiles (
                 juce::File::findFiles, false, pattern))
        {
            auto tree = preset::read (f);

            // unreadable file: restore the embedded copy and try again
            if (! tree.isValid())
                if (const auto* e = findEmbedded (f.getFileName()))
                {
                    f.replaceWithData (e->data, (size_t) e->size);
                    tree = preset::read (f);
                }

            if (! tree.isValid()) continue;

            auto name = preset::nameOf (tree);
            if (name.isEmpty()) name = f.getFileNameWithoutExtension();

            factoryTrees[name] = tree;
            factoryNames.addIfNotAlreadyThere (name);
        }

        // folder unreachable: the embedded copies hold the list up
        if (factoryNames.isEmpty())
            loadEmbeddedFactory();

        factoryNames.sort (true);

        // Init heads the list: it is the starting point, not one more preset
        factoryTrees[initName] = makeInitTree();
        factoryNames.insert (0, initName);
    }

    void loadEmbeddedFactory()
    {
        for (const auto& e : embedded())
        {
            juce::MemoryInputStream in (e.data, (size_t) e.size, false);
            const auto tree = preset::read (in);
            if (! tree.isValid()) continue;

            auto name = preset::nameOf (tree);
            if (name.isEmpty()) name = juce::File (e.filename).getFileNameWithoutExtension();

            factoryTrees[name] = tree;
            factoryNames.addIfNotAlreadyThere (name);
        }
    }

    void apply (const juce::ValueTree& tree)
    {
        /*  Values in the file are the parameter's real values, linear scale.

            An older batch went out on a percent scale and was converted here
            at load time, based on the stored version. That held the numbering
            hostage: releasing 1.0.0 would have sent every new preset down the
            compatibility path. The files were rewritten once and that path no
            longer exists.                                                  */
        std::map<juce::String, float> values;
        for (const auto& child : tree)
            if (child.hasProperty ("id"))
                values[child.getProperty ("id").toString()]
                    = (float) (double) child.getProperty ("value");

        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                const auto it = values.find (rp->paramID);

                // a missing parameter falls back to its default, so an older
                // preset without a newer field still loads clean rather than
                // inheriting whatever was there before
                const auto norm = it != values.end()
                    ? rp->convertTo0to1 (
                          rp->getNormalisableRange().snapToLegalValue (it->second))
                    : rp->getDefaultValue();

                rp->beginChangeGesture();
                rp->setValueNotifyingHost (norm);
                rp->endChangeGesture();
            }
    }

    void finish (const juce::String& name)
    {
        current = name;
        if (onChange) onChange();
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::map<juce::String, juce::ValueTree> factoryTrees;
    juce::StringArray factoryNames;
    juce::String current;
};
} // namespace pakku
