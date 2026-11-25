#include "Decoder.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include <algorithm>
#include <cmath>
#include <vector>

namespace kp
{

namespace
{
void quiet()
{
    static bool done = false;
    if (! done) { av_log_set_level (AV_LOG_QUIET); done = true; }
}

int readPacket (void* opaque, uint8_t* buf, int size)
{
    auto* in = static_cast<juce::InputStream*> (opaque);
    const int n = in->read (buf, size);
    return n > 0 ? n : AVERROR_EOF;
}

int64_t seekStream (void* opaque, int64_t offset, int whence)
{
    auto* in = static_cast<juce::InputStream*> (opaque);
    if (whence & AVSEEK_SIZE) return in->getTotalLength();
    whence &= ~AVSEEK_FORCE;
    int64_t target = offset;
    if (whence == SEEK_CUR) target = in->getPosition() + offset;
    else if (whence == SEEK_END) target = in->getTotalLength() + offset;
    return in->setPosition (target) ? target : -1;
}

std::string friendlyCodec (AVCodecID id, const char* fallback)
{
    switch (id)
    {
        case AV_CODEC_ID_MP1: return "MP1";
        case AV_CODEC_ID_MP2: return "MP2";
        case AV_CODEC_ID_MP3: case AV_CODEC_ID_MP3ADU: case AV_CODEC_ID_MP3ON4: return "MP3";
        case AV_CODEC_ID_MUSEPACK7: case AV_CODEC_ID_MUSEPACK8: return "Musepack";
        case AV_CODEC_ID_AAC: case AV_CODEC_ID_AAC_LATM: return "AAC";
        case AV_CODEC_ID_ALAC: return "ALAC";
        case AV_CODEC_ID_WMAV1: case AV_CODEC_ID_WMAV2: return "WMA";
        case AV_CODEC_ID_WMAPRO: return "WMA Pro";
        case AV_CODEC_ID_WMALOSSLESS: return "WMA Lossless";
        case AV_CODEC_ID_WMAVOICE: return "WMA Voice";
        case AV_CODEC_ID_VORBIS: return "Vorbis";
        case AV_CODEC_ID_FLAC: return "FLAC";
        case AV_CODEC_ID_WAVPACK: return "WavPack";
        case AV_CODEC_ID_SPEEX: return "Speex";
        case AV_CODEC_ID_OPUS: return "Opus";
        case AV_CODEC_ID_APE: return "Monkey's Audio";
        case AV_CODEC_ID_TTA: return "TTA";
        case AV_CODEC_ID_TAK: return "TAK";
        case AV_CODEC_ID_AC3: return "AC-3";
        case AV_CODEC_ID_EAC3: return "E-AC-3";
        case AV_CODEC_ID_DTS: return "DTS";
        case AV_CODEC_ID_MP4ALS: return "MPEG-4 ALS";
        case AV_CODEC_ID_PCM_ALAW: return "A-law";
        case AV_CODEC_ID_PCM_MULAW: return "\xc2\xb5-law";
        default: break;
    }
    const auto* d = avcodec_descriptor_get (id);
    if (d && (d->props & AV_CODEC_PROP_LOSSLESS) && std::string (d->name).rfind ("pcm_", 0) == 0) return "PCM";
    if (std::string (fallback).rfind ("dsd_", 0) == 0) return "DSD";
    if (std::string (fallback).rfind ("adpcm", 0) == 0) return "ADPCM";
    return juce::String (fallback).toUpperCase().toStdString();
}
} // namespace

struct Decoder::Impl
{
    std::unique_ptr<juce::InputStream> input;
    AVIOContext* io = nullptr;
    AVFormatContext* fmt = nullptr;
    AVCodecContext* codec = nullptr;
    SwrContext* swr = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* frame = nullptr;
    int stream = -1;
    Info info;

    int outRate = 0, outChannels = 0;
    // The swr's input, as last set up.
    int swrInRate = 0, swrInFormat = -1;
    AVChannelLayout swrInLayout {};

