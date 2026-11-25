# 0019: CDs on Windows and Linux are read from the drive

Accepted, 25 November 2026.

A Mac shows an audio CD as AIFF files; Windows (.cda stubs) and Linux
(nothing) don't. There the player asks the drive itself: its table of
contents (IOCTL_CDROM_READ_TOC; CDROMREADTOCHDR and CDROMREADTOCENTRY) and
raw audio sectors (IOCTL_CDROM_RAW_READ; CDROMREADAUDIO), 24 at a time. A
track is "cdda:" + the drive + "|" + its number, and opens as a stream that
is a WAV file: a 44-byte header, then the sectors as they're read. So the
decoder, seeking, the converter and ripping need nothing new. An unreadable
sector is silence rather than an end. An enhanced CD's data track is left
out, and the audio's end counted 11,400 sectors before it, as MusicBrainz
counts it for the disc ID. No library (libcdio) was needed.
