#include "Encoder.h"

#include <juce_audio_formats/juce_audio_formats.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include <algorithm>

namespace kp
{

const std::vector<EncodeFormat>& encodeFormats()
{
    static const std::vector<EncodeFormat> f {
        { "flac", "FLAC", "flac", true, true, false, false, {} },
        { "alac", "Apple Lossless (ALAC)", "m4a", true, true, false, false, {} },
        { "wavpack", "WavPack", "wv", true, true, false, false, {} },
        { "wav", "WAV", "wav", true, true, false, false, {} },
        { "aiff", "AIFF", "aiff", true, true, false, false, {} },
        { "mp3", "MP3 (LAME)", "mp3", false, false, true, true, { 128, 160, 192, 224, 256, 320 } },
        { "aac", "AAC", "m4a", false, false, true, false, { 96, 128, 160, 192, 256, 320 } },
        { "opus", "Opus", "opus", false, false, true, false, { 64, 96, 128, 160, 192, 256 } },
        { "vorbis", "Ogg Vorbis", "ogg", false, false, false, true, {} },
    };
    return f;
}

const EncodeFormat* encodeFormat (const std::string& id)
{
    for (auto& f : encodeFormats()) if (f.id == id) return &f;
    return nullptr;
}

std::string EncodeSettings::describe() const
{
    const auto* f = encodeFormat (format);
    if (! f) return format;
    std::string out = f->name;
    if (f->hasBitDepth) out += ", " + std::to_string (bitDepth) + "-bit";
    else if (format == "mp3" && quality >= 0) out += ", V" + std::to_string (quality);
    else if (format == "vorbis") out += ", quality " + std::to_string (std::max (0, quality));
    else if (f->hasBitrate) out += ", " + std::to_string (bitrate) + " kb/s";
    return out;
}

namespace
{
//==============================================================================
// WAV, AIFF, FLAC, Vorbis: JUCE's writers.
class JuceEncoder : public Encoder
{
public:
    std::unique_ptr<juce::AudioFormatWriter> writer;
    bool write (const float* const* ch, int frames) override { return writer->writeFromFloatArrays (ch, (int) writer->getNumChannels(), frames); }
    bool finish() override { writer.reset(); return true; }
};

//==============================================================================
// MP3, AAC, ALAC, Opus, WavPack: FFmpeg's.
int writePacket (void* opaque, const uint8_t* buf, int size)
{
    auto* out = static_cast<juce::FileOutputStream*> (opaque);
    return out->write (buf, (size_t) size) ? size : AVERROR (EIO);
}

int64_t seekOut (void* opaque, int64_t offset, int whence)
{
    auto* out = static_cast<juce::FileOutputStream*> (opaque);
    if (whence & AVSEEK_SIZE) return -1;
    whence &= ~AVSEEK_FORCE;
    int64_t target = offset;
    if (whence == SEEK_CUR) target = out->getPosition() + offset;
    else if (whence == SEEK_END) return -1;
    out->flush();
    return out->setPosition (target) ? target : -1;
}

class FFmpegEncoder : public Encoder
{
public:
    ~FFmpegEncoder() override { cleanup(); }

