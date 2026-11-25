#include "Player.h"
#include "Sources.h"
#include "Track.h"

#include <cmath>

namespace kp
{

namespace
{
constexpr double halfPi = 1.5707963267948966;
}

Player::Player() : juce::Thread ("Kalibre Player decoding")
{
    fifoData.resize ((size_t) fifoFrames * 2);
    startThread (juce::Thread::Priority::high);
}

Player::~Player()
{
    signalThreadShouldExit();
    notify();
    stopThread (4000);
    delete current.exchange (nullptr);
    delete pending.exchange (nullptr);
    delete retired.exchange (nullptr);
}

//==============================================================================
// The window's side.

void Player::send (Command c)
{
    {
        std::lock_guard<std::mutex> g (commandLock);
        commands.push_back (std::move (c));
    }
    notify();
}

void Player::play (const Item& item)
{
    paused = false;
    send ({ Command::Kind::play, item, 0 });
}

void Player::setNext (std::optional<Item> item)
{
    if (item) send ({ Command::Kind::next, *item, 0 });
    else send ({ Command::Kind::clearNext, {}, 0 });
}

void Player::stop()
{
    paused = false;
    send ({ Command::Kind::stop, {}, 0 });
}

void Player::pause (bool p) { paused = p; }

bool Player::isPlaying() const { return active.load(); }

void Player::seek (double seconds)
{
    Item audible;
    audible.token = currentToken();
    send ({ Command::Kind::seek, audible, seconds });
}

void Player::setGain (std::int64_t token, double gain)
{
    Item i;
    i.token = token;
    i.gain = gain;
    send ({ Command::Kind::gain, i, 0 });
}

std::int64_t Player::currentToken() const
{
    std::lock_guard<std::mutex> g (segmentLock);
    const auto read = readFrames.load();
    std::int64_t token = 0;
    for (auto& s : segments) if (s.startFrame <= read) token = s.token;
    return active.load() ? token : 0;
}

double Player::position() const
{
    std::lock_guard<std::mutex> g (segmentLock);
    const auto read = readFrames.load();
    const Segment* cur = nullptr;
    for (auto& s : segments) if (s.startFrame <= read) cur = &s;
    if (! cur) return 0;
    const double pos = cur->startSeconds + (double) (read - cur->startFrame) / deviceRate.load();
    return cur->length > 0 ? std::min (pos, cur->length) : pos;
}

double Player::length() const
{
    std::lock_guard<std::mutex> g (segmentLock);
    const auto read = readFrames.load();
    double len = 0;
    for (auto& s : segments) if (s.startFrame <= read) len = s.length;
    return len;
}

void Player::post (Event e)
{
    std::lock_guard<std::mutex> g (eventLock);
    events.push_back (std::move (e));
}

std::vector<Player::Event> Player::poll()
{
    // Which track is heard, from where the audio thread has read to.
    {
        std::lock_guard<std::mutex> g (segmentLock);
        const auto read = readFrames.load();
        while (segments.size() > 1 && segments[1].startFrame <= read) segments.pop_front();
        if (! segments.empty() && segments.front().startFrame <= read && segments.front().token != lastStartedToken)
        {
            lastStartedToken = segments.front().token;
            post ({ Event::Kind::started, segments.front().token, segments.front().location, {} });
        }
        const auto end = endAt.load();
        if (end >= 0 && read >= end && ! endedSent)
        {
            endedSent = true;
            active = false;
            segments.clear();
            lastStartedToken = 0;
            post ({ Event::Kind::ended, 0, {}, {} });
        }
    }
    std::lock_guard<std::mutex> g (eventLock);
    auto out = std::move (events);
    events.clear();
    return out;
}

void Player::setChain (std::unique_ptr<PluginChain> c)
{
    // One waiting at a time: a newer one replaces it.
    delete pending.exchange (c.release());
    if (! callbackRunning.load())
    {
        // No audio thread to take it: swapped here.
        if (auto* p = pending.exchange (nullptr)) delete current.exchange (p);
    }
}

std::unique_ptr<PluginChain> Player::takeRetiredChain() { return std::unique_ptr<PluginChain> (retired.exchange (nullptr)); }

//==============================================================================
// The decoding thread.

bool Player::openSlot (const Item& item, std::unique_ptr<Slot>& slot)
{
    juce::String error;
    auto in = sources::open (item.location, error);
    std::unique_ptr<Decoder> d;
    if (in) d = Decoder::open (std::move (in), juce::String::fromUTF8 (location::fileName (item.location).c_str()), error);
    if (! d)
    {
        post ({ Event::Kind::failed, item.token, item.location, error.toStdString() });
        return false;
    }
    d->setOutput ((int) decodeRate, 2);
    slot = std::make_unique<Slot>();
    slot->item = item;
    slot->gain = slot->targetGain = item.gain;
    slot->length = d->info().length;
    slot->decoder = std::move (d);
    return true;
}

void Player::addSegment (const Slot& s, std::int64_t at, double startSeconds)
{
    std::lock_guard<std::mutex> g (segmentLock);
    segments.push_back ({ at, s.item.token, startSeconds, s.length, s.item.location });
}

void Player::flush()
{
    const int asked = ++flushAsked;
    // The audio thread empties the buffer at its next block; without one running, it's emptied here.
    for (int waited = 0; flushDone.load() != asked; ++waited)
    {
        if (! callbackRunning.load() || waited > 300)
        {
            fifo.reset();
            readFrames = writtenFrames.load();
            flushDone = asked;
            break;
        }
        juce::Thread::sleep (1);
    }
    std::lock_guard<std::mutex> g (segmentLock);
    segments.clear();
    endAt = -1;
}

void Player::handle (const Command& c)
{
    switch (c.kind)
    {
        case Command::Kind::play:
        {
            flush();
            now.reset();
            after.reset();
            nextItem.reset();
            nextAsked = false;
            crossfadeAt = -1;
            streamEnded = false;
            endedSent = false;
            {
                std::lock_guard<std::mutex> g (segmentLock);
                lastStartedToken = 0;
            }
            if (! openSlot (c.item, now)) { active = false; streamEnded = true; return; }
            active = true;
            addSegment (*now, writtenFrames.load(), 0);
            nextAsked = true;
            post ({ Event::Kind::wantsNext, now->item.token, now->item.location, {} });
            return;
        }
        case Command::Kind::next:
            nextItem = c.item;
            nextAsked = false;
            after.reset();
            return;
        case Command::Kind::clearNext:
            nextItem.reset();
            nextAsked = false;
            after.reset();
            return;
        case Command::Kind::stop:
            flush();
            now.reset();
            after.reset();
            nextItem.reset();
            streamEnded = true;
            active = false;
            endedSent = true;
            {
                std::lock_guard<std::mutex> g (segmentLock);
                lastStartedToken = 0;
            }
            return;
        case Command::Kind::seek:
        {
            // The track heard may not be the one decoding (the next one may have begun): open it again.
            if (! now || now->item.token != c.item.token)
            {
                Item again;
                {
                    std::lock_guard<std::mutex> g (segmentLock);
                    for (auto& s : segments) if (s.token == c.item.token) again = { s.location, s.token, 1.0 };
                }
                if (again.location.empty()) return;
                if (now && now->item.token == again.token) again.gain = now->targetGain;
                else if (after && after->item.token == again.token) again.gain = after->targetGain;
                handle ({ Command::Kind::play, again, 0 });
                if (! now) return;
            }
            flush();
            after.reset();
            crossfadeAt = -1;
            streamEnded = false;
            endedSent = false;
            const double to = now->length > 0 ? std::min (c.seconds, std::max (0.0, now->length - 0.05)) : c.seconds;
            now->decoder->seek (to);
            active = true;
            addSegment (*now, writtenFrames.load(), to);
            return;
        }
        case Command::Kind::gain:
            if (now && now->item.token == c.item.token) now->targetGain = c.item.gain;
            if (after && after->item.token == c.item.token) after->targetGain = c.item.gain;
            if (nextItem && nextItem->token == c.item.token) nextItem->gain = c.item.gain;
            return;
        case Command::Kind::rate:
        {
            if (std::abs (c.seconds - decodeRate) < 0.5) return;
            decodeRate = c.seconds;
            if (! now) return;
            // Opened again at the new rate, where it's heard.
            const auto token = currentToken();
            const double at = position();
            handle ({ Command::Kind::seek, Item { {}, token, 1.0 }, at });
            if (now) { now->decoder->setOutput ((int) decodeRate, 2); now->decoder->seek (at); }
            return;
        }
    }
}

int Player::decode (float* out, int frames)
{
    std::vector<float> mix;
    int filled = 0;
    const double rate = decodeRate;
    while (filled < frames && now)
    {
        int want = frames - filled;
        float* dst = out + (size_t) filled * 2;

        // Crossfade: the next begins this many seconds before this one ends.
        const double cross = crossfadeSeconds.load();
        // Not past where it would begin until the window has said what's next (the buffer covers the wait).
        if (cross > 0 && crossfadeAt < 0 && ! after && nextAsked && now->length > cross)
        {
            const double room = (now->length - cross) - now->decoder->position();
            if (room <= 0) return filled;
            want = std::min (want, std::max (1, (int) std::ceil (room * rate)));
        }
        if (cross > 0 && crossfadeAt < 0 && after && now->length > 0)
        {
            const double remaining = now->length - now->decoder->position();
            if (remaining <= cross && remaining > 0.05)
            {
                crossfadeAt = 0;
                addSegment (*after, writtenFrames.load() + filled, 0);
            }
        }

        int got = now->decoder->read (dst, want);
        // ReplayGain, faded to a new value over 50 ms.
        for (int i = 0; i < got; ++i)
        {
            if (now->gain != now->targetGain)
            {
                const double step = 1.0 / (0.05 * rate);
                now->gain += std::clamp (now->targetGain - now->gain, -step, step);
            }
            dst[i * 2] *= (float) now->gain;
            dst[i * 2 + 1] *= (float) now->gain;
        }

        if (crossfadeAt >= 0 && after)
        {
            const double span = std::max (1.0, cross * rate);
            mix.resize ((size_t) want * 2);
            const int gotNext = after->decoder->read (mix.data(), want);
            for (int i = 0; i < want; ++i)
            {
                const double t = std::min (1.0, (crossfadeAt + i) / span);
                const float a = i < got ? (float) std::cos (t * halfPi) : 0.0f;
                const float b = (float) (std::sin (t * halfPi) * after->gain);
                const float nl = i < gotNext ? mix[(size_t) i * 2] : 0.0f, nr = i < gotNext ? mix[(size_t) i * 2 + 1] : 0.0f;
                const float cl = i < got ? dst[i * 2] : 0.0f, cr = i < got ? dst[i * 2 + 1] : 0.0f;
                dst[i * 2] = cl * a + nl * b;
                dst[i * 2 + 1] = cr * a + nr * b;
            }
            crossfadeAt += want;
            if (got < want || crossfadeAt >= span)
            {
                // The faded-out track is done: the next is now the one playing.
                now = std::move (after);
                nextItem.reset();
                crossfadeAt = -1;
                nextAsked = true;
                post ({ Event::Kind::wantsNext, now->item.token, now->item.location, {} });
            }
            filled += std::max (got, gotNext);
            if (gotNext == 0 && got < want) break;
            continue;
        }

        filled += got;
        if (got == want) break;

        // This track has ended: the next follows straight on (gapless).
        if (! after && nextItem)
        {
            auto item = *nextItem;
            nextItem.reset();
            if (! openSlot (item, after))
            {
                nextAsked = true;
                post ({ Event::Kind::wantsNext, item.token, item.location, {} });
                return filled;
            }
        }
        if (after)
        {
            now = std::move (after);
            nextItem.reset();
            addSegment (*now, writtenFrames.load() + filled, 0);
            nextAsked = true;
            post ({ Event::Kind::wantsNext, now->item.token, now->item.location, {} });
            continue;
        }
        if (nextAsked) return filled;   // the window hasn't answered yet: wait for it
        // Nothing after: the end, once what's buffered has been heard.
        endAt = writtenFrames.load() + filled;
        streamEnded = true;
        now.reset();
        break;
    }
    return filled;
}

void Player::run()
{
    std::vector<float> block (4096 * 2);
    while (! threadShouldExit())
    {
        std::deque<Command> todo;
        {
            std::lock_guard<std::mutex> g (commandLock);
            todo.swap (commands);
        }
        for (auto& c : todo) handle (c);

        // The next opened ahead, so it follows without a gap.
        if (now && nextItem && ! after)
        {
            auto item = *nextItem;
            if (! openSlot (item, after))
            {
                nextItem.reset();
                nextAsked = true;
                post ({ Event::Kind::wantsNext, item.token, item.location, {} });
            }
        }

        if (! now || streamEnded) { wait (20); continue; }
        const int target = std::min (fifoFrames - 4096, (int) (decodeRate * 0.75));
        const int ready = fifo.getNumReady();
        if (ready >= target) { wait (10); continue; }
        const int want = std::min (4096, target - ready);
        const int got = decode (block.data(), want);
        if (got > 0)
        {
            int s1, n1, s2, n2;
            fifo.prepareToWrite (got, s1, n1, s2, n2);
            std::copy_n (block.data(), (size_t) n1 * 2, fifoData.data() + (size_t) s1 * 2);
            if (n2 > 0) std::copy_n (block.data() + (size_t) n1 * 2, (size_t) n2 * 2, fifoData.data() + (size_t) s2 * 2);
            fifo.finishedWrite (n1 + n2);
            writtenFrames += n1 + n2;
        }
        else if (now) wait (5);   // waiting for the window to say what's next
    }
}

//==============================================================================
// The audio thread's side.

void Player::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    const double rate = device->getCurrentSampleRate();
    const int block = device->getCurrentBufferSizeSamples();
    prepareForTest (rate, block);
    // Plug-ins learn the new rate and size.
    if (auto* c = current.load())
        for (auto& p : c->plugins) { p->releaseResources(); p->setRateAndBufferSizeDetails (rate, block); p->prepareToPlay (rate, block); }
    send ({ Command::Kind::rate, {}, rate });
    callbackRunning = true;
}

