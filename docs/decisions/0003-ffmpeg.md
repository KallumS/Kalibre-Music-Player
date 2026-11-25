# 0003: FFmpeg decodes everything

Accepted, 25 November 2026.

The list asked for (Musepack, WMA, Speex, WavPack, Opus, MP1/MP2) is beyond JUCE's and Core Audio's own readers. FFmpeg decodes all of it, and more, and cuts encoder delay for gapless. It's built static and small by tools/build-ffmpeg.sh (only audio decoders and demuxers, no network, no autodetected system libraries), so the app needs nothing beside itself. It reads through a custom AVIO from a juce::InputStream, so a file inside an archive is read the same way. LAME and libopus are built in for the converter's MP3 and Opus, as FFmpeg's own MP3 encoder doesn't exist and its Opus encoder is weaker. All LGPL or BSD, under the app's GPL-3.0.
