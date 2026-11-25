# Architecture

```
Source/Core    no JUCE: tracks, title formatting, queries, the library model,
               playlists and play order, ReplayGain maths, audio-CD disc IDs,
               batch tagging, the layout tree, the Lua add-on host
Source/Engine  JUCE and the libraries: FFmpeg decoding and encoding, archives
               (libarchive), tags (TagLib), the player and its DSP, plug-ins,
               the library scanner and folder watcher, background jobs, CDs
Source/App     the window: the controller, panels, layout, dialogs, menus
Tests          Core (KPTests, no JUCE) and App (KPAppTests, under Xvfb)
tools          build-ffmpeg.sh; RenderWindow.cpp (KPRender) draws the window to PNGs
```

## How sound flows (0014)

```
file / archive entry ──► Sources::open ──► Decoder (FFmpeg + swresample)
                                              │  stereo float at the device's rate
                     decoding thread ─────────┤  × ReplayGain (per track, ramped)
                                              │  crossfade mixes the next in
                                              ▼
                                  a FIFO of ~0.75 s (AbstractFifo)
                                              │
                     audio thread ────────────┤  equaliser → plug-in chain → volume
                                              ▼
                               output  +  taps: scope, levels, loudness
```

- The **window** says what to play (`Player::play`) and, each time a track
  starts decoding, what follows it (`Player::setNext`, in answer to the
  `wantsNext` event). The decoder opens the next ahead, so it joins the last
  sample of one to the first of the next: gapless.
- **Segments** record where in the stream each track starts; the window
  works out which is heard, and its position, from how far the audio thread
  has read (`readFrames`), so "now playing" changes exactly when the sound does.
- **Seeking and stopping** flush the FIFO: the decoding thread asks, the
  audio thread empties it at its next block.
- **Tokens** name each play of an item, so a playlist edited while it plays
  doesn't confuse what plays next (`Controller::playlistEdited`).

## Rules easy to break

- The audio thread (`Player::renderForTest`, `Player::process`) never
  allocates or locks. Settings are atomics; a plug-in chain is built and
  prepared on the window's thread, then swapped in whole; the old one is
  deleted by the window's timer.
- A `Decoder` is used by one thread at a time.
- The library is only touched on the window's thread; the scanner sends batches.
- Add-ons run on the window's thread, each in its own Lua, with a time limit.
- Tags inside archives are never written.
- Everything the player writes (library, playlists, settings) goes through a
  temporary file, so a crash mid-write loses nothing.

## Formats

FFmpeg is built small and static by `tools/build-ffmpeg.sh`: only audio
decoders, demuxers and parsers, the converter's encoders (with LAME and
libopus built first), libswresample, no protocols (the player reads through
its own I/O, so archives and files are the same to it), nothing found on the
system. `cmake/FFmpeg.cmake` runs it once, when configuring.
