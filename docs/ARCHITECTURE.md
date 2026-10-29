# Architecture and remote validation

## Files

| Area | Files | Responsibility |
|---|---|---|
| Math | `include/neon/math.hpp` | Vectors, bounds, intersections, deterministic RNG |
| City | `world.hpp`, `src/world.cpp` | Seeded layout, collision queries, occlusion, street graph BFS |
| Simulation | `simulation.hpp`, `src/simulation.cpp` | Fixed-step gameplay and controller state; no graphics dependency |
| Progress contract | `include/neon/progress.hpp` | Shared limits and semantic validation for simulation and saves |
| Persistence | `save.hpp`, `src/save.cpp` | Bounded parsing, checksummed progress, temporary writes, backups and recovery |
| Presentation | `presentation.hpp`, `src/presentation.cpp` | Camera, procedural geometry, shader, HUD, maps, menus, particles |
| Audio | `src/audio.cpp` | Short synthesized sound buffers and main-thread music stream generation |
| Application | `src/main.cpp` | CLI, window/input, accumulator, menus, save lifecycle, CI smoke mode |
| Verification | `tests/` | Core and review C++ suites, CLI contracts, framebuffer/audio validators and validator unit tests |

## Simulation boundary

`neon_core` has no raylib dependency. It advances on 1/60-second ticks; invalid/nonfinite deltas or controller values are rejected without mutation, and analog controls are clamped. Real-time input is sampled in the application; one-shot actions remain pending until consumed by a simulation tick. At most six steps are run per rendered frame, with frame delta capped at 0.1 seconds. Pause, settings, full-map screens, and loss of focus stop simulation. Garages are live, with distance checked before every purchase. Overlay transitions clear queued actions; mouse firing is suppressed until release after returning from menus.

Events communicate shots, collisions, rewards, and text to presentation/audio. The simulation clears its event list at each valid tick; the application consumes it after every step, including when multiple steps occur during one frame. Visual RNG is separate from gameplay RNG. The digest covers dynamic gameplay state, RNG/controller timers, progress, missions, bullets and events, including inactive vehicle slots; the city is identified by its seed. It does not hash renderer/audio state or externally mutated city geometry. Same-platform replay is tested; floating-point results are not promised to be bit-identical between compilers or CPU architectures.

Vehicle IDs are monotonic and failed spawns do not consume IDs or RNG. Storage is capped at 96 slots; destroyed distant cars and abandoned contract cars are recycled, while distant traffic is replenished. Entity lookup uses IDs rather than pointers that survive container mutations. Bullets retain their source vehicle and compare entry times against buildings, boundaries and actors to resolve the first impact independent of storage order. Wrecks resolve immediately after gunfire; defeat precedes contract completion. Cleared patrols despawn at distance or rejoin traffic nearby. Bullets and visual particles are also capped. Collision and occlusion are linear scans over a small fixed city; there is no ECS or spatially streamed world.

## Save contract

Format version 1 stores seed, cash, completed contracts, best chain, upgrade levels, and reputation counters. `progress.hpp` supplies the shared bounds and validates aggregate reputation against completed contracts. Rewards saturate at those bounds. Parsing uses locale-independent `from_chars`, including strict unsigned seeds/checksums; the actual read is capped at 4 KB to avoid a file-size check/read race. Invalid headers, truncated fields, extra trailing data, and checksum mismatches are rejected. Writes use a temporary file, check flush/close errors, and rotate only a valid primary to `.bak`. Missing/damaged primaries recover from `.bak`; directory/symlink write targets are refused and temporary files are cleaned up on failure. The application logs and displays failures and retries autosaves with a five-second backoff.

The checksum detects accidental corruption, not malicious tampering. Rotation is recoverable, not a cross-platform power-loss durability guarantee. Saves assume **one writer per save directory**; simultaneous game instances should use separate `--save-dir` paths. Normal exit attempts a save, but cannot guarantee persistence when the filesystem fails.

## GitHub Actions

All development compilation/testing is remote. No local build or test is used for this review.

- **Desktop matrix:** Ubuntu 24.04 x64, Windows Server 2022 x64 / MSVC, macOS 14 arm64.
- **Core tests:** both C++ suites run on the desktop matrix, covering existing mechanics plus strict input boundaries, first-impact combat, same-tick wrecks, posthumous reward prevention, economy caps, stale/no-op purchases, cleared patrols, population recovery, digest sensitivity and save recovery/error paths. CLI argument tests execute the compiled binary without graphics or disk initialization.
- **Sanitizers:** a separate headless Linux Debug build runs AddressSanitizer and UndefinedBehaviorSanitizer with leak detection. A second instrumented desktop build exercises screens, resize and render-target lifecycles; leak checking is disabled only there because Mesa/X11 retain process-global allocations.
- **Renderer:** Linux release binary launched in Xvfb using Mesa software OpenGL; gameplay/title/map/garage/pause/settings/waypoint captures, multi-size framebuffers, overlay/resize/effects lifecycle, shader-error checks, and dimensions/variance/color/highlight validation.
- **Audio:** PulseAudio null-sink monitor recordings checked for finite float samples, sustained audible windows, absence of clipping, and true master-mix silence in mute mode.
- **Analysis:** cppcheck on headless project code, warnings-as-errors on project targets, and positive/negative validator unit tests under Python `-O` (validation must not rely on removable assertions).
- **Artifacts:** portable platform packages plus PNGs, captured audio, and logs, retained for 30 days.
- **Style:** clang-format 18 checks all project C++ on a hosted runner; the manual formatting workflow can apply style changes remotely.
- **Releases:** a `v*` tag must match the CMake project version. Packages, SHA-256 checksums, and screenshots publish only after desktop, core/renderer sanitizers, analysis and style succeed.

A passing framebuffer test proves that geometry, shader, HUD, and a graphics context produced a nonblank frame. It does not prove every visual detail is correct. Screenshots should also be inspected. Audio smoke verifies device startup and non-silent output on Linux; perceived mix quality and other platforms' device behavior still need human testing.

To reproduce remotely, use **Actions → CI → Run workflow** or submit a PR. To publish, push a version tag after reviewing the CI evidence. CMake fetches raylib 5.5 by immutable release commit **and SHA-256 archive hash**; the core-only build makes no external dependency fetch. Third-party compiler warnings are not promoted by the project's warning policy. See [REVIEW.md](REVIEW.md) for the review record.