    std::vector<float> pending;   // converted, not yet read (interleaved)
    size_t pendingAt = 0;
    int64_t delivered = 0;        // output frames read since the start of the file
    int64_t discard = 0;          // output frames to drop after a seek
    bool ended = false, flushedDecoder = false;

    ~Impl()
    {
        swr_free (&swr);
        av_frame_free (&frame);
        av_packet_free (&packet);
        avcodec_free_context (&codec);
        if (fmt) avformat_close_input (&fmt);
        if (io) { av_freep (&io->buffer); avio_context_free (&io); }
        av_channel_layout_uninit (&swrInLayout);
    }

    int rate() const { return outRate > 0 ? outRate : info.sampleRate; }
    int channels() const { return outChannels > 0 ? outChannels : info.channels; }

    bool setupSwr (const AVFrame* f)
    {
        if (swr && f->sample_rate == swrInRate && f->format == swrInFormat && av_channel_layout_compare (&f->ch_layout, &swrInLayout) == 0)
            return true;
        swr_free (&swr);
        AVChannelLayout out {};
        av_channel_layout_default (&out, channels());
        AVChannelLayout in {};
        if (f->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC) av_channel_layout_default (&in, f->ch_layout.nb_channels);
        else av_channel_layout_copy (&in, &f->ch_layout);
        const int r = swr_alloc_set_opts2 (&swr, &out, AV_SAMPLE_FMT_FLT, rate(), &in, (AVSampleFormat) f->format, f->sample_rate, 0, nullptr);
        av_channel_layout_uninit (&out);
        av_channel_layout_uninit (&swrInLayout);
        av_channel_layout_copy (&swrInLayout, &in);
        av_channel_layout_uninit (&in);
        if (r < 0 || ! swr) return false;
        // The best resampler swr has; a music player has time for it.
        av_opt_set_int (swr, "filter_size", 32, 0);
        av_opt_set_int (swr, "phase_shift", 10, 0);
        av_opt_set_int (swr, "linear_interp", 1, 0);
        av_opt_set_double (swr, "cutoff", 0.97, 0);
        if (swr_init (swr) < 0) { swr_free (&swr); return false; }
        swrInRate = f->sample_rate;
        swrInFormat = f->format;
        return true;
    }

    void convert (const AVFrame* f)
    {
        // No frame: what the resampler still holds, at the end.
        if (f ? ! setupSwr (f) : swr == nullptr) return;
        const int ch = channels();
        const int maxOut = swr_get_out_samples (swr, f ? f->nb_samples : 0);
        if (maxOut <= 0) return;
        const size_t keep = pending.size() - pendingAt;
        std::vector<float> next (keep + (size_t) maxOut * (size_t) ch);
        std::copy (pending.begin() + (std::ptrdiff_t) pendingAt, pending.end(), next.begin());
        uint8_t* outPtr = reinterpret_cast<uint8_t*> (next.data() + keep);
        const int got = swr_convert (swr, &outPtr, maxOut, f ? (const uint8_t**) f->extended_data : nullptr, f ? f->nb_samples : 0);
        next.resize (keep + (size_t) std::max (0, got) * (size_t) ch);
        pending = std::move (next);
        pendingAt = 0;
    }

