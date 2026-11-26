# 25-26 November 2026: from Kalibre to a music player, on three systems

The first session: Kalibre Player made from nothing (the repository held only
its licence) to 0.3.1 on the Mac, Windows and Linux. This is the
conversation's record, so the next session can start from here.

## What the musician asked, in their words

1. > You've been making a DAW for me, Kalibre, and it's just had version 1.10
   > release. I wanted to adapt it into a music player, similar to something
   > like VLC Media Player or Foobar 2000. Kalibre should already have most of
   > what you need for a music player app. At the moment let's focus on
   > releasing on MacOS Silicon.

   The features list came in a second message:

   > **Core Playback & Format Support**
   > - Wide native format support: Plays MP1, MP2, MP3, Musepack (MPC), AAC,
   >   WMA, Ogg Vorbis, FLAC/Ogg FLAC, ALAC, WavPack, WAV, AIFF, AU, SND, Audio
   >   CD, Speex, and Opus out of the box.
   > - Archive reading: Reads audio directly inside ZIP, GZIP, and RAR archives.
   > - Gapless playback & ReplayGain: Plays seamless tracks without gaps and
   >   calculates volume adjustments on the fly.
   > - Audio DSP & effects: Includes built-in DSP options like equalization and
   >   crossfade, and supports VSTs.
   > - Built in metering tools such as oscilloscope and goniometer
   >
   > **Library, Tagging & File Tools**
   > - Media library & queries: Watches folders automatically and lets you
   >   filter your collection using search queries or browse by metadata fields
   >   like artist, album, year, and genre.
   > - Batch tagging & renaming: Edits metadata tags and renames or moves files
   >   using title-formatting patterns.
   > - CD ripping & format conversion: Rips Audio CDs and converts tracks
   >   between formats
   >
   > **Customization & Extensibility**
   > - Customizable UI & shortcuts: Lets you rearrange layout elements, switch
   >   themes, and set custom keyboard shortcuts.
   > - Ability to import add ons scripted in Lua

2. > Thanks for building. Working pretty well so far, it would be cool if in
   > View there was an option to switch to a mini player that takes up as
   > little space on the screen as possible

3. > Thanks. Could you port Kalibre Music Player to Windows and Linux?

4. > Please update the chat history to the repo, create a user guide and update
   > the docs on the repo as I'm about to clear the chat

## What was done

### 0.1.0: the player (Mac)

- **A new app, not a cut-down DAW.** Kalibre (KallumS/Kalibre, 1.1.0) was
  read first. Its arrangement, piano roll and devices don't belong in a
  player, so Kalibre Player is a new codebase that takes from Kalibre what
  fits: JUCE 9.0.5, `Theme.h`/`Theme.cpp` (the twelve palettes, the 7:1 rule),
  `Platform.h` (the words each system uses), the BS.1770 loudness meter,
  vendored Lua 5.4 built as C++, `Check.h` (the test runner), FilePanels, the
  render tool and the CI's shape (decision 0001).
- **The core** (`Source/Core`, no JUCE, 36 tests): tracks and locations,
  foobar2000's title formatting (0005) and query syntax (0006), the library,
  playlists and play order (0007), ReplayGain 2.0 (0008), audio-CD disc IDs
  and MusicBrainz answers (0010; the disc ID checked against MusicBrainz's
  own example), batch tagging, the layout tree (0011), the Lua add-on host
  (0012).
- **The engine** (`Source/Engine`): FFmpeg built small and static by
  `tools/build-ffmpeg.sh`, with LAME and libopus for the converter (0003);
  libarchive for ZIP/RAR/7-Zip/tar/gzip (0004); TagLib (0009); the gapless
  player, a decoding thread feeding a FIFO with segments (0014); equaliser,
  plug-in chain and volume on the audio thread (0013); the library scanner and
  FSEvents folder watcher; jobs on every core (0010).
- **The window** (`Source/App`): the controller, the playlist with
  title-formatting columns, the library browser (search and facets, filling a
  Library Selection playlist), visualisers (0016), transport, equaliser, the
  editable layout with five presets, tag editor, guess tags, rename/move/copy,
  converter, CD ripping, ReplayGain scanning, Preferences, add-ons, help
  pages, every command with changeable keys (0015), media keys (0017).
- **Four example add-ons** in `Addons/`.
- **CI** builds the Mac's .dmg and tests on Linux and macOS.

