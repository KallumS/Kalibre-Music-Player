#include "Controller.h"
#include "Sources.h"
#include "TagIO.h"
#include "TitleFormat.h"

#include <random>

namespace kp
{

namespace
{
juce::String str (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }
std::string std8 (const juce::String& s) { return s.toStdString(); }
double nowSeconds() { return juce::Time::getMillisecondCounterHiRes() / 1000.0; }
} // namespace

Controller::Controller (const juce::File& settingsFolder, bool withAudio)
    : settings (settingsFolder), plugins (settings.folder())
{
    loadState();

    scanner.onBatch = [this] (LibraryScanner::Batch b)
    {
        for (auto& t : b.found)
        {
            if (t.added == 0) t.added = juce::Time::currentTimeMillis();
            outside.erase (t.location);
            library.put (std::move (t));
        }
        for (auto& g : b.gone) library.remove (g);
        scanStatus = b.status;
        if (! b.found.empty() || ! b.gone.empty()) { dirty = true; refreshAutoPlaylists(); listeners.call ([] (Listener& l) { l.libraryChanged(); }); }
        if (b.finished)
        {
            scanStatus = {};
            if (addons) addons->fire ("library_changed");
        }
        listeners.call ([] (Listener& l) { l.messageChanged(); });
    };
    watcher.onChange = [this] (const juce::StringArray& folders) { rescan (folders); };

    if (withAudio)
    {
        auto saved = settings.getXml ("audio");
        const auto error = devices.initialise (0, 2, saved.get(), true);
        if (error.isNotEmpty()) say ("The audio device didn't start: " + error + " Choose one in Preferences > Playback.", true);
        devices.addAudioCallback (&player);
    }
    else player.prepareForTest (48000, 512);
    if (! plugins.slots.empty())
        if (auto e = plugins.apply (player); e.isNotEmpty()) say ("Plug-ins: " + e, true);

    juce::StringArray folders;
    for (auto& f : library.folders) folders.add (str (f));
    watcher.watch (folders);
    if (withAudio && ! folders.isEmpty()) rescan();   // what changed while the player was closed

    addons = std::make_unique<addons::Host> (*this);
    if (withAudio) loadAddons();
    startTimerHz (20);
}

Controller::~Controller()
{
    quitting = true;
    stopTimer();
    if (addons) addons->fire ("shutdown");
    saveAll();
    scanner.cancel();
    jobs.cancelAll();
    devices.removeAudioCallback (&player);
    devices.closeAudioDevice();
    addons.reset();
}

//==============================================================================
void Controller::loadState()
{
    library.fromJson (std8 (settings.load ("Library.json")));
    if (! playlists.fromJson (std8 (settings.load ("Playlists.json")))) playlists = Playlists();
    order = (Order) std::clamp (settings.getInt ("order", 0), 0, orderCount - 1);
    player.volumeDb = (float) settings.getDouble ("volume", 0.0);
    replayGain.mode = (rg::Mode) std::clamp (settings.getInt ("rgMode", (int) rg::Mode::track), 0, 3);
    replayGain.preampWith = settings.getDouble ("rgPreampWith", 0);
    replayGain.preampWithout = settings.getDouble ("rgPreampWithout", 0);
    replayGain.preventClipping = settings.getBool ("rgPreventClipping", true);
    replayGain.scanMissing = settings.getBool ("rgScanMissing", true);
    player.crossfadeSeconds = (float) settings.getDouble ("crossfade", 0.0);
    player.eq.enabled = settings.getBool ("eqOn", false);
    player.eq.preampDb = (float) settings.getDouble ("eqPreamp", 0.0);
    for (int b = 0; b < Equalizer::bands; ++b) player.eq.gainDb[(size_t) b] = (float) settings.getDouble ("eq" + juce::String (b), 0.0);
    player.eq.changed();
    if (auto chain = settings.getXml ("plugins")) plugins.fromXml (*chain);
    refreshAutoPlaylists();
}

void Controller::saveAll()
{
    settings.store ("Library.json", str (library.toJson()));
    settings.store ("Playlists.json", str (playlists.toJson()));
    settings.set ("order", (int) order);
    settings.set ("volume", (double) player.volumeDb.load());
    settings.set ("rgMode", (int) replayGain.mode);
    settings.set ("rgPreampWith", replayGain.preampWith);
    settings.set ("rgPreampWithout", replayGain.preampWithout);
    settings.set ("rgPreventClipping", replayGain.preventClipping);
    settings.set ("rgScanMissing", replayGain.scanMissing);
    settings.set ("crossfade", (double) player.crossfadeSeconds.load());
    settings.set ("eqOn", player.eq.enabled.load());
    settings.set ("eqPreamp", (double) player.eq.preampDb.load());
    for (int b = 0; b < Equalizer::bands; ++b) settings.set ("eq" + juce::String (b), (double) player.eq.gainDb[(size_t) b].load());
    plugins.capture (player);
    auto chain = plugins.toXml();
    settings.setXml ("plugins", chain.get());
    if (auto audio = devices.createStateXml()) settings.setXml ("audio", audio.get());
    settings.save();
    dirty = false;
}

//==============================================================================
const Track& Controller::track (const std::string& l)
{
    if (auto* t = library.find (l)) return *t;
    auto it = outside.find (l);
    if (it != outside.end()) return it->second;
    Track t;
    juce::String error;
    if (! tagio::read (l, t, error)) t.location = l;
    return outside.emplace (l, std::move (t)).first->second;
}

void Controller::forget (const std::string& l) { outside.erase (l); }

std::optional<Track> Controller::nowPlayingTrack() const { return player.isPlaying() ? nowTrack : std::nullopt; }

//==============================================================================
// Playback.

double Controller::gainFor (const Track& t)
{
    auto v = rg::read (t);
    if (v.empty())
        if (auto it = measuredGain.find (t.location); it != measuredGain.end()) v.trackGain = it->second;
    const bool albums = order == Order::normal || order == Order::repeatPlaylist || order == Order::shuffleAlbums;
    return rg::linearGain (v, replayGain, replayGain.mode == rg::Mode::smart && albums);
}

void Controller::measureLater (const Track& t, std::int64_t)
{
    if (measuring.count (t.location) || measuredGain.count (t.location)) return;
    measuring.insert (t.location);
    auto loc = t.location;
    auto result = std::make_shared<rg::Result>();
    auto ok = std::make_shared<bool> (false);
    jobs.run ("Measuring loudness", 1,
              [loc, result, ok] (int, Job& job) { juce::String e; *ok = measure (loc, *result, e, &job.cancelled); },
              [this, loc, result, ok] (Job&)
              {
                  measuring.erase (loc);
                  if (! *ok || quitting) return;
                  measuredGain[loc] = result->gain;
                  // Every queued play of it learns its gain.
                  for (auto& [token, placed] : tokens)
                      if (placed.location == loc) player.setGain (token, gainFor (track (loc)));
              }, 1);
}

Player::Item Controller::itemFor (int p, int i)
{
    const auto& loc = playlists.lists[(size_t) p].items[(size_t) i];
    Player::Item item;
    item.location = loc;
    item.token = nextToken++;
    tokens[item.token] = { p, i, loc };
    const auto& t = track (loc);
    item.gain = gainFor (t);
    if (replayGain.mode != rg::Mode::off && replayGain.scanMissing && rg::read (t).empty()) measureLater (t, item.token);
    // Old tokens forgotten (a long session plays many).
    if (tokens.size() > 64)
        for (auto it = tokens.begin(); it != tokens.end();)
            it = it->first < item.token - 32 ? tokens.erase (it) : std::next (it);
    return item;
}

std::vector<std::string> Controller::albumKeys (const Playlist& p)
{
    std::vector<std::string> keys;
    keys.reserve (p.items.size());
    for (auto& l : p.items) keys.push_back (tf::format ("%album artist%|%album%|%discnumber%", track (l)));
    return keys;
}

std::optional<std::pair<int, int>> Controller::after (int p, int i, bool manual)
{
    while (! upNext.empty())
    {
        auto q = upNext.front();
        upNext.pop_front();
        if (q.first >= 0 && q.first < (int) playlists.lists.size() && q.second >= 0 && q.second < (int) playlists.lists[(size_t) q.first].items.size())
            return q;
    }
    if (p < 0 || p >= (int) playlists.lists.size()) return std::nullopt;
    const auto& list = playlists.lists[(size_t) p];
    const int n = navigator.next (order, i, (int) list.items.size(), order == Order::shuffleAlbums ? albumKeys (list) : std::vector<std::string> {}, manual);
    if (n < 0) return std::nullopt;
    return std::make_pair (p, n);
}

void Controller::playItem (int p, int i)
{
    if (p < 0 || p >= (int) playlists.lists.size()) return;
    const auto& list = playlists.lists[(size_t) p];
    if (i < 0 || i >= (int) list.items.size()) return;
    playlists.playing = p;
    player.play (itemFor (p, i));
}

void Controller::play()
{
    if (playing())
    {
        if (paused()) { player.pause (false); if (addons) addons->fire ("resumed"); listeners.call ([] (Listener& l) { l.playbackChanged(); }); }
        return;
    }
    const int p = playlists.active;
    int i = selected.empty() ? 0 : selected.front();
    if (order == Order::shuffleTracks || order == Order::random || order == Order::shuffleAlbums)
        if (selected.empty()) i = navigator.next (order, -1, (int) active().items.size(), order == Order::shuffleAlbums ? albumKeys (active()) : std::vector<std::string> {});
    playItem (p, i);
}

void Controller::pause()
{
    if (! playing() || paused()) return;
    player.pause (true);
    if (addons) addons->fire ("paused");
    listeners.call ([] (Listener& l) { l.playbackChanged(); });
}

void Controller::playPause()
{
    if (playing() && ! paused()) pause();
    else play();
}

void Controller::stop()
{
    if (nowTrack && addons) addons->fire ("track_finished", *nowTrack);
    player.stop();
    nowTrack.reset();
    nowIndex = -1;
    if (addons) addons->fire ("stopped");
    listeners.call ([] (Listener& l) { l.playbackChanged(); });
}

void Controller::next()
{
    if (! playing()) { play(); return; }
    if (auto n = after (playlists.playing, nowIndex, true)) playItem (n->first, n->second);
    else stop();
}

void Controller::previous()
{
    if (! playing()) return;
    if (player.position() > 3.0) { seek (0); return; }
    const int p = playlists.playing;
    if (p < 0 || p >= (int) playlists.lists.size()) return;
    const auto& list = playlists.lists[(size_t) p];
    const int prev = navigator.previous (order, nowIndex, (int) list.items.size(), order == Order::shuffleAlbums ? albumKeys (list) : std::vector<std::string> {});
    if (prev >= 0) playItem (p, prev);
}

void Controller::seek (double s)
{
    player.seek (s);
    if (addons) addons->fire ("seeked", s);
}

void Controller::playRandom()
{
    const auto& list = active();
    if (list.items.empty()) return;
    std::uniform_int_distribution<int> pick (0, (int) list.items.size() - 1);
    static std::mt19937 rng { std::random_device {}() };
    playItem (playlists.active, pick (rng));
}

void Controller::setOrder (Order o)
{
    order = o;
    navigator.forget();
    // What follows the track playing changes with it.
    if (playing())
        if (auto token = player.currentToken(); token && tokens.count (token))
        {
            auto n = after (playlists.playing, nowIndex, false);
            player.setNext (n ? std::optional<Player::Item> (itemFor (n->first, n->second)) : std::nullopt);
        }
    dirty = true;
    listeners.call ([] (Listener& l) { l.playbackChanged(); });
}

void Controller::setVolume (double db)
{
    player.volumeDb = (float) std::clamp (db, -60.0, 0.0);
    if (addons) addons->fire ("volume_changed", db);
    dirty = true;
}

void Controller::addToQueue (int p, int i)
{
    upNext.emplace_back (p, i);
    // The first queued follows the track playing straight away.
    if (playing() && upNext.size() == 1)
    {
        auto n = upNext.front();
        upNext.pop_front();
        player.setNext (itemFor (n.first, n.second));
    }
    listeners.call ([] (Listener& l) { l.playlistsChanged(); });
}

void Controller::setReplayGain (const rg::Settings& s)
{
    replayGain = s;
    dirty = true;
    // The track heard takes the new gain at once.
    if (auto token = player.currentToken(); token && nowTrack) player.setGain (token, gainFor (*nowTrack));
}

void Controller::setCrossfade (double seconds)
{
    player.crossfadeSeconds = (float) std::clamp (seconds, 0.0, 20.0);
    dirty = true;
}

void Controller::handle (const Player::Event& e)
{
    using K = Player::Event::Kind;
    switch (e.kind)
    {
        case K::started:
        {
            if (nowTrack && addons) addons->fire ("track_finished", *nowTrack);
            auto it = tokens.find (e.token);
            if (it != tokens.end())
            {
                playlists.playing = it->second.playlist;
                nowIndex = it->second.index;
                // The playlist may have changed since: the index follows the location.
                const auto& items = playlists.lists[(size_t) std::clamp (playlists.playing, 0, (int) playlists.lists.size() - 1)].items;
                if (nowIndex >= (int) items.size() || items[(size_t) nowIndex] != e.location)
                {
                    auto found = std::find (items.begin(), items.end(), e.location);
                    nowIndex = found == items.end() ? -1 : (int) (found - items.begin());
                }
            }
            nowTrack = track (e.location);
            if (addons) addons->fire ("track_started", *nowTrack);
            listeners.call ([] (Listener& l) { l.playbackChanged(); });
            break;
        }
        case K::wantsNext:
        {
            auto it = tokens.find (e.token);
            if (it == tokens.end()) { player.setNext (std::nullopt); break; }
            if (stopAfterCurrent) { stopAfterCurrent = false; player.setNext (std::nullopt); break; }
            auto n = after (it->second.playlist, it->second.index, false);
            player.setNext (n ? std::optional<Player::Item> (itemFor (n->first, n->second)) : std::nullopt);
            break;
        }
        case K::ended:
            if (nowTrack && addons) addons->fire ("track_finished", *nowTrack);
            nowTrack.reset();
            nowIndex = -1;
            if (addons) addons->fire ("stopped");
            listeners.call ([] (Listener& l) { l.playbackChanged(); });
            break;
        case K::failed:
            say ("Couldn't play " + str (location::fileName (e.location)) + ": " + str (e.error) + ".", true);
            break;
    }
}

void Controller::timerCallback()
{
    for (auto& e : player.poll()) handle (e);
    player.takeRetiredChain();   // a chain the audio thread let go of goes here
    if (addons) addons->tick (nowSeconds() - startedAt);
    if (++saveCountdown >= 20 * 60)
    {
        saveCountdown = 0;
        if (dirty) saveAll();
    }
}

//==============================================================================
// Playlists.

void Controller::notifyPlaylists()
{
    dirty = true;
    listeners.call ([] (Listener& l) { l.playlistsChanged(); });
}

void Controller::choosePlaylist (int i)
{
    if (i < 0 || i >= (int) playlists.lists.size() || i == playlists.active) return;
    playlists.active = i;
    selected.clear();
    notifyPlaylists();
}

int Controller::newPlaylist (const juce::String& name)
{
    const int i = playlists.create (std8 (name));
    playlists.active = i;
    selected.clear();
    notifyPlaylists();
    return i;
}

void Controller::removePlaylist (int i)
{
    if (i == playlists.playing) stop();
    playlists.removeList (i);
    for (auto it = tokens.begin(); it != tokens.end();)
    {
        if (it->second.playlist == i) it = tokens.erase (it);
        else { if (it->second.playlist > i) --it->second.playlist; ++it; }
    }
    upNext.clear();
    selected.clear();
    notifyPlaylists();
}

void Controller::renamePlaylist (int i, const juce::String& name)
{
    if (auto* p = playlists.get (i); p && name.trim().isNotEmpty()) { p->name = std8 (name.trim()); notifyPlaylists(); }
}

void Controller::expand (const juce::File& f, std::vector<std::string>& out, int depth)
{
    if (depth > 32) return;
    if (f.isDirectory())
    {
        auto kids = f.findChildFiles (juce::File::findFilesAndDirectories, false);
        std::sort (kids.begin(), kids.end(), [] (const juce::File& a, const juce::File& b)
                   { return naturalCompare (a.getFileName().toStdString(), b.getFileName().toStdString()) < 0; });
        for (auto& k : kids) if (! k.getFileName().startsWithChar ('.')) expand (k, out, depth + 1);
        return;
    }
    const auto ext = f.getFileExtension().fromFirstOccurrenceOf (".", false, false).toLowerCase().toStdString();
    const auto path = f.getFullPathName().toStdString();
    if (ext == "m3u" || ext == "m3u8") { for (auto& l : readM3u (std8 (f.loadFileAsString()), std8 (f.getParentDirectory().getFullPathName()))) out.push_back (l); return; }
    if (ext == "pls") { for (auto& l : readPls (std8 (f.loadFileAsString()), std8 (f.getParentDirectory().getFullPathName()))) out.push_back (l); return; }
    if (isArchiveExtension (ext))
    {
        juce::String error;
        auto entries = sources::list (f, error);
        std::sort (entries.begin(), entries.end(), [] (auto& a, auto& b) { return naturalCompare (a.path, b.path) < 0; });
        for (auto& e : entries) out.push_back (location::archive (path, e.path));
        return;
    }
    if (isAudioExtension (ext)) out.push_back (path);
}

int Controller::addFiles (int p, const juce::Array<juce::File>& files, int at)
{
    std::vector<std::string> locs;
    for (auto& f : files) expand (f, locs);
    addLocations (p, locs, at);
    return (int) locs.size();
}

void Controller::addLocations (int p, const std::vector<std::string>& locs, int at)
{
    auto* list = playlists.get (p);
    if (! list || locs.empty()) return;
    Playlists::insertItems (*list, at, locs);
    // Tags of files outside the library read now, in the background.
    std::vector<std::string> unread;
    for (auto& l : locs) if (! library.find (l) && ! outside.count (l)) unread.push_back (l);
    if (! unread.empty())
    {
        for (auto& l : unread) { Track t; t.location = l; outside[l] = t; }
        auto results = std::make_shared<std::vector<Track>> (unread.size());
        jobs.run ("Reading tags", (int) unread.size(),
                  [unread, results] (int i, Job&) { juce::String e; tagio::read (unread[(size_t) i], (*results)[(size_t) i], e); (*results)[(size_t) i].location = unread[(size_t) i]; },
                  [this, results] (Job&)
                  {
                      if (quitting) return;
                      for (auto& t : *results) outside[t.location] = t;
                      listeners.call ([] (Listener& l) { l.playlistsChanged(); });
                  });
    }
    playlistEdited (p);
}

void Controller::playlistEdited (int p)
{
    // What plays and what's queued follow their tracks to their new places.
    auto* list = playlists.get (p);
    if (! list) return;
    auto reindex = [&] (int& index, const std::string& loc)
    {
        if (index >= 0 && index < (int) list->items.size() && list->items[(size_t) index] == loc) return;
        auto found = std::find (list->items.begin(), list->items.end(), loc);
        index = found == list->items.end() ? -1 : (int) (found - list->items.begin());
    };
    for (auto& [token, placed] : tokens) if (placed.playlist == p) reindex (placed.index, placed.location);
    if (p == playlists.playing && nowTrack) reindex (nowIndex, nowTrack->location);
    navigator.forget();
    notifyPlaylists();
}

void Controller::removeItems (int p, const std::vector<int>& indices)
{
    if (auto* list = playlists.get (p)) { Playlists::removeItems (*list, indices); if (p == playlists.active) selected.clear(); playlistEdited (p); }
}

void Controller::moveItems (int p, const std::vector<int>& indices, int to)
{
    if (auto* list = playlists.get (p))
    {
        auto now = Playlists::moveItems (*list, indices, to);
        if (p == playlists.active) selected = now;
        playlistEdited (p);
    }
}

void Controller::sortPlaylist (int p, const std::string& pattern, bool descending)
{
    auto* list = playlists.get (p);
    if (! list) return;
    std::vector<const Track*> ts;
    for (auto& l : list->items) ts.push_back (&track (l));
    sortByPattern (ts, pattern, descending);
    for (size_t k = 0; k < ts.size(); ++k) list->items[k] = ts[k]->location;
    playlistEdited (p);
}

void Controller::shufflePlaylist (int p)
{
    if (auto* list = playlists.get (p))
    {
        static std::mt19937 rng { std::random_device {}() };
        std::shuffle (list->items.begin(), list->items.end(), rng);
        playlistEdited (p);
    }
}

void Controller::clearPlaylist (int p)
{
    if (auto* list = playlists.get (p)) { list->items.clear(); playlistEdited (p); }
}

void Controller::refreshAutoPlaylists()
{
    for (size_t k = 0; k < playlists.lists.size(); ++k)
    {
        auto& p = playlists.lists[k];
        if (! p.isAuto()) continue;
        std::vector<std::string> items;
        for (auto i : library.search (Query (p.query))) items.push_back (library.tracks()[i].location);
        if (items != p.items) { p.items = std::move (items); playlistEdited ((int) k); }
    }
}

int Controller::librarySelectionPlaylist()
{
    for (size_t k = 0; k < playlists.lists.size(); ++k)
        if (playlists.lists[k].name == "Library Selection") return (int) k;
    const int i = playlists.create ("Library Selection");
    notifyPlaylists();
    return i;
}

bool Controller::exportPlaylist (int p, const juce::File& f)
{
    auto* list = playlists.get (p);
    if (! list) return false;
    std::vector<std::string> titles;
    std::vector<double> lengths;
    for (auto& l : list->items) { auto& t = track (l); titles.push_back (tf::format ("[%artist% - ]%title%", t)); lengths.push_back (t.length); }
    return f.replaceWithText (str (writeM3u (list->items, titles, lengths)), false, false, "\n");
}

bool Controller::importPlaylist (const juce::File& f)
{
    const int p = newPlaylist (f.getFileNameWithoutExtension());
    return addFiles (p, { f }) > 0;
}

void Controller::setSelection (std::vector<int> s)
{
    selected = std::move (s);
    if (addons) addons->fire ("selection_changed");
    listeners.call ([] (Listener& l) { l.selectionChanged(); });
}

std::vector<std::string> Controller::selectedLocations()
{
    std::vector<std::string> out;
    const auto& items = active().items;
    for (int i : selected) if (i >= 0 && i < (int) items.size()) out.push_back (items[(size_t) i]);
    return out;
}

//==============================================================================
// The library.

void Controller::addFolder (const juce::File& f)
{
    const auto path = std8 (f.getFullPathName());
    if (std::find (library.folders.begin(), library.folders.end(), path) != library.folders.end()) { rescan ({ f.getFullPathName() }); return; }
    library.folders.push_back (path);
    juce::StringArray all;
    for (auto& x : library.folders) all.add (str (x));
    watcher.watch (all);
    dirty = true;
    rescan ({ f.getFullPathName() });
    listeners.call ([] (Listener& l) { l.libraryChanged(); });
}

void Controller::removeFolder (const juce::String& folder)
{
    auto& fs = library.folders;
    fs.erase (std::remove (fs.begin(), fs.end(), std8 (folder)), fs.end());
    library.removeUnder (std8 (folder));
    juce::StringArray all;
    for (auto& x : library.folders) all.add (str (x));
    watcher.watch (all);
    dirty = true;
    refreshAutoPlaylists();
    listeners.call ([] (Listener& l) { l.libraryChanged(); });
}

void Controller::rescan (const juce::StringArray& foldersIn)
{
    juce::StringArray folders = foldersIn;
    if (folders.isEmpty()) for (auto& f : library.folders) folders.add (str (f));
    if (folders.isEmpty()) return;
    // A scan already going looks at every folder it was given and these too.
    std::unordered_map<std::string, LibraryScanner::Known> known;
    for (auto& t : library.tracks()) known[t.location] = { t.modified, t.fileSize };
    scanStatus = juce::String::fromUTF8 ("Looking through the library\xe2\x80\xa6");
    scanner.scan (folders, std::move (known));
    listeners.call ([] (Listener& l) { l.messageChanged(); });
}

void Controller::tracksChanged (const std::vector<std::string>& locs)
{
    for (auto& l : locs)
    {
        Track t;
        juce::String error;
        if (! tagio::read (l, t, error)) continue;
        if (auto* old = library.find (l)) { t.added = old->added; library.put (std::move (t)); }
        else outside[l] = std::move (t);
        if (nowTrack && nowTrack->location == l) nowTrack = track (l);
        measuredGain.erase (l);
    }
    dirty = true;
    refreshAutoPlaylists();
    listeners.call ([] (Listener& l) { l.libraryChanged(); });
    listeners.call ([] (Listener& l) { l.playlistsChanged(); });
}

void Controller::fileMoved (const std::string& from, const std::string& to)
{
    library.move (from, to);
    if (auto it = outside.find (from); it != outside.end()) { auto t = it->second; t.location = to; outside.erase (it); outside[to] = t; }
    for (auto& p : playlists.lists) for (auto& i : p.items) if (i == from) i = to;
    for (auto& [token, placed] : tokens) if (placed.location == from) placed.location = to;
    if (nowTrack && nowTrack->location == from) nowTrack->location = to;
    dirty = true;
}

//==============================================================================
void Controller::say (const juce::String& m, bool important)
{
    messageText = m;
    messageImportant = important;
    listeners.call ([] (Listener& l) { l.messageChanged(); });
}

std::vector<Track> Controller::query (const std::string& q)
{
    std::vector<Track> out;
    for (auto i : library.search (Query (q)))
    {
        out.push_back (library.tracks()[i]);
        if (out.size() >= 10000) break;
    }
    return out;
}

std::vector<std::string> Controller::playlistNames()
{
    std::vector<std::string> out;
    for (auto& p : playlists.lists) out.push_back (p.name);
    return out;
}

std::vector<Track> Controller::playlistItems (int i)
{
    std::vector<Track> out;
    if (auto* p = playlists.get (i)) for (auto& l : p->items) out.push_back (track (l));
    return out;
}

void Controller::queue (const std::string& l)
{
    auto& items = active().items;
    auto found = std::find (items.begin(), items.end(), l);
    int index;
    if (found == items.end()) { addLocations (playlists.active, { l }); index = (int) active().items.size() - 1; }
    else index = (int) (found - items.begin());
    addToQueue (playlists.active, index);
}

std::vector<Track> Controller::selection()
{
    std::vector<Track> out;
    for (auto& l : selectedLocations()) out.push_back (track (l));
    return out;
}

std::optional<Track> Controller::readTrack (const std::string& l)
{
    const auto& t = track (l);
    if (t.length <= 0 && t.tags.empty() && ! sources::fileOf (l).existsAsFile()) return std::nullopt;
    return t;
}

bool Controller::writeTags (const std::string& l, const Tags& changes)
{
    Tags tags = track (l).tags;
    for (auto& [k, v] : changes)
    {
        if (v.empty()) tags.erase (k);
        else tags[k] = v;
    }
    juce::String error;
    if (! tagio::write (l, tags, error)) { say ("Tags not written to " + str (location::fileName (l)) + ": " + error + "."); return false; }
    tracksChanged ({ l });
    return true;
}

void Controller::print (const std::string& addon, const std::string& text)
{
    addonConsole.add (str ("[" + addon + "] " + text));
    while (addonConsole.size() > 500) addonConsole.remove (0);
    listeners.call ([] (Listener& l) { l.messageChanged(); });
}

std::string Controller::settingsGet (const std::string& addon, const std::string& key)
{
    return std8 (settings.get (str ("addon." + addon + "." + key)));
}

void Controller::settingsSet (const std::string& addon, const std::string& key, const std::string& value)
{
    settings.set (str ("addon." + addon + "." + key), str (value));
}

void Controller::loadAddons()
{
    addons->unloadAll();
    const auto dir = settings.addonsFolder();
    dir.createDirectory();
    for (auto& f : dir.findChildFiles (juce::File::findFilesAndDirectories, false))
    {
        juce::File code;
        if (f.hasFileExtension ("lua")) code = f;
        else if (f.isDirectory() && f.getChildFile ("main.lua").existsAsFile()) code = f.getChildFile ("main.lua");
        else continue;
        const auto id = std8 (f.getFileName());
        if (auto* info = addons->info (id)) info->enabled = settings.getBool ("addon." + f.getFileName() + ".enabled", true);
        else if (! settings.getBool ("addon." + f.getFileName() + ".enabled", true))
        {
            // Known but turned off: listed, not run.
            addons->load (id, "");
            addons->info (id)->enabled = false;
            addons->unload (id);
            continue;
        }
        addons->load (id, std8 (code.loadFileAsString()));
    }
    if (addons) addons->fire ("startup");
    listeners.call ([] (Listener& l) { l.messageChanged(); });
}

juce::String Controller::importAddon (const juce::File& f)
{
    const auto dir = settings.addonsFolder();
    dir.createDirectory();
    if (f.hasFileExtension ("lua"))
    {
        if (! f.copyFileTo (dir.getChildFile (f.getFileName()))) return "it couldn't be copied";
    }
    else if (f.isDirectory())
    {
        if (! f.getChildFile ("main.lua").existsAsFile()) return "a folder add-on needs a main.lua";
        if (! f.copyDirectoryTo (dir.getChildFile (f.getFileName()))) return "it couldn't be copied";
    }
    else if (f.hasFileExtension ("zip"))
    {
        juce::ZipFile zip (f);
        const auto target = dir.getChildFile (f.getFileNameWithoutExtension());
        target.deleteRecursively();
        if (zip.uncompressTo (target).failed()) return "the zip couldn't be opened";
        // A zip of a folder: that folder's main.lua.
        if (! target.getChildFile ("main.lua").existsAsFile())
        {
            auto inner = target.findChildFiles (juce::File::findDirectories, false);
            if (inner.size() == 1 && inner[0].getChildFile ("main.lua").existsAsFile())
            {
                auto tmp = dir.getChildFile (".import-" + juce::String (juce::Random::getSystemRandom().nextInt()));
                inner[0].moveFileTo (tmp);
                target.deleteRecursively();
                tmp.moveFileTo (target);
            }
            else { target.deleteRecursively(); return "the zip has no main.lua"; }
        }
    }
    else return "an add-on is a .lua file, a folder with main.lua, or a .zip of one";
    loadAddons();
    return {};
}

} // namespace kp
