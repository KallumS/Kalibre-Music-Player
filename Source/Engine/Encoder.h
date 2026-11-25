/*
    Encoder - writing audio files, for the converter and CD ripping
    (decision 0010): WAV, AIFF, FLAC and Ogg Vorbis through JUCE; MP3
    (LAME), AAC and ALAC (in .m4a), Opus (libopus) and WavPack through
    FFmpeg. Tags are written afterwards, by TagIO, the same for all.
*/

#pragma once

#include <juce_core/juce_core.h>

#include <memory>
#include <string>
#include <vector>

namespace kp
{

struct EncodeFormat
{
    std::string id, name, extension;
    bool lossless = false;
    bool hasBitDepth = false;      // WAV, AIFF, FLAC, ALAC, WavPack: 16 or 24 bits
    bool hasBitrate = false;       // AAC, Opus, MP3 (CBR)
    bool hasQuality = false;       // MP3 (VBR V0-V9), Vorbis (0-10)
    std::vector<int> bitrates;     // kb/s offered
};
const std::vector<EncodeFormat>& encodeFormats();
const EncodeFormat* encodeFormat (const std::string& id);

struct EncodeSettings
{
    std::string format = "flac";
    int bitDepth = 16;
    int bitrate = 256;             // kb/s
    int quality = -1;              // MP3: V0 (best) to V9, -1 for constant bitrate; Vorbis: 0 to 10
    bool operator== (const EncodeSettings&) const = default;
    std::string describe() const;  // "FLAC, 24-bit", "MP3, V0"
};

class Encoder
{
public:
    virtual ~Encoder() = default;
    static std::unique_ptr<Encoder> create (const juce::File&, const EncodeSettings&, double sampleRate, int channels, juce::String& error);
    // Planar float samples.
    virtual bool write (const float* const* channels, int frames) = 0;
    virtual bool finish() = 0;
};

} // namespace kp
