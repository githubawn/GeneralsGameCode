# Web (WebAssembly) build

The `web-generalsmd-sdl3-bgfx` preset cross-compiles GeneralsMD to WebAssembly with
Emscripten. It renders through bgfx on WebGL 2, uses SDL3 for windowing and input,
and Emscripten's OpenAL for audio.

The build output is a plain static directory: `index.html`, `index.js`, `index.wasm`.

## Building

Needs an activated [emsdk](https://emscripten.org/docs/getting_started/downloads.html),
plus ninja and a host C++ compiler (the cross build compiles its shaders with a host
shaderc, which it builds from the bgfx tree it already fetches).

```sh
# FFmpeg first: Emscripten has no FFmpeg port and there is no system one to find.
scripts/build/web/build-ffmpeg-wasm.sh "$PWD/build/ffmpeg-wasm"

cmake --preset web-generalsmd-sdl3-bgfx \
  -DFFMPEG_INCLUDE_DIR="$PWD/build/ffmpeg-wasm/include" \
  -DFFMPEG_AVCODEC_LIBRARY="$PWD/build/ffmpeg-wasm/lib/libavcodec.a" \
  -DFFMPEG_AVFORMAT_LIBRARY="$PWD/build/ffmpeg-wasm/lib/libavformat.a" \
  -DFFMPEG_AVUTIL_LIBRARY="$PWD/build/ffmpeg-wasm/lib/libavutil.a" \
  -DFFMPEG_SWSCALE_LIBRARY="$PWD/build/ffmpeg-wasm/lib/libswscale.a"

cmake --build build/web-generalsmd-sdl3-bgfx --config Release --target z_generals
```

`.github/workflows/web-build.yml` runs exactly this in the `emscripten/emsdk`
container on every push.

## Running it locally

```sh
python scripts/build/web/serve.py
```

Then open <http://localhost:8000>. The server streams the `.big` archives and the
loose `Data/` tree out of a Zero Hour installation on the same machine; put its path
in `scripts/build/web/serve.ini`:

```ini
[Paths]
STEAM_ZH_DIR = C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Generals - Zero Hour
```

## Hosting it

The page ships no game data and never uploads any. With no dev server present, it
asks the player for their own installation folder and copies the archives into the
browser's origin-private filesystem (OPFS), so later visits start from local storage.
That is about 1.7 GB, and the page requests persistent storage before copying.

The one hard hosting requirement is **cross-origin isolation**: the build uses
threads, so the page needs `SharedArrayBuffer`, which browsers only grant when the
response carries

```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

`vercel.json` (staged next to the build output) sets both. To deploy from CI, add
these repository secrets; without them the deploy step is skipped and the workflow
just uploads the artifact:

| Secret | Where it comes from |
| --- | --- |
| `VERCEL_TOKEN` | Vercel account settings, Tokens |
| `VERCEL_ORG_ID` | `.vercel/project.json` after `vercel link` |
| `VERCEL_PROJECT_ID` | `.vercel/project.json` after `vercel link` |

To deploy by hand instead:

```sh
npx vercel deploy stage-web/GeneralsZH-web --prod
```

Static hosts that cannot set response headers (GitHub Pages among them) need a
service-worker shim to become cross-origin isolated; that is not included here.