    bool open (const juce::File& file, const EncodeSettings& s, double rate, int channels, juce::String& error)
    {
        const char* codecName = s.format == "mp3" ? "libmp3lame" : s.format == "aac" ? "aac" : s.format == "alac" ? "alac"
                              : s.format == "opus" ? "libopus" : s.format == "wavpack" ? "wavpack" : nullptr;
        const char* muxer = s.format == "mp3" ? "mp3" : (s.format == "aac" || s.format == "alac") ? "ipod" : s.format == "opus" ? "opus" : "wv";
        const AVCodec* codec = codecName ? avcodec_find_encoder_by_name (codecName) : nullptr;
        if (! codec) { error = "this player can't write that format"; return false; }
        inChannels = channels;
        inRate = (int) rate;

        file.deleteFile();
        out = std::make_unique<juce::FileOutputStream> (file);
        if (! out->openedOk()) { error = "the file couldn't be written"; return false; }
        if (avformat_alloc_output_context2 (&fmt, nullptr, muxer, nullptr) < 0 || ! fmt) { error = "the file couldn't be made"; return false; }
        const int bufferSize = 64 * 1024;
        io = avio_alloc_context ((unsigned char*) av_malloc (bufferSize), bufferSize, 1, out.get(), nullptr, writePacket, seekOut);
        fmt->pb = io;
        fmt->flags |= AVFMT_FLAG_CUSTOM_IO;

        ctx = avcodec_alloc_context3 (codec);
        // The rate: the file's own if the encoder takes it, else the nearest it does (Opus: 48 kHz).
        const int* rates = nullptr;
        int numRates = 0;
        avcodec_get_supported_config (nullptr, codec, AV_CODEC_CONFIG_SAMPLE_RATE, 0, (const void**) &rates, &numRates);
        int encRate = inRate;
        if (rates && numRates > 0)
        {
            bool ok = false;
            for (int k = 0; k < numRates; ++k) ok = ok || rates[k] == inRate;
            if (! ok)
            {
                encRate = rates[0];
                for (int k = 0; k < numRates; ++k)
                    if (std::abs (rates[k] - inRate) < std::abs (encRate - inRate) || (rates[k] == 48000 && inRate > 48000)) encRate = rates[k];
            }
        }
        const AVSampleFormat* formats = nullptr;
        int numFormats = 0;
        avcodec_get_supported_config (nullptr, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, (const void**) &formats, &numFormats);
        AVSampleFormat sf = numFormats > 0 ? formats[0] : AV_SAMPLE_FMT_FLTP;
        // Lossless at 24 bits wants 32-bit samples; at 16, 16-bit ones.
        if (numFormats > 0)
        {
            auto want = [&] (std::initializer_list<AVSampleFormat> prefs)
            {
                for (auto p : prefs) for (int k = 0; k < numFormats; ++k) if (formats[k] == p) return p;
                return formats[0];
            };
            if (s.format == "alac" || s.format == "wavpack")
                sf = s.bitDepth > 16 ? want ({ AV_SAMPLE_FMT_S32P, AV_SAMPLE_FMT_S32 }) : want ({ AV_SAMPLE_FMT_S16P, AV_SAMPLE_FMT_S16 });
            else sf = want ({ AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_FLT, AV_SAMPLE_FMT_S16P, AV_SAMPLE_FMT_S16 });
        }
        ctx->sample_fmt = sf;
        ctx->sample_rate = encRate;
        av_channel_layout_default (&ctx->ch_layout, channels);
        ctx->time_base = { 1, encRate };
        if (s.format == "alac" || s.format == "wavpack") ctx->bits_per_raw_sample = s.bitDepth > 16 ? 24 : 16;
        if (s.format == "mp3" && s.quality >= 0)
        {
            ctx->flags |= AV_CODEC_FLAG_QSCALE;
            ctx->global_quality = FF_QP2LAMBDA * std::clamp (s.quality, 0, 9);
        }
        else if (s.format == "mp3" || s.format == "aac" || s.format == "opus") ctx->bit_rate = (int64_t) s.bitrate * 1000;
        if (s.format == "aac") ctx->profile = AV_PROFILE_AAC_LOW;
        if (fmt->oformat->flags & AVFMT_GLOBALHEADER) ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        if (avcodec_open2 (ctx, codec, nullptr) < 0) { error = "the encoder couldn't start with these settings"; return false; }

        stream = avformat_new_stream (fmt, nullptr);
        avcodec_parameters_from_context (stream->codecpar, ctx);
        stream->time_base = ctx->time_base;
        if (avformat_write_header (fmt, nullptr) < 0) { error = "the file's header couldn't be written"; return false; }

        AVChannelLayout layout {};
        av_channel_layout_default (&layout, channels);
        swr_alloc_set_opts2 (&swr, &ctx->ch_layout, ctx->sample_fmt, encRate, &layout, AV_SAMPLE_FMT_FLTP, inRate, 0, nullptr);
        av_channel_layout_uninit (&layout);
        if (! swr || swr_init (swr) < 0) { error = "the resampler couldn't start"; return false; }
        fifo = av_audio_fifo_alloc (ctx->sample_fmt, channels, 4096);
        frame = av_frame_alloc();
        packet = av_packet_alloc();
        frameSize = ctx->frame_size > 0 ? ctx->frame_size : 4096;
        return true;
    }

    bool write (const float* const* ch, int frames) override
    {
        if (frames <= 0) return true;
        const int maxOut = swr_get_out_samples (swr, frames);
        uint8_t** converted = nullptr;
        av_samples_alloc_array_and_samples (&converted, nullptr, inChannels, maxOut, ctx->sample_fmt, 0);
        const int got = swr_convert (swr, converted, maxOut, (const uint8_t**) ch, frames);
        if (got > 0) av_audio_fifo_write (fifo, (void**) converted, got);
        av_freep (&converted[0]);
        av_freep (&converted);
        return drain (false);
    }

