/*
    FolderWatcher - notices when files change in the library's folders
    (decision 0006). On a Mac, the system tells it (FSEvents), within a few
    seconds of a change; elsewhere it looks again every few minutes.
*/

#pragma once

#include <juce_events/juce_events.h>

#include <functional>
#include <memory>

namespace kp
{

class FolderWatcher
{
public:
    FolderWatcher();
    ~FolderWatcher();
    // Called on the window's thread with the watched folders that changed.
    std::function<void (const juce::StringArray&)> onChange;
    void watch (const juce::StringArray& folders);

    struct Impl;

private:
    std::unique_ptr<Impl> impl;
};

} // namespace kp
