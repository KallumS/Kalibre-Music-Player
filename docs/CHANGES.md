# What changed, version by version

The version is in Kalibre Player > About Kalibre Player. Every change raises
it: the last number for a fix, the middle one for something new.

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
