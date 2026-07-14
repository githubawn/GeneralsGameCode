# Part B touchpoint checklist: GO's gameplay hunks

Phase 0.1 deliverable for `go-multiplayer-backport-plan.md`. Catalogs every
`GENERALS_ONLINE`-gated hunk in GO's `GameLogic` tree (merge base `bf8d5be02`,
GO tip as vendored under remote `gameclient/main`), so Part B can be ported as
reviewable, per-system PRs. Not yet ported — Part A (multiplayer stack +
WOL-screen wiring) is done; this is the next unclaimed slice of work.

Method: `git grep -l "GENERALS_ONLINE" gameclient/main -- "*/GameLogic/*"`,
excluding the `GeneralsOnline/` NGMP tree itself, then read every hit with
context. 24 files matched. Two of them (`Scripts.cpp` x2) turned out to be
false positives — see below.

## The headline finding: this is almost entirely one change

GO runs the game-logic sim at **60 ticks/sec instead of 30**
(`GENERALS_ONLINE_HIGH_FPS_SERVER`, `GENERALS_ONLINE_HIGH_FPS_FRAME_MULTIPLIER
= 2`, both already defined in our vendored `NextGenMP_defines.h`). Every
INI-authored value in the game — weapon delays, physics forces, animation
timers, turret turn rates — was tuned assuming 30 ticks/sec. Doubling the tick
rate without compensation would make everything happen twice as fast (turrets
snap instantly, forces apply twice as often, timers expire in half the real
time). Almost every hunk below exists purely to cancel that out, via one of
three mechanical patterns:

1. **Halve the per-tick delta.** Forces, angular rates, and animation
   increments get divided by `GENERALS_ONLINE_HIGH_FPS_FRAME_MULTIPLIER` (2)
   so the *per-second* effect matches retail.
2. **Gate the update to run on alternate ticks.** `TheGameLogic->
   HasLegacyFrameAdvanced()` returns true only every other real tick; code
   that shouldn't double its cadence (pathfinding queue processing, turret AI
   state updates, sleepy-update modules) early-returns when it's false.
3. **Use a "legacy frame" counter for INI-authored delay comparisons.**
   `GameLogic` keeps a second counter, `m_frameLegacy`, that increments once
   every two real ticks — so `TheGameLogic->getFrameLegacy()` counts at the
   same rate a retail 30fps game would, and delay-frame math
   (`now - startFrame >= data->m_someDelayFromINI`) keeps working unmodified
   against INI values authored for 30fps.

This is why Part B reads as repetitive: it isn't ~40 independent gameplay
changes, it's the same three-pattern compensation applied file-by-file
everywhere a timer, force, or per-tick delta touches the sim. **Porting it
faithfully is mechanical but high-stakes** — miss one spot and that one
object type will desync against an official GO client (runs 2x too fast or
too slow), which is exactly the failure mode Phase 5.2's cross-play
verification exists to catch.

The multiplier lives in one place (`GENERALS_ONLINE_HIGH_FPS_FRAME_MULTIPLIER`
in `NextGenMP_defines.h`, `= GENERALS_ONLINE_HIGH_FPS_LIMIT/30`, currently 2
since `GENERALS_ONLINE_HIGH_FPS_LIMIT` is 60) — if GO ever changes the server
tick rate, this constant is the only thing that needs to move, and every hunk
below is written generically against it rather than hardcoding "2" or "half".

### `GameLogic.h` / `GameLogic.cpp` — the mechanism itself

This is the file to port first; everything else depends on it existing.

- **`GameLogic.h`**: adds `m_frameLegacy`/`m_frameLegacyLast` members (only
  under `GENERALS_ONLINE_HIGH_FPS_SERVER`) and three accessors —
  `getFrameLegacy()`, `getFrameLegacyLast()`, `HasLegacyFrameAdvanced()`
  (`= m_frameLegacy != m_frameLegacyLast`).
- **`GameLogic.cpp::update()`**: right before the real `m_frame++`, snapshots
  `m_frameLegacyLast = m_frameLegacy` when `m_frame % 2 != 0`, and after the
  increment, does `if (m_frame % 2 == 0) m_frameLegacy++`. Net effect:
  `m_frameLegacy` ticks up once every two real frames, and
  `HasLegacyFrameAdvanced()` is true on exactly the frame where it just did —
  giving every other file in this list a cheap "did a retail-equivalent tick
  just happen" check.
- Both counters are reset to 0 alongside `m_frame` in the constructor,
  `setDefaults()`, and `startNewGame()` (three separate reset sites, all
  updated identically) — with a `DEBUG_ASSERTCRASH` added confirming they're
  0 at the point retail already asserts `m_frame == 0`.

