#include "Dsp.h"

#include <cmath>

namespace kp
{

const std::vector<Equalizer::Preset>& Equalizer::presets()
{
    static const std::vector<Preset> p {
        { "Flat", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
        { "Bass Boost", { 6, 5, 4, 2, 0, 0, 0, 0, 0, 0 } },
        { "Treble Boost", { 0, 0, 0, 0, 0, 1, 2, 4, 5, 6 } },
        { "Loudness", { 5, 4, 2, 0, -1, -1, 0, 2, 4, 5 } },
        { "Vocal", { -2, -2, -1, 1, 3, 4, 3, 1, 0, -1 } },
        { "Rock", { 4, 3, 2, 0, -1, -1, 1, 2, 3, 4 } },
        { "Jazz", { 3, 2, 1, 2, -1, -1, 0, 1, 2, 3 } },
        { "Classical", { 3, 2, 1, 0, 0, 0, -1, -1, 1, 2 } },
        { "Electronic", { 5, 4, 1, 0, -2, 1, 0, 2, 4, 5 } },
        { "Headphones", { 3, 2, 0, -1, -1, 0, 1, 2, 3, 2 } },
        { "Small Speakers", { 6, 5, 4, 2, 0, 0, 0, 0, -1, -2 } },
    };
    return p;
}

void Equalizer::prepare (double sampleRate)
{
    rate = sampleRate > 0 ? sampleRate : 48000;
    madeFor = -1;
    for (auto& f : filters) { f.z1 = {}; f.z2 = {}; }
}

void Equalizer::design()
{
    // Peaking filters (Robert Bristow-Johnson's), an octave wide.
    const double q = 1.41;
    for (int b = 0; b < bands; ++b)
    {
        auto& f = filters[(size_t) b];
        const double fc = std::min (frequencies[(size_t) b], rate * 0.45);
        const double A = std::pow (10.0, gainDb[(size_t) b].load() / 40.0);
        const double w = 2.0 * 3.141592653589793 * fc / rate;
        const double alpha = std::sin (w) / (2.0 * q);
        const double c = std::cos (w);
        const double a0 = 1.0 + alpha / A;
        f.b0 = (1.0 + alpha * A) / a0;
        f.b1 = (-2.0 * c) / a0;
        f.b2 = (1.0 - alpha * A) / a0;
        f.a1 = (-2.0 * c) / a0;
        f.a2 = (1.0 - alpha / A) / a0;
    }
}

void Equalizer::process (float* left, float* right, int n)
{
    if (! enabled.load (std::memory_order_relaxed)) return;
    const int v = version.load (std::memory_order_acquire);
    if (v != madeFor) { design(); madeFor = v; }
    const float pre = std::pow (10.0f, preampDb.load (std::memory_order_relaxed) / 20.0f);
    for (int i = 0; i < n; ++i) { left[i] *= pre; right[i] *= pre; }
    for (auto& f : filters)
    {
        // A band at 0 dB is left out: it changes nothing.
        if (std::abs (f.b0 - 1.0) < 1e-12 && std::abs (f.b2 - f.a2) < 1e-12) continue;
        float* ch[2] = { left, right };
        for (int c = 0; c < 2; ++c)
        {
            double z1 = f.z1[(size_t) c], z2 = f.z2[(size_t) c];
            float* x = ch[c];
            for (int i = 0; i < n; ++i)
            {
                const double in = x[i];
                const double y = f.b0 * in + z1;
                z1 = f.b1 * in - f.a1 * y + z2;
                z2 = f.b2 * in - f.a2 * y;
                x[i] = (float) y;
            }
            f.z1[(size_t) c] = z1;
            f.z2[(size_t) c] = z2;
        }
    }
}

void ScopeTap::write (const float* left, const float* right, int frames)
{
    auto at = total.load (std::memory_order_relaxed);
    for (int i = 0; i < frames; ++i)
    {
        const auto k = (size_t) ((at + i) & (size - 1));
        l[k] = left[i];
        r[k] = right[i];
    }
    total.store (at + frames, std::memory_order_release);
}

void ScopeTap::read (float* left, float* right, int frames) const
{
    const auto end = total.load (std::memory_order_acquire);
    frames = std::min (frames, size);
    for (int i = 0; i < frames; ++i)
    {
        const auto k = (size_t) ((end - frames + i) & (size - 1));
        left[i] = end - frames + i >= 0 ? l[k] : 0.0f;
        right[i] = end - frames + i >= 0 ? r[k] : 0.0f;
    }
}

void LevelTap::write (const float* left, const float* right, int frames)
{
    float pl = 0, pr = 0;
    double sl = 0, sr = 0;
    for (int i = 0; i < frames; ++i)
    {
        pl = std::max (pl, std::abs (left[i]));
        pr = std::max (pr, std::abs (right[i]));
        sl += (double) left[i] * left[i];
        sr += (double) right[i] * right[i];
    }
    // Peaks held until the window reads them (it sets them back to 0).
    float cur = peakL.load (std::memory_order_relaxed);
    if (pl > cur) peakL.store (pl, std::memory_order_relaxed);
    cur = peakR.load (std::memory_order_relaxed);
    if (pr > cur) peakR.store (pr, std::memory_order_relaxed);
    if (frames > 0)
    {
        // RMS, smoothed over about 300 ms.
        const float k = std::min (1.0f, (float) frames / 14400.0f);
        rmsL.store (rmsL.load (std::memory_order_relaxed) * (1 - k) + (float) (sl / frames) * k, std::memory_order_relaxed);
        rmsR.store (rmsR.load (std::memory_order_relaxed) * (1 - k) + (float) (sr / frames) * k, std::memory_order_relaxed);
    }
}

} // namespace kp
