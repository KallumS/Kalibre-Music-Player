#!/bin/bash
# Builds the small, static FFmpeg Kalibre Player decodes and encodes with
# (decision 0003): only the audio decoders, demuxers and parsers for the
# formats the player opens, the encoders its converter writes, and
# libswresample. No programs, no network, no video, nothing found on the
# system: the app needs nothing beside itself.
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
ENCODERS="flac,alac,aac,opus,wavpack,pcm_s16le,pcm_s24le,pcm_s16be,pcm_s24be"
MUXERS="ipod,mp4,ogg,opus,wv,flac,wav,aiff"

EXTRA=()
if [ "$(uname)" = "Darwin" ]; then
  EXTRA+=(--arch=arm64 --enable-cross-compile --target-os=darwin --cc=clang
          "--extra-cflags=-arch arm64 -mmacosx-version-min=12.0" "--extra-ldflags=-arch arm64 -mmacosx-version-min=12.0")
fi

cd "$SRC"
./configure --prefix="$PREFIX" \
  --enable-static --disable-shared --enable-pic \
  --disable-programs --disable-doc --disable-network --disable-autodetect --disable-debug \
  --disable-everything --disable-avdevice --disable-swscale --disable-avfilter --enable-swresample \
  --disable-iconv --disable-zlib --disable-bzlib --disable-lzma \
  --enable-decoder="$DECODERS" --enable-demuxer="$DEMUXERS" --enable-parser="$PARSERS" \
  --enable-encoder="$ENCODERS" --enable-muxer="$MUXERS" \
  --disable-x86asm \
  "${EXTRA[@]+"${EXTRA[@]}"}"
make -j"$JOBS"
make install
echo "$VERSION" > "$PREFIX/VERSION"
