#include "ReplayGain.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace kp::rg
{

namespace
{
constexpr double pi = 3.141592653589793;
inline double lufsOf (double meanSquare) { return meanSquare > 0 ? -0.691 + 10.0 * std::log10 (meanSquare) : -200.0; }
inline double energyOf (double lufs) { return std::pow (10.0, (lufs + 0.691) / 10.0); }
}

void Analyzer::prepare (double sampleRate, int channels)
{
    rate = sampleRate > 0 ? sampleRate : 48000;
    chans = std::max (1, channels);
    filters.assign ((size_t) chans, {});
    // K-weighting (BS.1770-4) from its analogue prototype, as in Kalibre's Loudness.
    const double f0s = 1681.974450955533, gain = 3.999843853973347, qs = 0.7071752369554196;
    const double ks = std::tan (pi * f0s / rate);
    const double vh = std::pow (10.0, gain / 20.0), vb = std::pow (vh, 0.4996667741545416);
    const double a0s = 1.0 + ks / qs + ks * ks;
    const double f0h = 38.13547087602444, qh = 0.5003270373238773;
    const double kh = std::tan (pi * f0h / rate);
    const double a0h = 1.0 + kh / qh + kh * kh;
    for (auto& f : filters)
    {
        f[0].b0 = (vh + vb * ks / qs + ks * ks) / a0s;
        f[0].b1 = 2.0 * (ks * ks - vh) / a0s;
        f[0].b2 = (vh - vb * ks / qs + ks * ks) / a0s;
        f[0].a1 = 2.0 * (ks * ks - 1.0) / a0s;
        f[0].a2 = (1.0 - ks / qs + ks * ks) / a0s;
        f[1].b0 = 1.0; f[1].b1 = -2.0; f[1].b2 = 1.0;
        f[1].a1 = 2.0 * (kh * kh - 1.0) / a0h;
        f[1].a2 = (1.0 - kh / qh + kh * kh) / a0h;
    }
    hopLength = std::max (1, (int) std::lround (rate * 0.1));
    hopFill = 0;
    hopEnergy = 0;
    hops.clear();
    peak = 0;
}

void Analyzer::process (const float* const* ch, int numChannels, int n)
{
    const int c = std::min (numChannels, chans);
    for (int i = 0; i < n; ++i)
    {
        double sum = 0;
        for (int k = 0; k < c; ++k)
        {
            const double x = ch[k][i];
            peak = std::max (peak, std::abs (x));
            // BS.1770 weights: the surrounds (5th and 6th of 5.1 as ordered L R C LFE Ls Rs) count 1.41; the LFE not at all.
            if (c >= 5 && k == 3) continue;
            const double w = (c >= 5 && k >= 4) ? 1.41 : 1.0;
            const double y = filters[(size_t) k][1].run (filters[(size_t) k][0].run (x));
            sum += w * y * y;
        }
        hopEnergy += sum;
        if (++hopFill == hopLength)
        {
            hops.push_back (hopEnergy / hopLength);
            hopEnergy = 0;
            hopFill = 0;
        }
    }
}

Result Analyzer::finish()
{
    Result r;
    r.peak = peak;
    // Blocks of 400 ms, each 100 ms on from the last (75% overlap).
    for (size_t k = 3; k < hops.size(); ++k)
        r.blocks.push_back ((float) ((hops[k] + hops[k - 1] + hops[k - 2] + hops[k - 3]) / 4.0));
    // A track shorter than a block is measured as one block of what there is.
    if (r.blocks.empty() && ! hops.empty())
    {
        double s = 0;
        for (auto h : hops) s += h;
        r.blocks.push_back ((float) (s / (double) hops.size()));
    }
    r.loudness = integrated (r.blocks);
    r.gain = r.loudness <= -70 ? 0 : referenceLufs - r.loudness;
    return r;
}

double integrated (const std::vector<float>& blocks)
{
    const double absGate = energyOf (-70.0);
    double sum = 0;
    size_t count = 0;
    for (auto b : blocks) if (b > absGate) { sum += b; ++count; }
    if (count == 0) return -70;
    const double relGate = energyOf (lufsOf (sum / (double) count) - 10.0);
    sum = 0;
    count = 0;
    for (auto b : blocks) if (b > absGate && b > relGate) { sum += b; ++count; }
    if (count == 0) return -70;
    return std::max (-70.0, lufsOf (sum / (double) count));
}

Result album (const std::vector<const Result*>& tracks)
{
    Result r;
    for (auto* t : tracks)
    {
        r.blocks.insert (r.blocks.end(), t->blocks.begin(), t->blocks.end());
        r.peak = std::max (r.peak, t->peak);
    }
    r.loudness = integrated (r.blocks);
    r.gain = r.loudness <= -70 ? 0 : referenceLufs - r.loudness;
    return r;
}

std::string formatGain (double db)
{
    char buf[32];
    std::snprintf (buf, sizeof buf, "%+.2f dB", db);
    // ReplayGain writes "-6.54 dB" and "+1.20 dB".
    return buf;
}

std::string formatPeak (double v)
{
    char buf[32];
    std::snprintf (buf, sizeof buf, "%.6f", v);
    return buf;
}

std::optional<double> parseGain (const std::string& s)
{
    auto t = trim (s);
    if (t.empty()) return std::nullopt;
    char* end = nullptr;
    const double v = std::strtod (t.c_str(), &end);
    if (end == t.c_str() || ! std::isfinite (v)) return std::nullopt;
    return v;
}

Values read (const Track& t)
{
    Values v;
    v.trackGain = parseGain (t.first ("replaygain_track_gain"));
    v.trackPeak = parseGain (t.first ("replaygain_track_peak"));
    v.albumGain = parseGain (t.first ("replaygain_album_gain"));
    v.albumPeak = parseGain (t.first ("replaygain_album_peak"));
    // Opus keeps its own (R128_TRACK_GAIN, Q7.8 dB against -23 LUFS).
    if (! v.trackGain && t.has ("r128_track_gain"))
        if (auto q = parseGain (t.first ("r128_track_gain"))) v.trackGain = *q / 256.0 + 5.0;
    if (! v.albumGain && t.has ("r128_album_gain"))
        if (auto q = parseGain (t.first ("r128_album_gain"))) v.albumGain = *q / 256.0 + 5.0;
    return v;
}

Tags tagsFor (const Result& track, const Result* albumResult)
{
    Tags t;
    t["replaygain_track_gain"] = { formatGain (track.gain) };
    t["replaygain_track_peak"] = { formatPeak (track.peak) };
    if (albumResult)
    {
        t["replaygain_album_gain"] = { formatGain (albumResult->gain) };
        t["replaygain_album_peak"] = { formatPeak (albumResult->peak) };
    }
    return t;
}

const char* modeName (Mode m)
{
    switch (m)
    {
        case Mode::off: return "Off";
        case Mode::track: return "Track Gain";
        case Mode::album: return "Album Gain";
        case Mode::smart: return "Album Gain When Playing Albums";
    }
    return "";
}

double linearGain (const Values& v, const Settings& s, bool albumContext)
{
    if (s.mode == Mode::off) return 1.0;
    const bool wantAlbum = s.mode == Mode::album || (s.mode == Mode::smart && albumContext);
    std::optional<double> gain = wantAlbum ? (v.albumGain ? v.albumGain : v.trackGain) : (v.trackGain ? v.trackGain : v.albumGain);
    std::optional<double> peak = wantAlbum ? (v.albumGain ? v.albumPeak : v.trackPeak) : (v.trackGain ? v.trackPeak : v.albumPeak);
    double db = gain ? *gain + s.preampWith : s.preampWithout;
    double linear = std::pow (10.0, db / 20.0);
    if (s.preventClipping && peak && *peak > 0 && linear * *peak > 1.0) linear = 1.0 / *peak;
    return linear;
}

} // namespace kp::rg
