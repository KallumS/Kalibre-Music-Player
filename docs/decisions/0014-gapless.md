# 0014: A decoding thread, a FIFO and segments: gapless

Accepted, 25 November 2026.

A decoding thread fills a FIFO with stereo floats at the device's rate; the audio thread only reads it. When a track ends, the next (already opened) continues in the same block, so there's no gap. Each track's start in the stream is a segment; the window reads which is heard from how far the audio thread has read. The window answers "what's next" as each track starts decoding, so the decision uses the playlist as it is then. Crossfade mixes the next in during the last seconds, equal power.
