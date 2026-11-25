// Playback: one track follows another without a gap; crossfade overlaps
// them; ReplayGain's gain is applied; seeking lands where asked.
#include "AppTestHelpers.h"
#include "Check.h"
#include "Player.h"

using namespace kp;

namespace
{
// Plays through, answering "what's next" from a list, and returns all that was heard.
std::vector<float> playThrough (Player& p, std::vector<Player::Item> items, int maxFrames, int answerAfterMs = 0)
{
    p.prepareForTest (48000, 512);
    p.play (items.front());
    size_t next = 1;
    std::vector<float> heard;
    std::vector<float> l (512), r (512);
    bool ended = false;
    while ((int) heard.size() < maxFrames && ! ended)
    {
        for (auto& e : p.poll())
        {
            if (e.kind == Player::Event::Kind::wantsNext && answerAfterMs > 0) juce::Thread::sleep (answerAfterMs);
            if (e.kind == Player::Event::Kind::wantsNext)
                p.setNext (next < items.size() ? std::optional<Player::Item> (items[next++]) : std::nullopt);
            if (e.kind == Player::Event::Kind::ended) ended = true;
        }
        // The decoding thread has had time to fill, so nothing heard is a gap made by the test.
        const auto deadline = juce::Time::getMillisecondCounter() + 2000;
        while (p.bufferedFrames() < 512 && juce::Time::getMillisecondCounter() < deadline)
        {
            juce::Thread::sleep (1);
            for (auto& e : p.poll())
            {
                if (e.kind == Player::Event::Kind::wantsNext && answerAfterMs > 0) juce::Thread::sleep (answerAfterMs);
                if (e.kind == Player::Event::Kind::wantsNext)
                    p.setNext (next < items.size() ? std::optional<Player::Item> (items[next++]) : std::nullopt);
                if (e.kind == Player::Event::Kind::ended) ended = true;
            }
            if (! p.isPlaying()) break;
        }
        if (p.bufferedFrames() == 0 && ! p.isPlaying()) break;
        p.renderForTest (l.data(), r.data(), 512);
        heard.insert (heard.end(), l.begin(), l.end());
    }
    return heard;
}

juce::File ramp (const juce::String& name, int frames, float from, float to)
{
    return test::write (name, test::fmt ("wav", 24), 48000, frames, [=] (int, int i) { return from + (to - from) * (float) i / (float) frames; });
}
} // namespace

TEST ("player: gapless - the next track's first sample follows this one's last")
{
    auto a = ramp ("gapA.wav", 30000, -0.5f, 0.0f);
    auto b = ramp ("gapB.wav", 20000, 0.0f, 0.5f);
    Player p;
    auto heard = playThrough (p, { { a.getFullPathName().toStdString(), 1, 1.0 }, { b.getFullPathName().toStdString(), 2, 1.0 } }, 60000);
    // Every frame of both, in order, with nothing between.
    CHECK ((int) heard.size() >= 50000);
    if (heard.size() < 50000) return;
    int jumps = 0;
    for (int i = 1; i < 50000; ++i)
        if (std::abs (heard[(size_t) i] - heard[(size_t) i - 1]) > 0.001f) ++jumps;
    CHECK_EQ (jumps, 0);
    CHECK (std::abs (heard[29999] - (-0.5f + 0.5f * 29999 / 30000)) < 1e-4f);
    CHECK (std::abs (heard[30000] - 0.0f) < 1e-4f);
    CHECK (std::abs (heard[49999] - 0.5f * 19999 / 20000) < 1e-4f);
    // After the end, silence.
    for (size_t i = 50000; i < heard.size(); ++i) if (heard[i] != 0.0f) { check::fail (__FILE__, __LINE__, "sound after the end"); break; }
}

TEST ("player: ReplayGain's gain is applied per track")
{
    auto a = test::write ("gainA.wav", test::fmt ("wav", 24), 48000, 10000, [] (int, int) { return 0.4f; });
    Player p;
    auto heard = playThrough (p, { { a.getFullPathName().toStdString(), 1, 0.5 } }, 12000);
    CHECK (heard.size() >= 10000);
    if (heard.size() < 10000) return;
    CHECK (std::abs (heard[5000] - 0.2f) < 1e-4f);
}

TEST ("player: crossfade overlaps the tracks")
{
    auto a = test::write ("xA.wav", test::fmt ("wav", 24), 48000, 48000, [] (int, int) { return 0.3f; });
    auto b = test::write ("xB.wav", test::fmt ("wav", 24), 48000, 48000, [] (int, int) { return 0.3f; });
    Player p;
    p.crossfadeSeconds = 0.5f;
    auto heard = playThrough (p, { { a.getFullPathName().toStdString(), 1, 1.0 }, { b.getFullPathName().toStdString(), 2, 1.0 } }, 120000, 600);
    // 1 s + 1 s - 0.5 s overlapping, even when what's next is said late (the decoder waits for it).
    CHECK (heard.size() >= 72000);
    if (heard.size() < 72000) return;
    int last = 0;
    for (int i = 0; i < (int) heard.size(); ++i) if (heard[(size_t) i] != 0.0f) last = i;
    CHECK (std::abs (last - 72000) < 1200);
    // Equal power across the fade: in the middle, cos + sin of 45 degrees.
    CHECK (std::abs (heard[48000 - 12000] - 0.3f * 1.41421f) < 0.02f);
}

TEST ("player: seeking, and position follows")
{
    auto a = ramp ("seek.wav", 96000, 0.0f, 1.0f);
    Player p;
    p.prepareForTest (48000, 512);
    p.play ({ a.getFullPathName().toStdString(), 7, 1.0 });
    test::waitFor ([&] { return p.bufferedFrames() > 4096; });
    std::vector<float> l (512), r (512);
    p.renderForTest (l.data(), r.data(), 512);
    p.poll();
    CHECK_EQ (p.currentToken(), (std::int64_t) 7);
    p.seek (1.0);
    test::waitFor ([&] { p.renderForTest (l.data(), r.data(), 0); return p.bufferedFrames() > 4096 && std::abs (p.position() - 1.0) < 0.001; });
    CHECK (std::abs (p.position() - 1.0) < 0.001);
    p.renderForTest (l.data(), r.data(), 512);
    CHECK (std::abs (l[0] - 0.5f) < 0.001f);
    CHECK (std::abs (p.length() - 2.0) < 0.001);
    p.stop();
    test::waitFor ([&] { return ! p.isPlaying(); });
    CHECK (! p.isPlaying());
}

TEST ("player: the equaliser at 0 dB and the volume at 0 dB leave the sound alone")
{
    auto a = test::write ("neutral.wav", test::fmt ("wav", 24), 48000, 20000, [] (int c, int i) { return test::sine (c, i, 48000.0); });
    Player p;
    p.eq.enabled = true;
    p.eq.changed();
    auto heard = playThrough (p, { { a.getFullPathName().toStdString(), 1, 1.0 } }, 20480);
    CHECK (heard.size() >= 20000);
    if (heard.size() < 20000) return;
    double worst = 0;
    for (int i = 0; i < 20000; ++i) worst = std::max (worst, (double) std::abs (heard[(size_t) i] - test::sine (0, i, 48000.0)));
    CHECK (worst < 1e-4);
}
