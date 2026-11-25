/*
    Track - one piece of music the player knows: where it is, its tags, and
    what its file says about itself (length, rate, codec).

    A location is a path on disk, or an entry inside an archive (decision
    0004): "archive:" then the archive's path, "|", and the entry's path in it
    ("archive:/Music/Live.zip|CD1/01 Intro.flac").

    Tags are kept by lower-case name ("artist", "album artist", "tracknumber"),
    each with one value or several (two artists). Shown joined by ", ".

    No JUCE in here.
*/

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kp
{

using Tags = std::map<std::string, std::vector<std::string>>;

struct Track
{
    std::string location;
    Tags tags;

    double length = 0;          // seconds
    int sampleRate = 0;
    int channels = 0;
    int bitsPerSample = 0;      // 0 for lossy
    int bitrate = 0;            // kb/s
    std::string codec;          // "FLAC", "MP3", "AAC"
    std::int64_t fileSize = 0;
    std::int64_t modified = 0;  // ms since 1970, for the library's rescans
    std::int64_t added = 0;     // ms since 1970, when the library first saw it

    // The first value of a tag, or "".
    std::string first (const std::string& name) const;
    // Every value joined by ", ".
    std::string joined (const std::string& name) const;
    bool has (const std::string& name) const;
    void set (const std::string& name, const std::string& value);   // "" removes it
};

std::string lower (std::string_view);
std::string upper (std::string_view);
std::string trim (std::string_view);
bool containsIgnoringCase (std::string_view haystack, std::string_view needle);
bool equalsIgnoringCase (std::string_view, std::string_view);

namespace location
{
inline constexpr std::string_view archivePrefix = "archive:";
bool isArchive (std::string_view);
std::string archive (const std::string& archivePath, const std::string& entry);
// The archive's path and the entry's, or nothing for a plain file.
std::optional<std::pair<std::string, std::string>> split (std::string_view);
// The file the location is (or is inside): the archive for an entry.
std::string container (std::string_view);
// What to call it: the file's name without folders ("01 Intro.flac").
std::string fileName (std::string_view);
std::string extension (std::string_view);   // lower-case, without the dot: "flac"
std::string directory (std::string_view);   // the folder it's in (inside an archive: the archive's folder joined with the entry's)
} // namespace location

// The extensions the player opens (decision 0003), lower-case.
const std::vector<std::string>& audioExtensions();
bool isAudioExtension (std::string_view ext);
const std::vector<std::string>& archiveExtensions();   // zip, gz, rar, 7z, tar...
bool isArchiveExtension (std::string_view fileNameOrExt);

// "4:05", "1:02:03".
std::string formatTime (double seconds);

} // namespace kp
