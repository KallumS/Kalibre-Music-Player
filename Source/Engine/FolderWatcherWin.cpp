// FolderWatcher on Windows: ReadDirectoryChangesW on each folder and those
// inside it, told within seconds of a change.
#include "FolderWatcher.h"

#include <windows.h>

#include <atomic>
#include <thread>

namespace kp
{
juce::StringArray foldersHolding (const juce::StringArray& watched, const juce::StringArray& changed);

struct FolderWatcher::Impl
{
    FolderWatcher& owner;
    juce::StringArray folders;
    std::vector<std::thread> threads;
    std::vector<HANDLE> stops;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    explicit Impl (FolderWatcher& o) : owner (o) {}
    ~Impl() { *alive = false; stop(); }

    void stop()
    {
        for (auto h : stops) SetEvent (h);
        for (auto& t : threads) if (t.joinable()) t.join();
        for (auto h : stops) CloseHandle (h);
        threads.clear();
        stops.clear();
    }

    void start()
    {
        stop();
        for (auto& f : folders)
        {
            HANDLE stopEvent = CreateEventW (nullptr, TRUE, FALSE, nullptr);
            stops.push_back (stopEvent);
            threads.emplace_back ([this, folder = f, stopEvent, a = alive] { watch (folder, stopEvent, a); });
        }
    }

    void watch (juce::String folder, HANDLE stopEvent, std::shared_ptr<std::atomic<bool>> a)
    {
        HANDLE dir = CreateFileW (folder.toWideCharPointer(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
        if (dir == INVALID_HANDLE_VALUE) return;
        OVERLAPPED ov {};
        ov.hEvent = CreateEventW (nullptr, TRUE, FALSE, nullptr);
        alignas (DWORD) char buffer[16384];
        const DWORD what = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE;
        for (;;)
        {
            ResetEvent (ov.hEvent);
            if (! ReadDirectoryChangesW (dir, buffer, sizeof buffer, TRUE, what, nullptr, &ov, nullptr)) break;
            HANDLE waits[] = { ov.hEvent, stopEvent };
            if (WaitForMultipleObjects (2, waits, FALSE, INFINITE) != WAIT_OBJECT_0)
            {
                DWORD got = 0;
                CancelIoEx (dir, &ov);
                GetOverlappedResult (dir, &ov, &got, TRUE);
                break;
            }
            // Changes come in bursts (a copy writes many times): told once, two seconds on.
            // What changes meanwhile is found by the look through the folder that follows.
            if (WaitForSingleObject (stopEvent, 2000) == WAIT_OBJECT_0) break;
            juce::MessageManager::callAsync ([this, a, folder]
            {
                if (*a && owner.onChange) owner.onChange (juce::StringArray { folder });
            });
        }
        CloseHandle (ov.hEvent);
        CloseHandle (dir);
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
