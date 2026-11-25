#include "CdDrive.h"
#include "CdRom.h"

namespace kp
{

namespace
{
// A plist <dict> as key -> element.
std::map<juce::String, juce::XmlElement*> entries (juce::XmlElement* dict)
{
    std::map<juce::String, juce::XmlElement*> out;
    if (! dict) return out;
    juce::String key;
    for (auto* e : dict->getChildIterator())
    {
        if (e->hasTagName ("key")) key = e->getAllSubText();
        else if (key.isNotEmpty()) { out[key] = e; key = {}; }
    }
    return out;
}
} // namespace

std::optional<cd::Toc> parseTocPlist (const juce::String& text)
{
    auto xml = juce::XmlDocument::parse (text);
    if (! xml) return std::nullopt;
    auto* root = xml->getChildByName ("dict");
    auto top = entries (root);
    if (! top.count ("Sessions")) return std::nullopt;
    auto* sessions = top["Sessions"];
    auto* first = sessions ? sessions->getChildByName ("dict") : nullptr;
    auto s = entries (first);
    if (! s.count ("Track Array") || ! s.count ("Leadout Block")) return std::nullopt;
    cd::Toc toc;
    // The plist's blocks start at 0; MusicBrainz's count the 150-sector lead-in.
    toc.leadOut = s["Leadout Block"]->getAllSubText().getLargeIntValue() + 150;
    toc.firstTrack = s.count ("First Track") ? s["First Track"]->getAllSubText().getIntValue() : 1;
    toc.lastTrack = s.count ("Last Track") ? s["Last Track"]->getAllSubText().getIntValue() : 0;
    std::vector<std::pair<int, long>> tracks;
    for (auto* t : s["Track Array"]->getChildWithTagNameIterator ("dict"))
    {
        auto e = entries (t);
        if (! e.count ("Start Block")) continue;
        // Data tracks (an enhanced CD's last) aren't sound.
        if (e.count ("Data") && e["Data"]->hasTagName ("true")) continue;
        const int point = e.count ("Point") ? e["Point"]->getAllSubText().getIntValue() : (int) tracks.size() + 1;
        tracks.emplace_back (point, (long) e["Start Block"]->getAllSubText().getLargeIntValue() + 150);
    }
    std::sort (tracks.begin(), tracks.end());
    for (auto& t : tracks) toc.offsets.push_back (t.second);
    if (toc.offsets.empty()) return std::nullopt;
    if (toc.lastTrack < toc.firstTrack || toc.lastTrack - toc.firstTrack + 1 != (int) toc.offsets.size())
        toc.lastTrack = toc.firstTrack + (int) toc.offsets.size() - 1;
    return toc;
}

std::vector<AudioCdDisc> findAudioCds (const juce::File& volumes)
{
    std::vector<AudioCdDisc> out;
    // Mounted as files (the Mac).
    for (auto& v : volumes.findChildFiles (juce::File::findDirectories, false))
    {
        auto plist = v.getChildFile (".TOC.plist");
        if (! plist.existsAsFile()) continue;
        AudioCdDisc d;
        d.label = v.getFileName();
        auto files = v.findChildFiles (juce::File::findFiles, false, "*.aiff;*.aif;*.AIFF");
        std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
                   { return a.getFileName().getIntValue() < b.getFileName().getIntValue(); });
        for (auto& f : files) if (! f.getFileName().startsWithChar ('.')) d.tracks.push_back (f.getFullPathName().toStdString());
        if (auto toc = parseTocPlist (plist.loadFileAsString())) d.toc = *toc;
        if (! d.tracks.empty()) out.push_back (std::move (d));
    }
    // Read from the drive (Windows, Linux).
    for (auto& drive : cdrom::drives())
    {
        juce::String error;
        auto disc = cdrom::readToc (drive.device, error);
        if (! disc) continue;
        AudioCdDisc d;
        d.label = "Audio CD (" + juce::String (drive.name) + ")";
        d.toc = disc->toc;
        for (int k = 0; k < (int) disc->firstLba.size(); ++k) d.tracks.push_back (cdrom::location (drive.device, disc->toc.firstTrack + k));
        out.push_back (std::move (d));
    }
    return out;
}

std::vector<cd::Album> lookUp (const cd::Toc& toc, juce::String& error)
{
    const juce::URL url (juce::String (cd::lookupUrl (toc)));
    int status = 0;
    // MusicBrainz asks every program to name itself.
    auto stream = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                             .withExtraHeaders ("User-Agent: KalibrePlayer/" KP_VERSION "\r\nAccept: application/json")
                                             .withConnectionTimeoutMs (15000)
                                             .withStatusCode (&status));
    if (! stream) { error = "MusicBrainz couldn't be reached"; return {}; }
    const auto body = stream->readEntireStreamAsString();
    if (status == 404) { error = "MusicBrainz doesn't know this disc"; return {}; }
    if (status != 200) { error = "MusicBrainz answered " + juce::String (status); return {}; }
    auto albums = cd::parseLookup (body.toStdString(), cd::discId (toc));
    if (albums.empty()) error = "MusicBrainz doesn't know this disc";
    return albums;
}

} // namespace kp
