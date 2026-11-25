/*
    TagIO - reading and writing tags with TagLib (decision 0009), the same
    for every format: ID3 in MP3s, Vorbis comments in FLAC, Ogg and Opus,
    MP4 atoms, APE tags in Musepack and WavPack, ASF in WMA, and the RIFF
    and AIFF chunks.

    Tags are named as foobar2000 names them ("album artist", "tracknumber",
    "totaltracks"), whatever each format calls them. Files inside archives
    are read, never written.
*/

#pragma once

#include "Track.h"

#include <juce_graphics/juce_graphics.h>

namespace kp::tagio
{

// The track's tags and what its file says about itself. False if it isn't audio.
bool read (const std::string& location, Track&, juce::String& error);
// Writes every tag the track has, taking out those it hasn't.
bool write (const std::string& location, const Tags&, juce::String& error);
bool canWrite (const std::string& location);

// The cover: the picture inside the file, or cover.jpg, folder.jpg and the like beside it.
juce::Image cover (const std::string& location);
// The picture's bytes inside the file (for copying into a converted file).
juce::MemoryBlock embeddedCover (const std::string& location, juce::String& mimeType);
bool writeCover (const std::string& location, const juce::MemoryBlock&, const juce::String& mimeType);

} // namespace kp::tagio