    // Decodes more into pending; false at the end.
    bool decodeMore()
    {
        while (! ended)
        {
            const int r = avcodec_receive_frame (codec, frame);
            if (r == 0) { convert (frame); av_frame_unref (frame); return true; }
            if (r == AVERROR_EOF)
            {
                // What the resampler still holds.
                if (swr) convert (nullptr);
                ended = true;
                return pending.size() > pendingAt;
            }
            if (r != AVERROR (EAGAIN)) { ended = true; return false; }
            if (flushedDecoder) { ended = true; return false; }
            const int rr = av_read_frame (fmt, packet);
            if (rr < 0)
            {
                avcodec_send_packet (codec, nullptr);
                flushedDecoder = true;
                continue;
            }
            if (packet->stream_index == stream)
                avcodec_send_packet (codec, packet);   // a bad packet is skipped, as players do
            av_packet_unref (packet);
        }
        return false;
    }
};

Decoder::Decoder (std::unique_ptr<Impl> i) : impl (std::move (i)) {}
Decoder::~Decoder() = default;

std::unique_ptr<Decoder> Decoder::open (std::unique_ptr<juce::InputStream> input, const juce::String& nameHint, juce::String& error)
{
    quiet();
    if (! input) { error = "it couldn't be opened"; return nullptr; }
    auto d = std::make_unique<Impl>();
    d->input = std::move (input);
    const int bufferSize = 64 * 1024;
    auto* buffer = static_cast<unsigned char*> (av_malloc (bufferSize));
    d->io = avio_alloc_context (buffer, bufferSize, 0, d->input.get(), readPacket, nullptr, seekStream);
    d->fmt = avformat_alloc_context();
    d->fmt->pb = d->io;
    d->fmt->flags |= AVFMT_FLAG_CUSTOM_IO;
    if (avformat_open_input (&d->fmt, nameHint.toRawUTF8(), nullptr, nullptr) < 0)
    {
        d->fmt = nullptr;   // freed by avformat_open_input on failure
        error = "it isn't a kind of audio file the player knows";
        return nullptr;
    }
    avformat_find_stream_info (d->fmt, nullptr);
    const AVCodec* codec = nullptr;
    d->stream = av_find_best_stream (d->fmt, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (d->stream < 0 || ! codec) { error = "it has no sound the player can decode"; return nullptr; }
    auto* st = d->fmt->streams[d->stream];
    // Only the audio is read.
    for (unsigned k = 0; k < d->fmt->nb_streams; ++k)
        if ((int) k != d->stream) d->fmt->streams[k]->discard = AVDISCARD_ALL;
    d->codec = avcodec_alloc_context3 (codec);
    avcodec_parameters_to_context (d->codec, st->codecpar);
    d->codec->pkt_timebase = st->time_base;
    d->codec->thread_count = 1;
    if (avcodec_open2 (d->codec, codec, nullptr) < 0) { error = "its decoder couldn't start"; return nullptr; }
    d->packet = av_packet_alloc();
    d->frame = av_frame_alloc();

    auto& info = d->info;
    info.sampleRate = d->codec->sample_rate;
    info.channels = d->codec->ch_layout.nb_channels;
    info.codec = friendlyCodec (codec->id, codec->name);
    const auto* desc = avcodec_descriptor_get (codec->id);
    const bool lossless = desc && (desc->props & AV_CODEC_PROP_LOSSLESS);
    if (lossless)
        info.bitsPerSample = st->codecpar->bits_per_raw_sample > 0 ? st->codecpar->bits_per_raw_sample : st->codecpar->bits_per_coded_sample;
    if (st->duration != AV_NOPTS_VALUE && st->duration > 0) info.length = (double) st->duration * av_q2d (st->time_base);
    else if (d->fmt->duration != AV_NOPTS_VALUE && d->fmt->duration > 0) info.length = (double) d->fmt->duration / AV_TIME_BASE;
    int64_t br = st->codecpar->bit_rate > 0 ? st->codecpar->bit_rate : d->fmt->bit_rate;
    if (br <= 0 && info.length > 0 && d->input->getTotalLength() > 0) br = (int64_t) (d->input->getTotalLength() * 8 / info.length);
    info.bitrate = (int) (br / 1000);
    if (info.sampleRate <= 0 || info.channels <= 0) { error = "its sound has no rate or channels"; return nullptr; }
    return std::unique_ptr<Decoder> (new Decoder (std::move (d)));
}

const Decoder::Info& Decoder::info() const { return impl->info; }

void Decoder::setOutput (int sampleRate, int channels)
{
    impl->outRate = sampleRate;
    impl->outChannels = channels;
    swr_free (&impl->swr);
}

int Decoder::outputRate() const { return impl->rate(); }
int Decoder::outputChannels() const { return impl->channels(); }

int Decoder::read (float* out, int frames)
{
    auto& d = *impl;
    const int ch = d.channels();
    int written = 0;
    while (written < frames)
    {
        size_t available = (d.pending.size() - d.pendingAt) / (size_t) ch;
        if (available == 0)
        {
            d.pending.clear();
            d.pendingAt = 0;
            if (! d.decodeMore()) break;
            continue;
        }
        // Dropping what's before a seek's target.
        if (d.discard > 0)
        {
            const auto drop = (size_t) std::min<int64_t> (d.discard, (int64_t) available);
            d.pendingAt += drop * (size_t) ch;
            d.discard -= (int64_t) drop;
            continue;
        }
        const auto n = std::min (available, (size_t) (frames - written));
        std::copy_n (d.pending.begin() + (std::ptrdiff_t) d.pendingAt, n * (size_t) ch, out + (size_t) written * (size_t) ch);
        d.pendingAt += n * (size_t) ch;
        written += (int) n;
    }
    d.delivered += written;
    return written;
}

bool Decoder::seek (double seconds)
{
    auto& d = *impl;
    seconds = std::max (0.0, seconds);
    auto* st = d.fmt->streams[d.stream];
    const int64_t ts = (int64_t) (seconds / av_q2d (st->time_base)) + (st->start_time != AV_NOPTS_VALUE ? st->start_time : 0);
    if (av_seek_frame (d.fmt, d.stream, ts, AVSEEK_FLAG_BACKWARD) < 0)
    {
        // A stream that can't seek by time (raw AAC): from the start, decoding forward.
        if (av_seek_frame (d.fmt, d.stream, 0, AVSEEK_FLAG_BYTE | AVSEEK_FLAG_BACKWARD) < 0) return false;
    }
    avcodec_flush_buffers (d.codec);
    swr_free (&d.swr);
    d.pending.clear();
    d.pendingAt = 0;
    d.ended = false;
    d.flushedDecoder = false;
    // Where the decoder actually landed: the first frame's time says.
    double landed = 0;
    for (;;)
    {
        const int r = avcodec_receive_frame (d.codec, d.frame);
        if (r == 0)
        {
            const int64_t pts = d.frame->best_effort_timestamp;
            landed = pts == AV_NOPTS_VALUE ? seconds : (double) (pts - (st->start_time != AV_NOPTS_VALUE ? st->start_time : 0)) * av_q2d (st->time_base);
            d.convert (d.frame);
            av_frame_unref (d.frame);
            break;
        }
        if (r != AVERROR (EAGAIN)) { d.ended = true; break; }
        if (av_read_frame (d.fmt, d.packet) < 0) { avcodec_send_packet (d.codec, nullptr); d.flushedDecoder = true; continue; }
        if (d.packet->stream_index == d.stream) avcodec_send_packet (d.codec, d.packet);
        av_packet_unref (d.packet);
    }
    landed = std::min (std::max (0.0, landed), seconds);
    d.discard = (int64_t) std::llround ((seconds - landed) * d.rate());
    d.delivered = (int64_t) std::llround (seconds * d.rate()) - d.discard;
    // read() counts the discarded frames out of delivered as it drops them... it doesn't: account here.
    d.delivered += d.discard;
    return true;
}

double Decoder::position() const { return (double) impl->delivered / impl->rate(); }

juce::StringArray decoderNames()
{
    juce::StringArray names;
    void* it = nullptr;
    while (const AVCodec* c = av_codec_iterate (&it))
        if (av_codec_is_decoder (c) && c->type == AVMEDIA_TYPE_AUDIO) names.add (c->name);
    return names;
}

bool hasDecoder (const char* name) { return avcodec_find_decoder_by_name (name) != nullptr; }

} // namespace kp
