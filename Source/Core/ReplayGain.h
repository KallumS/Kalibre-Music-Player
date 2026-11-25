/*
    ReplayGain - every track played at the same loudness (decision 0008).

    Scanning: ReplayGain 2.0, which is EBU R128's integrated loudness (ITU-R
    BS.1770-4, the same measure as Kalibre's loudness meter) against a
    reference of -18 LUFS. A track's gain is how far it is from that; an
    album's comes from all its tracks' audio gated together, so the album
    keeps its own quiet and loud songs. Peaks are the highest sample.

    Playing: the gain from the tags (REPLAYGAIN_TRACK_GAIN and the rest),
    track or album as chosen, with a preamp, and turned down if the peak
    would clip. A track with no tags can be scanned as it's queued, so its
    gain is known before it plays ("on the fly").

    No JUCE in here.
*/

#pragma once

#include "Track.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace kp::rg
{

inline constexpr double referenceLufs = -18.0;

struct Result
{
    double loudness = -70;           // integrated, LUFS (-70 for silence)
    double gain = 0;                 // dB to bring it to the reference
    double peak = 0;                 // linear, the highest sample (1.0 = full scale)
    std::vector<float> blocks;       // the 400 ms blocks' energies, for an album's gain
};

class Analyzer
{
public:
    void prepare (double sampleRate, int channels);
    // Planar samples: channels[c][i].
    void process (const float* const* channels, int numChannels, int samples);
    Result finish();

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        double run (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    };
    double rate = 48000;
    int chans = 2;
    std::vector<std::array<Biquad, 2>> filters;   // per channel: shelf, high-pass
    int hopLength = 4800, hopFill = 0;
    double hopEnergy = 0;
    std::vector<double> hops;                     // 100 ms energies
    double peak = 0;
};

// Integrated loudness from blocks' energies (gated at -70 LUFS and 10 LU under the mean).
double integrated (const std::vector<float>& blocks);
Result album (const std::vector<const Result*>& tracks);

struct Values
{
    std::optional<double> trackGain, trackPeak, albumGain, albumPeak;
    bool empty() const { return ! trackGain && ! albumGain; }
};
Values read (const Track&);
// The tags to write for a result (and an album's, if given).
Tags tagsFor (const Result& track, const Result* album = nullptr);
std::string formatGain (double db);     // "-6.54 dB"
std::string formatPeak (double linear); // "0.987654"
std::optional<double> parseGain (const std::string&);

enum class Mode { off, track, album, smart };   // smart: album gain while an album plays in order, track gain otherwise
const char* modeName (Mode);

struct Settings
{
    Mode mode = Mode::track;
    double preampWith = 0;       // dB, for tracks with ReplayGain
    double preampWithout = 0;    // dB, for tracks without
    bool preventClipping = true;
    bool scanMissing = true;     // scan tracks without tags as they're queued
};

// The linear gain to play a track at. `albumContext`: smart mode is playing an album in order.
double linearGain (const Values&, const Settings&, bool albumContext);

} // namespace kp::rg
