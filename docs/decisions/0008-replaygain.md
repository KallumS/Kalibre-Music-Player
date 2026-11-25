# 0008: ReplayGain 2.0, measured on the fly

Accepted, 25 November 2026.

ReplayGain 2.0 is BS.1770 integrated loudness against -18 LUFS, the same measure as Kalibre's meter; an album's gain gates all its tracks' blocks together. Playing uses the tags; a track without them is measured in a background job as soon as it's queued (decoding is many times faster than real time), and the gain is faded in when known: "calculates volume adjustments on the fly". Opus's R128 gains are read too.
