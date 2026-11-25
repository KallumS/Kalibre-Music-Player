/*
    Dsp - what the sound passes through after it's decoded (decision 0013),
    on the audio thread, in this order:

      1. The equaliser: ten bands an octave apart (31 Hz to 16 kHz), each
         +-12 dB, and a preamp.
      2. Plug-ins: AU and VST3 effects, in a chain.
      3. The volume.

    Then the meters read it: a copy of the last moments for the
    oscilloscope, goniometer and spectrum, and the loudness meter.

    Nothing here allocates or locks on the audio thread (Kalibre's rule): new
    settings are atomics read each block, and a new plug-in chain is swapped
    in whole.
*/

#pragma once

#include "Loudness.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace kp
{

class Equalizer
{
public:
    static constexpr int bands = 10;
    static constexpr std::array<double, bands> frequencies { 31.25, 62.5, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };

    std::array<std::atomic<float>, bands> gainDb {};
    std::atomic<float> preampDb { 0 };
    std::atomic<bool> enabled { false };

    void prepare (double sampleRate);
    void process (float* left, float* right, int frames);   // audio thread
    void changed() { ++version; }                           // after setting gains

    struct Preset { const char* name; std::array<float, bands> gains; };
    static const std::vector<Preset>& presets();

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        std::array<double, 2> z1 {}, z2 {};
    };
    std::array<Biquad, bands> filters;
    double rate = 48000;
    std::atomic<int> version { 0 };
    int madeFor = -1;
    void design();
};

// A chain of plug-ins, swapped whole into the audio thread.
struct PluginChain
{
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> plugins;
    std::vector<bool> bypassed;
};

// The last moments of what was played, for the visualisers (written by the audio thread, read by the window).
class ScopeTap
{
public:
    static constexpr int size = 1 << 15;
    void write (const float* left, const float* right, int frames);
    // The last `frames` frames, oldest first (the window's thread).
    void read (float* left, float* right, int frames) const;
    std::int64_t written() const { return total.load (std::memory_order_acquire); }

private:
    std::array<float, size> l {}, r {};
    std::atomic<std::int64_t> total { 0 };
};

// Peaks and RMS per channel, held for the meters.
struct LevelTap
{
    std::atomic<float> peakL { 0 }, peakR { 0 }, rmsL { 0 }, rmsR { 0 };
    void write (const float* left, const float* right, int frames);
};

} // namespace kp
