// FolderWatcher on Linux: inotify on each folder and every folder inside it
// (inotify doesn't look inside by itself), told a few seconds after a change.
#include "FolderWatcher.h"

#include <poll.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <atomic>
#include <map>
#include <thread>

namespace kp
{

struct FolderWatcher::Impl
{
    FolderWatcher& owner;
    juce::StringArray folders;
    std::thread thread;
    int wake[2] = { -1, -1 };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    explicit Impl (FolderWatcher& o) : owner (o) {}
    ~Impl() { *alive = false; stop(); }

    void stop()
    {
        if (thread.joinable())
        {
            const char c = 1;
            (void) ::write (wake[1], &c, 1);
            thread.join();
        }
        for (auto& fd : wake) if (fd >= 0) { ::close (fd); fd = -1; }
    }

    void start()
    {
        stop();
        if (folders.isEmpty() || ::pipe (wake) != 0) return;
        thread = std::thread ([this, watched = folders, a = alive, stopFd = wake[0]] { run (watched, a, stopFd); });
    }

    static constexpr uint32_t mask = IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE | IN_DELETE_SELF;

    void add (int fd, const juce::File& dir, const juce::String& root, std::map<int, juce::String>& roots)
    {
        const int wd = inotify_add_watch (fd, dir.getFullPathName().toRawUTF8(), mask);
        if (wd >= 0) roots[wd] = root;
        for (auto& d : dir.findChildFiles (juce::File::findDirectories, false))
            if (! d.getFileName().startsWithChar ('.')) add (fd, d, root, roots);
    }

    void run (juce::StringArray watched, std::shared_ptr<std::atomic<bool>> a, int stopFd)
    {
        const int fd = inotify_init1 (IN_NONBLOCK | IN_CLOEXEC);
        if (fd < 0) return;
        std::map<int, juce::String> roots;   // watch -> the folder it's in
        for (auto& f : watched) add (fd, juce::File (f), f, roots);
        juce::StringArray pending;
        alignas (inotify_event) char buffer[16384];
        for (;;)
        {
            pollfd fds[] = { { fd, POLLIN, 0 }, { stopFd, POLLIN, 0 } };
            // Changes come in bursts: told once they've been quiet for two seconds.
            const int r = ::poll (fds, 2, pending.isEmpty() ? -1 : 2000);
            if (r < 0 || (fds[1].revents & POLLIN)) break;
            if (r == 0)
            {
                juce::MessageManager::callAsync ([this, a, pending] { if (*a && owner.onChange) owner.onChange (pending); });
                pending.clear();
                continue;
            }
            const auto n = ::read (fd, buffer, sizeof buffer);
            for (ssize_t at = 0; n > 0 && at < n;)
            {
                auto* e = reinterpret_cast<inotify_event*> (buffer + at);
                at += (ssize_t) sizeof (inotify_event) + e->len;
                auto root = roots.find (e->wd);
                if (root == roots.end()) continue;
                pending.addIfNotAlreadyThere (root->second);
                // A new folder is watched too.
                if ((e->mask & IN_ISDIR) && (e->mask & (IN_CREATE | IN_MOVED_TO)) && e->len > 0)
                {
                    for (auto& f : watched)
                        if (f == root->second)
                        {
                            // Its path isn't known from the watch alone: look again from the root.
                            add (fd, juce::File (f), f, roots);
                            break;
                        }
                }
            }
        }
        ::close (fd);
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
