/*
    Loudness - the master's loudness, as streaming services and broadcasters
    measure it (decision 0033): ITU-R BS.1770-4 and EBU R128.

      - Momentary: the last 400 ms.     - Short-term: the last 3 s.
      - Integrated: the whole song since Reset, gated (blocks below -70 LUFS,
        and 10 LU below the average, don't count).
      - Loudness range (LRA): how much the short-term loudness moves, 10th to
        95th percentile, gated at -70 LUFS and 20 LU below the average.
      - True peak: the highest the waveform goes between samples, read at 4x.
      - Correlation: +1 mono, 0 wide, -1 out of phase (the last 300 ms).

    It runs on the audio thread: nothing allocates after prepare(), and the
    readings are atomics the window reads.

    No JUCE in here.
*/

#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace kal::loudness
{

inline constexpr double silence = -100.0;

class Meter
{
public:
    void prepare (double sampleRate);
    void reset();   // starts the integrated reading and range again
    void process (const float* left, const float* right, int samples);

    double momentary() const   { return m.load(); }
    double shortTerm() const   { return s.load(); }
    double integrated() const  { return i.load(); }
    double range() const       { return lra.load(); }
    double truePeakDb() const  { return tp.load(); }
    double correlation() const { return corr.load(); }
    double maxMomentary() const { return mMax.load(); }

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1 = 0, z2 = 0;
        double run (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    };

    double rate = 48000;
    std::array<Biquad, 2> shelf, highpass;   // per channel
    int hopLength = 4800;                    // 100 ms
    int hopFill = 0;
    double hopEnergy = 0;
    std::array<double, 30> hops {};          // the last 3 s of 100 ms energies
    int hopCount = 0;

    static constexpr int bins = 1000;        // 0.1 LU each from -70 to +30 LUFS
    std::array<std::uint32_t, bins> blockHistogram {}, shortHistogram {};

    // True peak: 4x polyphase interpolation of each channel.
    static constexpr int taps = 12;
    std::array<std::array<double, taps>, 4> phases {};
    std::array<std::array<double, taps>, 2> history {};
    int historyAt = 0;
    double peak = 0;

    double corrLR = 0, corrLL = 0, corrRR = 0;
    double corrDecay = 0.999;

    std::atomic<double> m { silence }, s { silence }, i { silence }, lra { 0 }, tp { silence }, corr { 0 }, mMax { silence };

    void hopDone();
    static int binOf (double lufs);
    static double lufsOfBin (int bin);
};

inline double toLufs (double meanSquareSum) { return meanSquareSum > 0 ? -0.691 + 10.0 * std::log10 (meanSquareSum) : silence; }

// The integrated loudness and true peak of a whole stereo buffer, for export.
struct Measured { double integrated = silence, truePeak = silence; };
Measured measure (const float* left, const float* right, int samples, double sampleRate);

} // namespace kal::loudness