### 0.1.1: two Mac-only races, found by CI

- Crossfade could begin late when the playlist answered "what's next" after
  the decoder had passed the point where it should start; the decoder now
  waits there (the buffer covers it). A test answering late on purpose fails
  without the fix.
- The player said "stopped" for a moment after Play; it's playing from the
  moment it's asked.

The musician merged this as pull request #1.

### 0.2.0: the mini player (decision 0018)

View > Mini Player (Command-Shift-M): the same window, without its title
bar, as a 440 x 68 point strip with the cover, title and artist, seek bar
and time, Previous/Play/Next and a button back; in front of other windows
(right-click: Keep in Front); dragged by its ground; reopens as it was left.
The command was added at the end of the list, as saved shortcuts name
commands by number.

### 0.3.0: Windows and Linux (decisions 0019, 0020)

- **Windows**: MSVC, the runtime inside the .exe; FFmpeg 9.0.2 with LAME and
  libopus, and zlib, from vcpkg pinned at release 2026.10.16
  (x64-windows-static), cached in CI. Folders watched with
  ReadDirectoryChangesW; media keys as system-wide hot keys. A .zip.
- **Linux**: FFmpeg from the script, as the tests always had; libcurl loaded
  when first used (MusicBrainz); folders watched with inotify. A .tar.gz with
  `install.sh` (no administrator) and a desktop entry.
- **CDs from the drive** on Windows and Linux: the TOC and raw sectors by
  ioctl, a track opened as a WAV stream (`cdda:D:|3`), so playing, seeking,
  ripping need nothing new.
- **FFmpeg 9.0.2 on all three** (vcpkg's version).
- Words on screen per system (Ctrl+E, Show in File Explorer).

Bugs found on the way, each with a test that failed first:

- Jobs and the library scanner could call back into a controller already
  gone (a crash when quitting mid-measurement). Both now drop late callbacks.
- On the Mac, FSEvents names a folder by its real path (`/private/var` for
  `/var`, a link's target): a library folder under a link wasn't noticed
  changing. The watcher compares real paths.
- libarchive's configure stopped on Windows (no regex library); its regex
  use is turned off (it's only for its own tools).
- The visual-studio setup step sets `VCPKG_ROOT` to its own vcpkg: ours is
  `KP_VCPKG`. The vcpkg cache is saved straight after building it.

### 0.3.1 (this last part): docs, and Preferences' ground

- `docs/USER-GUIDE.md` rewritten whole, for all three systems, with fresh
  pictures of the window and, newly, of the tag editor, converter, Move
  Files, Preferences and add-ons (KPRender draws those windows too now).
- This record; README, FEATURES, ARCHITECTURE, CLAUDE.md brought up to date.
- Preferences painted its own ground behind the tabs (it showed white
  where the window's didn't reach).

## Where it stands

- **All three systems build and pass every test on CI** (36 core tests, 29
  app tests). Last green run before the docs:
  https://github.com/KallumS/Kalibre-Music-Player/actions/runs/45721014375
- **Branch** `claude/sleepy-pasteur-y7ag9n` holds 0.2.0, 0.3.0 and 0.3.1;
  `main` holds 0.1.1 (pull request #1). **A new pull request is needed** to
  bring the mini player and Windows/Linux to `main`; then a tag `v0.3.1` makes
  a release with the .dmg, .zip and .tar.gz.
- **Not yet tried by hand** (no test can reach them): a real audio CD on any
  system; AU/VST3 plug-in windows; the media keys; folder watching on Windows;
  the mini player's window behaviour on Windows and Linux.
- **Not done**: media keys on Linux (MPRIS over D-Bus); signing (Apple
  Developer ID, Windows code signing); crossfade on manual skips; writing tags
  inside archives; AccurateRip when ripping.

## Things to know next time

- The musician isn't a developer: plain words, pictures (KPRender), and every
  change reaching them as CI's downloads.
- The first Windows CI run of a new vcpkg pin takes about 15 minutes more
  (FFmpeg); after that it's cached.
- The Mac's and Windows' own files only compile on CI: push, then read the run.
- Locally (Linux): FFmpeg 9 was built into `/home/user/deps/ffmpeg9` and used
  with `-DKP_FFMPEG_DIR`; a fresh container builds it again (3 minutes).
