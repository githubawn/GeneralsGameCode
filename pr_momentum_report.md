# Open PRs Review Urgency & Momentum Report (All 90 PRs)

This report lists pull requests for **TheSuperHackers/GeneralsGameCode**, sorted by review urgency. It analyzes review decisions, comment logs, timelines, and mergeability checks to determine momentum.

## Review Urgency Classification Rules:
- **1 - Very High**: Open PRs with no reviews, or PRs where the author has addressed changes/feedback and is waiting for a re-review.
- **2 - High**: Open PRs in active discussion (with reviews or comments) that still need a review decision (approved/rejected).
- **3 - Medium**: PRs with approvals but recent follow-up comments, active drafts with approvals, or older open PRs.
- **4 - Low (Merge/Draft)**: Approved PRs ready to merge, or drafts in early development.
- **5 - Deferred / Waiting**: PRs with merge conflicts, blocked by dependencies, marked as WIP, on hold, or waiting for the author to respond to requested changes.

---

## Review Urgency Summary Table

| PR# | Title | Author | Status | Review Urgency | Reasoning |
| :--- | :--- | :--- | :--- | :--- | :--- |
| #1785 | feat(screenshot): Add threaded JPEG/PNG screenshots without game stalls | @bobtista | Changes Requested | **1 - Very High** | Author responded/updated code after CHANGES_REQUESTED review (updated 0d ago); needs re-review. |
| #2803 | bugfix(logic): Restore retail compatibility after change to frozen time check | @Caball009 | Needs Review | **1 - Very High** | No reviews yet on this recently updated PR (0d ago); needs initial review. |
| #2781 | bugfix(specialpower): Charge uninitialized special powers on new buildings | @bobtista | Needs Review | **1 - Very High** | No reviews yet on this recently updated PR (0d ago); needs initial review. |
| #2774 | bugfix(milesaudiomanager): Use reference counted DynamicAudioEventRTS class in AudioRequest and PlayingAudio to prevent race conditions when sharing audio event data in MilesAudioManager::startNextLoop() | @xezon | Needs Review | **1 - Very High** | Author responded to questions/comments recently (4d ago); needs review follow-up. |
| #2786 | fix(heightmap): Prevent per-frame full terrain rebuild with DrawEntireTerrain | @sailro | Needs Review | **1 - Very High** | Author responded to questions/comments recently (5d ago); needs review follow-up. |
| #2613 | chore(ww3d2): add IRenderBackend Interface | @bobtista | Needs Review | **1 - Very High** | Author responded to questions/comments recently (13d ago); needs review follow-up. |
| #1837 | fix: crash when replay file is deleted during version mismatch prompt | @bobtista | Changes Requested | **1 - Very High** | Author responded/updated code after CHANGES_REQUESTED review (updated 79d ago); needs re-review. |
| #2519 | fix(data): Sanitize GlobalData values after reading INI data | @Cellcote | Needs Review | **2 - High** | Active discussion on recently updated PR (0d ago); needs review decision. |
| #2744 | bugfix(memory): Harden memory manager edge cases | @IbrahimAlzaidi | Needs Review | **2 - High** | Active discussion on recently updated PR (0d ago); needs review decision. |
| #2793 | bugfix: Fix issue where builders could resume completed tasks after being disabled | @Stubbjax | Needs Review | **2 - High** | Active discussion on recently updated PR (2d ago); needs review decision. |
| #2668 | tweak(gamemessage): Reduce number of MSG_DESTROY_SELECTED_GROUP messages | @Caball009 | Needs Review | **2 - High** | Active discussion on recently updated PR (3d ago); needs review decision. |
| #1119 | [ZH] Prevent hang in network lobby with long player names | @slurmlord | Needs Review | **2 - High** | Author responded to reviews (79d ago); needs follow-up review. |
| #887 | [GEN][ZH] Implement interlocked compat helper functions | @Mauller | Needs Review | **2 - High** | Author responded to reviews (79d ago); needs follow-up review. |
| #2160 | fix(savegame): Use getFinalOverride in WeaponSet xfer load to match Object constructor | @bobtista | Needs Review | **2 - High** | Author responded to reviews (79d ago); needs follow-up review. |
| #2714 | refactor: Refactor evaluateContextCommand phase 1 (#1192) | @RikuAnt | Needs Review | **3 - Medium** | Active discussion on PR updated 13d ago; needs review decision. |
| #1711 | fix(view): Adjust default camera height to compensate for screen aspect ratio | @Mauller | Needs Review | **3 - Medium** | Active discussion on PR updated 13d ago; needs review decision. |
| #2760 | chore: Prevent conflict between clang-format and pre-C++11 nested template parsing | @DevGeniusCode | Approved / Ready | **3 - Medium** | Approved, but has recent discussions (updated 15d ago); needs final check. |
| #2528 | feat(string): add UTF-8 string conversion and validation functions | @bobtista | Needs Review | **3 - Medium** | Active discussion on PR updated 19d ago; needs review decision. |
| #2738 | tweak(gui): Show Money Per Minute in Player Info List | @L3-M | Needs Review | **3 - Medium** | Active discussion on PR updated 19d ago; needs review decision. |
| #2701 | build: Fix compile error in Smudge.h with docker-build | @ucosty | Needs Review | **3 - Medium** | Active discussion on PR updated 32d ago; needs review decision. |
| #2542 | ci(vcpkg): Switch binary cache from local files to GitHub Packages NuGet feed | @bobtista | Needs Review | **3 - Medium** | Open PR with no reviews, updated 72d ago; low momentum. |
| #925 | [ZH] Simulate Replays with CSV files | @helmutbuhler | Needs Review | **3 - Medium** | Active discussion on PR updated 79d ago; needs review decision. |
| #1821 | feat: Update LAN chat messages and emotes for consistency with WOL | @tintinhamans | Needs Review | **3 - Medium** | Active discussion on PR updated 79d ago; needs review decision. |
| #1799 | Creating documentation for Modders  | @ahmed007boss | Needs Review | **3 - Medium** | Active discussion on PR updated 79d ago; needs review decision. |
| #1444 | bugfix(specialpower): Fix availability of building based special powers | @Mauller | Needs Review | **3 - Medium** | Active discussion on PR updated 79d ago; needs review decision. |
| #2043 | bugfix(Geometry): Checks Hitboxes for buildings with BOX Geometry, preventing the building to take damage without being hit | @IamInnocent3X | Needs Review | **3 - Medium** | Active discussion on PR updated 79d ago; needs review decision. |
| #2379 | build(cmake): Add retail compatibility options and features in CMake config | @tintinhamans | Needs Review | **3 - Medium** | Active discussion on PR updated 79d ago; needs review decision. |
| #2676 | bugfix(lan): Fix crash when changing settings in the LAN lobby | @Caball009 | Draft | **4 - Low (Merge/Draft)** | Draft PR in active development. Updated 0d ago. |
| #2789 | bugfix(textureloader): Fix faulty texture reduction implementations | @xezon | Approved / Ready | **4 - Low (Merge/Draft)** | Approved and ready to merge. Updated 0d ago. |
| #2296 | bugfix(pathfinder): Fix inaccurate single unit movement destinations when unobstructed | @stephanmeesters | Approved / Ready | **4 - Low (Merge/Draft)** | Approved and ready to merge. Updated 1d ago. |
| #2796 | bugfix(crc): Fix spurious mismatches for disconnected players at low CRC intervals | @Caball009 | Approved / Ready | **4 - Low (Merge/Draft)** | Approved and ready to merge. Updated 1d ago. |
| #2649 | feat(replay): Check CRC messages from all players in replays | @Caball009 | Draft | **4 - Low (Merge/Draft)** | Draft PR in active development. Updated 1d ago. |
| #2761 | perf: Reduce cost of retrieving labels from Waypoint and PolygonTrigger by 90% | @Mauller | Approved / Ready | **4 - Low (Merge/Draft)** | Approved and ready to merge. Updated 1d ago. |
| #2792 | unify(selectionxlat): Merge SelectionXlat | @xezon | Approved / Ready | **4 - Low (Merge/Draft)** | Approved and ready to merge. Updated 1d ago. |
| #2788 | bugfix(textureloader): Lift hardcoded texture aspect ratio limit of 1:8 to allow load all textures with modern gpu | @xezon | Approved / Ready | **4 - Low (Merge/Draft)** | Approved and ready to merge. Updated 2d ago. |
| #2736 | bugfix(network): Add command id overflow check to keep network commands in chronological order | @Caball009 | Approved / Ready | **4 - Low (Merge/Draft)** | Approved and ready to merge. Updated 3d ago. |
| #2659 | perf(network): Improve performance by reducing packet allocations and improving loop logic | @Caball009 | Draft | **4 - Low (Merge/Draft)** | Draft PR in active development. Updated 15d ago. |
| #2469 | build(cmake): Add Clang MinGW-w64 cross-compilation toolchain | @JohnsterID | Draft | **4 - Low (Merge/Draft)** | Draft PR in active development. Updated 50d ago. |
| #2598 | bugfix(drawable): Decouple visual render from projectile's logic position | @githubawn | Draft | **4 - Low (Merge/Draft)** | Draft PR in active development. Updated 58d ago. |
| #2510 | feat(script): Extend the functionality of team generic scripts | @Mauller | Draft | **4 - Low (Merge/Draft)** | Draft PR in active development. Updated 76d ago. |
| #2436 | feat(stats): Add JSON game stats export alongside replay files Store stats | @bill-rich | Draft (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 0d ago. |
| #2500 | perf(terrain): Hoist global light ray computation out of per-vertex loop | @Cellcote | Approved / Ready (Blocked/WIP) | **5 - Deferred / Waiting on Author** | Indicated as Blocked, WIP, or waiting on nightly upstream in description/comments. Updated 0d ago. |
| #2675 | unify(controlbar): Merge and move ControlBar and related code to core | @DevGeniusCode | Changes Requested (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 0d ago. |
| #2197 | bugfix(ai): Fix bugged unit behavior when attacked during guard mode | @CookieLandProjects | Approved / Ready (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 0d ago. |
| #2709 | tweak(particlesys): Decouple Particles render update from logic step | @xezon | Draft (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 1d ago. |
| #2773 | refactor(audio): Simplify available audio samples management | @xezon | Changes Requested | **5 - Deferred / Waiting on Author** | Waiting on author to address requested changes. Updated 2d ago. |
| #2670 | feat(math): Route game logic math through WWMath with 3-mode deterministic support | @Okladnoj | Changes Requested (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 2d ago. |
| #2639 | feat(input): Implement SDL3 input and window management | @githubawn | Needs Review (Blocked/WIP) | **5 - Deferred / Waiting on Author** | WIP architectural change waiting on nightly upstream release. Updated 3d ago. |
| #2661 | feat(stats): Add 1% low FPS tracking | @githubawn | Changes Requested | **5 - Deferred / Waiting on Author** | Waiting on author to address requested changes. Updated 4d ago. |
| #2663 | perf: eliminate AsciiString ref-count overhead in rendering, logic, and UI hot paths | @githubawn | Draft (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 4d ago. |
| #2785 | fix(terrain): Cover the visible ground at high camera zoom | @sailro | Needs Review (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 5d ago. |
| #2267 | feat(intro): Add short Intro Logo for The Super Hackers team | @xezon | Needs Review (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 16d ago. |
| #2638 | style: apply automated formatting baseline with clang-format | @DevGeniusCode | Needs Review (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 17d ago. |
| #2699 | feat(frame-pacer): overhaul logic and render FPS limits and presets | @githubawn | Changes Requested (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 20d ago. |
| #2056 | tweak(gui): Decouple GUI transition and world animation timing from render update | @bobtista | Changes Requested (Blocked/WIP) | **5 - Deferred / Waiting on Author** | Indicated as Blocked, WIP, or waiting on nightly upstream in description/comments. Updated 43d ago. |
| #2646 | perf(scene): Create light list iterators on the stack at points of use | @Mauller | Approved / Ready (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 46d ago. |
| #2602 | feat: Add cross-platform deterministic math via fdlibm | @Okladnoj | Changes Requested (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 48d ago. |
| #2589 | Custom hotkey remapping for unit/building production keys in Options | @wahidkurdo | Needs Review (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 48d ago. |
| #2176 | Remove d3dx8d dependency from MinGW builds | @JohnsterID | Approved / Ready (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 50d ago. |
| #2163 | ci(build): Add MinGW-w64 i686 cross-compilation to CI workflow | @JohnsterID | Approved / Ready (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 50d ago. |
| #2501 | feat(crc): Write deep CRC snapshots to disk when game mismatches | @Caball009 | Draft (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 54d ago. |
| #2608 | feat(input): Add Alt+Enter support for fullscreen toggling | @githubawn | Draft (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 58d ago. |
| #2455 | bugfix: Fix delays when requesting paths too quickly | @Stubbjax | Changes Requested (Blocked/WIP) | **5 - Deferred / Waiting on Author** | Indicated as Blocked, WIP, or waiting on nightly upstream in description/comments. Updated 60d ago. |
| #1607 | bugfix(gui): implement resolution scaling for unit health and info | @Mauller | Needs Review (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 64d ago. |
| #2597 | tweak(ini): Improve consistency in use of INI::getNextTokenOrNull | @Caball009 | Draft (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 65d ago. |
| #2395 | unify(common-ini): Move shared INI loaders to Core | @OmarAglan | Draft (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 78d ago. |
| #1574 | Feat/debugwindow wxwidgets conversion | @JohnsterID | Needs Review (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 79d ago. |
| #1404 | feat(network): Send product information to distinguish clients | @Caball009 | Needs Review (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 79d ago. |
| #2051 | performance(weapon): Optimized ProjectileStreamUpdate Behaviors + Added Variables to Optimize Weapon FX Rendering | @IamInnocent3X | Needs Review (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 79d ago. |
| #2053 | Tweak(Gui): Hide the custom overlay during video playback and add the ability to correctly scale campaign videos | @Mauller | Needs Review (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 79d ago. |
| #1573 | bugfix(gui): fix resolution and zoom scaling of unit information | @Mauller | Needs Review (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 79d ago. |
| #2060 | bugfix(armorstore): Create overrides for ArmorTemplate data from custom maps to avoid CRC mismatch in the next multiplayer game session | @Caball009 | Needs Review (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 79d ago. |
| #2069 | bugfix(system):  Update crash message | @JohnsterID | Needs Review (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 79d ago. |
| #2074 | tweak: Allow exit during match outcome | @Stubbjax | Needs Review (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 79d ago. |
| #2139 | bugfix(savegame): Fix crashes when saving a game in headless mode | @bobtista | Draft (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 79d ago. |
| #1749 | fix(mismatch): Fix mismatch due to custom upgrades in custom maps not properly resetting after match | @helmutbuhler | Needs Review (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 79d ago. |
| #1763 | refactor(w3dview): Migrate from MEMBER_ADD/MEMBER_RELEASE macros to RefCountPtr<T> | @bobtista | Needs Review (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 79d ago. |
| #1757 | bugfix: Jets now defer attack, force attack, attack move and guard commands while reloading | @Stubbjax | Needs Review (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 79d ago. |
| #1291 | [GEN][ZH] Implement file info generation for Generals executables | @xezon | Draft (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 79d ago. |
| #1935 | feat(hotkey): Enable players to hold down hotkeys to queue multiple units | @Caball009 | Draft (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 79d ago. |
| #1445 | feat(system): Add command line option to skip force set of cwd | @CoChiefResident | Needs Review (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 79d ago. |
| #1461 | bugfix(specialpower): Fix Special Power ready state during construction | @xezon | Needs Review (Blocked/WIP) | **5 - Deferred / Waiting on Author** | Indicated as Blocked, WIP, or waiting on nightly upstream in description/comments. Updated 79d ago. |
| #2044 | performance(Drawable): Rework for IterateDrawablesInRegion Functions for both GameClient and W3DView to be more Efficient | @IamInnocent3X | Needs Review (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 79d ago. |
| #2046 | performance(Drawable): Adjusts Turret Positioning, Recoil and Muzzle for Model Draw to Update Only when Necessary | @IamInnocent3X | Changes Requested (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 79d ago. |
| #2083 | feat(override): Create interface class for INI overrides | @Caball009 | Draft (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 79d ago. |
| #2055 | tweak(drawable): Decouple physics and fade timing from render update | @bobtista | Needs Review (Blocked/WIP) | **5 - Deferred / Waiting on Author** | Indicated as Blocked, WIP, or waiting on nightly upstream in description/comments. Updated 79d ago. |
| #2152 | feat(replay): Add checkpoint save and resume functionality for replays | @bobtista | Draft (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 79d ago. |
| #2273 | refactor(loadscreen): Refactor the single player load screen to use TheDisplay for intro video playback | @Mauller | Needs Review (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 79d ago. |
| #2282 | feat(input): Use force attack to get the latest building orientation | @Caball009 | Draft (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 79d ago. |
| #2399 | bugfix(production): Prevent cancelling production when units already produced | @tintinhamans | Changes Requested (Conflict) | **5 - Deferred / Waiting on Author** | Has merge conflicts; blocked until author resolves conflicts. Updated 79d ago. |
