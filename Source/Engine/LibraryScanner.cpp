#include "LibraryScanner.h"
#include "Sources.h"
#include "TagIO.h"

#include <set>

namespace kp
{

LibraryScanner::LibraryScanner() : juce::Thread ("Kalibre Player library") {}
LibraryScanner::~LibraryScanner() { cancel(); }

void LibraryScanner::cancel()
{
    signalThreadShouldExit();
    stopThread (10000);
}

void LibraryScanner::scan (const juce::StringArray& f, std::unordered_map<std::string, Known> k)
{
    cancel();
    folders = f;
    known = std::move (k);
    startThread (juce::Thread::Priority::low);
}

void LibraryScanner::deliver (Batch b)
{
    juce::MessageManager::callAsync ([this, b = std::move (b)]() mutable { if (onBatch) onBatch (std::move (b)); });
}

void LibraryScanner::run()
{
    std::set<std::string> seen;
    // The files first, so progress can be told.
    std::vector<juce::File> files;
    for (auto& folder : folders)
    {
        const juce::File dir (folder);
        if (! dir.isDirectory()) continue;
        for (const auto& entry : juce::RangedDirectoryIterator (dir, true, "*", juce::File::findFiles, juce::File::FollowSymlinks::noCycles))
        {
            if (threadShouldExit()) return;
            const auto f = entry.getFile();
            const auto ext = f.getFileExtension().fromFirstOccurrenceOf (".", false, false).toStdString();
            if (f.getFileName().startsWithChar ('.')) continue;
            if (isAudioExtension (ext) || isArchiveExtension (ext)) files.push_back (f);
        }
    }

    Batch batch;
    auto send = [&] (bool force)
    {
        if (force || batch.found.size() >= 200)
        {
            deliver (std::move (batch));
            batch = {};
        }
    };
    int index = 0;
    for (auto& f : files)
    {
        if (threadShouldExit()) return;
        ++index;
        const auto path = f.getFullPathName().toStdString();
        const auto mod = f.getLastModificationTime().toMilliseconds();
        const auto size = f.getSize();
        const auto ext = f.getFileExtension().fromFirstOccurrenceOf (".", false, false).toStdString();
        if (index % 50 == 0) { batch.status = "Reading " + juce::String (index) + " of " + juce::String ((int) files.size()) + juce::String::fromUTF8 ("\xe2\x80\xa6"); send (true); }

        if (isArchiveExtension (ext))
        {
            juce::String error;
            auto entries = sources::list (f, error);
            for (auto& e : entries)
            {
                const auto loc = location::archive (path, e.path);
                seen.insert (loc);
                auto it = known.find (loc);
                if (it != known.end() && it->second.modified == mod && it->second.size == size) continue;
                Track t;
                if (tagio::read (loc, t, error)) { batch.found.push_back (std::move (t)); send (false); }
            }
            continue;
        }
        seen.insert (path);
        auto it = known.find (path);
        if (it != known.end() && it->second.modified == mod && it->second.size == size) continue;
        Track t;
        juce::String error;
        if (tagio::read (path, t, error)) { batch.found.push_back (std::move (t)); send (false); }
    }
    // What the library has in these folders that's no longer there.
    for (auto& [loc, k] : known)
    {
        if (seen.count (loc)) continue;
        const auto container = juce::String (location::container (loc));
        for (auto& folder : folders)
            if (container.startsWith (folder.endsWithChar ('/') ? folder : folder + "/")) { batch.gone.push_back (loc); break; }
    }
    batch.finished = true;
    batch.status = {};
    send (true);
}

} // namespace kp
