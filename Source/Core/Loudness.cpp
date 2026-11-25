#include "Loudness.h"

#include <algorithm>

namespace kal::loudness
{

namespace
{
constexpr double pi = 3.141592653589793;
}

void Meter::prepare (double sampleRate)
{
    rate = sampleRate > 0 ? sampleRate : 48000;
    // K-weighting (BS.1770-4), from its analogue prototype so it holds at any rate.
    {
        const double f0 = 1681.974450955533, gain = 3.999843853973347, q = 0.7071752369554196;
        const double k = std::tan (pi * f0 / rate);
        const double vh = std::pow (10.0, gain / 20.0), vb = std::pow (vh, 0.4996667741545416);
        const double a0 = 1.0 + k / q + k * k;
        for (auto& f : shelf)
        {
            f.b0 = (vh + vb * k / q + k * k) / a0;
            f.b1 = 2.0 * (k * k - vh) / a0;
            f.b2 = (vh - vb * k / q + k * k) / a0;
            f.a1 = 2.0 * (k * k - 1.0) / a0;
            f.a2 = (1.0 - k / q + k * k) / a0;
        }
    }
    {
        const double f0 = 38.13547087602444, q = 0.5003270373238773;
        const double k = std::tan (pi * f0 / rate);
        const double a0 = 1.0 + k / q + k * k;
        for (auto& f : highpass)
        {
            f.b0 = 1.0; f.b1 = -2.0; f.b2 = 1.0;
            f.a1 = 2.0 * (k * k - 1.0) / a0;
            f.a2 = (1.0 - k / q + k * k) / a0;
        }
    }
    hopLength = std::max (1, (int) std::lround (rate * 0.1));
    // A 4x windowed-sinc interpolator, split into its four phases.
    for (int p = 0; p < 4; ++p)
    {
        double sum = 0;
        for (int t = 0; t < taps; ++t)
        {
            const double x = (double) (t - taps / 2) + (double) p / 4.0;
            const double sinc = std::abs (x) < 1e-12 ? 1.0 : std::sin (pi * x) / (pi * x);
            const double w = 0.5 + 0.5 * std::cos (pi * x / (taps / 2 + 1));
            phases[(size_t) p][(size_t) t] = sinc * w;
            sum += sinc * w;
        }
        for (auto& v : phases[(size_t) p]) v /= sum;
    }
    corrDecay = std::exp (-1.0 / (0.3 * rate));
    reset();
}

void Meter::reset()
{
    for (auto* f : { &shelf, &highpass }) for (auto& b : *f) { b.z1 = b.z2 = 0; }
    hopFill = 0;
    hopEnergy = 0;
    hops.fill (0);
    hopCount = 0;
    blockHistogram.fill (0);
    shortHistogram.fill (0);
    for (auto& h : history) h.fill (0);
    historyAt = 0;
    peak = 0;
    corrLR = corrLL = corrRR = 0;
    m = silence; s = silence; i = silence; lra = 0; tp = silence; corr = 0; mMax = silence;
}

int Meter::binOf (double lufs) { return std::clamp ((int) std::floor ((lufs + 70.0) * 10.0), 0, bins - 1); }
double Meter::lufsOfBin (int bin) { return -70.0 + (bin + 0.5) / 10.0; }

void Meter::process (const float* left, const float* right, int n)
{
    for (int k = 0; k < n; ++k)
    {
        const double x[2] { left[k], right[k] };
        double e = 0;
        for (size_t c = 0; c < 2; ++c)
        {
            const double y = highpass[c].run (shelf[c].run (x[c]));
            e += y * y;
            // True peak.
            history[c][(size_t) historyAt] = x[c];
        }
        for (size_t c = 0; c < 2; ++c)
            for (size_t p = 0; p < 4; ++p)
            {
                double v = 0;
                for (int t = 0; t < taps; ++t)
                    v += phases[p][(size_t) t] * history[c][(size_t) ((historyAt - t + taps) % taps)];
                peak = std::max (peak, std::abs (v));
            }
        historyAt = (historyAt + 1) % taps;
        // Correlation, over about 300 ms.
        corrLR = corrLR * corrDecay + x[0] * x[1];
        corrLL = corrLL * corrDecay + x[0] * x[0];
        corrRR = corrRR * corrDecay + x[1] * x[1];

        hopEnergy += e;
        if (++hopFill >= hopLength) hopDone();
    }
    tp = peak > 0 ? 20.0 * std::log10 (peak) : silence;
    const double d = std::sqrt (corrLL * corrRR);
    corr = d > 1e-12 ? corrLR / d : 0.0;
}

void Meter::hopDone()
{
    hops[(size_t) (hopCount % (int) hops.size())] = hopEnergy / hopLength;
    ++hopCount;
    hopEnergy = 0;
    hopFill = 0;

    auto meanOf = [this] (int count)
    {
        double sum = 0;
        const int have = std::min (count, hopCount);
        for (int j = 0; j < have; ++j) sum += hops[(size_t) ((hopCount - 1 - j) % (int) hops.size())];
        return have > 0 ? sum / count : 0.0;
    };
    const double momentaryLufs = hopCount >= 4 ? toLufs (meanOf (4)) : silence;
    // Before three seconds have passed, short-term reads what there is so far.
    const double shortLufs = hopCount >= 30 ? toLufs (meanOf (30)) : (hopCount >= 4 ? toLufs (meanOf (hopCount)) : silence);
    m = momentaryLufs;
    s = shortLufs;
    if (momentaryLufs > mMax.load()) mMax = momentaryLufs;

    // Gating blocks are the momentary windows, every 100 ms (75% overlap).
    if (hopCount >= 4 && momentaryLufs > -70.0) ++blockHistogram[(size_t) binOf (momentaryLufs)];
    if (hopCount >= 30 && shortLufs > -70.0) ++shortHistogram[(size_t) binOf (shortLufs)];

    // Integrated: the average above -70, then again above (that - 10 LU).
    {
        double energy = 0;
        std::uint64_t count = 0;
        for (int b = 0; b < bins; ++b)
            if (blockHistogram[(size_t) b]) { energy += blockHistogram[(size_t) b] * std::pow (10.0, (lufsOfBin (b) + 0.691) / 10.0); count += blockHistogram[(size_t) b]; }
        if (count > 0)
        {
            const double gate = toLufs (energy / (double) count) - 10.0;
            double e2 = 0;
            std::uint64_t c2 = 0;
            for (int b = binOf (gate); b < bins; ++b)
                if (blockHistogram[(size_t) b] && lufsOfBin (b) >= gate) { e2 += blockHistogram[(size_t) b] * std::pow (10.0, (lufsOfBin (b) + 0.691) / 10.0); c2 += blockHistogram[(size_t) b]; }
            i = c2 > 0 ? toLufs (e2 / (double) c2) : silence;
        }
    }
    // Range: 10th to 95th percentile of short-term, above (average - 20 LU).
    {
        double energy = 0;
        std::uint64_t count = 0;
        for (int b = 0; b < bins; ++b)
            if (shortHistogram[(size_t) b]) { energy += shortHistogram[(size_t) b] * std::pow (10.0, (lufsOfBin (b) + 0.691) / 10.0); count += shortHistogram[(size_t) b]; }
        if (count > 0)
        {
            const double gate = toLufs (energy / (double) count) - 20.0;
            std::uint64_t total = 0;
            for (int b = binOf (gate); b < bins; ++b) total += shortHistogram[(size_t) b];
            if (total > 0)
            {
                auto percentile = [&] (double q)
                {
                    const auto want = (std::uint64_t) std::ceil (q * (double) total);
                    std::uint64_t seen = 0;
                    for (int b = binOf (gate); b < bins; ++b)
                    {
                        seen += shortHistogram[(size_t) b];
                        if (seen >= want) return lufsOfBin (b);
                    }
                    return lufsOfBin (bins - 1);
                };
                lra = percentile (0.95) - percentile (0.10);
            }
        }
    }
}

Measured measure (const float* left, const float* right, int samples, double sampleRate)
{
    Meter meter;
    meter.prepare (sampleRate);
    meter.process (left, right, samples);
    return { meter.integrated(), meter.truePeakDb() };
}

} // namespace kal::loudness