Everything else in `GameLogic.cpp` is unrelated to the tick-rate mechanism —
genuinely separate policy/gameplay changes, listed under "Independent
changes" below.

### Files using pattern 1 (halve the delta) or 3 (legacy-frame delay math)

| File | What's scaled |
|---|---|
| `Object/Weapon.cpp` | Minimum `delayToUse` clamp (`getDelayBetweenShots`) so a weapon can't fire faster than 2 legacy-equivalent ticks apart even if INI says a 1-tick delay; a Gatling-building-specific `/1.5` rate-of-fire hack; a hardcoded +25% rate-of-fire fix for veterancy-1 Quad Cannons (comment: "Fix v1 quad cannon damage this way for now" — reads as a live balance bug being patched around, not fps-cleanup) |
| `Object/Update/EMPUpdate.cpp` | Visual scale-in/out lerp rate (`m_currentScale` interpolation); disabled-particle-system lifetime and initial-delay |
| `Object/Update/StealthUpdate.cpp` | "was a weapon fired in the last legacy tick" lookback window; pulse-opacity animation rate |
| `Object/Update/PhysicsUpdate.cpp` | `applyShock()`'s resisted force; `applyRandomRotation()`'s yaw/pitch/roll shock rates |
| `Object/Update/SpecialAbilityUpdate.cpp` | Capture-flash animation phase increment |
| `Object/Update/NeutronMissileUpdate.cpp` | All four state-transition timestamps (prelaunch/launch/attack/frameAtLaunch) switch to `getFrameLegacy()`; `update()` early-returns unless `HasLegacyFrameAdvanced()` |
| `Object/Update/AIUpdate/DeployStyleAIUpdate.cpp` + `.h` | Pack/unpack manual-animation frame math uses `getFrameLegacy()`; the animation-frame-set code inside `DEPLOY`/`UNDEPLOY` is skipped on non-legacy ticks; `getPackTime()`/`getUnpackTime()` divide the INI value by the multiplier |
| `Object/Update/AIUpdate/RailroadGuideAIUpdate.cpp` | Track-distance-per-tick halved ("compensate for 60hz update rate so movement matches retail 30hz" — GO's own comment) |
| `Object/Behavior/SlowDeathBehavior.cpp` | Random fling force (x/y/z) halved |
| `Object/Behavior/JetSlowDeathBehavior.cpp` | Death-frame and on-ground-frame timestamps switch to `getFrameLegacy()`; both post-death delay comparisons (`m_delaySecondaryFromInitialDeath`, `m_delayFinalBlowUpFromHitGround`) follow suit |
| `Object/Behavior/BattleBusSlowDeathBehavior.cpp` | Passenger-ejection throw force halved |
| `Object/Update/SlavedUpdate.cpp` | `update()` early-returns unless `HasLegacyFrameAdvanced()`; welding-spark particle lifetime explicitly does **not** scale with tick rate (inline comment: "Don't increase the time just because the logic tick rate increased" — i.e. this one is deliberately *not* halved, since it's driven by `m_framesToWait` which is already legacy-frame-scale) |
| `Object/ObjectCreationList.cpp` | `calcRandomForce()`'s magnitude halved; three separate death-fling force blocks (`SEND_IT_FLYING`, `SEND_IT_UP`, generic) each halve their horizontal/vertical force and yaw/roll/pitch spin |
| `AI/AI.cpp` | `AI::update()` only calls `m_pathfinder->processPathfindQueue()` when `HasLegacyFrameAdvanced()` — pathfinding runs at retail cadence, not doubled |
| `AI/TurretAI.cpp` | Turn rate doubled in `friend_turnTowardsAngle()` (compensates the other direction — since this runs every real tick, not just legacy ticks, the rate itself must double to cover the same angular distance per legacy-tick); `updateTurretAI()` early-returns unless `HasLegacyFrameAdvanced()`; `TurretAIIdleScanState::update()`'s alignment threshold changes from `0.0f` to `0.5f` for the angle check specifically — this one is *not* fps-related, it's a looser "close enough" snap tolerance, functionally a minor behavior tweak riding along in the same hunk |
| `ScriptEngine/ScriptEngine.cpp` | `legacyFrameAdvanced` bool gates the particle-editor-update path the same way; `startEndGameTimer` timer-length math unaffected (`FRAMES_TO_SHOW_WIN_LOSE_MESSAGE` is identical in both branches — see quirk below); the `SET_TIMER`/`SET_RANDOM_TIMER` script-action-to-frames conversion uses a `LEGACY_FPS_INT` constant instead of the real-time-based `ConvertDurationFromMsecsToFrames()` so script-authored countdown timers (in real-world seconds) still resolve to the correct number of *legacy* frames |

**Quirk worth flagging during the real port**: in both
`HelicopterSlowDeathUpdate.cpp` (`m_forwardSpeed = modData->
m_spiralOrbitForwardSpeed;`) and `ScriptEngine.cpp`
(`FRAMES_TO_SHOW_WIN_LOSE_MESSAGE`/`FRAMES_TO_FADE_IN_AT_START`), the `#if`
and `#else` branches are byte-identical — the ifdef is a no-op there. Almost
certainly leftover scaffolding from when GO was still tuning these two
values and never got cleaned up before merging. Port them as plain
unconditional code (drop the dead ifdef) rather than reproducing a
distinction that isn't real.

### `Object/Update/HelicopterSlowDeathUpdate.cpp` — same patterns, all three in one file

Bundled separately because it's the single file using all three compensation
patterns at once: `update()` early-returns unless `HasLegacyFrameAdvanced()`;
`m_lastSelfSpinUpdateFrame`/`m_hitGroundFrame` timestamps and their delay
comparisons (`m_selfSpinUpdateDelay`, `m_delayFromGroundToFinalDeath`) switch
to `getFrameLegacy()`/`getFrameLegacyLast()`; the downward-spiral forward
force is halved. Good file to use as the reference implementation when
porting the rest, since it exercises the whole toolkit.

## Independent changes (not tick-rate related)

Four genuinely separate gameplay/policy changes rode along in the same
`GENERALS_ONLINE`-gated files. These need their own review attention —
faithfulness matters for sim-parity same as the fps stuff, but they're not
mechanical and deserve an actual read, not a find-and-replace port.

1. **Starting-position conflict resolution** (`GameLogic.cpp`, gated by its
   own sub-define `GENERALS_ONLINE_IBRA_STARTING_POS_LOGIC`, not the general
   flag). Retail's slot-assignment check is
   `if (posIdx >= 0 || posIdx >= numPlayers)` — note the `||`, which is true
   for essentially any non-negative `posIdx` including out-of-range ones, and
   does nothing to catch two slots claiming the same start position. GO's
   replacement is `if (posIdx >= 0 && posIdx < numPlayers)` plus an actual
   `taken[]` duplicate check that reassigns a colliding slot to random
   (`slot->setStartPos(-1)`). Reads as a real bug fix in the base game's
   logic (the `||` looks like a typo for `&&`), just gated behind a
   GO-specific define instead of being applied unconditionally. Worth a
   conversation with the TSH maintainers about whether this is actually a
   bug worth fixing outside the GO flag too — out of scope for Part B itself,
   but flag it when this file comes up for review.

2. **Observer auto-kick** (`GameLogic.cpp::update()`, right after the weapon/
   locomotor/victory-condition `UPDATE()` calls). When the host has disabled
   observers (`!TheNGMPGame->getAllowObservers()`) and the local player has
   just been defeated but hasn't already dropped to observer status: if any
   ally is still alive, exit immediately; otherwise the non-host gets a
   10-second grace countdown (`LOGICFRAMES_PER_SECOND * 10` — note this
   countdown itself isn't legacy-frame-gated, so at 60 ticks/sec it actually
   elapses in 5 real seconds, not 10; worth confirming with GO whether that's
   intentional or a latent bug in their own code before porting it verbatim)
   before `TheGameLogic->exitGame()` force-disconnects them from the match.
   Internet games only (`GAME_INTERNET`).

3. **Richer CRC-mismatch diagnostics** (`GameLogic.cpp`, the desync-detection
   path). Retail logs which players' CRCs disagreed to the debug log only.
   GO additionally builds a formatted `strMismatchDetails` string (frame
   number, run-ahead-adjusted frame, per-player CRC values, and which
   specific players' CRCs differed from the majority) and passes it to
   `TheNetwork->setSawCRCMismatch(...)` — this is the same
   `setSawCRCMismatch(UnicodeString&)` virtual the plan's Phase 1.2 already
   added to `NetworkInterface` for Part A. Then, only under
   `GENERALS_ONLINE_USE_SENTRY` (Part C, currently disabled — see
   `[[go-backport-project]]` memory on the missing `sentry.dll`), attaches
   user ID/display name and forwards the same string to Sentry. The
   CRC-detail-string half of this is independent of Sentry and can land in
   Part B; the Sentry call site should stay a no-op until Part C.

4. **Per-player load-timeout tuning + minimum load-screen display time**
   (`GameLogic.cpp` + `GameLogic.h`). Retail gives every player a flat
   60-second timeout to finish loading before the host can force-start
   (`PROGRESS_COMPLETE_TIMEOUT`). GO splits this into two constants —
   `PROGRESS_COMPLETE_TIMEOUT_PROGRESS_MADE` (30s, used if the player's
   `m_progressMade[i]` — a new per-slot percent-complete field — is above 0)
   and `PROGRESS_COMPLETE_TIMEOUT_ZERO_PROGRESS_MADE` (5s, used if they
   haven't reported any progress at all, e.g. still downloading the map or
   stalled before even starting). Separately, `startNewGame()` enforces a
   minimum 2-second load-screen display time in internet games
   (`isInInternetGame()`) so players have time to read their ELO/army/stats
   on the loading screen before it fades — purely a UX polish item, not sim-
   affecting, safe to treat as lower priority than the other three.

## Non-findings

- **`Generals/.../ScriptEngine/Scripts.cpp`** and
  **`GeneralsMD/.../ScriptEngine/Scripts.cpp`** matched the grep only because
  the string `"ShellGeneralsOnlineLogin"` (a script-hook name) contains the
  literal substring `GeneralsOnline`, not because of a `GENERALS_ONLINE`
  preprocessor guard — these three new `SHELL_SCRIPT_HOOK_GENERALS_ONLINE_*`
  hook names (`LOGIN`, `LOGOUT`, `ENTERED_FROM_GAME`) are declared and wired
  **unconditionally** in both `Generals` and `GeneralsMD` trees, always
  present regardless of the flag (harmless no-ops when GO never fires them).
  No porting decision needed here beyond adding the three enum values +
  string-table entries + `static_assert` count bump; call sites (e.g.
  `SignalUIInteraction(SHELL_SCRIPT_HOOK_GENERALS_ONLINE_ENTERED_FROM_GAME)`
  in `WOLQuickMatchMenuUpdate`, already ported in Part A) already reference
  them.
- **`AIUpdate.cpp`**'s one hit (`evaluateHordeBonuses`'s
  `HORDEACTION_HORDE_FIXED` case) is double-gated behind `!RETAIL_COMPATIBLE_CRC
  && GENERALS_ONLINE_ENABLE_CONTROVERSIAL_NON_RETAIL_CHANGES` — the second
  define is commented out in GO's own `NextGenMP_defines.h`
  (`//#define GENERALS_ONLINE_ENABLE_CONTROVERSIAL_NON_RETAIL_CHANGES 1`), so
  this is dead code in GO's own shipped client too. Lowest priority in the
  whole list; can be ported last or skipped until someone asks for it.
- **`ScriptEngine.h`**'s one hit is just the `startEndGameTimer` signature
  change (adds a `bExtendForErrorMsg` bool param) that pairs with the
  `ScriptEngine.cpp` entry above — not a separate item.

## Suggested PR grouping for the actual port

1. **The mechanism**: `GameLogic.h`/`.cpp`'s `m_frameLegacy` machinery alone
   (no consumers yet) — small, self-contained, easy to review in isolation.
2. **Movement/physics consumers**: `PhysicsUpdate`, `ObjectCreationList`,
   `SlowDeathBehavior`, `JetSlowDeathBehavior`, `BattleBusSlowDeathBehavior`,
   `HelicopterSlowDeathUpdate`, `RailroadGuideAIUpdate`, `NeutronMissileUpdate`,
   `DeployStyleAIUpdate`(+`.h`) — all straightforward pattern-1/3 applications.
3. **AI/combat consumers**: `AI.cpp`, `TurretAI.cpp`, `SlavedUpdate.cpp`,
   `SpecialAbilityUpdate.cpp`, `StealthUpdate.cpp`, `EMPUpdate.cpp`,
   `Weapon.cpp` — same patterns, grouped separately since they touch combat
   balance and deserve sim-parity-focused review.
4. **Script hooks**: the three unconditional `SHELL_SCRIPT_HOOK_GENERALS_ONLINE_*`
   additions to both `Scripts.cpp` files + `ScriptEngine.cpp`/`.h`'s
   `startEndGameTimer`/timer-conversion pieces — small, low-risk.
5. **`GameLogic.cpp` independent changes**: starting-position fix, observer
   auto-kick, CRC-mismatch details, load-timeout tuning — reviewed
   individually per the "Independent changes" section above, since each is
   an actual behavior decision, not a mechanical scale-factor.
6. Skip/defer: the `AIUpdate.cpp` controversial-non-retail hunk (dead code
   even upstream).

Once this lands, Phase 5.2 (desync-free cross-play + replay exchange against
an official GO client) is the actual verification gate — nothing here is
confirmed correct until that passes.
