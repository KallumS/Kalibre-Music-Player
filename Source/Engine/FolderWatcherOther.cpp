// FolderWatcher away from the Mac: every folder looked at again every five minutes.
#include "FolderWatcher.h"

namespace kp
{

struct FolderWatcher::Impl : public juce::Timer
{
    FolderWatcher& owner;
    juce::StringArray folders;
    explicit Impl (FolderWatcher& o) : owner (o) {}
    void timerCallback() override { if (owner.onChange && ! folders.isEmpty()) owner.onChange (folders); }
};

FolderWatcher::FolderWatcher() : impl (std::make_unique<Impl> (*this)) {}
FolderWatcher::~FolderWatcher() = default;

void FolderWatcher::watch (const juce::StringArray& folders)
{
    impl->folders = folders;
    if (folders.isEmpty()) impl->stopTimer();
    else impl->startTimer (5 * 60 * 1000);
}

} // namespace kp
