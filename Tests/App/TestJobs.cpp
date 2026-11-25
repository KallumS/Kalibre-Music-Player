// Converting, measuring ReplayGain, moving files, and the jobs that run them.
#include "AppTestHelpers.h"
#include "Check.h"
#include "Jobs.h"
#include "TagIO.h"

using namespace kp;

TEST ("jobs: converting keeps the tags and names the file by the pattern")
{
    auto src = test::write ("conv/src.wav", test::fmt ("wav"), 44100, 44100, [] (int c, int i) { return test::sine (c, i); });
    Track t;
    t.location = src.getFullPathName().toStdString();
    t.tags = { { "artist", { "AC/DC" } }, { "album", { "Live" } }, { "title", { "Song?" } }, { "tracknumber", { "4" } }, { "replaygain_track_gain", { "-6.02 dB" } } };
    for (auto* id : { "flac", "mp3", "aac", "opus" })
    {
        ConvertOptions o;
        o.encode = test::fmt (id);
        o.folder = test::scratch().getChildFile ("out-" + juce::String (id));
        o.pattern = "%artist%/%album%/%tracknumber% %title%";
        juce::File out;
        juce::String e;
        CHECK (convert (t, o, out, e));
        // Folders as the system writes them (\ on Windows).
        CHECK_EQ (out.getRelativePathFrom (o.folder).replaceCharacter ('\\', '/'), juce::String ("AC_DC/Live/04 Song_.") + encodeFormat (id)->extension);
        Track back;
        CHECK (tagio::read (out.getFullPathName().toStdString(), back, e));
        CHECK_EQ (back.first ("title"), std::string ("Song?"));
        CHECK_EQ (back.first ("replaygain_track_gain"), std::string ("-6.02 dB"));
        CHECK (std::abs (back.length - 1.0) < 0.05);
        // Not again without Replace.
        CHECK (! convert (t, o, out, e));
    }
    // ReplayGain applied to the sound: half as loud, and the tags left out.
    ConvertOptions o;
    o.encode = test::fmt ("wav");
    o.folder = test::scratch().getChildFile ("out-gain");
    o.applyReplayGain = true;
    o.replayGain.mode = rg::Mode::track;
    juce::File out;
    juce::String e;
    CHECK (convert (t, o, out, e));
    rg::Result before, after;
    CHECK (measure (src.getFullPathName().toStdString(), before, e));
    CHECK (measure (out.getFullPathName().toStdString(), after, e));
    CHECK (std::abs ((before.loudness - after.loudness) - 6.02) < 0.1);
    Track back;
    tagio::read (out.getFullPathName().toStdString(), back, e);
    CHECK (! back.has ("replaygain_track_gain"));
}

TEST ("jobs: moving files, and a change of case only")
{
    auto a = test::scratch().getChildFile ("move/a/one.txt");
    a.create();
    juce::String e;
    auto to = test::scratch().getChildFile ("move/b/c/two.txt");
    CHECK (moveFile (a, to, false, e));
    CHECK (to.existsAsFile());
    CHECK (! a.getParentDirectory().exists());   // the folder left empty went too
    auto upper = to.getSiblingFile ("TWO.txt");
    CHECK (moveFile (to, upper, false, e));
    CHECK (upper.existsAsFile());
    auto other = test::scratch().getChildFile ("move/other.txt");
    other.create();
    CHECK (! moveFile (other, upper, false, e));   // never replaces one that's there
    CHECK (moveFile (other, test::scratch().getChildFile ("move/copy.txt"), true, e));
    CHECK (other.existsAsFile());
}

TEST ("jobs: run in parallel and say when they're done")
{
    Jobs jobs;
    std::atomic<int> sum { 0 };
    bool finished = false;
    auto job = jobs.run ("Adding", 100, [&] (int i, Job&) { sum += i; }, [&] (Job& j) { finished = true; CHECK_EQ (j.done.load(), 100); });
    CHECK (test::waitFor ([&] { return finished; }));
    CHECK_EQ (sum.load(), 4950);
    bool stopped = false;
    auto slow = jobs.run ("Slow", 1000, [] (int, Job&) { juce::Thread::sleep (5); }, [&] (Job& j) { stopped = j.cancelled.load(); });
    jobs.cancelAll();
    CHECK (test::waitFor ([&] { return stopped; }));
    CHECK (slow->done.load() < 1000);
}

TEST ("jobs: what's left to say when a job ends is dropped once its owner is gone")
{
    bool said = false;
    {
        Jobs jobs;
        jobs.run ("Quick", 1, [] (int, Job&) {}, [&] (Job&) { said = true; });
        juce::Thread::sleep (200);   // done on the pool; its finish waits for the message loop
    }
    test::waitFor ([] { return false; }, 300);
    CHECK (! said);
}
