// What the app's tests share: a scratch folder, and sound made to order.
#pragma once

#include "Encoder.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include <cmath>
#include <functional>

namespace test
{

inline juce::File scratch()
{
    static const juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                      .getChildFile ("kalibre-player-tests-" + juce::String (juce::Time::currentTimeMillis()));
    dir.createDirectory();
    return dir;
}

// Writes `frames` of stereo sound made by f(channel, frame) in a format.
inline juce::File write (const juce::String& name, const kp::EncodeSettings& s, double rate, int frames,
                         std::function<float (int, int)> f, int channels = 2)
{
    auto file = scratch().getChildFile (name);
    file.getParentDirectory().createDirectory();
    juce::String error;
    auto enc = kp::Encoder::create (file, s, rate, channels, error);
    if (! enc) return {};
    const int block = 1000;
    std::vector<std::vector<float>> data ((size_t) channels, std::vector<float> ((size_t) block));
    std::vector<const float*> ptrs;
    for (auto& d : data) ptrs.push_back (d.data());
    for (int at = 0; at < frames; at += block)
    {
        const int n = std::min (block, frames - at);
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < n; ++i) data[(size_t) c][(size_t) i] = f (c, at + i);
        enc->write (ptrs.data(), n);
    }
    enc->finish();
    return file;
}

inline kp::EncodeSettings fmt (const char* id, int bits = 16, int bitrate = 256, int quality = -1)
{
    kp::EncodeSettings s;
    s.format = id;
    s.bitDepth = bits;
    s.bitrate = bitrate;
    s.quality = quality;
    return s;
}

inline float sine (int c, int i, double rate = 44100.0) { return 0.5f * (float) std::sin (2 * 3.14159265358979 * (440.0 + c * 110.0) * i / rate); }

// Runs the message loop until done() or the time is up.
inline bool waitFor (std::function<bool()> done, int ms = 10000)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
    while (! done())
    {
        if (juce::Time::getMillisecondCounter() > end) return false;
        juce::MessageManager::getInstance()->runDispatchLoopUntil (5);
    }
    return true;
}

} // namespace test
