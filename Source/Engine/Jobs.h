/*
    Jobs - long work done on every core while the music plays (decision
    0010): converting, ripping a CD, scanning ReplayGain, renaming and
    moving files. Each job is a list of items, worked on in parallel, with
    its progress shown in the window and a Stop.

    The work itself is in plain functions below, so the tests call them
    directly.
*/

#pragma once

#include "Encoder.h"
#include "ReplayGain.h"
#include "Track.h"

#include <juce_events/juce_events.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

namespace kp
{

//==============================================================================
// The work.

struct ConvertOptions
{
    EncodeSettings encode;
    juce::File folder;                 // where the files go
    std::string pattern = "%album artist%/%album%/%tracknumber% - %title%";
    bool overwrite = false;
    bool applyReplayGain = false;      // the gain applied to the sound itself (and its tags left out)
    rg::Settings replayGain;
    bool copyCover = true;
};

// Where a converted track goes.
juce::File convertTarget (const Track&, const ConvertOptions&);
// Converts one track; `out` is the file made. Tags and the cover go with it.
bool convert (const Track&, const ConvertOptions&, juce::File& out, juce::String& error, const std::atomic<bool>* cancel = nullptr);

// Decodes a track whole and measures it.
bool measure (const std::string& location, rg::Result&, juce::String& error, const std::atomic<bool>* cancel = nullptr);

// Moves (or copies) a file, making the folders it needs. Never replaces one that's there.
bool moveFile (const juce::File& from, const juce::File& to, bool copy, juce::String& error);

//==============================================================================
// Running it.

class Job
{
public:
    juce::String title;
    int total = 0;
    std::atomic<int> done { 0 };
    std::atomic<bool> cancelled { false };
    std::atomic<bool> finished { false };

    void addError (const juce::String& e) { std::lock_guard<std::mutex> g (lock); errors.add (e); }
    juce::StringArray errorsSoFar() { std::lock_guard<std::mutex> g (lock); return errors; }

private:
    std::mutex lock;
    juce::StringArray errors;
};

class Jobs
{
public:
    Jobs();
    ~Jobs();
    // Runs work(i) for each item on the pool; then finish() on the window's thread.
    std::shared_ptr<Job> run (const juce::String& title, int count, std::function<void (int, Job&)> work,
                              std::function<void (Job&)> finish, int threads = 0);
    std::vector<std::shared_ptr<Job>> running();
    void cancelAll();

private:
    juce::ThreadPool pool;
    std::mutex lock;
    // False once the jobs' owner is gone: a finish already posted to the window's thread does nothing.
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    std::vector<std::shared_ptr<Job>> jobs;
};

} // namespace kp
