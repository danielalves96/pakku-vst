#pragma once
#include <juce_data_structures/juce_data_structures.h>

namespace pakku
{
/*  The .pkku format — Pakku's preset container.

        bytes 0-3   'P','K','K','U'
        bytes 4-7   format version, uint32 little-endian
        bytes 8+    binary ValueTree

    The signature and the version sit outside the tree on purpose: the
    serialisation can change later without ambiguity, because the version is
    read before anything tries to interpret the rest.

    Tree:
        <PakkuPreset name plugin pluginVersion created>
            <PARAM id="inputGain" value="0.0"/>
            ...

    Values are the parameter's REAL values — dB, Hz, percent — not
    normalised ones, so a file stays readable if a range ever changes.     */
namespace preset
{
    inline constexpr const char* extension = "pkku";
    inline constexpr juce::uint32 currentVersion = 1;
    inline constexpr const char* rootTag = "PakkuPreset";

    inline constexpr char magic[4] = { 'P', 'K', 'K', 'U' };

    /** Builds the tree from id/value pairs. */
    inline juce::ValueTree makeTree (const juce::String& name,
                                     const juce::String& pluginVersion,
                                     const std::vector<std::pair<juce::String, float>>& values)
    {
        juce::ValueTree tree (rootTag);
        tree.setProperty ("name", name, nullptr);
        tree.setProperty ("plugin", "Pakku", nullptr);
        tree.setProperty ("pluginVersion", pluginVersion, nullptr);
        tree.setProperty ("created",
                          juce::Time::getCurrentTime().toISO8601 (true), nullptr);

        for (const auto& [id, v] : values)
        {
            juce::ValueTree p ("PARAM");
            p.setProperty ("id", id, nullptr);
            p.setProperty ("value", (double) v, nullptr);
            tree.appendChild (p, nullptr);
        }
        return tree;
    }

    inline bool write (const juce::File& file, const juce::ValueTree& tree)
    {
        if (! tree.hasType (rootTag)) return false;

        file.deleteFile();
        juce::FileOutputStream out (file);
        if (! out.openedOk()) return false;

        out.write (magic, sizeof (magic));
        out.writeInt ((int) currentVersion);
        tree.writeToStream (out);
        return out.getStatus().wasOk();
    }

    /** Returns an invalid tree if the signature or the version do not fit. */
    inline juce::ValueTree read (juce::InputStream& in)
    {
        char sig[4] {};
        if (in.read (sig, sizeof (sig)) != (int) sizeof (sig)) return {};
        if (std::memcmp (sig, magic, sizeof (magic)) != 0)      return {};

        const auto version = (juce::uint32) in.readInt();
        if (version == 0 || version > currentVersion)           return {};

        auto tree = juce::ValueTree::readFromStream (in);
        return tree.hasType (rootTag) ? tree : juce::ValueTree{};
    }

    inline juce::ValueTree read (const juce::File& file)
    {
        juce::FileInputStream in (file);
        return in.openedOk() ? read (in) : juce::ValueTree{};
    }

    inline juce::String nameOf (const juce::ValueTree& tree)
    {
        return tree.getProperty ("name").toString();
    }
}
} // namespace pakku
