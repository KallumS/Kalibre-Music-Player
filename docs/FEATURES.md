# Features

What was asked for, and where it stands. ✅ done and tested; 🟡 done, tested
only on the Mac by hand (no test can reach it from Linux); ⬜ not yet.

## Core playback and formats

| | |
|---|---|
| ✅ MP1, MP2, MP3, Musepack, AAC, WMA, Ogg Vorbis, FLAC, Ogg FLAC, ALAC, WavPack, WAV, AIFF, AU, SND, Speex, Opus | FFmpeg's decoders, built in (`tools/build-ffmpeg.sh`). Tests: every decoder is present; each format the converter writes decodes back (lossless exactly); AU/SND written by hand play. Also Monkey's Audio, TTA, TAK, AC-3, E-AC-3, DTS, TrueHD, DSD, MKA, CAF. |
| 🟡 Audio CD | macOS mounts a CD as AIFF files with a .TOC.plist; Windows and Linux are read from the drive (raw sectors as a WAV stream, tested with a stand-in drive). File > Play Audio CD. The TOC parsers and MusicBrainz disc ID are tested (MusicBrainz's own example). |
| ✅ ZIP, GZIP, RAR | libarchive (also 7-Zip and tar). Tests: a ZIP's tracks listed, tagged, decoded; a .gz; libarchive's own RAR test file. Archives are read-only (tags can't be written inside them). |
| ✅ Gapless | The next track is opened ahead and joined sample to sample; FFmpeg cuts encoder padding. Test: two ramps join with no jump. |
| ✅ ReplayGain | Track, album, smart; preamps; clipping prevention; tracks without tags measured on the fly (EBU R128, -18 LUFS). Scanner writes tags by track or album. Tests: R128 against EBU's sine, album gating, tag reading, gains applied in playback. |
| ✅ Equaliser | Ten bands, presets, preamp. Test: at 0 dB it leaves the sound exactly alone. |
| ✅ Crossfade | Equal-power, 0–12 s, on automatic track changes. Test: two 1 s tracks with 0.5 s crossfade last 1.5 s. Manual skips cut (with a 20 ms fade on pause). |
| 🟡 VST3 and AU | JUCE's hosting, scanned in Preferences > Plug-ins (a crashing plug-in is skipped next scan), chained after the EQ, editors in their own windows, settings kept. |
| ✅ Oscilloscope, goniometer | Also spectrum, peak/RMS meter and R128 loudness. Seen in the renders. |

## Library, tagging and files

| | |
|---|---|
| ✅ Watched folders | FSEvents on the Mac (🟡), a timer elsewhere; rescans read only what changed. Test: a folder added is scanned into the library. |
| ✅ Queries and browsing | foobar2000's query syntax (HAS, IS, GREATER, LESS, EQUAL, AFTER, BEFORE, MISSING, PRESENT, DURING LAST, NOT/AND/OR, SORT BY); three facet columns, each any field. Autoplaylists. 36 core tests. |
| ✅ Batch tagging | Tag editor for many tracks (<multiple values>), tags from file names, track numbering. Tags tested in nine formats. |
| ✅ Renaming and moving | By title-formatting patterns, with a preview; playlists and the library follow the file. |
| ✅ Format conversion | FLAC, ALAC, WavPack, WAV, AIFF, MP3 (LAME VBR/CBR), AAC, Opus (libopus), Vorbis; tags and covers copied; optional ReplayGain applied; every core. Tested. |
| 🟡 CD ripping | The CD's AIFF files converted, named and tagged from MusicBrainz (editable before ripping). |

## Customisation and add-ons

| | |
|---|---|
| ✅ Rearrangeable layout | Edit Layout: replace, split, tab, remove; dividers dragged; five presets. Tested, and in the renders. |
| ✅ Mini player | View > Mini Player: a strip with the cover, what's playing, the seek bar and Previous/Play/Next, in front of other windows. Tested; the window's behaviour on the Mac is 🟡. |
| ✅ Themes | Kalibre's twelve, and text size. |
| ✅ Keyboard shortcuts | Every command, changed in Preferences, kept. |
| ✅ Lua add-ons | Events, menus, timers, playback, library, playlists, tags, settings; imported from .lua, folders or .zip; stopped if they run away. Tested against a fake and the real controller; four examples. |

## Systems

| | |
|---|---|
| ✅ Mac (Apple silicon) | The first; built and tested on CI. |
| ✅ Windows (64-bit) | Built and tested on CI (MSVC, FFmpeg from vcpkg). CDs read from the drive, folders watched with ReadDirectoryChangesW, media keys as hot keys: 🟡. |
| ✅ Linux (64-bit) | Built and tested on CI; a .tar.gz with an installer. CDs read from the drive (🟡); folders watched with inotify (tested). |

## Not yet

- ⬜ Signing with an Apple Developer ID (first launch needs right-click > Open).
- ⬜ Crossfade on manual track changes (they cut cleanly).
- ⬜ Writing tags inside archives (they're read-only).
- ⬜ Media keys on Linux (MPRIS, through D-Bus).
- ⬜ Signing on Windows (SmartScreen asks once).
- ⬜ Accurate-rip verification when ripping CDs.
