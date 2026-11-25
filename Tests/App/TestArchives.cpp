// Music inside ZIP, gzip and RAR archives is listed, read and played.
#include "AppTestHelpers.h"
#include "Check.h"
#include "Decoder.h"
#include "Sources.h"
#include "TagIO.h"

using namespace kp;

TEST ("archives: a ZIP's tracks are listed, tagged and decoded")
{
    auto flac = test::write ("inzip.flac", test::fmt ("flac"), 44100, 22050, [] (int c, int i) { return test::sine (c, i); });
    juce::String e;
    CHECK (tagio::write (flac.getFullPathName().toStdString(), { { "title", { "Zipped" } }, { "artist", { "Band" } } }, e));
    auto wav = test::write ("inzip.wav", test::fmt ("wav"), 44100, 4410, [] (int c, int i) { return test::sine (c, i); });
    auto zipFile = test::scratch().getChildFile ("Album.zip");
    {
        juce::ZipFile::Builder b;
        b.addFile (flac, 9, "CD1/01 Song.flac");
        b.addFile (wav, 0, "CD1/02 Other.wav");
        juce::FileOutputStream out (zipFile);
        CHECK (b.writeToStream (out, nullptr));
    }
    auto entries = sources::list (zipFile, e);
    CHECK_EQ (entries.size(), (size_t) 2);
    if (entries.size() != 2) return;
    CHECK_EQ (entries[0].path, std::string ("CD1/01 Song.flac"));
    const auto loc = location::archive (zipFile.getFullPathName().toStdString(), entries[0].path);
    Track t;
    CHECK (tagio::read (loc, t, e));
    CHECK_EQ (t.first ("title"), std::string ("Zipped"));
    CHECK_EQ (t.codec, std::string ("FLAC"));
    CHECK (std::abs (t.length - 0.5) < 0.01);
    CHECK (! tagio::canWrite (loc));
    auto in = sources::open (loc, e);
    auto d = Decoder::open (std::move (in), "01 Song.flac", e);
    CHECK (d != nullptr);
    std::vector<float> got (44100 * 2);
    if (d) CHECK_EQ (d->read (got.data(), 44100), 22050);
}

TEST ("archives: a gzipped file is its own entry")
{
    auto wav = test::write ("gz.wav", test::fmt ("wav"), 44100, 4410, [] (int c, int i) { return test::sine (c, i); });
    auto gz = test::scratch().getChildFile ("Song.wav.gz");
    {
        juce::FileOutputStream raw (gz);
        juce::GZIPCompressorOutputStream z (raw, 9, juce::GZIPCompressorOutputStream::windowBitsGZIP);
        juce::FileInputStream in (wav);
        z.writeFromInputStream (in, -1);
    }
    juce::String e;
    auto entries = sources::list (gz, e);
    CHECK_EQ (entries.size(), (size_t) 1);
    if (entries.empty()) return;
    CHECK_EQ (entries[0].path, std::string ("Song.wav"));
    auto in = sources::open (location::archive (gz.getFullPathName().toStdString(), entries[0].path), e);
    auto d = Decoder::open (std::move (in), "Song.wav", e);
    CHECK (d != nullptr);
    if (d) CHECK_EQ (d->info().sampleRate, 44100);
}

TEST ("archives: RAR is read (libarchive's own test file)")
{
    const auto rar = juce::File (KP_SOURCE_DIR).getChildFile ("Tests/data/libarchive-test.rar");
    juce::MemoryBlock data;
    juce::String e;
    CHECK (sources::extract (rar, "test.txt", data, e));
    CHECK_EQ (data.toString(), juce::String ("test text document\r\n"));
    CHECK (! sources::extract (rar, "nothing.flac", data, e));
}
