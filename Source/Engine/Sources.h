/*
    Sources - where a location's bytes come from (decision 0004): a file on
    disk, or an entry inside a ZIP, RAR, 7-Zip or tar archive, or a gzipped
    file, read with libarchive.

    An entry is read whole into memory when it's opened (an archive's
    compressed stream can't be seeked), which a song's size allows.
*/

#pragma once

#include <juce_core/juce_core.h>

#include <memory>
#include <string>
#include <vector>

namespace kp::sources
{

// A stream of the location's bytes, or nothing (with why).
std::unique_ptr<juce::InputStream> open (const std::string& location, juce::String& error);

struct Entry
{
    std::string path;         // inside the archive
    std::int64_t size = 0;
};

// The audio files inside an archive (a .gz holds one: the file it compresses).
std::vector<Entry> list (const juce::File& archive, juce::String& error);
bool extract (const juce::File& archive, const std::string& entry, juce::MemoryBlock& out, juce::String& error);

juce::File fileOf (const std::string& location);   // the file on disk (the archive, for an entry)

} // namespace kp::sources
