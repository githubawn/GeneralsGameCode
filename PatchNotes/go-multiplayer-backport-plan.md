# Plan: Backport GeneralsOnline (GO) Multiplayer to TheSuperHackers (TSH)

Goal: make the TSH repo able to compile **both clients** — the stock TSH build and a
full GeneralsOnline build — from one tree. That means porting **everything** from
`GeneralsOnlineDevelopmentTeam/GameClient` into `TheSuperHackers/GeneralsGameCode`:
the NGMP online stack (auth, lobbies, matchmaking, P2P transport, social/stats) plus
GO's gameplay changes, stats uploaders, updater, and client policy — all fully gated
behind one build flag, so with the flag OFF nothing regresses TSH's build matrix
(VC6 + MSVC, Generals + GeneralsMD) or its LAN/replay/GameSpy code paths, and with
the flag ON the result is feature- and sim-equivalent to GO's official client.

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
- The multiplayer core ("NGMP") is **~14,700 lines of first-party code** in a
  self-contained tree, plus ~101 engine files touched with `#if defined(GENERALS_ONLINE)`
  blocks — a mix of networking/UI hooks and gameplay/QoL changes. **All of it is in
  scope** (full GO parity); only the formatting churn and `nullptr`→`NULL` reverts
  are discarded.

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
3. **GO implemented ZH-only, but our scope includes vanilla Generals**: GO selects the
   game via `GENERALS_ONLINE_GAMETYPE_ZEROHOUR` / `GENERALS_ONLINE_GAMETYPE_GENERALS`
   defines, so the code was written to be gametype-switchable — the Generals side was
   just never wired up. TSH's layout makes this natural: host the NGMP tree in
   `Core/GameEngine/.../GameNetwork/GeneralsOnline/` (where shared GameNetwork code
   already lives) with per-game gametype defines and UI hookups. ZH lands first
   (it's what GO validated against their service); Generals follows as Phase 3b.
4. **Base drift**: GO's base is ~140 PRs behind TSH main and reformats whole files.
   `git merge` is unusable; this must be a **curated, file-by-file port**, with the NGMP
   tree lifted mostly verbatim and the engine touchpoints re-derived by reading their
   ifdef hunks.
5. GO bundles client policy beyond networking: `DISABLE_DEBUG_CRASHING`,
   `GENERALS_ONLINE_DISABLE_TEXTURE_FILTERING_AND_AA`, forced terrain-draw hack,
   GameMemory re-enablement, self-updater, Sentry crash reporting, stats uploader,
   gameplay balance/QoL ifdefs in ~40 GameLogic files. **All included, all gated** —
   with the flag ON these must behave exactly as in GO's client (the gameplay ifdefs
   change the simulation, so sim-parity with official GO clients depends on porting
   them faithfully); with the flag OFF none of it exists in the binary.

---

## 2. Guiding decisions

- **VC6 never sees NGMP** (accepted constraint — hide the files, keep VC6 green):
  - New CMake option `RTS_BUILD_OPTION_GENERALS_ONLINE`, forced OFF for VC6
    (`CMAKE_DEPENDENT_OPTION` on `MSVC AND NOT RTS_BUILD_OPTION_VC6`-style guard);
    it defines `GENERALS_ONLINE` on the game targets (GeneralsMD, and Generals once
    Phase 3b lands) when ON, alongside the matching `GENERALS_ONLINE_GAMETYPE_*` define.
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
- **Dependencies via `FetchContent`, not blobs**: TSH already vendors bink/dx8/gamespy/
  miles/stlport/tracy/zlib this way (`cmake/*.cmake`, one `FetchContent_Declare` +
  `FetchContent_MakeAvailable` each) — GameNetworkingSockets, curl, libsodium, and
  nlohmann-json follow the same house pattern in a new `cmake/generals-online.cmake`.
  libplum/miniupnpc/libnatpmp stay source-vendored (plain C/C++, no build system needed).
  No `.dll`/`.lib`/`.pdb` committed to the repo.
- **Everything ports, everything gated**: every `GENERALS_ONLINE` hunk in GO — networking,
  UI, gameplay, telemetry, updater — ports verbatim behind the define. No cherry-picking
  of behavior; the flag-ON build is GO, the flag-OFF build is TSH. Only formatting churn
  and their `nullptr`→`NULL` reverts are dropped (re-apply their logic onto TSH-current
  file contents). Where GO changed a shared signature unconditionally (e.g. `Transport`
  base-classing), take the unconditional refactor as its own preparatory PR if it stands
  alone as an improvement; otherwise gate it too.
