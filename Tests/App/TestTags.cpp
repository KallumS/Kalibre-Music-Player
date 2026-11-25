// Tags written and read back the same in every format the converter makes.
#include "AppTestHelpers.h"
#include "Check.h"
#include "TagIO.h"

using namespace kp;

TEST ("tags: written and read back in every format")
{
    const Tags tags {
        { "title", { "Song" } }, { "artist", { "One", "Two" } }, { "album", { "Record" } }, { "album artist", { "Band" } },
        { "date", { "1999" } }, { "genre", { "Jazz" } }, { "tracknumber", { "3" } }, { "totaltracks", { "12" } },
        { "discnumber", { "1" } }, { "totaldiscs", { "2" } }, { "replaygain_track_gain", { "-6.50 dB" } }, { "mood", { "calm" } },
    };
    for (auto* id : { "flac", "mp3", "aac", "alac", "opus", "vorbis", "wavpack", "wav", "aiff" })
    {
        auto f = test::write (juce::String ("tags.") + id + "." + encodeFormat (id)->extension, test::fmt (id), 44100, 8000,
                              [] (int c, int i) { return test::sine (c, i); });
        const auto loc = f.getFullPathName().toStdString();
        juce::String e;
        CHECK (tagio::write (loc, tags, e));
        Track t;
        CHECK (tagio::read (loc, t, e));
        auto expect = [&] (const char* field, const std::string& v)
        {
            if (t.first (field) != v) check::fail (__FILE__, __LINE__, std::string (id) + ": " + field + " is '" + t.first (field) + "', wanted '" + v + "'");
        };
        expect ("title", "Song");
        expect ("album", "Record");
        expect ("album artist", "Band");
        expect ("tracknumber", "3");
        expect ("totaltracks", "12");
        expect ("discnumber", "1");
        expect ("date", "1999");
        expect ("replaygain_track_gain", "-6.50 dB");
        CHECK (t.length > 0.1 && t.length < 0.3);
        CHECK (! t.codec.empty());
        // Several artists: kept as several where the format can, joined where it can't.
        CHECK (t.joined ("artist").find ("One") != std::string::npos);
    }
}

TEST ("tags: a field taken out is gone; covers go in and come out")
{
    auto f = test::write ("cover.flac", test::fmt ("flac"), 44100, 4000, [] (int c, int i) { return test::sine (c, i); });
    const auto loc = f.getFullPathName().toStdString();
    juce::String e;
    CHECK (tagio::write (loc, { { "title", { "A" } }, { "comment", { "x" } } }, e));
    CHECK (tagio::write (loc, { { "title", { "A" } } }, e));
    Track t;
    CHECK (tagio::read (loc, t, e));
    CHECK (! t.has ("comment"));
    juce::Image img (juce::Image::RGB, 8, 8, true);
    juce::MemoryOutputStream png;
    juce::PNGImageFormat().writeImageToStream (img, png);
    CHECK (tagio::writeCover (loc, png.getMemoryBlock(), "image/png"));
    juce::String mime;
    CHECK_EQ (tagio::embeddedCover (loc, mime).getSize(), png.getDataSize());
    CHECK_EQ (mime, juce::String ("image/png"));
    CHECK_EQ (tagio::cover (loc).getWidth(), 8);
    // A cover beside it is found when none is inside.
    auto g = test::write ("folder/nocover.flac", test::fmt ("flac"), 44100, 4000, [] (int c, int i) { return test::sine (c, i); });
    CHECK (! tagio::cover (g.getFullPathName().toStdString()).isValid());
    juce::FileOutputStream beside (g.getSiblingFile ("Folder.png"));
    juce::PNGImageFormat().writeImageToStream (img, beside);
    beside.flush();
    CHECK (tagio::cover (g.getFullPathName().toStdString()).isValid());
}
