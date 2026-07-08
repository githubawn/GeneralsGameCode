# Plan: Backport GeneralsOnline (GO) Multiplayer to TheSuperHackers (TSH)

Goal: bring the GeneralsOnline "NGMP" (NextGen Multiplayer) online stack — auth, lobbies,
matchmaking, P2P transport, social/stats — from
`GeneralsOnlineDevelopmentTeam/GameClient` into `TheSuperHackers/GeneralsGameCode`,
as an optional, cleanly-gated feature that does not regress TSH's build matrix
(VC6 + MSVC, Generals + GeneralsMD) or its LAN/replay/GameSpy code paths.

The GO fork is available locally as remote `gameclient` (branch `gameclient/main`).
All analysis below is against merge base `bf8d5be02` (TSH PR #2707).

---

## 1. What we learned from the diff

### Scale
- GO diverged from TSH at `bf8d5be02` (~May 2026); TSH main has since advanced ~140 PRs.
- GO carries **1,178 commits, 506 changed files, +136k/−49k lines** on top of the merge base.
- Most of that is NOT multiplayer: wholesale reformatting (they re-indented entire files,
  reverted `nullptr` back to `NULL`), gameplay/balance tweaks in GameLogic gated by
  `GENERALS_ONLINE`, camera/QoL changes, Sentry crash reporting, stats uploaders,
  a self-updater, and vendored binary blobs.
- The actual multiplayer core ("NGMP") is **~14,700 lines of first-party code** in a
  self-contained tree, plus ~101 engine files touched with `#if defined(GENERALS_ONLINE)`
  blocks (many of which are gameplay tweaks we should NOT take).

### Architecture of NGMP (what we want)
All under `GeneralsMD/Code/GameEngine/{Include,Source}/GameNetwork/GeneralsOnline/`:

| Component | Files | Role |
|---|---|---|
| Services manager | `OnlineServices_Init.*` | Singleton `NGMP_OnlineServicesManager`; REST endpoint resolution, version check, WebSocket connection, service config |
| Auth | `OnlineServices_Auth.*` | Login/account (with credential encryption via libsodium) |
| Lobby | `OnlineServices_LobbyInterface.*` | Lobby CRUD, slots, chat — replaces GameSpy peer rooms |
| Rooms/Social/Stats/Matchmaking | `OnlineServices_*.*` | Chat rooms, buddies, persisted stats, MM |
| Game glue | `NGMPGame.*` | `NGMPGame : public GameInfo`, `NGMPGameSlot : public GameSlot` — plugs into the existing game-setup flow |
| Transport | `NextGenTransport.*`, `NetworkMesh.*`, `NetworkBitstream.*`, `NetworkPacket.*` | Valve GameNetworkingSockets (GNS) P2P mesh with relay/signaling via GO backend; replaces raw UDP `Transport` |
| HTTP | `HTTP/HTTPManager.*`, `HTTP/HTTPRequest.*` | libcurl-based async HTTP + WebSockets |
| Settings/defines | `GeneralsOnline_Settings.*`, `NextGenMP_defines.h` | `GENERALS_ONLINE` master define + feature toggles |
| Plugins | `PluginInterfaces.*` | Optional plugin hooks |

Key engine refactor that comes with it:
- `Transport` (Core `GameNetwork/Transport.h`) becomes an **abstract base class**;
  the original UDP implementation moves to new `UDPTransport.{h,cpp}` (GeneralsMD);
  `NextGenTransport` is the GNS-backed implementation. LAN keeps using UDPTransport.
- `NetworkInterface`/`ConnectionManager`/`Connection` gain `GENERALS_ONLINE`-gated
  virtuals (`SeedLatencyData`, `IsSlugging`, `setSawCRCMismatch(UnicodeString&)`,
  `GetConnectionManager`).
- The existing **WOL/GameSpy GUI screens are reused**, not replaced: `WOLLoginMenu`,
  `WOLLobbyMenu`, `WOLGameSetupMenu`, `PopupHostGame`, `PopupJoinGame`, `PopupPlayerInfo`,
  `WOLBuddyOverlay`, `ScoreScreen`, etc. get NGMP callbacks/ifdefs so the same layouts
  drive the new backend (no new `.wnd` GUIs needed — same philosophy as our other projects).
- `WinMain.cpp`, `GameEngine`/`GameLogic` init: NGMP services manager creation, per-frame
  tick, shutdown.

### Vendored dependencies (GO commits binaries; TSH must not)
- **Valve GameNetworkingSockets** — `GameNetworkingSockets.dll/.lib` + abseil, protobuf,
  OpenSSL 3 (`libcrypto-3.dll`, `libssl-3.dll`), `webrtc-lite.lib`, `steamwebrtc.lib` (BSD-3)
- **libcurl** — `libcurl.dll/.lib` + OpenSSL + zlib (curl license; WebSocket support required)
- **libsodium** — `libsodium.lib` (ISC) — credential encryption
- **libplum** (MPL-2.0), **miniupnpc** (BSD), **libnatpmp** (BSD) — source-vendored,
  NAT traversal / port mapping
- **sentry-native** — `sentry.lib` + hardcoded DSN (crash reporting — exclude or make ours)
- **nlohmann json** (`json.hpp`, MIT), **stb_image_resize/write** (public domain)

### Hard constraints discovered
1. **C++20 required**: NGMP uses `std::format`, `std::function`, lambdas, `std::map`,
   threads throughout. **VC6 cannot compile any of it.** GO simply deleted the VC6 and
   vanilla-Generals CI jobs. TSH must instead gate the whole feature.
2. **Backend**: the client talks to `https://api.playgenerals.online/...`
   (REST contract `/env/prod/contract/1/<endpoint>` + `wss://.../ws` WebSocket) plus GNS
   relay/signaling servers. The server side is not open source, but **the GO devs are in
   the TSH Discord and backend access is not a problem** — coordinate contract version,
   test-env access, and client-version handshake with them directly (Phase 0.3).
3. **ZH-only**: GO implements NGMP for GeneralsMD only (`GENERALS_ONLINE_GAMETYPE_ZEROHOUR`);
   vanilla Generals gets nothing. Backport scope should match (ZH first).
4. **Base drift**: GO's base is ~140 PRs behind TSH main and reformats whole files.
   `git merge` is unusable; this must be a **curated, file-by-file port**, with the NGMP
   tree lifted mostly verbatim and the engine touchpoints re-derived by reading their
   ifdef hunks.
5. GO bundles behavior TSH won't want on by default: `DISABLE_DEBUG_CRASHING`,
   `GENERALS_ONLINE_DISABLE_TEXTURE_FILTERING_AND_AA`, forced terrain-draw hack,
   GameMemory re-enablement, self-updater, Sentry with GO's DSN, stats uploader,
   gameplay balance ifdefs in ~40 GameLogic files. **All excluded from scope.**

---

## 2. Guiding decisions

- **VC6 never sees NGMP** (accepted constraint — hide the files, keep VC6 green):
  - New CMake option `RTS_BUILD_OPTION_GENERALS_ONLINE`, forced OFF for VC6
    (`CMAKE_DEPENDENT_OPTION` on `MSVC AND NOT RTS_BUILD_OPTION_VC6`-style guard);
    it defines `GENERALS_ONLINE` on the GeneralsMD target when ON.
  - The entire `GameNetwork/GeneralsOnline/` tree (and `UDPTransport`'s NGMP-only
    siblings) is added to `GAMEENGINE_SRC` only inside `if(RTS_BUILD_OPTION_GENERALS_ONLINE)`
    — VC6 never compiles, includes, or even lists these files.
  - Every touchpoint in shared/Core code is wrapped in `#if defined(GENERALS_ONLINE)`,
    including the NGMP `#include` lines, so the VC6 preprocessor drops them entirely.
    Rule: **no NGMP header may be included outside a `GENERALS_ONLINE` guard**, and no
    C++20 may leak into shared headers outside guards.
  - The only unconditional shared-code change (the `Transport` → `UDPTransport`
    base-class split, Phase 1.1) must itself stay VC6-clean — validated by a VC6
    build in that PR's CI run.
  - CI: all existing jobs stay green with the flag OFF; one new MSVC job builds ON.
- **Dependencies from source/package, not blobs**: use vcpkg or FetchContent for
  GameNetworkingSockets, curl, libsodium, nlohmann-json; keep libplum/miniupnpc/libnatpmp
  as source-vendored (as GO does — they compile as plain C/C++). No `.dll`/`.lib`/`.pdb`
  committed to the repo. (TSH already has a `win32-vcpkg` preset stub to build on.)
- **Minimal-diff engine touchpoints**: port only the `GENERALS_ONLINE` hunks that serve
  networking/lobby/UI; skip formatting churn and gameplay ifdefs. Where GO changed a
  shared signature unconditionally (e.g. `Transport` base-classing), prefer the
  unconditional refactor if it's a genuine improvement TSH would accept standalone —
  split those into their own preparatory PRs.
- **No regression to LAN/replays/GameSpy**: with the flag OFF the binary must be
  byte-for-byte-equivalent in behavior; with it ON, LAN and replays must still work
  (GO shipped this way, so the seams exist).
- **Retail compatibility**: NGMP does not change game sim; CRC-compat with retail is
  unaffected when flag OFF.

---

## 3. Phases

### Phase 0 — Scoping spike (1–2 days)
0.1 Extract the exact engine-touchpoint list: for each of the ~101 files with
    `GENERALS_ONLINE` outside the NGMP tree, classify each hunk as
    **transport/lobby/UI/init** (port) vs **gameplay/QoL/crash-reporting** (skip).
    Produce `PatchNotes/go-backport-touchpoints.md` as the working checklist.
0.2 Confirm dependency build: get GameNetworkingSockets + curl(+websockets) + libsodium
    building via vcpkg against a TSH MSVC preset. This is the highest technical risk
    after the backend question — do it before committing to the approach.
0.3 Coordinate with the GO devs (available in the TSH Discord): agree which GO release
    tag to port from, contract/net/service version to target, access to their test
    environment (`USE_TEST_ENV` / dev contract), and how TSH-built clients identify
    themselves to the version-check/update flow (so it doesn't force-update them away).

### Phase 1 — Preparatory refactors (flag-independent, small PRs)
1.1 Port the `Transport` → abstract base + `UDPTransport` split, unconditionally
    (it's a clean seam; LAN/GameSpy use `UDPTransport`). Verify VC6 still builds —
    the base class must stay VC6-clean (no C++20 in Core headers).
1.2 Port `NetworkInterface`/`ConnectionManager` seam additions — but behind
    `#if defined(GENERALS_ONLINE)` as GO did, so VC6/vanilla builds are untouched.
1.3 Add `RTS_BUILD_OPTION_GENERALS_ONLINE` CMake option + empty
    `GameNetwork/GeneralsOnline/` skeleton compiled only when ON; wire the new CI job.
    (Same scaffolding-first discipline as the engine-swap project.)

### Phase 2 — Lift the NGMP tree
2.1 Copy the ~30 first-party NGMP files (14.7k lines) into
    `GeneralsMD/Code/GameEngine/{Include,Source}/GameNetwork/GeneralsOnline/`,
    adapting includes to TSH-current headers (their base is 140 PRs old — expect
    compile fixes against renamed/moved TSH APIs).
2.2 Replace vendored-blob linkage with vcpkg/FetchContent targets; keep
    libplum/miniupnpc/libnatpmp as source under a `Vendor/` subdir (with upstream
    version + license files noted).
2.3 Strip GO-specific policy from `NextGenMP_defines.h`: remove
    `DISABLE_DEBUG_CRASHING`, texture-filtering/terrain hacks, Sentry, self-updater,
    `VANILLA_INI_CRC` pin; make endpoint base URLs configurable (INI/registry/
    command line) instead of hardcoded, defaulting per the Phase 0.3 decision.
2.4 Get it compiling with the flag ON (no UI hookup yet).

### Phase 3 — Engine + UI integration
3.1 Init/shutdown/tick: `WinMain.cpp`, engine init, per-frame service pump.
3.2 Port the WOL-screen ifdef hunks from the touchpoint checklist:
    `WOLLoginMenu`, `WOLWelcomeMenu`, `WOLLobbyMenu`, `WOLGameSetupMenu`,
    `WOLMapSelectMenu`, `WOLQuickMatchMenu`, `WOLBuddyOverlay`, `PopupHostGame`,
    `PopupJoinGame`, `PopupPlayerInfo`, `ScoreScreen`, `InGameChat`, `Diplomacy`,
    `MessageBox`, `MainMenu`, `GameSpyOverlay` glue, disconnect screens.
3.3 Port `NGMPGame`/`NGMPGameSlot` wiring into game start
    (`GameLogic`, `GameLogicDispatch`, `ConnectionManager::parseUserList` path,
    CRC-mismatch reporting with details string).
3.4 Port `FirewallHelper`/NAT changes only if the GNS path needs them
    (GNS relays may make the old NAT negotiation redundant for NGMP games).

### Phase 4 — Verification
4.1 Flag OFF: full build matrix green (VC6 + MSVC, Generals + GeneralsMD + tools);
    behavior unchanged (LAN game, replay playback, skirmish).
4.2 Flag ON: login → lobby → host/join → 2+ player game vs test backend
    (`USE_TEST_ENV` equivalent / localhost:9000 dev contract), desync-free full match,
    disconnect handling, rejoin/exit flows. LAN and replays still work in the same binary.
4.3 Cross-version sanity: NGMP client version handshake (`GENERALS_ONLINE_NET_VERSION`)
    against the live service if the GO team sanctions it.

### Phase 5 — Upstreaming
5.1 Split into reviewable PR series for TSH:
    (a) Transport/UDPTransport refactor, (b) CMake option + deps, (c) NGMP tree,
    (d) engine seams, (e) UI hookup, (f) CI job.
5.2 Documentation: build instructions for the flag, backend configuration,
    credits/licenses for vendored code, and a note that GO remains the upstream
    for NGMP protocol changes (define a re-sync process for future GO releases —
    their `GENERALS_ONLINE_VERSION_STRING` tags map to release branches).

---

## 4. Open questions (to resolve in Phase 0)

~~Backend consent~~ — **resolved**: GO devs are in the TSH Discord; backend access is
not a problem. Remaining coordination items folded into Phase 0.3.

1. **Version-check/update flow for TSH builds**: GO's client does a forced-update dance
   against `VersionCheck` keyed on `GENERALS_ONLINE_VERSION_STRING`; agree with the GO
   devs how a TSH-built client passes this (own version channel, or exemption).
2. **vcpkg vs FetchContent** for GNS/curl/sodium: TSH has a dormant `win32-vcpkg`
   preset; confirm maintainers' preferred dependency mechanism.
3. **Sentry**: drop entirely, or make DSN a build-time option? (Default: drop.)
4. **Vanilla Generals support**: GO has a `GENERALS_ONLINE_GAMETYPE_GENERALS` define
   suggesting planned support; out of scope here but the CMake gating should not
   preclude it.

## 5. Risks

| Risk | Impact | Mitigation |
|---|---|---|
| 140-PR base drift → subtle API mismatches | Compile/runtime bugs in ported code | Curated port with per-hunk review, not merge; compile early (Phase 2.4) |
| NGMP header/C++20 leak into a VC6-visible path | VC6 build breaks | Guard rule (no NGMP include outside `GENERALS_ONLINE`); VC6 job in CI on every PR |
| Dependency build complexity (GNS pulls protobuf/abseil/OpenSSL) | Build-time pain for contributors | vcpkg manifest pinning; feature OFF by default so casual builds never pay it |
| Hidden coupling to GO's skipped changes (GameMemory, stats, gameplay ifdefs) | NGMP code assumes excluded behavior | Touchpoint audit in Phase 0.1 catches references; stub or port minimally |
| GO protocol evolves while we port | Version-check rejects our client | Track a tagged GO release, not `main`; re-sync process in 5.2 |
| LAN/replay regression with flag ON | Breaks TSH's core promise | Phase 4.2 explicitly tests LAN+replay in the ON build |

## 6. Effort estimate

- Phase 0: 1–2 days. Phase 1: 2–3 days. Phase 2: 1 week (deps + compile-fix churn).
- Phase 3: 1–2 weeks (UI seams are many but mechanical, guided by GO's diffs).
- Phase 4–5: 1 week + review latency.
- Total: **~4–6 weeks** of focused work, dominated by integration/verification.
