/*
    Decoder - any audio file read into floating-point samples, through
    FFmpeg (decision 0003): MP1, MP2, MP3, Musepack, AAC, WMA, Vorbis, FLAC
    and Ogg FLAC, ALAC, WavPack, WAV, AIFF, AU and SND, Speex, Opus, and
    others besides (Monkey's Audio, TTA, AC-3, DSD...).

    It reads from a stream, not a path, so a file inside an archive plays as
    one on disk does. The output is interleaved floats at the rate and
    channel count asked for (resampled and mixed down by libswresample), or
    the file's own.

    Gapless: what the encoder added at the start and end (an MP3's LAME
    header, an AAC's iTunes gapless data, an Opus pre-skip) is cut by FFmpeg,
    so tracks join without a gap.

    One decoder is used by one thread at a time.
*/

#pragma once

#include <juce_core/juce_core.h>

#include <memory>
#include <string>

namespace kp
{

class Decoder
{
public:
    struct Info
    {
        double length = 0;        // seconds (0 if it can't be known)
        int sampleRate = 0, channels = 0, bitsPerSample = 0, bitrate = 0;
        std::string codec;        // "FLAC", "MP3", "AAC"
    };

    // Opens a stream. `nameHint` (the file's name) helps FFmpeg tell raw formats apart.
    static std::unique_ptr<Decoder> open (std::unique_ptr<juce::InputStream>, const juce::String& nameHint, juce::String& error);
    ~Decoder();

    const Info& info() const;
    // What read() delivers: 0 keeps the file's own.
    void setOutput (int sampleRate, int channels);
    int outputRate() const;
    int outputChannels() const;

    // Interleaved samples; returns the frames written, fewer only at the end (0: the end).
    int read (float* interleaved, int frames);
    // To a time in seconds, exactly (decoding from the key frame before it).
    bool seek (double seconds);
    // Seconds of output read so far, from the start of the file.
    double position() const;

    struct Impl;

private:
    explicit Decoder (std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl;
};

// The codec names FFmpeg was built with, for About and the tests.
juce::StringArray decoderNames();
bool hasDecoder (const char* ffmpegName);

} // namespace kp