- **Sim-parity with GO when ON**: the gameplay ifdefs alter the simulation, so the
  flag-ON build must desync-free-match official GO clients. This makes faithful,
  complete porting of the GameLogic hunks a correctness requirement, not optional
  polish — verify by cross-playing against an official GO client (Phase 5.2).
- **No regression when OFF**: with the flag OFF the binary must be behavior-equivalent
  to TSH main — LAN, replays, GameSpy, retail CRC-compat all untouched.

---

## 3. Phases

Three deliverables, in order, each landing as its own PR series and verifiable on
its own:

- **Part A (Phases 0–4)** — the GO multiplayer stack itself.
- **Part B (Phase 5)** — GO's gameplay fixes/changes. Cross-play sim-parity with
  official GO clients is only achievable after this part, since these ifdefs alter
  the simulation; Part A's flag-ON testing is therefore TSH-vs-TSH.
- **Part C (Phase 6)** — extensions: Sentry, self-updater, stats upload, client policy.
  Where Part A's NGMP code calls into these (Sentry init, stats hooks), stub the
  call sites behind secondary defines so Part A compiles and runs without them —
  the Phase 0.1 audit flags every such reference.

### Part A — GO multiplayer

### Phase 0 — Scoping spike (1–2 days)
0.1 Extract the exact engine-touchpoint list: for each of the ~101 files with
    `GENERALS_ONLINE` outside the NGMP tree, catalog each hunk and tag it
    **transport / lobby / UI / init / gameplay / telemetry / updater** — everything
    ports, the tags drive PR grouping and review focus (gameplay hunks get sim-parity
    scrutiny). Produce `PatchNotes/go-backport-touchpoints.md` as the working checklist.
    Also catalog GO's changes that are NOT under `GENERALS_ONLINE` guards (e.g. the
    GameMemory CMake re-enablement, `StatsExporter`/`StatsUploader`, `snprintf` fixes)
    and decide per-item: gate it, or adopt it unconditionally.
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
    `Core/GameEngine/{Include,Source}/GameNetwork/GeneralsOnline/` (Core, not
    GeneralsMD — both games consume it; the gametype define selects behavior),
    adapting includes to TSH-current headers (their base is 140 PRs old — expect
    compile fixes against renamed/moved TSH APIs). Anything genuinely ZH-specific
    stays in a thin GeneralsMD-side file.
2.2 Replace vendored-blob linkage with vcpkg/FetchContent targets; keep
    libplum/miniupnpc/libnatpmp as source under a `Vendor/` subdir (with upstream
    version + license files noted).
2.3 Split `NextGenMP_defines.h` by part: multiplayer defines stay active in Part A;
    the gameplay/policy toggles (`DISABLE_DEBUG_CRASHING`, texture-filtering/terrain
    hacks) and extension hooks (Sentry, self-updater, stats) are preserved but
    deactivated behind their Part B/C secondary defines until those parts land.
    Endpoint base URLs stay as GO ships them (per Phase 0.3 agreement), with an
    override for the dev/test contract.
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

### Phase 3b — Vanilla Generals wiring
3b.1 Enable the flag for the Generals target with `GENERALS_ONLINE_GAMETYPE_GENERALS`;
     fix whatever that define path never compiled against (it's untested in GO —
     expect real work here, not just flipping the define).
3b.2 Repeat the 3.1–3.3 hookups for Generals' WinMain, WOL screens, and game-start
     path (Generals' WOL/GameSpy menu code diverges slightly from ZH's).
3b.3 Backend coordination: confirm with the GO devs how vanilla-Generals games are
     represented service-side (separate lobby pool / gametype field / net version).

### Phase 4 — Part A verification
4.1 Flag OFF: full build matrix green (VC6 + MSVC, Generals + GeneralsMD + tools);
    behavior unchanged (LAN game, replay playback, skirmish).
4.2 Flag ON, TSH-vs-TSH: login → lobby → host/join → 2+ player game vs test backend
    (`USE_TEST_ENV` equivalent / localhost:9000 dev contract), desync-free full match,
    disconnect handling, rejoin/exit flows. (Cross-play vs official GO clients waits
    for Part B — the sim differs until the gameplay hunks are in.)

### Part B — GO gameplay fixes (Phase 5)
5.1 Port the `GENERALS_ONLINE` gameplay/QoL hunks (~40 GameLogic files — Weapon,
    StealthUpdate, EMPUpdate, SlavedUpdate, TurretAI, slow-death behaviors, etc. —
    plus W3D/client hunks) from the Phase 0.1 checklist, grouped by system into
    reviewable PRs. Everything stays behind the flag; port faithfully — these are
    sim-parity-critical, not up for local improvement.
