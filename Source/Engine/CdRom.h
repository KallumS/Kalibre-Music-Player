/*
    CdRom - reading an audio CD straight from the drive, on Windows and Linux
    (decision 0019). A Mac shows a CD as AIFF files (CdDrive.cpp); Windows
    and Linux don't, so there the player asks the drive for its table of
    contents and for raw audio sectors (2,352 bytes each, 1/75 of a second of
    44.1 kHz 16-bit stereo).

    A CD track's location is "cdda:" + the drive + "|" + its number
    ("cdda:D:|3", "cdda:/dev/sr0|3"). Opened, it reads as a WAV file whose
    sound comes from the disc as it's read, so it plays, seeks and converts
    as any file does.
*/

#pragma once

#include "AudioCd.h"

#include <juce_core/juce_core.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace kp::cdrom
{

inline constexpr int sectorBytes = 2352;

struct Drive
{
    std::string device;   // "D:" on Windows, "/dev/sr0" on Linux
    std::string name;     // what to call it
};
std::vector<Drive> drives();

// Reads raw audio sectors (by logical block address, from 0).
class SectorReader
{
public:
    virtual ~SectorReader() = default;
    virtual bool read (long lba, int count, unsigned char* out) = 0;
};
std::unique_ptr<SectorReader> open (const std::string& device, juce::String& error);

struct Disc
{
    cd::Toc toc;                   // MusicBrainz's form: offsets with the 150-sector lead-in
    std::vector<long> firstLba;    // each audio track's first sector
    std::vector<long> sectors;     // and how many it has
};
std::optional<Disc> readToc (const std::string& device, juce::String& error);

// A track as a WAV file: a header, then its sectors as they're read.
std::unique_ptr<juce::InputStream> trackStream (std::unique_ptr<SectorReader>, long firstLba, long sectors);

// Locations of CD tracks.
std::string location (const std::string& device, int track);
bool isLocation (const std::string&);
// The device and track number of a location (track from 1).
std::optional<std::pair<std::string, int>> split (const std::string&);
std::unique_ptr<juce::InputStream> openLocation (const std::string&, juce::String& error);

// For tests: the TOC's sectors as tracks (the last audio track ends 11,400 before a data track).
Disc discFrom (int first, const std::vector<long>& startLbas, const std::vector<bool>& isData, long leadOutLba);

} // namespace kp::cdrom
