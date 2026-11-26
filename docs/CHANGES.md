# What changed, version by version

The version is in Kalibre Player > About Kalibre Player. Every change raises
it: the last number for a fix, the middle one for something new.

## 0.3.2 (26 November 2026)

- An icon of its own: Kalibre's K on a bluer tile, its calibration ticks
  become level bars, as a meter shows sound (0021).

## 0.3.1 (26 November 2026)

- Preferences' tabs sit on the theme's ground (beside them showed white on
  some systems).
- The user guide is rewritten for all three systems, with pictures of the
  tag editor, converter, Move Files, Preferences and add-ons.

## 0.3.0 (25 November 2026)

- Kalibre Player for Windows (a .zip: KalibrePlayer-Windows) and Linux (a
  .tar.gz with an installer: KalibrePlayer-Linux), from the same code as
  the Mac's.
- Audio CDs on Windows and Linux are read straight from the drive, played
  and ripped as on the Mac.
- The library's folders are watched on Windows and Linux as on the Mac: a
  change is noticed within seconds.
- The keyboard's media keys on Windows, even when the player isn't in front.
- Keys and places are named as each system names them (Ctrl+E, Show in File
  Explorer).
- FFmpeg 9.0.2 everywhere.
- Fixed on the Mac: a library folder reached through a link (or under
  /var, /tmp) wasn't noticed changing.
- Fixed: quitting while a track's loudness was being measured, or the
  library looked through, could crash the player.

## 0.2.0 (25 November 2026)

- The mini player: View > Mini Player (Command-Shift-M) makes the window a
  small strip with the cover, what's playing, the seek bar and Previous,
  Play and Next, kept in front of other windows. Drag it anywhere;
  right-click to keep it in front or not. Its expand button, a
  double-click, or Command-Shift-M again brings back the full window where
  it was. The player opens as it was left.

## 0.1.1 (25 November 2026)

- Crossfade always lasts as long as it's set to: on a fast Mac the next
  track's fade could start late, when the playlist answered after the
  decoder had passed the point where it should begin.
- The player counts as playing from the moment Play is pressed, not a
  moment later when the file has opened (the window and add-ons could see
  "stopped" in between).

## 0.1.0 (25 November 2026)

The first Kalibre Player, made from Kalibre 1.1.0:

- Plays MP1, MP2, MP3, Musepack, AAC, WMA, Ogg Vorbis, FLAC and Ogg FLAC,
  ALAC, WavPack, WAV, AIFF, AU, SND, Speex and Opus (and more), through FFmpeg.
- Audio CDs; music inside ZIP, RAR, 7-Zip, tar and gzip archives.
- Gapless playback; crossfade; ReplayGain by track, album or both, measured
  on the fly for tracks without tags.
- A ten-band equaliser, and AU and VST3 effects in a chain.
- Oscilloscope, goniometer, spectrum, peak meter and loudness meter.
- A library that watches its folders, searched with words or queries,
  browsed by any field; autoplaylists.
- Tag editor, tags from file names, renaming, moving and copying by pattern,
  title formatting.
- Converter (FLAC, ALAC, WavPack, WAV, AIFF, MP3, AAC, Opus, Vorbis) and CD
  ripping named from MusicBrainz; ReplayGain scanning.
- A layout to rearrange, Kalibre's themes, keyboard shortcuts to change.
- Add-ons in Lua, with four examples.
