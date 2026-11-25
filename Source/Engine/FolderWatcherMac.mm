// FolderWatcher on the Mac: FSEvents says which folders changed, a few seconds after.
#include "FolderWatcher.h"

#include <CoreServices/CoreServices.h>

namespace kp
{
juce::StringArray foldersHolding (const juce::StringArray& watched, const juce::StringArray& changed);

struct FolderWatcher::Impl
{
    FolderWatcher& owner;
    juce::StringArray folders;
    FSEventStreamRef stream = nullptr;
    dispatch_queue_t queue = dispatch_queue_create ("com.kallumshah.kalibreplayer.watch", DISPATCH_QUEUE_SERIAL);
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    explicit Impl (FolderWatcher& o) : owner (o) {}
    ~Impl() { *alive = false; stop(); }

    void stop()
    {
        if (! stream) return;
        FSEventStreamStop (stream);
        FSEventStreamInvalidate (stream);
        FSEventStreamRelease (stream);
        stream = nullptr;
    }

    static void callback (ConstFSEventStreamRef, void* info, size_t count, void* paths, const FSEventStreamEventFlags*, const FSEventStreamEventId*)
    {
        auto* self = static_cast<Impl*> (info);
        juce::StringArray changed;
        auto** p = static_cast<char**> (paths);
        for (size_t k = 0; k < count; ++k) changed.add (juce::String::fromUTF8 (p[k]).trimCharactersAtEnd ("/"));
        auto alive = self->alive;
        juce::MessageManager::callAsync ([self, alive, changed]
        {
            if (! *alive || ! self->owner.onChange) return;
            auto which = foldersHolding (self->folders, changed);
            if (! which.isEmpty()) self->owner.onChange (which);
        });
    }

    void start()
    {
        stop();
        if (folders.isEmpty()) return;
        CFMutableArrayRef paths = CFArrayCreateMutable (nullptr, 0, &kCFTypeArrayCallBacks);
        for (auto& f : folders)
        {
            CFStringRef s = CFStringCreateWithCString (nullptr, f.toRawUTF8(), kCFStringEncodingUTF8);
            CFArrayAppendValue (paths, s);
            CFRelease (s);
        }
        FSEventStreamContext context { 0, this, nullptr, nullptr, nullptr };
        stream = FSEventStreamCreate (nullptr, &callback, &context, paths, kFSEventStreamEventIdSinceNow, 3.0, kFSEventStreamCreateFlagNoDefer);
        CFRelease (paths);
        if (! stream) return;
        FSEventStreamSetDispatchQueue (stream, queue);
        FSEventStreamStart (stream);
    }
};

FolderWatcher::FolderWatcher() : impl (std::make_unique<Impl> (*this)) {}
FolderWatcher::~FolderWatcher() = default;

void FolderWatcher::watch (const juce::StringArray& folders)
{
    impl->folders = folders;
    impl->start();
}

} // namespace kp