void Player::prepareForTest (double rate, int block)
{
    deviceRate = rate > 0 ? rate : 48000;
    deviceBlock = std::max (32, block);
    left.assign ((size_t) std::max (block, 4096), 0.0f);
    right.assign ((size_t) std::max (block, 4096), 0.0f);
    chainBuffer.setSize (8, std::max (block, 4096));
    eq.prepare (deviceRate);
    loudness.prepare (deviceRate);
    if (rate != decodeRate) send ({ Command::Kind::rate, {}, rate });
}

void Player::audioDeviceStopped() { callbackRunning = false; }

void Player::process (float* l, float* r, int n)
{
    eq.process (l, r, n);

    if (auto* c = current.load (std::memory_order_acquire))
    {
        for (size_t k = 0; k < c->plugins.size(); ++k)
        {
            auto* p = c->plugins[k].get();
            if (k < c->bypassed.size() && c->bypassed[k]) continue;
            const int ins = p->getTotalNumInputChannels(), outs = p->getTotalNumOutputChannels();
            const int ch = std::clamp (std::max (ins, outs), 1, chainBuffer.getNumChannels());
            juce::AudioBuffer<float> view (chainBuffer.getArrayOfWritePointers(), ch, n);
            view.clear();
            if (ins == 1)
                for (int i = 0; i < n; ++i) view.getWritePointer (0)[i] = 0.5f * (l[i] + r[i]);
            else
            {
                if (ch > 0) std::copy_n (l, n, view.getWritePointer (0));
                if (ch > 1) std::copy_n (r, n, view.getWritePointer (1));
            }
            midi.clear();
            p->processBlock (view, midi);
            std::copy_n (view.getReadPointer (0), n, l);
            std::copy_n (view.getReadPointer (outs > 1 && ch > 1 ? 1 : 0), n, r);
        }
    }

    // The volume, eased to a new value so it doesn't click.
    const float target = muted.load() ? 0.0f : std::pow (10.0f, volumeDb.load() / 20.0f);
    for (int i = 0; i < n; ++i)
    {
        volumeNow += (target - volumeNow) * 0.002f;
        l[i] *= volumeNow;
        r[i] *= volumeNow;
    }
    scope.write (l, r, n);
    levels.write (l, r, n);
    if (resetLoudness.exchange (false)) loudness.reset();
    loudness.process (l, r, n);
}

