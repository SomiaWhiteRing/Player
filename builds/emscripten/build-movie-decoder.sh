#!/usr/bin/env bash
# Decode-only FFmpeg: no encoders, muxers, filters, network or pthreads.
set -euo pipefail
source_root="$(cd "$(dirname "$0")/../.." && pwd)"
output="$(mkdir -p "$1" && cd "$1" && pwd)"
cache="$output/movie-decoder-build"
version=7.1.5
sha256=de668509caf9e35e3cd162473441fdb29538c6d96ed080292b3cf9e6fc5d558f
mkdir -p "$cache"
archive="$cache/ffmpeg-$version.tar.xz"
if [ ! -f "$archive" ]; then
  curl --fail --location --retry 3 "https://ffmpeg.org/releases/ffmpeg-$version.tar.xz" -o "$archive.tmp"
  mv "$archive.tmp" "$archive"
fi
echo "$sha256  $archive" | sha256sum --check
if [ ! -f "$cache/ffmpeg-$version/configure" ]; then
  tar -xf "$archive" -C "$cache"
fi
cd "$cache/ffmpeg-$version"
signature="$( { sha256sum "$source_root/builds/emscripten/build-movie-decoder.sh"; emcc --version; } | sha256sum)"
if [ ! -f .player-config ] || [ "$(cat .player-config)" != "$signature" ]; then
  [ ! -f ffbuild/config.mak ] || make distclean
  emconfigure ./configure --prefix="$cache/install" --cc=emcc --cxx=em++ --ar=emar --ranlib=emranlib \
    --nm=emnm --target-os=none --arch=wasm32 --enable-cross-compile \
    --disable-everything --disable-autodetect --disable-programs --disable-doc --disable-debug \
    --disable-network --disable-pthreads --disable-w32threads --disable-os2threads \
    --disable-asm --disable-x86asm --disable-avdevice --disable-avfilter --disable-postproc \
    --enable-small --enable-swscale --enable-swresample --enable-protocol=file \
    --enable-demuxer=avi,mpegps,mpegts,mov \
    --enable-parser=mpegvideo,mpeg4video,h263,h264,mpegaudio,aac,ac3 \
    --enable-decoder=mpeg1video,mpeg2video,mpeg4,msmpeg4v1,msmpeg4v2,msmpeg4v3,msvideo1,cinepak,indeo3,mjpeg,h264,rawvideo \
    --enable-decoder=mp1float,mp2float,mp3float,aac,ac3,pcm_u8,pcm_s16le,pcm_s24le,pcm_s32le,adpcm_ms,adpcm_ima_wav \
    --extra-cflags='-Oz -flto' --extra-ldflags='-flto'
  printf '%s' "$signature" > .player-config
fi
make -j "${CMAKE_BUILD_PARALLEL_LEVEL:-4}"
emcc "$source_root/src/platform/emscripten/movie_decoder.c" -I . -Oz -flto \
  libavformat/libavformat.a libavcodec/libavcodec.a libswscale/libswscale.a \
  libswresample/libswresample.a libavutil/libavutil.a \
  -sMODULARIZE=1 -sEXPORT_NAME=createMovieDecoder -sENVIRONMENT=worker,node \
  -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=33554432 -sMAXIMUM_MEMORY=268435456 \
  -sSTACK_SIZE=1048576 -sFILESYSTEM=1 -sFORCE_FILESYSTEM=1 -sEXIT_RUNTIME=0 \
  -sEXPORTED_RUNTIME_METHODS=FS,ccall,UTF8ToString -lworkerfs.js \
  -o "$output/movie-decoder.js"
cp COPYING.LGPLv2.1 "$output/movie-decoder.LICENSE.txt"
