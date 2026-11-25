/*
    LibraryScanner - looks through the watched folders for music (decision
    0006), on a thread of its own: new files and those changed since are
    read (their tags and what they are); those gone are reported gone.
    Files inside archives are listed and read as tracks of their own.

    What it finds is handed to the window's thread in batches, which puts
    it in the library.
*/

#pragma once

#include "Track.h"

#include <juce_events/juce_events.h>

#include <functional>
#include <mutex>
#include <unordered_map>

namespace kp
{

class LibraryScanner : private juce::Thread
{
public:
    LibraryScanner();
    ~LibraryScanner() override;

    struct Known { std::int64_t modified = 0, size = 0; };
    struct Batch
    {
        std::vector<Track> found;          // new or changed
        std::vector<std::string> gone;     // no longer there
        juce::String status;               // "Reading 1,204 of 5,000"
        bool finished = false;
    };
    // Called on the window's thread.
    std::function<void (Batch)> onBatch;

    // Looks through these folders, knowing what the library has (location -> modified, size).
    void scan (const juce::StringArray& folders, std::unordered_map<std::string, Known> known);
    void cancel();
    bool busy() const { return isThreadRunning(); }

private:
    juce::StringArray folders;
    std::unordered_map<std::string, Known> known;
    std::mutex lock;
    void run() override;
    void deliver (Batch);
};

} // namespace kp
