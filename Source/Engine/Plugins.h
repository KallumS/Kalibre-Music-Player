/*
    Plugins - AU and VST3 effects in the player's chain (decision 0013).

    The plug-ins found are kept in a list (Preferences > Plug-ins scans for
    them, with JUCE's scanner, which remembers one that crashed and skips
    it next time). The chain is a list of slots, each a plug-in, its
    settings and whether it's bypassed; any change builds the chain again,
    prepares it and hands it to the player whole.
*/

#pragma once

#include "Player.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace kp
{

class Plugins
{
public:
    explicit Plugins (const juce::File& folder);

    juce::AudioPluginFormatManager formats;
    juce::KnownPluginList known;
    juce::File knownFile() const { return folder.getChildFile ("Plug-ins.xml"); }
    juce::File deadMansPedal() const { return folder.getChildFile ("Plug-in Scan.txt"); }
    void saveKnown();

    struct Slot
    {
        juce::PluginDescription description;
        juce::MemoryBlock state;
        bool bypassed = false;
    };
    std::vector<Slot> slots;

    // Builds the chain for the player from the slots; errors name the plug-ins that wouldn't open.
    juce::String apply (Player&);
    // The live plug-ins' settings copied into the slots (before saving or rebuilding).
    void capture (Player&);

    std::unique_ptr<juce::XmlElement> toXml() const;
    void fromXml (const juce::XmlElement&);

    // The effects only (not instruments), by maker.
    juce::Array<juce::PluginDescription> effects() const;

private:
    juce::File folder;
};

} // namespace kp
