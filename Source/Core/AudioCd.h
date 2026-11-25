/*
    AudioCd - what the player knows of an audio CD (decision 0010): its table
    of contents, its MusicBrainz disc ID, and the album MusicBrainz names for
    it.

    On a Mac an audio CD appears as a disk of AIFF files, one per track, with
    the table of contents beside them (.TOC.plist); the app reads that and
    hands the offsets here. Playing a CD is playing those files; ripping is
    converting them, named and tagged from MusicBrainz.

    No JUCE in here.
*/

#pragma once

#include "Track.h"

#include <optional>
#include <string>
#include <vector>

namespace kp::cd
{

struct Toc
{
    int firstTrack = 1, lastTrack = 0;
    std::vector<long> offsets;   // each track's start in sectors (75 a second), with the 150-sector lead-in
    long leadOut = 0;            // where the disc ends, in sectors with the lead-in
    int count() const { return (int) offsets.size(); }
    double trackSeconds (int index) const;   // 0-based
};

std::string sha1 (const std::string& bytes);    // 20 raw bytes
std::string discId (const Toc&);                // MusicBrainz's 28-character ID
std::string lookupUrl (const Toc&);             // MusicBrainz's web service, asking for the release and its tracks

struct Album
{
    std::string title, artist, date, releaseId;
    std::vector<std::string> trackTitles, trackArtists;
    int discNumber = 1, totalDiscs = 1;
};
// The releases MusicBrainz's answer names (JSON): the first is usually the one.
std::vector<Album> parseLookup (const std::string& json, const std::string& discId);
// The tags for one track of a ripped album (index 0-based).
Tags tagsFor (const Album&, int index, int trackCount);

} // namespace kp::cd
