# Deep review: hardening and navigation

## Scope

Reviewed every tracked source/header, test, build/workflow file, issue template and document, plus the pre-existing untracked `include/neon/progress.hpp`. That header was preserved and integrated as the shared progress contract. Changes are isolated on `review/deep-hardening`; the existing release/main branch is not overwritten and no release tag is created by this review.

Compilation, executable tests, sanitizers, formatting and evidence generation run **only on GitHub-hosted runners**. Local work is source inspection/editing, Git operations, and downloading/inspecting hosted logs/screenshots—not local compilation or test execution.

## Findings and fixes

| Area | Finding | Resolution / regression evidence |
|---|---|---|
| Persistence | Missing primary skipped backup recovery | `recoverProgress` tries `.bak` even when rotation left no primary |
| Persistence | Saving after corruption could replace a good backup with damaged data | Only validated primaries rotate; corruption/recovery/write regression |
| Persistence | Size check did not bound the actual read | Read at most 4,097 bytes and reject over 4 KB |
| Persistence | Stream extraction accepted signed unsigned fields / locale-dependent numbers | `from_chars` and classic-locale serialization; valid-checksum malformed fixtures |
| Persistence | Reputation counters could individually pass while their sum was impossible | Shared aggregate validation used by loading, saving and restore |
| Persistence | Non-file targets, temporary cleanup and failure handling were weak | Directory/symlink targets rejected, flush/close/rotation checked, RAII cleanup |
| Persistence | Save errors spammed retries and returning to title discarded feedback | Five-second retry backoff, visible menu messages, save-success gate |
| Progression | Rewards/counters could exceed the accepted save range | Saturating cash/completion/chain values; repeated endgame rewards remain serializable |
| Progression | Public restore trusted unchecked data and mismatched city seeds | Atomic validated restore; `--seed` deliberately retains economy in a new city |
| Economy | Stale garage flag allowed a remote purchase | Distance checked before purchase and each update |
| Economy | Full repair/armor/ammo charged for no effect; wrecks could be repaired | Eligibility/no-op rejection tests |
| Missions | Dead players could receive a completion reward before recovery | Defeat precedes mission update; direct completion also refuses dead players |
| Missions | Invalid contact indices created invalid mission kinds | Contact range guard |
| Population | Abandoned acquisition targets exhausted the pool; traffic gradually disappeared | Retire distant unoccupied contract cars and replenish hidden traffic within the pool cap |
| Police | Nearby patrols pursued after wanted cleared | Distant despawn / nearby traffic conversion before controller update |
| Traffic | Waiting at a red light triggered the stuck/reverse behavior | Explicit light-wait state excludes the stuck timer |
| Combat | Wall checks discarded earlier actor hits; hit order depended on storage/entity type | Shared segment entry times choose the nearest wall/boundary/car/pedestrian/player impact |
| Combat | Hostile fire ignored traffic cover and had no source identity | Vehicle cover and source-vehicle exclusion |
| Combat | Invulnerable players did not consume bullets | Block the projectile without taking damage |
| Combat | Gunfire-created wrecks waited another tick; explosions could repeat | Same-tick wreck resolution and one-time explosion sentinel |
| Recovery | Reload/shot timers and depleted stamina survived defeat | Recovery reset regression |
| Math/input | Nonfinite input poisoned simulation; huge queries risked unsafe conversions or loops | Finite guards, bounded motion/node conversion and analog clamps |
| RNG | Inclusive integer ranges could overflow and modulo introduced bias | 64-bit range calculation and rejection sampling, including full-int-range tests |
| Replay | Digest omitted velocity, ammunition, mission and controller state | Field-wise dynamic-state hash and sensitivity tests |
| Graphics | Shader fallback could appear to pass; resize resource failures were unchecked | Required custom uniforms/resource checks and fatal smoke failures |
| UI | Garage hid the cursor, help disappeared permanently, menu clicks could shoot | Correct cursor mode, persistent F1 toggle, mouse-release guard and cleared transition input |
| UI | No keyboard path to settings/quit, and loss of focus kept gameplay running | Keyboard menu selection/confirmation and focus-loss pause |
| Navigation | No route to free-roam services | Map waypoint/service snapping, arrival clear and contract priority |
| Audio | Muting did not silence already-playing effects; partial resource init was accepted | Master mix volume and full stream/sound readiness checks |
| CI | Evidence validators relied on removable `assert` statements | Explicit failures and positive/negative unit tests under `python -O` |
| CI | Smoke coverage omitted pause/settings/waypoints, resizing and mute | Multi-scene, multi-size, effects/overlay/resize lifecycle and audible/muted recordings |
| Supply chain | Commit pin alone did not validate archive bytes | SHA-256 obtained from the hosted runner and enforced by CMake |
| Releases | New checks did not all gate publication, and tags could mismatch package version | Desktop, both sanitizer jobs, analysis and style gates plus project-version check |

## Verification

- 33 original C++ cases and 29 review regression cases on Linux, Windows/MSVC and macOS/AppleClang.
- Core ASan/UBSan with leak checking; desktop ASan/UBSan exercising graphics and overlay/resize lifecycle. Mesa/X11 process-global leak checking is disabled only for the renderer job, not the core.
- CLI boundary cases against compiled desktop binaries without window/persistence initialization.
- cppcheck and warnings-as-errors for project code, not bundled upstream code.
- Positive/negative framebuffer and audio validator unit tests with Python assertions disabled.
- Xvfb/Mesa gameplay, title, map, garage, pause, settings and waypoint captures, multiple sizes, and a same-process lifecycle that switches overlays, resizes three times and toggles post-effects. Paused frames explicitly verify that simulation time does not advance.
- PulseAudio output checked for finite samples, sustained signal, clipping and master-mix silence.
- clang-format 18 is applied and checked remotely.

The first expanded hosted run ([40877925354](https://github.com/SHOOP111/neon-borough/actions/runs/40877925354)) passed Linux/Windows, both sanitizer jobs, analysis, style and audio/framebuffer evidence. AppleClang identified a signed-character conversion promoted by warnings-as-errors; it was corrected with an explicit byte conversion rather than weakening the warning policy. The complete expanded run then passed on all three desktop platforms ([40880051353](https://github.com/SHOOP111/neon-borough/actions/runs/40880051353)), including all 62 C++ cases and every required job. Hosted screenshots were inspected directly; small waypoint-card and map-label layout corrections followed. The latest branch/PR checks are the authoritative verification of the final revision; artifacts and logs are available from [Actions](https://github.com/SHOOP111/neon-borough/actions/workflows/ci.yml).

## Remaining limits — not claimed fixed

- Automated captures do not prove subjective handling, readable UI at every DPI, all GPUs/audio drivers or physical input behavior. Interactive human playtesting remains necessary.
- Graphics/audio runtime verification is Linux-hosted; Windows/macOS builds and headless/CLI tests do not replace interactive platform validation.
- Arcade collision, simple traffic routing and pedestrian panic can still produce congested/stuck NPCs. There is no intersection reservation, rigid-body solver or pedestrian navigation mesh.
- Saves persist economy/upgrades, not missions or the entire city. They assume one writer per save directory, are not cryptographically authenticated, and are not a power-loss/fsync durability guarantee. External disk failure can still prevent exit-time persistence.
- Settings/waypoints are session-only. No controller, multiplayer, interiors, new weapons or story system was invented as a substitute for fixing existing mechanics.
- The state digest is same-platform diagnostic replay, not a cross-architecture lockstep/network protocol; externally edited city geometry and presentation/audio state are outside its scope.
- Packages remain unsigned and use the documented runtime prerequisites.
