# 0020: Windows and Linux from the same code

Accepted, 25 November 2026.

As Kalibre has done since its 0129: one branch, what's only one system's
behind JUCE_MAC, JUCE_WINDOWS or Linux, and CI building and testing all
three.

- **Windows** builds with Visual Studio's compiler, the C++ runtime inside
  the .exe, so the .zip needs nothing installed. FFmpeg (with LAME and
  libopus) and zlib come from vcpkg at a fixed release
  (x64-windows-static), as building FFmpeg's own way needs a Unix shell and
  compiler; what vcpkg builds is cached between runs. Folders are watched
  with ReadDirectoryChangesW; the media keys are system-wide hot keys.
- **Linux** builds as the tests always have, FFmpeg by tools/build-ffmpeg.sh,
  linked in. MusicBrainz is reached through libcurl, loaded when first used
  so the player starts without it. Folders are watched with inotify, a watch
  per folder inside. It's a .tar.gz with an installer that needs no
  administrator. Media keys (MPRIS, over D-Bus) aren't done yet.
- **FFmpeg 9.0.2** on all three, the version vcpkg's release has.
- Words on screen name keys and places as each system does (platform.h).
