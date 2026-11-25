// Every format the converter writes is read back by the decoder, and every
// format the player must open has its decoder.
#include "AppTestHelpers.h"
#include "Check.h"
#include "Decoder.h"
#include "Sources.h"

using namespace kp;

static std::unique_ptr<Decoder> openFile (const juce::File& f)
{
    juce::String e;
    auto in = sources::open (f.getFullPathName().toStdString(), e);
    return in ? Decoder::open (std::move (in), f.getFileName(), e) : nullptr;
}

TEST ("formats: every decoder the player needs is built in")
{
    for (auto* name : { "mp1", "mp2", "mp3", "mpc7", "mpc8", "aac", "wmav1", "wmav2", "wmapro", "wmalossless", "vorbis", "flac", "alac",
                        "wavpack", "pcm_s16le", "pcm_s16be", "pcm_s24be", "pcm_f32be", "pcm_alaw", "pcm_mulaw", "speex", "opus", "ape", "tta" })
    {
        if (! hasDecoder (name)) check::fail (__FILE__, __LINE__, std::string ("no decoder for ") + name);
    }
}

TEST ("formats: lossless formats come back exactly")
{
    const int frames = 44100;
    for (auto* id : { "flac", "alac", "wavpack", "wav", "aiff" })
    {
        auto f = test::write (juce::String ("lossless.") + encodeFormat (id)->extension, test::fmt (id, 16), 44100, frames,
                              [] (int c, int i) { return test::sine (c, i); });
        CHECK (f.existsAsFile());
        auto d = openFile (f);
        if (! d) { check::fail (__FILE__, __LINE__, std::string ("can't open ") + id); continue; }
        CHECK_EQ (d->info().sampleRate, 44100);
        CHECK_EQ (d->info().channels, 2);
        std::vector<float> got ((size_t) frames * 4);
        const int n = d->read (got.data(), frames * 2);
        CHECK_EQ (n, frames);
        double worst = 0;
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < 2; ++c) worst = std::max (worst, (double) std::abs (got[(size_t) (i * 2 + c)] - test::sine (c, i)));
        if (worst > 1.0 / 16384) check::fail (__FILE__, __LINE__, std::string (id) + " differs by " + std::to_string (worst));
    }
}

TEST ("formats: lossy formats come back at the same length and level")
{
    const int frames = 44100 * 2;
    for (auto [id, quality] : { std::pair { "mp3", 2 }, std::pair { "mp3", -1 }, std::pair { "aac", -1 }, std::pair { "opus", -1 }, std::pair { "vorbis", 6 } })
    {
        auto f = test::write (juce::String ("lossy-") + id + juce::String (quality) + "." + encodeFormat (id)->extension, test::fmt (id, 16, 192, quality),
                              44100, frames, [] (int c, int i) { return test::sine (c, i); });
        auto d = openFile (f);
        if (! d) { check::fail (__FILE__, __LINE__, std::string ("can't open ") + id); continue; }
        // Opus is always 48 kHz; read everything at 44.1 to compare.
        d->setOutput (44100, 2);
        std::vector<float> got ((size_t) frames * 4);
        const int n = d->read (got.data(), frames * 2);
        // Gapless: the encoder's padding is cut, so the length is the same (within a frame for formats without the data).
        if (std::abs (n - frames) > 1200) check::fail (__FILE__, __LINE__, std::string (id) + " length " + std::to_string (n));
        double sum = 0;
        for (int i = frames / 4; i < frames * 3 / 4 && i < n; ++i) sum += (double) got[(size_t) i * 2] * got[(size_t) i * 2];
        const double rms = std::sqrt (sum / (frames / 2));
        if (std::abs (rms - 0.5 / std::sqrt (2.0)) > 0.02) check::fail (__FILE__, __LINE__, std::string (id) + " level " + std::to_string (rms));
    }
}

TEST ("formats: AU and SND files, written by hand, play")
{
    // Sun's .au: ".snd", header size, data size, encoding 3 (16-bit), rate, channels; big-endian samples.
    juce::MemoryOutputStream out;
    out.write (".snd", 4);
    out.writeIntBigEndian (24);
    out.writeIntBigEndian (8000 * 2);
    out.writeIntBigEndian (3);
    out.writeIntBigEndian (8000);
    out.writeIntBigEndian (1);
    for (int i = 0; i < 8000; ++i) out.writeShortBigEndian ((short) (test::sine (0, i, 8000.0) * 32767));
    for (auto* name : { "sound.au", "sound.snd" })
    {
        auto f = test::scratch().getChildFile (name);
        f.replaceWithData (out.getData(), out.getDataSize());
        auto d = openFile (f);
        CHECK (d != nullptr);
        if (! d) continue;
        CHECK_EQ (d->info().sampleRate, 8000);
        CHECK (std::abs (d->info().length - 1.0) < 0.01);
        std::vector<float> got (16000);
        CHECK_EQ (d->read (got.data(), 8000), 8000);
    }
}

TEST ("formats: seeking lands exactly")
{
    const int frames = 44100 * 3;
    auto f = test::write ("seek.flac", test::fmt ("flac"), 44100, frames, [] (int, int i) { return (float) ((i % 1000) / 1000.0 - 0.5); });
    auto d = openFile (f);
    CHECK (d != nullptr);
    if (! d) return;
    CHECK (d->seek (1.5));
    CHECK (std::abs (d->position() - 1.5) < 1e-6);
    float got[20];
    d->read (got, 10);
    const int at = (int) (1.5 * 44100);
    for (int i = 0; i < 10; ++i) CHECK (std::abs (got[i * 2] - (float) (((at + i) % 1000) / 1000.0 - 0.5)) < 1e-3);
}

TEST ("formats: a file that isn't audio is refused, not crashed on")
{
    auto f = test::scratch().getChildFile ("not.mp3");
    f.replaceWithText ("this isn't an MP3 at all, only words");
    juce::String e;
    auto in = sources::open (f.getFullPathName().toStdString(), e);
    auto d = Decoder::open (std::move (in), f.getFileName(), e);
    CHECK (d == nullptr || d->read (nullptr, 0) == 0);
    CHECK (Decoder::open (nullptr, "x", e) == nullptr);
}
