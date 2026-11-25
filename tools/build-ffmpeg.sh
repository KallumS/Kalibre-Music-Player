#!/bin/bash
# Builds the small, static FFmpeg Kalibre Player decodes and encodes with
# (decision 0003): only the audio decoders, demuxers and parsers for the
# formats the player opens, the encoders its converter writes, and
# libswresample. No programs, no network, no video, nothing found on the
# system: the app needs nothing beside itself. LAME (MP3) and libopus (Opus)
# are built first, into the same place, for the converter's MP3s and Opus.
#
#   tools/build-ffmpeg.sh <prefix>
#
# CMake runs this once when configuring (cmake/FFmpeg.cmake) unless the prefix
# already holds a build of this version; CI caches the prefix.
set -euo pipefail
PREFIX="$1"
VERSION="${FFMPEG_VERSION:-n8.0.3}"
SRC="${PREFIX}-src"
JOBS="$( (command -v nproc >/dev/null && nproc) || sysctl -n hw.ncpu || echo 4)"

LAME_VERSION=3.100
LAME_SHA256=ddfe36cab873794038ae2c1210557ad34857a4b6bdc515785d1da9e175b1da1e
OPUS_VERSION=v1.5.2
mkdir -p "$PREFIX"
HOST=()
if [ "$(uname)" = "Darwin" ]; then
  export CFLAGS="-arch arm64 -mmacosx-version-min=12.0 -O2"
  export LDFLAGS="-arch arm64 -mmacosx-version-min=12.0"
  HOST=(--host=aarch64-apple-darwin)
fi

# LAME (LGPL): MP3s.
if [ ! -f "$PREFIX/lib/libmp3lame.a" ]; then
  rm -rf "$PREFIX-lame" && mkdir -p "$PREFIX-lame"
  curl -fsSL --retry 4 -o "$PREFIX-lame/lame.tar.gz" "https://downloads.sourceforge.net/project/lame/lame/$LAME_VERSION/lame-$LAME_VERSION.tar.gz"
  echo "$LAME_SHA256  $PREFIX-lame/lame.tar.gz" | shasum -a 256 -c -
  tar -xzf "$PREFIX-lame/lame.tar.gz" -C "$PREFIX-lame"
  (cd "$PREFIX-lame/lame-$LAME_VERSION" &&
   ./configure --prefix="$PREFIX" --disable-shared --enable-static --disable-frontend --disable-decoder --disable-gtktest --with-pic "${HOST[@]+"${HOST[@]}"}" &&
   make -j"$JOBS" && make install)
fi

# libopus (BSD): Opus, better than FFmpeg's own encoder.
if [ ! -f "$PREFIX/lib/libopus.a" ]; then
  rm -rf "$PREFIX-opus"
  git clone --depth 1 --branch "$OPUS_VERSION" https://github.com/xiph/opus.git "$PREFIX-opus"
  cmake -S "$PREFIX-opus" -B "$PREFIX-opus/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_INSTALL_LIBDIR=lib \
        -DBUILD_SHARED_LIBS=OFF -DOPUS_BUILD_TESTING=OFF -DOPUS_BUILD_PROGRAMS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 -DOPUS_DISABLE_INTRINSICS=OFF
  cmake --build "$PREFIX-opus/build" -j"$JOBS"
  cmake --install "$PREFIX-opus/build"
fi
unset CFLAGS LDFLAGS

if [ ! -d "$SRC/.git" ]; then
  rm -rf "$SRC"
  git clone --depth 1 --branch "$VERSION" https://github.com/FFmpeg/FFmpeg.git "$SRC"
fi

DECODERS="mp1,mp1float,mp2,mp2float,mp3,mp3float,mp3adu,mp3on4,mpc7,mpc8,aac,aac_fixed,aac_latm,alac,als,\
wmav1,wmav2,wmapro,wmalossless,wmavoice,vorbis,flac,wavpack,speex,opus,ape,tta,tak,ac3,eac3,dca,truehd,mlp,\
dsd_lsbf,dsd_msbf,dsd_lsbf_planar,dsd_msbf_planar,\
pcm_s8,pcm_u8,pcm_s16le,pcm_s16be,pcm_s24le,pcm_s24be,pcm_s32le,pcm_s32be,pcm_f32le,pcm_f32be,pcm_f64le,pcm_f64be,\
pcm_s16le_planar,pcm_s24le_planar,pcm_s32le_planar,pcm_s8_planar,pcm_u16le,pcm_u16be,pcm_u24le,pcm_u24be,pcm_u32le,pcm_u32be,\
pcm_alaw,pcm_mulaw,pcm_bluray,pcm_dvd,adpcm_ms,adpcm_ima_wav,adpcm_ima_qt,adpcm_g726,adpcm_g722,gsm,gsm_ms,qdm2,qdmc,mace3,mace6"
DEMUXERS="mp3,mpc,mpc8,aac,mov,asf,ogg,flac,wv,wav,w64,aiff,au,ape,tta,tak,ac3,eac3,dts,truehd,mlp,matroska,caf,dsf,iff,loas,latm,pcm_s16le,xwma,rso,voc,amr,ircam,sox"
PARSERS="mpegaudio,aac,aac_latm,flac,vorbis,opus,ac3,dca,mlp,tak,gsm"
ENCODERS="flac,alac,aac,libopus,libmp3lame,wavpack,pcm_s16le,pcm_s24le,pcm_s16be,pcm_s24be"
MUXERS="ipod,mp4,ogg,opus,wv,flac,wav,aiff,mp3"

EXTRA=()
if [ "$(uname)" = "Darwin" ]; then
  EXTRA+=(--arch=arm64 --enable-cross-compile --target-os=darwin --cc=clang
          "--extra-cflags=-arch arm64 -mmacosx-version-min=12.0 -I$PREFIX/include" "--extra-ldflags=-arch arm64 -mmacosx-version-min=12.0 -L$PREFIX/lib")
fi

cd "$SRC"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
./configure --prefix="$PREFIX" \
  --enable-static --disable-shared --enable-pic \
  --disable-programs --disable-doc --disable-network --disable-autodetect --disable-debug \
  --disable-everything --disable-avdevice --disable-swscale --disable-avfilter --enable-swresample \
  --disable-iconv --disable-zlib --disable-bzlib --disable-lzma \
  --enable-decoder="$DECODERS" --enable-demuxer="$DEMUXERS" --enable-parser="$PARSERS" \
  --enable-encoder="$ENCODERS" --enable-muxer="$MUXERS" \
  --enable-libmp3lame --enable-libopus \
  "--extra-cflags=-I$PREFIX/include" "--extra-ldflags=-L$PREFIX/lib" \
  --pkg-config-flags=--static \
  --disable-x86asm \
  "${EXTRA[@]+"${EXTRA[@]}"}"
make -j"$JOBS"
make install
echo "$VERSION+lame+opus" > "$PREFIX/VERSION"
