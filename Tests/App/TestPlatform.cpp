// What differs by system: CDs read from the drive (Windows, Linux), folders
// watched, and the words on screen.
#include "AppTestHelpers.h"
#include "CdRom.h"
#include "Check.h"
#include "Decoder.h"
#include "FolderWatcher.h"
#include "Platform.h"
#include "Track.h"

using namespace kp;

namespace
{
// A drive whose sectors hold a ramp, so where each sample came from can be checked.
struct FakeDrive : cdrom::SectorReader
{
    int reads = 0;
    bool read (long lba, int count, unsigned char* out) override
    {
        ++reads;
        for (int s = 0; s < count; ++s)
            for (int f = 0; f < cdrom::sectorBytes / 4; ++f)
            {
                const auto frame = (lba + s) * (cdrom::sectorBytes / 4) + f;
                const auto v = (short) (frame % 30000);
                for (int c = 0; c < 2; ++c)
                {
                    out[(s * cdrom::sectorBytes) + f * 4 + c * 2] = (unsigned char) (v & 0xff);
                    out[(s * cdrom::sectorBytes) + f * 4 + c * 2 + 1] = (unsigned char) ((v >> 8) & 0xff);
                }
            }
        return true;
    }
};
} // namespace

TEST ("platform: a CD track reads as WAV, from its own sectors, seeking anywhere")
{
    const long first = 1000, sectors = 75 * 4;   // four seconds
    auto stream = cdrom::trackStream (std::make_unique<FakeDrive>(), first, sectors);
    CHECK_EQ (stream->getTotalLength(), (juce::int64) (44 + sectors * cdrom::sectorBytes));
    juce::String e;
    auto d = Decoder::open (std::move (stream), "Track 01.wav", e);
    CHECK (d != nullptr);
    if (! d) return;
    CHECK_EQ (d->info().sampleRate, 44100);
    CHECK_EQ (d->info().channels, 2);
    CHECK (std::abs (d->info().length - 4.0) < 0.001);
    float got[8];
    CHECK_EQ (d->read (got, 4), 4);
    const auto firstFrame = first * (cdrom::sectorBytes / 4);
    CHECK (std::abs (got[0] - (float) (firstFrame % 30000) / 32768.0f) < 1e-4f);
    CHECK (d->seek (2.5));
    d->read (got, 1);
    CHECK (std::abs (got[0] - (float) ((firstFrame + 2 * 44100 + 22050) % 30000) / 32768.0f) < 1e-4f);
}

TEST ("platform: a CD's contents, with an enhanced CD's data track left out")
{
    // Three audio tracks, then data: the audio ends 11,400 sectors before it (MusicBrainz's rule).
    auto disc = cdrom::discFrom (1, { 0, 15000, 30000, 60000 }, { false, false, false, true }, 90000);
    CHECK_EQ (disc.toc.lastTrack, 3);
    CHECK_EQ (disc.toc.offsets, (std::vector<long> { 150, 15150, 30150 }));
    CHECK_EQ (disc.toc.leadOut, 60000L - 11400 + 150);
    CHECK_EQ (disc.sectors.back(), 60000L - 11400 - 30000);
    auto plain = cdrom::discFrom (1, { 0, 20000 }, { false, false }, 40000);
    CHECK_EQ (plain.toc.leadOut, 40150L);
    CHECK_EQ (cdrom::location ("D:", 3), std::string ("cdda:D:|3"));
    auto where = cdrom::split ("cdda:/dev/sr0|12");
    CHECK (where && where->first == "/dev/sr0" && where->second == 12);
    CHECK_EQ (location::fileName ("cdda:D:|3"), std::string ("Track 03.wav"));
}

TEST ("platform: a watched folder says when a file's added")
{
    auto dir = test::scratch().getChildFile ("Watched");
    dir.getChildFile ("Inside").createDirectory();
    FolderWatcher w;
    juce::StringArray told;
    w.onChange = [&] (const juce::StringArray& f) { told.addArray (f); };
    w.watch ({ dir.getFullPathName() });
    test::waitFor ([] { return false; }, 1500);   // the watch is set up
    dir.getChildFile ("Inside/new.flac").replaceWithText ("x");
   #if JUCE_MAC || JUCE_WINDOWS || JUCE_LINUX
    CHECK (test::waitFor ([&] { return ! told.isEmpty(); }, 15000));
    CHECK (told.contains (dir.getFullPathName()));
   #endif
}

TEST ("platform: keys and places named as each system names them")
{
    const auto& w = platform::words();
   #if JUCE_MAC
    CHECK_EQ (platform::commandKey ("E"), std::string ("Command-E"));
    CHECK_EQ (std::string (w.showInFiles), std::string ("Show in Finder"));
   #elif JUCE_WINDOWS
    CHECK_EQ (platform::commandKey ("E"), std::string ("Ctrl+E"));
    CHECK_EQ (std::string (w.showInFiles), std::string ("Show in File Explorer"));
   #else
    CHECK_EQ (platform::commandKey ("E"), std::string ("Ctrl+E"));
    CHECK_EQ (std::string (w.showInFiles), std::string ("Show in File Manager"));
   #endif
}
