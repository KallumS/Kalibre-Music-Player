/*
    CdDrive - audio CDs in the drive (decision 0010). macOS shows an audio
    CD as a disk of AIFF files, "1 Audio Track.aiff" and on, with its table
    of contents beside them in a hidden .TOC.plist. Windows and Linux are
    asked for the disc's contents and its sectors (CdRom, decision 0019).
    Either way a disc is a list of track locations: playing a CD is playing
    those; ripping converts them, named from MusicBrainz.
*/

#pragma once

#include "AudioCd.h"

#include <juce_core/juce_core.h>

#include <optional>

namespace kp
{

struct AudioCdDisc
{
    juce::String label;                 // the volume's name, or the drive's
    std::vector<std::string> tracks;    // locations, in order
    cd::Toc toc;
    juce::String name() const { return label; }
};

// The audio CDs in the drives now (on a Mac, those mounted under `volumes`).
std::vector<AudioCdDisc> findAudioCds (const juce::File& volumes = juce::File ("/Volumes"));
// The table of contents from a .TOC.plist's text.
std::optional<cd::Toc> parseTocPlist (const juce::String& xml);
// Asks MusicBrainz (on a background thread: it waits for the network).
std::vector<cd::Album> lookUp (const cd::Toc&, juce::String& error);

} // namespace kp