void Player::renderForTest (float* outL, float* outR, int frames)
{
    // The plug-in chain swapped at a block's start.
    if (pending.load() != nullptr && retired.load() == nullptr)
        if (auto* p = pending.exchange (nullptr)) retired.store (current.exchange (p));

    if (flushAsked.load() != flushDone.load())
    {
        const int asked = flushAsked.load();
        const int n = fifo.getNumReady();
        int s1, n1, s2, n2;
        fifo.prepareToRead (n, s1, n1, s2, n2);
        fifo.finishedRead (n1 + n2);
        readFrames += n1 + n2;
        flushDone = asked;
    }

    const int block = std::max (32, deviceBlock.load());
    for (int done = 0; done < frames;)
    {
        const int n = std::min ({ frames - done, block, (int) left.size() });
        float* l = left.data();
        float* r = right.data();
        std::fill_n (l, n, 0.0f);
        std::fill_n (r, n, 0.0f);
        const bool wantPaused = paused.load();
        const bool hearing = active.load() && ! (wantPaused && fade <= 0.0f);
        if (hearing)
        {
            int s1, n1, s2, n2;
            fifo.prepareToRead (n, s1, n1, s2, n2);
            for (int i = 0; i < n1; ++i) { l[i] = fifoData[(size_t) (s1 + i) * 2]; r[i] = fifoData[(size_t) (s1 + i) * 2 + 1]; }
            for (int i = 0; i < n2; ++i) { l[n1 + i] = fifoData[(size_t) (s2 + i) * 2]; r[n1 + i] = fifoData[(size_t) (s2 + i) * 2 + 1]; }
            fifo.finishedRead (n1 + n2);
            readFrames += n1 + n2;
            // Pausing and resuming fade over 20 ms.
            const float step = 1.0f / (float) (0.02 * deviceRate.load());
            for (int i = 0; i < n; ++i)
            {
                fade = std::clamp (fade + (wantPaused ? -step : step), 0.0f, 1.0f);
                l[i] *= fade;
                r[i] *= fade;
            }
        }
        process (l, r, n);
        std::copy_n (l, n, outL + done);
        std::copy_n (r, n, outR + done);
        done += n;
    }
}

void Player::audioDeviceIOCallbackWithContext (const float* const*, int, float* const* outputs, int numOutputs, int numSamples,
                                               const juce::AudioIODeviceCallbackContext&)
{
    callbackRunning = true;
    if (numOutputs <= 0) return;
    float* l = outputs[0];
    float* r = numOutputs > 1 ? outputs[1] : nullptr;
    if (r) renderForTest (l, r, numSamples);
    else
    {
        // One speaker: both sides, mixed. A small block at a time, into the output and a scratch half.
        float scratch[256];
        for (int done = 0; done < numSamples;)
        {
            const int n = std::min (256, numSamples - done);
            renderForTest (l + done, scratch, n);
            for (int i = 0; i < n; ++i) l[done + i] = 0.5f * (l[done + i] + scratch[i]);
            done += n;
        }
    }
    for (int c = 2; c < numOutputs; ++c)
        if (outputs[c]) std::fill_n (outputs[c], numSamples, 0.0f);
}

} // namespace kp
