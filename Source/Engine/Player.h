/*
    Player - plays tracks, one after another without a gap (decision 0014).

    A decoding thread reads tracks ahead into a second of sound at the
    device's rate; the audio thread plays from it. When a track ends the
    next is already open, so the first sample of the next follows the last
    of this one: gapless. With crossfade on, the next starts that many
    seconds before this one ends, and they're faded across.

    The window says what plays (play) and, as each track starts decoding,
    what comes after it (setNext): it's told through events (poll()) that a
    track wants its next, that a track became the one heard, and that the
    end was reached. ReplayGain is applied per track as it's decoded; a
    gain learned late (scanned on the fly) is faded to.

    After decoding, on the audio thread: the equaliser, plug-ins and the
    volume (Dsp.h), then the meters' taps.
*/

#pragma once

#include "Decoder.h"
#include "Dsp.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <deque>
#include <mutex>
#include <optional>

namespace kp
{

class Player : public juce::AudioIODeviceCallback, private juce::Thread
{
public:
    Player();
    ~Player() override;

    struct Item
    {
        std::string location;
        std::int64_t token = 0;   // the window's name for this play of it (a place in a playlist)
        double gain = 1.0;        // ReplayGain, linear
    };

    // Starts playing an item now (what was playing stops).
    void play (const Item&);
    // What comes after the item that last asked (Event::wantsNext); nothing ends playback after it.
    void setNext (std::optional<Item>);
    void stop();
    void pause (bool);
    bool isPaused() const { return paused.load(); }
    bool isPlaying() const;   // something is loaded (paused or not)
    void seek (double seconds);
    // A gain learned after the item was queued.
    void setGain (std::int64_t token, double gain);

    // What's heard now.
    std::int64_t currentToken() const;
    double position() const;
    double length() const;

    struct Event
    {
        enum class Kind { started, wantsNext, ended, failed } kind;
        std::int64_t token = 0;
        std::string location, error;
    };
    // The events since last asked (the window's thread, a few times a second).
    std::vector<Event> poll();

    // Settings, read as the sound is made.
    std::atomic<float> volumeDb { 0 };
    std::atomic<bool> muted { false };
    std::atomic<float> crossfadeSeconds { 0 };   // 0: gapless
    Equalizer eq;
    ScopeTap scope;
    LevelTap levels;
    loudness::Meter loudness;
    std::atomic<bool> resetLoudness { false };   // the window asks; the audio thread does it

    // A new chain of plug-ins (prepared already); the old one is handed back when the audio thread lets go of it.
    void setChain (std::unique_ptr<PluginChain>);
    std::unique_ptr<PluginChain> takeRetiredChain();
    PluginChain* chain() const { return current.load(); }   // the window may look, not change
    double sampleRate() const { return deviceRate.load(); }
    int blockSize() const { return deviceBlock.load(); }

    // AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext (const float* const*, int, float* const*, int, int, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;

    // For tests: runs the audio thread's work without a device.
    void renderForTest (float* left, float* right, int frames);
    void prepareForTest (double rate, int block);
    int bufferedFrames() const { return fifo.getNumReady(); }

private:
    // Commands to the decoding thread.
    struct Command
    {
        enum class Kind { play, next, clearNext, stop, seek, gain, rate } kind;
        Item item;
        double seconds = 0;
    };
    std::mutex commandLock;
    std::deque<Command> commands;
    void send (Command);

    // What the decoding thread has open.
    struct Slot
    {
        Item item;
        std::unique_ptr<Decoder> decoder;
        double gain = 1.0, targetGain = 1.0;
        double length = 0;
    };
    std::unique_ptr<Slot> now, after;
    std::optional<Item> nextItem;
    bool nextAsked = false, streamEnded = false;
    double crossfadeAt = -1;           // frames into the crossfade, -1 when none
    double decodeRate = 48000;

    // The sound between the threads: interleaved stereo.
    static constexpr int fifoFrames = 48000 * 2;   // two seconds at most; filled to about one
    juce::AbstractFifo fifo { fifoFrames };
    std::vector<float> fifoData;
    std::atomic<std::int64_t> writtenFrames { 0 }, readFrames { 0 };
    std::atomic<int> flushAsked { 0 }, flushDone { 0 };
    std::atomic<std::int64_t> endAt { -1 };         // the stream ends when this has been read
    std::atomic<bool> paused { false }, active { false }, callbackRunning { false };
    std::atomic<double> deviceRate { 48000 };
    std::atomic<int> deviceBlock { 512 };

    // Where each track starts in the stream (decoding thread and window only).
    struct Segment { std::int64_t startFrame; std::int64_t token; double startSeconds, length; std::string location; };
    mutable std::mutex segmentLock;
    std::deque<Segment> segments;
    std::int64_t lastStartedToken = 0;
    bool endedSent = true;

    std::mutex eventLock;
    std::vector<Event> events;
    void post (Event);

    // Plug-ins.
    std::atomic<PluginChain*> current { nullptr }, pending { nullptr }, retired { nullptr };
    juce::AudioBuffer<float> chainBuffer;
    juce::MidiBuffer midi;

    // The audio thread's own.
    std::vector<float> left, right;
    float fade = 1.0f;        // 0..1, for pausing without a click
    float volumeNow = 1.0f;

    void run() override;
    bool openSlot (const Item&, std::unique_ptr<Slot>&);
    void flush();
    void handle (const Command&);
    int decode (float* out, int frames);    // interleaved stereo
    void addSegment (const Slot&, std::int64_t at, double startSeconds);
    void process (float* l, float* r, int frames);
};

} // namespace kp
