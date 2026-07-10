# PS2 Port Plan (Demake)

This is a demake, not a port. The plan is organized around two hard limits: **32 MB main RAM** and **4 MB VRAM (GS local memory)**. Everything else is negotiable; those two are not.

This work lives on `ps2-port`, branched from `android-macos-iphone-windows64-vibecode-test`, so it inherits the ILP32/LP64 fixes, `time_compat`, `RTS_GAMEMEMORY_ENABLE` support, and headless/FileSystem/FramePacer work already done there. It does not target `main` directly.

## Ifdef discipline

Per repo convention, **every PS2-specific change must be guarded**, not just visually distinct. Concretely:

- Guard macro: `__PS2__` (defined explicitly by `cmake/toolchains/ps2.cmake`, matching the ps2sdk/gsKit convention -- ps2sdk does not predefine it on its own) wrapping all PS2-only code paths (VU1 transform, GS register packets, IOP/audsrv audio, libpad input, PS2 memory arena).
- Any change that looks like a general correctness fix but is being made *for* PS2 must still go behind the ifdef. Do not touch shared files (`DX8Wrapper`, `BgfxBackend.cpp`, `FramePacer.cpp`, `GameMemory`, cmake) without a guard, even if the fix looks universally right — assume Windows/Linux/macOS/Android/iOS/Switch/WASM all currently work and must not regress.
- If a change *would* legitimately benefit other targets (e.g. a genuine bug found while porting, or a shared budgeting utility), do **not** fold it into this branch's PS2 ifdef. Instead:
  1. Land it as its own guarded or platform-neutral change, ideally cherry-picked/PR'd back onto `android-macos-iphone-windows64-vibecode-test` (or `main` if it's a real bug) independently of the PS2 work.
  2. Only add it to the "shared uplift" list below once it's landed upstream of this branch, so PS2 consumes it rather than forking it.
- New PS2-only files (VU1 microprograms, GS backend, asset repack tool) don't need ifdefs internally, but their CMake inclusion must be gated on the PS2 target so other platform builds never see them.

### Shared uplift candidates (land separately, not inside PS2 ifdefs)
Track here as they're identified during the port; move to "landed" once merged elsewhere.
- INI/template lazy-hydration (if TheThingFactory eager-load becomes a real bottleneck) — likely helps Android low-RAM devices too.
- Any additional ILP32 fixes discovered (should be rare — wasm32 already exercised this).
- Controller/UI-scaling primitives from Phase 4 below, if this experiment graduates — shared with a future Switch effort.

## Hardware constraints

| Resource | PS2 | This engine currently wants |
|---|---|---|
| CPU | 294 MHz MIPS R5900 (EE), in-order, non-IEEE FPU | 800 MHz+ PIII-class minimum historically |
| Main RAM | 32 MB | ~250–400 MB on current ports |
| VRAM (GS) | 4 MB, no shaders, fixed-function, huge flat-fill rate | ~100+ MB of TGA/DDS assets |
| Audio RAM | 2 MB (SPU2), IOP co-processor | ~600 MB music/voice/SFX |
| Disc | 4.7 GB DVD, slow seeks | 3.4 GB install |

The EE is little-endian ILP32 — same class as wasm32, already proven to run this codebase. The GS being fixed-function is actually a closer shape to the original DX8 fixed-function path than bgfx.

## Philosophy: keep systems first, cut only on measured evidence

Earlier drafts of this plan cut video, online networking, and retail/replay determinism on day one as assumptions ("PS2 obviously can't do that"). That assumption was wrong on all three counts, and the corrected rule going forward is: **keep a system until a real measurement (RAM budget, CPU profile, or code-size report) proves it doesn't fit**, not because it sounds implausible for 2000s console hardware. PS2 homebrew has working prior art for the first two, and a live in-project effort addresses the third:

- **Video is not a cut.** [Simple Media System (SMS)](https://github.com/ps2homebrew/SMS) is a real, shipped PS2 homebrew media player built on a heavily modified FFmpeg fork (DivX/XviD/MPEG-2 decode up to 1024x1024, MP3 audio), hand-tuned to use the EE's IPU hardware MPEG2 decoder and VU cycles instead of brute-force scalar decode. Cutscene/video playback is a real Phase 6+ candidate using that fork (or techniques from it) as a base, not an assumed impossibility. Budget it against IOP/EE cycles and SPU2 RAM once profiled, rather than deleting it up front.
- **Online networking is not a cut.** ps2sdk ships `smap.irx` (driver for the official PS2 Network Adapter's Ethernet chip) and `ps2ip.irx` (lwIP-based TCP/IP stack) as standard, working ports/libraries — this is real, supported hardware (SCPH-10281, and built-in Ethernet on later fat models), not a stretch. The GameSpy master-server integration specifically is dead everywhere (the service no longer exists, PS2 or otherwise) and can be dropped as dead code independent of the port, but the LAN/socket-level GameNetwork code (direct-IP play, broadcast discovery) should be kept and ported against ps2ip's BSD-socket-like API, then cut only if a real profiling pass shows it doesn't fit the RAM/CPU budget.
- **Retail/replay determinism is not a cut either.** A software FPU mode targeting cross-platform determinism (denormals + rounding-mode behavior matching x86) is in active development elsewhere in the project. If it lands, the R5900's non-IEEE hardware FPU stops being the blocker — the port would run sim math through the software path instead of hardware FPU instructions. Don't foreclose this in PS2-specific code: keep sim-side floating point behind the same abstraction that software FPU mode will hook, and revisit "permanent fork target" status once that effort has a landing point to build against, rather than assuming divergence is inherent to the hardware.

Keep this "measure, then cut" discipline for everything below the two big-ticket items too. The systems most likely to still need trimming after real measurement (informed by console generation and genre norms, not certainty) are: shadow volumes/projected shadows, the water reflection/soft-edge pass, smudge, heat effects, and particle counts — these are the fringe rendering systems the Android/water-tracks work already showed are disproportionately expensive relative to their visual contribution. Treat them as the first profiling targets in Phase 3, not as pre-decided cuts.

## Known-impossible or clearly-dead (actual cut list)

Unlike the systems above, these are not measurement-dependent — they're already dead on every platform this codebase targets, independent of PS2:

1. **GameSpy master-server integration specifically** (not the LAN networking code around it — see above). The service is defunct on every platform this codebase targets; this is general dead-code removal, not a PS2-specific sacrifice.
2. **All dev tooling, Tracy, debug builds.** MFC/Win32/DirectX-based tools obviously don't cross-compile for a freestanding MIPS target (already disabled in `cmake/toolchains/ps2.cmake` via `RTS_BUILD_*_TOOLS OFF`).

Retail/replay determinism is deliberately *not* in this list — see the software-FPU note above.

## Likely tuning knobs (confirm with real numbers before touching)

These are informed guesses about what Phase 1–3 profiling will probably force, not decisions:

- Music/EVA lines may need to move from fully-resident assets to IOP-streamed ADPCM if SPU2's 2 MB can't hold them resident — confirm against actual asset size once the audio pipeline is profiled, don't pre-cut.
- Particle counts, shadow techniques, and water effects are likely tuning targets (see above) — cut only the specific technique profiling flags as expensive, not particles/shadows/water wholesale.
- Player/map size caps will be set by the Phase 2 pathfinding/AI CPU profile, not guessed in advance.
- Single-language localization and campaign scope (Zero Hour-only vs. both) are asset-size decisions to make once the actual `.big` footprint is measured against the disc/RAM budget, not assumed.
- `RTS_GAMEMEMORY_ENABLE` may need `OFF` on PS2 the way Switch needed it (see `cmake/toolchains/ps2.cmake` note), but only flip it if the custom pool allocator is actually observed to misbehave under this GCC/newlib combination — don't disable it preemptively.

## Rendering: flat/Gouraud as the foundation, not a fallback

The GS fill-rates untextured Gouraud triangles extremely fast; its weakness is textures (4 MB VRAM, constant DMA streaming). Invert the usual priority:

**Tier 0 — flat/Gouraud (first playable target):**
- Units: no textures, per-faction/house vertex color, Gouraud-lit from existing vertex normals. This is the N64-style look, and on this hardware it's cheap and correct, not a compromise.
- Terrain: bake blended tile textures down to per-vertex color at map load (sample source tiles at each grid vertex, average) — one Gouraud mesh, near-zero VRAM.
- Water: flat translucent quad, alpha blend (GS alpha blending is free).
- UI: the one place textures stay. Control bar and cursor must remain readable — keep a persistent ~1 MB VRAM slot for UI atlases, palettized (see Tier 1).

**Tier 1 — selective palettized textures (stretch, after Tier 0 ships):**
- GS supports native 4-bit/8-bit CLUT textures. Offline pipeline quantizes a whitelist (terrain macro texture, a handful of hero unit skins) to 8-bit 64×64/128×128 CLUT (4–16 KB each, streamable). Everything else stays flat-shaded.

**Backend integration point:** not bgfx — bgfx has no GS backend and its shader-centric model doesn't fit fixed-function GS. Implement at the `DX8Wrapper` seam instead (`Set_Texture`, render state, `Draw_Indexed_Primitive` map naturally onto GS register packets — the same seam the original DX8 device already fills). Bring-up with **gsKit** first (CPU-transformed, slow but simple), then move vertex transform to a **VU1 microprogram** once Tier 0 is functionally correct. The VU1 program is the single largest chunk of new PS2-specific code and needs specialist time; there is no fallback if it stalls (see risks).

## Memory budget (32 MB, defended from day one — treat as the project's controlling document)

- Code + static data (`-Os`, `--gc-sections`): **10 MB** — measure in Phase 1 with the *full* feature set (networking and video included) before cutting anything; if the ELF alone eats 20 MB, that's the signal to start trimming, not an assumption to build in up front.
- Object/INI templates (lazy-load if measurement shows eager-load is too big): **4 MB**
- Map + pathfinding grids: **4 MB**
- Game objects/partition/AI: **5 MB** (unit cap set by Phase 2 profiling, not guessed)
- Baked terrain vertex mesh + W3D geometry (lowest LOD only, shipped on disc): **5 MB**
- Streaming/IO buffers, audio staging, network (ps2ip/smap) buffers, slack: **4 MB**

The PC-side asset repack tool is a real subproject: reads `.big` files, strips to lowest LODs, quantizes whitelisted textures, converts audio to ADPCM as needed, and emits a seek-friendly PS2 disc image. Scope of what it strips follows the measured tuning knobs above, not a fixed list decided today.

## Toolchain setup (done, reproducible)

The ps2dev/ps2sdk toolchain is installed and build-verified as of this writing:

- Installed to `C:/code/ps2dev` from the official Windows release (`ps2dev/ps2dev` GitHub releases, `ps2dev-windows-latest.tar.gz`), matching the existing `C:/code/devkitPro` convention for the Switch toolchain.
- **Known issue (Windows release only):** the `mips64r5900el-ps2-elf-*` binaries are 32-bit (x86) and the release tarball does not bundle their MinGW32 runtime DLLs. Symptom is a silent exit code with no diagnostic text (or, under Git Bash, a garbled `error while loading shared libraries: ?: cannot open shared object file`). Fixed by sourcing `libiconv-2.dll`, `libwinpthread-1.dll`, `libcharset-1.dll`, `libzstd.dll`, `libgcc_s_dw2-1.dll`, `libstdc++-6.dll`, `libatomic-1.dll`, `libgomp-1.dll`, `libquadmath-0.dll`, `libgmp-10.dll`, `libgmpxx-4.dll`, `libisl-23.dll`, `libmpc-3.dll`, `libmpfr-6.dll` from the official MSYS2 `mingw32` package repo (`mirror.msys2.org/mingw/mingw32/`) and copying them into **all four** directories that hold a toolchain binary GCC actually invokes: `ee/bin`, `ee/libexec/gcc/mips64r5900el-ps2-elf/15.2.0`, `ee/mips64r5900el-ps2-elf/bin` (the sysroot-relative `as`/`ld`/`ar` GCC's driver resolves at link time — easy to miss), and the `iop/` equivalents of the last two.
- `mips64r5900el-ps2-elf-gcc-ar`/`-gcc-ranlib` wrappers crash outright even with the DLLs present (unrelated cause, not chased down) — use plain `mips64r5900el-ps2-elf-ar`/`-ranlib` instead. Already reflected in `cmake/toolchains/ps2.cmake`.
- CMake gotcha: `target_link_libraries()` treats the bare word `debug` as a reserved build-configuration keyword (like `optimized`/`general`), not a literal library name — since ps2sdk's console library is itself named `libdebug.a`, this silently drops it. Use `-ldebug` (flag form) instead.
- `cmake/toolchains/ps2.cmake` encodes all of the above; a standalone smoke-test target lives at `ps2-port/bringup/` (not part of the main engine build) and has been built and run successfully in PCSX2, confirming the full compile → link → boot pipeline works before any engine code is touched.
- PCSX2 (2.6.3) is set up with a real BIOS dump and `[EmuCore/CPU] ExtraMemory = true` in `PCSX2.ini` (Settings > Advanced > "Enable 128MB RAM (Dev Console)" in the UI) — this is real devkit hardware behavior PCSX2 emulates, not a hack, and gives headroom to prototype against before narrowing to the retail 32 MB budget. Confirmed via emulog: `128MB RAM is enabled`. Boot a built ELF directly with `pcsx2-qt.exe -batch -elf <path> -fastboot`.

## Phases

1. **Toolchain + headless sim boot (go/no-go gate).** Toolchain is done (see above). Remaining goal: `-headless -replay` — sim only, **no renderer** — running on real hardware or PCSX2 (128MB dev-console RAM mode first, for headroom; narrow to 32 MB once something boots) with the *full, untrimmed* feature set. This single milestone proves the game fits, or shows exactly what doesn't, before any rendering time is spent or anything gets cut on assumption.
2. **Sim performance pass.** Profile pathfinding/AI on the EE at whatever unit count the full feature set first boots with; tune caps down only as far as the profile demands (logic is already fixed-step 30 fps, which suits this hardware well).
3. **Renderer Tier 0.** gsKit bring-up → shell map flat-shaded → in-match. Reuse the win32-bgfx A/B harness discipline: compare against the flat-shaded reference image, not retail.
4. **Input + UI.** libpad directly (SDL optional, likely unnecessary overhead on a 32 MB budget). Controller scheme: cursor-on-stick plus radial/hotkey menu. Design this as shared work behind ifdefs — a future Switch controller effort wants the same primitives — per the shared-uplift note above.
5. **Audio + networking bring-up.** audsrv for SFX/music (trim to streamed ADPCM only if SPU2's 2 MB proves too small); `ps2ip.irx`/`smap.irx` for LAN/direct-IP play (drop only the dead GameSpy master-server layer, not the socket code around it).
6. **VU1 transform path + Tier 1 textures.** Only after Phases 1–5 hold on real hardware.
7. **Video.** Evaluate the SMS FFmpeg-for-PS2 fork against the IPU/EE cycle budget once everything else is profiled; scope cutscene support to what the budget actually allows.
8. **Disc mastering + soak test.** PCSX2 is not ground truth for seek behavior; real DVD seek timing will reshape loading-screen placement and streaming granularity.

## Risks, ranked

1. **Code size.** The ELF may not leave room for the game, especially keeping networking and (eventually) video resident. Mitigate with aggressive dead-stripping, possibly a shell/UI overlay split. Measure in Phase 1 — this is the most likely project-ending risk, and the reason Phase 1 tests the full feature set rather than a pre-trimmed one.
2. **INI/template RAM.** `TheThingFactory` loads everything eagerly today. May need real lazy-hydration engineering, not just INI pruning (see shared-uplift candidates).
3. **Pathfinding CPU.** A 294 MHz in-order MIPS may not hold 30 Hz logic at PC-typical unit counts; Phase 2's profile sets the real cap, which may land well below what feels like Generals.
4. **VU1 expertise.** Tier 0 on gsKit alone might land around ~15 fps; the VU1 microprogram is specialist work with no fallback if it stalls.
5. **Video/EE cycle contention.** Even with SMS's PS2-tuned FFmpeg fork as a base, decode cost competes directly with sim + render cycles on the same EE core; Phase 7 may still land on cutting or shrinking video after real measurement, just not by assumption on day one.
