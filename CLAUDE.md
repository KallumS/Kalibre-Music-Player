# Kalibre Player

A music player for Apple silicon Macs, Windows and Linux (JUCE 9, C++20), made from Kalibre the
DAW (KallumS/Kalibre): its theme, loudness meter, Lua and conventions, around
a player in the spirit of foobar2000. Built for one musician, who is not a
developer: explain in plain words, show renders of what changed, and make
sure every change reaches them as the CI's `.dmg` (Actions > Build >
KalibrePlayer-macOS); Windows' `.zip` and Linux's `.tar.gz` come from the same run (decision 0020).

## Where the reasons are

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md): the shape, how sound flows, rules easy to break. Start here.
- [`docs/decisions/`](docs/decisions/README.md): one record per choice, by number: **(0014)** is the gapless engine.
- [`docs/FEATURES.md`](docs/FEATURES.md): what's done, what's partly done, what isn't. Keep it true.
- [`docs/USER-GUIDE.md`](docs/USER-GUIDE.md): for the musician: every function, menu and key, with pictures (`docs/guide/`, from KPRender).
- [`docs/ADD-ONS.md`](docs/ADD-ONS.md), [`docs/TITLE-FORMATTING.md`](docs/TITLE-FORMATTING.md), [`docs/QUERIES.md`](docs/QUERIES.md): made from the Help menu's words (`Source/App/Dialogs.cpp`); change both.
- [`docs/CHANGES.md`](docs/CHANGES.md): what changed in each version.

## Working in it

```sh
# the core and its tests: seconds, no JUCE
cmake -B build-core -G Ninja -DKP_BUILD_APP=OFF && cmake --build build-core && ./build-core/KPTests
# everything (fetches JUCE 9.0.5, TagLib, libarchive; builds FFmpeg, LAME and libopus once)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build
xvfb-run -a -s "-screen 0 1600x1000x24" ./build/KPAppTests_artefacts/Debug/KPAppTests
xvfb-run -a -s "-screen 0 1600x1000x24" ./build/KPRender_artefacts/Debug/KPRender renders
```

- FFmpeg is built by `tools/build-ffmpeg.sh` into `build/ffmpeg` (or `-DKP_FFMPEG_DIR=...` to keep one between builds). Adding a format means adding its decoder/demuxer there.
- Linux needs: ninja-build libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libxcomposite-dev libxi-dev libgl1-mesa-dev zlib1g-dev xvfb.
- The Mac and Windows apps are built and tested on CI (`.mm` and Windows-only files only build there). After pushing, check the run.
- Windows takes FFmpeg from vcpkg (pinned in the workflow); Mac and Linux from `tools/build-ffmpeg.sh`. Keep the versions the same.
- What's one system's goes behind `JUCE_MAC`, `JUCE_WINDOWS` or `JUCE_LINUX`, or in a file of its own (`...Mac.mm`, `...Win.cpp`, `...Linux.cpp`).
- Look at a render after any drawing change. When fixing a bug, have a test fail first.

## Rules

- **Every change raises the version** (`project(KalibrePlayer VERSION …)` in `CMakeLists.txt`): the last number for a fix, the middle for something new. Add its line to `docs/CHANGES.md`.
- **A release** is a tag `v<version>` on `main`: CI attaches the .dmg, .zip and .tar.gz. The musician makes the release (Releases > Draft a new release).
- **No JUCE in `Source/Core`.**
- **The audio thread doesn't allocate or lock** (`Player::renderForTest` and what it calls). Settings are atomics; the plug-in chain is swapped whole.
- **Every colour from `Theme.h`**, as in Kalibre (themes change them).
- **Messages in the message line at the top**; the transport at the bottom, as the musician has it in Kalibre.
- Words on screen follow Apple's writing guidelines: Title Case on buttons and menus, an ellipsis when more input follows, no "we".
