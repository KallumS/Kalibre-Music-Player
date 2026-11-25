/*
    Settings - what the player remembers between runs, in
    ~/Library/Application Support/Kalibre Player: Settings.xml for choices,
    and beside it the library, the playlists, the layout, the plug-in chain
    and the add-ons.
*/

#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace kp
{

class Settings
{
public:
    // `folder` empty: the usual place. The tests give one of their own.
    explicit Settings (const juce::File& folder = {});
    ~Settings();

    juce::File folder() const { return dir; }
    juce::File file (const juce::String& name) const { return dir.getChildFile (name); }
    juce::File addonsFolder() const { return dir.getChildFile ("Add-ons"); }

    juce::String get (const juce::String& key, const juce::String& fallback = {}) const;
    int getInt (const juce::String& key, int fallback) const;
    double getDouble (const juce::String& key, double fallback) const;
    bool getBool (const juce::String& key, bool fallback) const;
    void set (const juce::String& key, const juce::var& value);
    std::unique_ptr<juce::XmlElement> getXml (const juce::String& key) const;
    void setXml (const juce::String& key, const juce::XmlElement*);
    void save();

    // Text kept in a file of its own (the library, the playlists).
    juce::String load (const juce::String& name) const;
    bool store (const juce::String& name, const juce::String& text) const;

private:
    juce::File dir;
    std::unique_ptr<juce::PropertiesFile> props;
};

} // namespace kp
