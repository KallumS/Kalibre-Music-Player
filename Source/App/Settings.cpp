#include "Settings.h"

namespace kp
{

Settings::Settings (const juce::File& folder)
{
    dir = folder != juce::File() ? folder
        : juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             #if JUCE_MAC
              .getChildFile ("Application Support")
             #endif
              .getChildFile ("Kalibre Player");
    dir.createDirectory();
    juce::PropertiesFile::Options o;
    o.millisecondsBeforeSaving = 2000;
    o.storageFormat = juce::PropertiesFile::storeAsXML;
    props = std::make_unique<juce::PropertiesFile> (dir.getChildFile ("Settings.xml"), o);
}

Settings::~Settings() { save(); }

juce::String Settings::get (const juce::String& k, const juce::String& f) const { return props->getValue (k, f); }
int Settings::getInt (const juce::String& k, int f) const { return props->getIntValue (k, f); }
double Settings::getDouble (const juce::String& k, double f) const { return props->getDoubleValue (k, f); }
bool Settings::getBool (const juce::String& k, bool f) const { return props->getBoolValue (k, f); }
void Settings::set (const juce::String& k, const juce::var& v) { props->setValue (k, v); }
std::unique_ptr<juce::XmlElement> Settings::getXml (const juce::String& k) const { return props->getXmlValue (k); }
void Settings::setXml (const juce::String& k, const juce::XmlElement* x) { props->setValue (k, x); }
void Settings::save() { props->saveIfNeeded(); }

juce::String Settings::load (const juce::String& name) const
{
    auto f = dir.getChildFile (name);
    return f.existsAsFile() ? f.loadFileAsString() : juce::String();
}

bool Settings::store (const juce::String& name, const juce::String& text) const
{
    // Written beside, then put in place: a crash mid-write never loses the library.
    auto f = dir.getChildFile (name);
    juce::TemporaryFile temp (f);
    if (! temp.getFile().replaceWithText (text, false, false, "\n")) return false;
    return temp.overwriteTargetFileWithTemporary();
}

} // namespace kp
