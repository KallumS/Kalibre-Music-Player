#include "Jobs.h"
#include "Decoder.h"
#include "Sources.h"
#include "TagIO.h"
#include "TitleFormat.h"

#include <cmath>

namespace kp
{

juce::File convertTarget (const Track& t, const ConvertOptions& o)
{
    const auto* f = encodeFormat (o.encode.format);
    auto rel = tf::formatPath (o.pattern, t);
    if (rel.empty()) rel = tf::safeFileName (tf::format ("%filename%", t));
    return o.folder.getChildFile (juce::String::fromUTF8 (rel.c_str()) + "." + (f ? f->extension : std::string ("flac")));
}

bool convert (const Track& t, const ConvertOptions& o, juce::File& out, juce::String& error, const std::atomic<bool>* cancel)
{
    out = convertTarget (t, o);
    if (out.existsAsFile() && ! o.overwrite) { error = out.getFileName() + " is there already"; return false; }
    if (! out.getParentDirectory().createDirectory()) { error = "the folder couldn't be made"; return false; }
    auto in = sources::open (t.location, error);
    if (! in) return false;
    auto d = Decoder::open (std::move (in), juce::String::fromUTF8 (location::fileName (t.location).c_str()), error);
    if (! d) return false;
    const int channels = std::clamp (d->info().channels, 1, 8);
    const int rate = d->info().sampleRate;
    d->setOutput (rate, channels);
    // Written beside, then put in place, so a stopped conversion leaves nothing half made.
    const auto temp = out.getSiblingFile ("." + out.getFileNameWithoutExtension() + ".part." + out.getFileExtension().substring (1));
    auto enc = Encoder::create (temp, o.encode, rate, channels, error);
    if (! enc) return false;
    const double gain = o.applyReplayGain ? rg::linearGain (rg::read (t), o.replayGain, false) : 1.0;
    const int block = 4096;
    std::vector<float> inter ((size_t) block * (size_t) channels);
    std::vector<std::vector<float>> planar ((size_t) channels, std::vector<float> ((size_t) block));
    std::vector<const float*> ptrs ((size_t) channels);
    for (int c = 0; c < channels; ++c) ptrs[(size_t) c] = planar[(size_t) c].data();
    for (;;)
    {
        if (cancel && cancel->load()) { enc.reset(); temp.deleteFile(); error = "stopped"; return false; }
        const int got = d->read (inter.data(), block);
        if (got <= 0) break;
        for (int i = 0; i < got; ++i)
            for (int c = 0; c < channels; ++c)
                planar[(size_t) c][(size_t) i] = (float) (inter[(size_t) i * (size_t) channels + (size_t) c] * gain);
        if (! enc->write (ptrs.data(), got)) { enc.reset(); temp.deleteFile(); error = "the file couldn't be written"; return false; }
    }
    if (! enc->finish()) { enc.reset(); temp.deleteFile(); error = "the file couldn't be finished"; return false; }
    enc.reset();

    // Tags: the track's, without ReplayGain if it's in the sound now.
    Tags tags = t.tags;
    if (o.applyReplayGain)
        for (auto it = tags.begin(); it != tags.end();)
            it = it->first.rfind ("replaygain_", 0) == 0 || it->first.rfind ("r128_", 0) == 0 ? tags.erase (it) : std::next (it);
    const auto tempLoc = temp.getFullPathName().toStdString();
    juce::String tagError;
    if (! tags.empty()) tagio::write (tempLoc, tags, tagError);
    if (o.copyCover)
    {
        juce::String mime;
        auto cover = tagio::embeddedCover (t.location, mime);
        if (! cover.isEmpty()) tagio::writeCover (tempLoc, cover, mime);
    }
    if (out.existsAsFile()) out.deleteFile();
    if (! temp.moveFileTo (out)) { temp.deleteFile(); error = "the file couldn't be put in place"; return false; }
    return true;
}

bool measure (const std::string& location, rg::Result& result, juce::String& error, const std::atomic<bool>* cancel)
{
    auto in = sources::open (location, error);
    if (! in) return false;
    auto d = Decoder::open (std::move (in), juce::String::fromUTF8 (location::fileName (location).c_str()), error);
    if (! d) return false;
    const int channels = std::clamp (d->info().channels, 1, 8);
    d->setOutput (d->info().sampleRate, channels);
    rg::Analyzer a;
    a.prepare (d->info().sampleRate, channels);
    const int block = 8192;
    std::vector<float> inter ((size_t) block * (size_t) channels);
    std::vector<std::vector<float>> planar ((size_t) channels, std::vector<float> ((size_t) block));
    std::vector<const float*> ptrs ((size_t) channels);
    for (int c = 0; c < channels; ++c) ptrs[(size_t) c] = planar[(size_t) c].data();
    for (;;)
    {
        if (cancel && cancel->load()) { error = "stopped"; return false; }
        const int got = d->read (inter.data(), block);
        if (got <= 0) break;
        for (int i = 0; i < got; ++i)
            for (int c = 0; c < channels; ++c) planar[(size_t) c][(size_t) i] = inter[(size_t) i * (size_t) channels + (size_t) c];
        a.process (ptrs.data(), channels, got);
    }
    result = a.finish();
    return true;
}

bool moveFile (const juce::File& from, const juce::File& to, bool copy, juce::String& error)
{
    if (from == to) return true;
    if (! from.existsAsFile()) { error = from.getFileName() + " isn't there"; return false; }
    // A change of case only, on a disk that ignores case, is a rename through another name.
    const bool sameButCase = from.getFullPathName().equalsIgnoreCase (to.getFullPathName());
    if (to.exists() && ! sameButCase) { error = to.getFileName() + " is there already"; return false; }
    if (! to.getParentDirectory().createDirectory()) { error = "the folder couldn't be made"; return false; }
    if (copy) { if (! from.copyFileTo (to)) { error = "it couldn't be copied"; return false; } return true; }
    if (sameButCase)
    {
        auto between = to.getSiblingFile (".kp-rename-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
        if (! from.moveFileTo (between) || ! between.moveFileTo (to)) { error = "it couldn't be renamed"; return false; }
        return true;
    }
    if (! from.moveFileTo (to)) { error = "it couldn't be moved"; return false; }
    // A folder left empty by the move goes too.
    auto dir = from.getParentDirectory();
    for (int k = 0; k < 3 && dir.isDirectory() && dir.getNumberOfChildFiles (juce::File::findFilesAndDirectories, "*") == 0; ++k)
    {
        auto up = dir.getParentDirectory();
        dir.deleteFile();
        dir = up;
    }
    return true;
}

//==============================================================================
Jobs::Jobs() : pool (juce::ThreadPoolOptions().withThreadName ("Kalibre Player jobs")
                         .withNumberOfThreads (std::max (2, juce::SystemStats::getNumCpus() - 1))
                         .withDesiredThreadPriority (juce::Thread::Priority::low)) {}

Jobs::~Jobs() { cancelAll(); pool.removeAllJobs (true, 20000); }

void Jobs::cancelAll()
{
    std::lock_guard<std::mutex> g (lock);
    for (auto& j : jobs) j->cancelled = true;
}

std::vector<std::shared_ptr<Job>> Jobs::running()
{
    std::lock_guard<std::mutex> g (lock);
    jobs.erase (std::remove_if (jobs.begin(), jobs.end(), [] (auto& j) { return j->finished.load(); }), jobs.end());
    return jobs;
}

std::shared_ptr<Job> Jobs::run (const juce::String& title, int count, std::function<void (int, Job&)> work, std::function<void (Job&)> finish, int threads)
{
    auto job = std::make_shared<Job>();
    job->title = title;
    job->total = count;
    {
        std::lock_guard<std::mutex> g (lock);
        jobs.push_back (job);
    }
    if (count == 0)
    {
        job->finished = true;
        juce::MessageManager::callAsync ([job, finish] { if (finish) finish (*job); });
        return job;
    }
    // Items are taken in turn by as many workers as asked (all the cores by default).
    auto next = std::make_shared<std::atomic<int>> (0);
    auto workersLeft = std::make_shared<std::atomic<int>> (0);
    const int workers = std::max (1, std::min (count, threads > 0 ? threads : pool.getNumThreads()));
    *workersLeft = workers;
    for (int w = 0; w < workers; ++w)
        pool.addJob ([job, next, workersLeft, work, finish, count]
        {
            for (;;)
            {
                const int i = (*next)++;
                if (i >= count || job->cancelled) break;
                work (i, *job);
                ++job->done;
            }
            if (--(*workersLeft) == 0)
                juce::MessageManager::callAsync ([job, finish] { job->finished = true; if (finish) finish (*job); });
        });
    return job;
}

} // namespace kp
