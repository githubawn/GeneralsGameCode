#!/usr/bin/env bash
#
# TheSuperHackers @build 27/06/2026
# Build a minimal, static FFmpeg for one of the mobile/web targets and package
# it for consumption by cmake/ffmpeg.cmake (GGC_FFMPEG_PREBUILT_URL / _DIR).
#
# Host model:
#   Windows (Git Bash / MSYS2, with `make`) -> android, wasm
#   macOS                                   -> ios   (macOS itself uses brew)
#
# Usage:
#   scripts/ffmpeg/build-ffmpeg.sh <android|ios|wasm|macos> [output-dir]
#
# Output (default ./build/ffmpeg-prebuilt):
#   <out>/ffmpeg-<plat>/{include,lib}      install tree   -> GGC_FFMPEG_PREBUILT_DIR
#   <out>/ffmpeg-<plat>.tar.xz             archive        -> GGC_FFMPEG_PREBUILT_URL
#
# Requirements per target:
#   android : ANDROID_NDK_HOME set, `make`
#   wasm    : emsdk activated (emcc/emconfigure/emmake on PATH)
#   ios     : Xcode command line tools (xcrun), `make`
#   macos   : clang + `make`
#
# Only audio (wav/mp3) + video (Bink + swscale) decoding is built. No asm
# (so no yasm/nasm needed). swresample/avfilter/avdevice/postproc are disabled.

set -euo pipefail

PLAT="${1:-}"
OUT_DIR="${2:-build/ffmpeg-prebuilt}"
FFMPEG_VERSION="${GGC_FFMPEG_VERSION:-6.1.1}"
ANDROID_API="${GGC_ANDROID_API:-26}"
IOS_MIN="${GGC_IOS_MIN:-13.0}"

if [[ -z "$PLAT" ]]; then
    echo "usage: $0 <android|ios|wasm|macos> [output-dir]" >&2
    exit 2
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK="$(cd "$OUT_DIR" 2>/dev/null && pwd || (mkdir -p "$OUT_DIR" && cd "$OUT_DIR" && pwd))"
PRISTINE_DIR="$WORK/ffmpeg-${FFMPEG_VERSION}"
# In-tree build: configure + make run *inside* a per-platform copy of the source
# tree. Out-of-tree builds generate a stub Makefile that `include`s the source
# Makefile via an absolute MSYS-form path (/c/Users/...), which the NDK's native
# Windows `make` cannot resolve. An in-tree build keeps all Makefile paths
# relative, so native make works.
SRC_DIR="$WORK/ffmpeg-${PLAT}-src"
INSTALL_DIR="$WORK/ffmpeg-${PLAT}"
TARBALL="$WORK/ffmpeg-${PLAT}.tar.xz"

# --- fetch sources ---------------------------------------------------------
if [[ ! -f "$PRISTINE_DIR/configure" ]]; then
    TAR="$WORK/ffmpeg-${FFMPEG_VERSION}.tar.xz"
    if [[ ! -f "$TAR" ]]; then
        echo ">> downloading ffmpeg-${FFMPEG_VERSION}"
        curl -fL "https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz" -o "$TAR"
    fi
    tar -C "$WORK" -xf "$TAR"
fi

# Fresh, pristine per-platform source copy for an in-tree build.
rm -rf "$SRC_DIR" "$INSTALL_DIR"
cp -a "$PRISTINE_DIR" "$SRC_DIR"

# FFmpeg's configure creates + executes many shell/compiler test files in $TMPDIR.
# A Windows-style TMPDIR (C:\Users\...\Temp) gets its backslashes eaten under
# MSYS/Git Bash and the sanity test fails ("Unable to create and execute files").
# Pin it to a clean path that both bash and the Windows clang wrappers accept.
mkdir -p "$SRC_DIR/.tmp"
if command -v cygpath >/dev/null 2>&1; then
    TMPDIR="$(cygpath -m "$SRC_DIR/.tmp")"
else
    TMPDIR="$SRC_DIR/.tmp"
fi
export TMPDIR

# --- shared, audio+video, minimal config ----------------------------------
COMMON_ARGS=(
    "--prefix=$INSTALL_DIR"
    --disable-shared --enable-static --enable-pic
    --disable-programs --disable-doc
    --disable-htmlpages --disable-manpages --disable-podpages --disable-txtpages
    --disable-network --disable-autodetect --disable-debug
    --disable-everything
    --disable-avdevice --disable-avfilter --disable-postproc --disable-swresample
    --enable-avcodec --enable-avformat --enable-avutil --enable-swscale
    --enable-decoder=pcm_s8,pcm_u8,pcm_s16le,pcm_s16be,pcm_s24le,pcm_s32le,pcm_f32le,pcm_mulaw,pcm_alaw,adpcm_ima_wav,adpcm_ms,mp3,mp3float,bink,binkaudio_dct,binkaudio_rdft
    --enable-demuxer=wav,mp3,aiff,w64,pcm_s16le,bink
    --enable-parser=mpegaudio
    --enable-protocol=file
    --disable-asm
)

