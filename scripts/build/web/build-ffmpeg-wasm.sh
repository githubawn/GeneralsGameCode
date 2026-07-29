#!/usr/bin/env bash
set -euo pipefail

# TheSuperHackers @build githubawn 29/07/2026 Build a minimal, static FFmpeg for
# the Emscripten (WebAssembly) build, the wasm counterpart of
# scripts/build/linux/build-ffmpeg-minimal.sh. Emscripten has no FFmpeg port and
# a browser target has no system FFmpeg to find, but the OpenAL audio backend and
# the FFmpeg video player both need one, so it is compiled from source. Only the
# decoders the game's cutscenes and audio use are enabled, and no assembly is
# built, so this needs no nasm/yasm.
#
# Usage: build-ffmpeg-wasm.sh [install_prefix]
# Point the build at it with:
#   -DFFMPEG_INCLUDE_DIR=<prefix>/include
#   -DFFMPEG_AVCODEC_LIBRARY=<prefix>/lib/libavcodec.a  (likewise avformat/avutil/swscale)
# Setting those cache variables is what makes this usable at all: find_library is
# confined to the Emscripten sysroot, so ordinary discovery would never find them.
#
# Requires: an activated emsdk (emconfigure/emmake), make, curl, xz.

prefix="${1:-${GGC_FFMPEG_PREFIX:-${PWD}/ffmpeg-wasm}}"
version="${FFMPEG_VERSION:-6.1.2}"
sha256="${FFMPEG_SHA256:-3b624649725ecdc565c903ca6643d41f33bd49239922e45c9b1442c63dca4e38}"

command -v emconfigure >/dev/null || {
	echo "emconfigure not found; activate the emsdk first." >&2
	exit 1
}

# Cache hit: a previous build already installed here.
if [ -f "${prefix}/lib/libavcodec.a" ]; then
	echo "Minimal wasm FFmpeg already present at ${prefix}; skipping build."
	exit 0
fi

work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT

tarball="${work}/ffmpeg-${version}.tar.xz"
echo "Downloading ffmpeg ${version}..."
curl -fsSL "https://ffmpeg.org/releases/ffmpeg-${version}.tar.xz" -o "${tarball}"
echo "${sha256}  ${tarball}" | sha256sum -c -

tar -xf "${tarball}" -C "${work}"
src="${work}/ffmpeg-${version}"

# Configure and build inside the source tree. An out-of-tree build writes a stub
# Makefile that includes the source Makefile through an absolute path, which
# breaks under MSYS/Git Bash when this is run from a Windows host.
cd "${src}"

# Same shape as the Linux script: --disable-everything, then re-enable only the
# file protocol and the demuxers/decoders the game actually plays. Static, since
# a wasm module links everything in, and without threads or assembly.
emconfigure ./configure \
	--prefix="${prefix}" \
	--enable-static --disable-shared \
	--enable-pic --disable-debug \
	--disable-programs --disable-doc --disable-htmlpages --disable-manpages \
	--disable-avdevice --disable-avfilter --disable-postproc --disable-network \
	--disable-everything \
	--enable-protocol=file \
	--enable-demuxer=bink,smacker,mov,avi,matroska,asf,mpegps,mpegts,ogg,wav,flv,h264,hevc,m4v \
	--enable-decoder=bink,binkaudio_dct,binkaudio_rdft,smackaud,smacker,h264,hevc,mpeg1video,mpeg2video,mpeg4,msmpeg4v1,msmpeg4v2,msmpeg4v3,wmv1,wmv2,wmv3,vc1,vp6,vp6a,vp6f,vp8,vp9,theora,flv,vorbis,aac,ac3,mp3,pcm_s16le,pcm_u8 \
	--enable-parser=h264,hevc,mpeg4video,mpegvideo,vp8,vp9,vorbis,aac,ac3 \
	--enable-cross-compile --target-os=none --arch=x86_32 \
	--disable-asm --disable-inline-asm --disable-pthreads --disable-autodetect \
	--nm=llvm-nm --ar=emar --ranlib=emranlib --cc=emcc --cxx=em++

emmake make -j"$(nproc)"
emmake make install

echo "Minimal wasm FFmpeg installed to ${prefix} ($(du -shc "${prefix}"/lib/*.a 2>/dev/null | tail -1 | cut -f1))"
