#include "Check.h"
#include "ReplayGain.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace kp;

static rg::Result scan (double amplitude, double seconds, double rate = 48000, double hz = 1000)
{
    rg::Analyzer a;
    a.prepare (rate, 2);
    const int n = (int) (rate * seconds);
    std::vector<float> l ((size_t) n), r ((size_t) n);
    for (int i = 0; i < n; ++i) l[(size_t) i] = r[(size_t) i] = (float) (amplitude * std::sin (2 * 3.141592653589793 * hz * i / rate));
    // In pieces, as a decoder hands them over.
    for (int at = 0; at < n; at += 4096)
    {
        const float* ch[] = { l.data() + at, r.data() + at };
        a.process (ch, 2, std::min (4096, n - at));
    }
    return a.finish();
}

TEST ("replaygain: a 1 kHz sine at -20 dBFS in stereo is -20 LUFS... near enough, and its gain brings it to -18")
{
    // EBU's check: a 1 kHz sine at -23 dBFS in each of two channels reads -23 LUFS... per channel it's
    // -23 dBFS peak-to-RMS-free: a full-scale sine is -3.01 dBFS RMS, K-weighting adds ~0.69 at 1 kHz.
    const double amp = std::pow (10.0, -23.0 / 20.0);
    auto r = scan (amp, 10);
    CHECK (std::abs (r.loudness - (-23.0)) < 0.3);
    CHECK (std::abs (r.gain - 5.0) < 0.3);
    CHECK (std::abs (r.peak - amp) < 1e-3);
}

TEST ("replaygain: silence has no gain; an album is gated together")
{
    auto quiet = scan (0, 3);
    CHECK_EQ (quiet.gain, 0.0);
    auto a = scan (std::pow (10.0, -23.0 / 20.0), 10);
    auto b = scan (std::pow (10.0, -13.0 / 20.0), 10);
    auto album = rg::album ({ &a, &b });
    // Louder than the quiet track, quieter than the loud one, nearer the loud (energy averages).
    CHECK (album.loudness > a.loudness && album.loudness < b.loudness);
    CHECK (album.loudness > -16.5);
    CHECK_EQ (album.peak, b.peak);
}

TEST ("replaygain: reading tags and choosing a gain")
{
    Track t;
    t.tags["replaygain_track_gain"] = { "-6.00 dB" };
    t.tags["replaygain_track_peak"] = { "0.5" };
    t.tags["replaygain_album_gain"] = { "+3.00 dB" };
    t.tags["replaygain_album_peak"] = { "0.9" };
    auto v = rg::read (t);
    rg::Settings s;
    s.mode = rg::Mode::track;
    CHECK (std::abs (rg::linearGain (v, s, false) - std::pow (10.0, -6.0 / 20.0)) < 1e-9);
    s.mode = rg::Mode::album;
    // +3 dB would push 0.9 past full scale: held at 1/0.9.
    CHECK (std::abs (rg::linearGain (v, s, false) - 1.0 / 0.9) < 1e-9);
    s.preventClipping = false;
    CHECK (std::abs (rg::linearGain (v, s, false) - std::pow (10.0, 3.0 / 20.0)) < 1e-9);
    s.mode = rg::Mode::smart;
    CHECK (std::abs (rg::linearGain (v, s, false) - std::pow (10.0, -6.0 / 20.0)) < 1e-9);
    s.mode = rg::Mode::off;
    CHECK_EQ (rg::linearGain (v, s, false), 1.0);
    // Without tags: the other preamp.
    s.mode = rg::Mode::track;
    s.preampWithout = -6;
    CHECK (std::abs (rg::linearGain ({}, s, false) - std::pow (10.0, -6.0 / 20.0)) < 1e-9);
    CHECK_EQ (rg::formatGain (-6.544), std::string ("-6.54 dB"));
    CHECK_EQ (rg::formatGain (1.2), std::string ("+1.20 dB"));
}

TEST ("replaygain: Opus's R128 gains")
{
    Track t;
    t.tags["r128_track_gain"] = { "-1280" };   // -5 dB against -23 LUFS: 0 dB against -18
    auto v = rg::read (t);
    CHECK (v.trackGain && std::abs (*v.trackGain) < 1e-9);
}