host_tag() {
    case "$(uname -s)" in
        Linux*)  echo "linux-x86_64" ;;
        Darwin*) echo "darwin-x86_64" ;;
        MINGW*|MSYS*|CYGWIN*) echo "windows-x86_64" ;;
        *) echo "linux-x86_64" ;;
    esac
}

configure_and_make() {
    # $1 = configure runner prefix (e.g. "emconfigure" or empty)
    # $2 = make runner (e.g. "emmake make" or "make")
    local cfg_prefix="$1"; shift
    local make_cmd="$1"; shift
    echo ">> configuring FFmpeg for $PLAT"
    ( cd "$SRC_DIR" && $cfg_prefix ./configure "${COMMON_ARGS[@]}" "$@" )
    echo ">> building"
    ( cd "$SRC_DIR" && $make_cmd -j"${NPROC:-4}" )
    ( cd "$SRC_DIR" && $make_cmd install )
}

case "$PLAT" in
android)
    : "${ANDROID_NDK_HOME:?set ANDROID_NDK_HOME to your NDK}"
    HT="$(host_tag)"
    TC="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/$HT"
    CLANG_EXT=""
    [[ "$HT" == windows-* ]] && CLANG_EXT=".cmd"
    # No --sysroot: the NDK per-API clang wrapper sets target + sysroot itself
    # (an MSYS-form --sysroot path would not be understood by the Windows clang).
    configure_and_make "" "make" \
        --enable-cross-compile --target-os=android --arch=aarch64 --cpu=armv8-a \
        "--cc=$TC/bin/aarch64-linux-android${ANDROID_API}-clang${CLANG_EXT}" \
        "--cxx=$TC/bin/aarch64-linux-android${ANDROID_API}-clang++${CLANG_EXT}" \
        "--ar=$TC/bin/llvm-ar" "--nm=$TC/bin/llvm-nm" \
        "--ranlib=$TC/bin/llvm-ranlib" "--strip=$TC/bin/llvm-strip"
    ;;
wasm)
    command -v emconfigure >/dev/null || { echo "activate emsdk first (emconfigure not found)" >&2; exit 1; }
    configure_and_make "emconfigure" "emmake make" \
        --enable-cross-compile --target-os=none --arch=x86_32 \
        --disable-pthreads --disable-inline-asm \
        --nm=llvm-nm --ar=emar --ranlib=emranlib --cc=emcc --cxx=em++
    ;;
ios)
    command -v xcrun >/dev/null || { echo "Xcode command line tools required (xcrun)" >&2; exit 1; }
    SDK="$(xcrun --sdk iphoneos --show-sdk-path)"
    CC="$(xcrun --sdk iphoneos -f clang)"
    configure_and_make "" "make" \
        --enable-cross-compile --target-os=darwin --arch=arm64 \
        "--cc=$CC" "--sysroot=$SDK" \
        "--extra-cflags=-arch arm64 -isysroot $SDK -miphoneos-version-min=${IOS_MIN}" \
        "--extra-ldflags=-arch arm64 -isysroot $SDK -miphoneos-version-min=${IOS_MIN}"
    ;;
macos)
    # macOS normally consumes brew FFmpeg via find_package; this is provided for
    # completeness / fully-static bundles.
    ARCH="$(uname -m)"
    configure_and_make "" "make" --arch="$ARCH" "--extra-cflags=-arch $ARCH" "--extra-ldflags=-arch $ARCH"
    ;;
*)
    echo "unknown target: $PLAT (expected android|ios|wasm|macos)" >&2
    exit 2
    ;;
esac

# --- package ---------------------------------------------------------------
echo ">> packaging $TARBALL"
( cd "$WORK" && tar -cJf "ffmpeg-${PLAT}.tar.xz" "ffmpeg-${PLAT}" )

echo
echo "Done. Prebuilt FFmpeg for $PLAT:"
echo "  install tree : $INSTALL_DIR"
echo "  archive      : $TARBALL"
echo
echo "Consume it in the game build with either:"
echo "  -DGGC_FFMPEG_PREBUILT_DIR=$INSTALL_DIR"
echo "  -DGGC_FFMPEG_PREBUILT_URL=<host-this-archive>/ffmpeg-${PLAT}.tar.xz"