    bool finish() override
    {
        // What the resampler holds, then what's left, then the encoder's own.
        const int maxOut = swr_get_out_samples (swr, 0);
        if (maxOut > 0)
        {
            uint8_t** converted = nullptr;
            av_samples_alloc_array_and_samples (&converted, nullptr, inChannels, maxOut, ctx->sample_fmt, 0);
            const int got = swr_convert (swr, converted, maxOut, nullptr, 0);
            if (got > 0) av_audio_fifo_write (fifo, (void**) converted, got);
            av_freep (&converted[0]);
            av_freep (&converted);
        }
        bool ok = drain (true);
        ok = send (nullptr) && ok;
        ok = av_write_trailer (fmt) == 0 && ok;
        avio_flush (io);
        out->flush();
        cleanup();
        return ok;
    }

private:
    std::unique_ptr<juce::FileOutputStream> out;
    AVFormatContext* fmt = nullptr;
    AVIOContext* io = nullptr;
    AVCodecContext* ctx = nullptr;
    AVStream* stream = nullptr;
    SwrContext* swr = nullptr;
    AVAudioFifo* fifo = nullptr;
    AVFrame* frame = nullptr;
    AVPacket* packet = nullptr;
    int inChannels = 2, inRate = 44100, frameSize = 1024;
    int64_t pts = 0;

    void cleanup()
    {
        av_packet_free (&packet);
        av_frame_free (&frame);
        if (fifo) { av_audio_fifo_free (fifo); fifo = nullptr; }
        swr_free (&swr);
        avcodec_free_context (&ctx);
        if (io) { av_freep (&io->buffer); avio_context_free (&io); }
        if (fmt) { avformat_free_context (fmt); fmt = nullptr; }
        out.reset();
    }

    bool drain (bool last)
    {
        while (av_audio_fifo_size (fifo) >= frameSize || (last && av_audio_fifo_size (fifo) > 0))
        {
            const int n = std::min (frameSize, av_audio_fifo_size (fifo));
            av_frame_unref (frame);
            frame->nb_samples = n;
            frame->format = ctx->sample_fmt;
            frame->sample_rate = ctx->sample_rate;
            av_channel_layout_copy (&frame->ch_layout, &ctx->ch_layout);
            if (av_frame_get_buffer (frame, 0) < 0) return false;
            av_audio_fifo_read (fifo, (void**) frame->data, n);
            frame->pts = pts;
            pts += n;
            if (! send (frame)) return false;
        }
        return true;
    }

    bool send (AVFrame* f)
    {
        if (avcodec_send_frame (ctx, f) < 0) return false;
        for (;;)
        {
            const int r = avcodec_receive_packet (ctx, packet);
            if (r == AVERROR (EAGAIN) || r == AVERROR_EOF) return true;
            if (r < 0) return false;
            av_packet_rescale_ts (packet, ctx->time_base, stream->time_base);
            packet->stream_index = stream->index;
            if (av_interleaved_write_frame (fmt, packet) < 0) return false;
        }
    }
};
} // namespace

std::unique_ptr<Encoder> Encoder::create (const juce::File& file, const EncodeSettings& s, double rate, int channels, juce::String& error)
{
    av_log_set_level (AV_LOG_QUIET);
    if (s.format == "wav" || s.format == "aiff" || s.format == "flac" || s.format == "vorbis")
    {
        std::unique_ptr<juce::AudioFormat> format;
        if (s.format == "wav") format = std::make_unique<juce::WavAudioFormat>();
        else if (s.format == "aiff") format = std::make_unique<juce::AiffAudioFormat>();
        else if (s.format == "flac") format = std::make_unique<juce::FlacAudioFormat>();
        else format = std::make_unique<juce::OggVorbisAudioFormat>();
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
        if (! static_cast<juce::FileOutputStream*> (stream.get())->openedOk()) { error = "the file couldn't be written"; return nullptr; }
        const int bits = s.format == "vorbis" ? 32 : std::clamp (s.bitDepth, 16, 24);
        // Vorbis' quality is an index into JUCE's list of qualities (0 to 10).
        const int qualityIndex = s.format == "vorbis" ? std::clamp (s.quality < 0 ? 6 : s.quality, 0, (int) format->getQualityOptions().size() - 1) : 0;
        auto writer = format->createWriterFor (stream,
                                               juce::AudioFormatWriterOptions().withSampleRate (rate).withNumChannels (channels)
                                                   .withBitsPerSample (bits).withQualityOptionIndex (qualityIndex));
        if (! writer) { error = "that format can't keep " + juce::String (channels) + " channels at " + juce::String (rate) + " Hz"; return nullptr; }
        auto e = std::make_unique<JuceEncoder>();
        e->writer = std::move (writer);
        return e;
    }
    auto e = std::make_unique<FFmpegEncoder>();
    if (! e->open (file, s, rate, channels, error)) return nullptr;
    return e;
}

} // namespace kp