5.2 Verify sim parity: desync-free cross-play matches against an **official GO
    client** on the live/test service; replay exchange between the two clients.

### Part C — Extensions (Phase 6)
6.1 Sentry crash reporting: port the integration; DSN per Phase 0.3 discussion with
    GO devs (their DSN for GO-flavored builds, or build-time option).
6.2 Self-updater and version-check/forced-update flow (agree how TSH-built clients
    are versioned so the service accepts them).
6.3 `StatsExporter`/`StatsUploader` and persisted-stats upload.
6.4 Client policy toggles: `DISABLE_DEBUG_CRASHING`, texture-filtering/AA and
    terrain-draw workarounds, GameMemory re-enablement — activate their defines,
    matching GO behavior exactly when the flag is ON.

### Phase 7 — Upstreaming & docs (continuous, finalized here)
7.1 PR series per part: A = (Transport/UDPTransport refactor, CMake option + deps,
    NGMP tree, engine seams, UI hookup, Generals wiring, CI job); B = per-system
    gameplay PRs; C = per-extension PRs.
7.2 Documentation: build instructions for the flag, backend configuration,
    credits/licenses for vendored code, and a note that GO remains the upstream
    for NGMP protocol changes (define a re-sync process for future GO releases —
    their `GENERALS_ONLINE_VERSION_STRING` tags map to release branches).

---

## 4. Open questions (to resolve in Phase 0)

~~Backend consent~~ — **resolved**: GO devs are in the TSH Discord; backend access is
not a problem. Remaining coordination items folded into Phase 0.3.

1. **Version-check/update flow for TSH builds**: GO's client does a forced-update dance
   against `VersionCheck` keyed on `GENERALS_ONLINE_VERSION_STRING`; agree with the GO
   devs how a TSH-built client passes this — matters for Part A already (login path),
   fully resolved in Phase 6.2.
2. **vcpkg vs FetchContent** for GNS/curl/sodium: TSH has a dormant `win32-vcpkg`
   preset; confirm maintainers' preferred dependency mechanism.
3. **Sentry DSN** (Sentry = crash-reporting telemetry; uploads crash minidumps to a
   sentry.io dashboard via a key hardcoded to GO's account): when Part C ports it,
   do TSH-built GO-flavored clients report to GO's dashboard (GO devs' call) or a
   TSH one? Build-time option keeps both possible.
4. **Vanilla Generals service-side representation**: Generals is in scope (Phase 3b);
   confirm with GO devs how the backend distinguishes Generals vs ZH lobbies/matches.

## 5. Risks

| Risk | Impact | Mitigation |
|---|---|---|
| 140-PR base drift → subtle API mismatches | Compile/runtime bugs in ported code | Curated port with per-hunk review, not merge; compile early (Phase 2.4) |
| NGMP header/C++20 leak into a VC6-visible path | VC6 build breaks | Guard rule (no NGMP include outside `GENERALS_ONLINE`); VC6 job in CI on every PR |
| Part A NGMP code references Part B/C symbols (stats hooks, Sentry init, notification UI) | Part A doesn't compile standalone | Phase 0.1 audit flags every cross-part reference; stub behind secondary defines |
| A missed or mis-ported gameplay hunk | Desyncs vs official GO clients — hard to trace | Port Part B hunks verbatim from the checklist; cross-play + replay-exchange verification (5.2) |
| GO protocol evolves while we port | Version-check rejects our client | Track a tagged GO release, not `main`; re-sync process in 7.2 |
| Generals gametype path untested in GO | Phase 3b uncovers latent bugs in NGMP's `GENERALS_ONLINE_GAMETYPE_GENERALS` branches | Land ZH first; treat 3b as its own verification cycle with GO devs |

Not considered risks: LAN/replay behavior differences between flag-ON and flag-OFF
builds (VS2022 and VC6 builds are already mutually incompatible for multiplayer, so
per-build-flavor separation is the accepted status quo), and dependency build
complexity (vcpkg handles GNS/curl/sodium; flag is OFF by default anyway).

## 6. Effort estimate

- Part A: Phase 0: 1–2 days; Phase 1: 2–3 days; Phase 2: 1 week (deps + compile-fix
  churn); Phase 3: 1–2 weeks (UI seams are many but mechanical, guided by GO's diffs);
  Phase 3b: ~1 week (untested gametype path); Phase 4: 2–3 days.
- Part B: ~1 week (mechanical porting from checklist) + cross-play verification.
- Part C: ~1 week across the three extensions.
- Total: **~6–9 weeks** of focused work + review latency, dominated by
  integration/verification. Part A alone (playable multiplayer, TSH-vs-TSH) is
  ~4–5 weeks and is a shippable milestone.
