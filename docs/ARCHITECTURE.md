# Architecture and remote validation

## Files

| Area | Files | Responsibility |
|---|---|---|
| Math | `include/neon/math.hpp` | Vectors, bounds, intersections, deterministic RNG |
| City | `world.hpp`, `src/world.cpp` | Seeded layout, collision queries, occlusion, street graph BFS |
| Simulation | `simulation.hpp`, `src/simulation.cpp` | Fixed-step gameplay and controller state; no graphics dependency |
| Persistence | `save.hpp`, `src/save.cpp` | Validated, checksummed progress, temporary writes and backups |
| Presentation | `presentation.hpp`, `src/presentation.cpp` | Camera, procedural geometry, shader, HUD, maps, menus, particles |
| Audio | `src/audio.cpp` | Short synthesized sound buffers and main-thread music stream generation |
| Application | `src/main.cpp` | CLI, window/input, accumulator, menus, save lifecycle, CI smoke mode |
| Verification | `tests/` | Headless C++ test executable and CI framebuffer validator |

## Simulation boundary

`neon_core` has no raylib dependency. It advances on 1/60-second ticks. Real-time input is sampled in the application; one-shot actions remain pending until consumed by a simulation tick. At most six steps are run per rendered frame, with frame delta capped at 0.1 seconds. Pause, settings, and full-map screens stop simulation.

Events communicate shots, collisions, rewards, and text to presentation/audio. The simulation clears its event list at each tick; the application consumes it after every step, including when multiple steps occur during one frame. Visual RNG is separate from gameplay RNG. A digest supports same-platform deterministic replay tests; floating-point results are not promised to be bit-identical between compilers or CPU architectures.

Vehicle IDs are monotonic. Storage is capped at 96 slots and destroyed distant cars are recycled. Entity lookup uses IDs rather than pointers that survive container mutations. Bullets and visual particles are also capped. Collision and occlusion are linear scans over a small fixed city; there is no misleading claim of an ECS or spatially streamed world.

## Save contract

Format version 1 stores seed, cash, completed contracts, best chain, upgrade levels, and reputation counters. Both parsing and serialization validate numeric bounds. Files over 4 KB, invalid headers, truncated lines, extra trailing data, and checksum mismatches are rejected. Writes go to a temporary file and rotate the last save to `.bak` before committing. On failure the application reports the error rather than silently claiming success. The checksum detects accidental corruption; it is not cryptographic authentication. Backup rotation is recoverable, not a cross-platform power-loss durability guarantee.

## GitHub Actions

All development compilation/testing is remote. No local build or test has been executed as part of this project setup.

- **Desktop matrix:** Ubuntu 24.04 x64, Windows Server 2022 x64 / MSVC, macOS 14 arm64.
- **Core tests:** reproducible layout, all-pairs shortest routes, collision tunneling and sliding, timestep guards, stamina, driving, theft, gunfire, armor, reload, dispatch/escape, all three mission types, mission failure, pool exhaustion, economy, recovery, save round trips and corruption, and long simulation stress.
- **Sanitizers:** separate headless Linux Debug build with AddressSanitizer and UndefinedBehaviorSanitizer, including leak detection.
- **Renderer:** Linux release binary launched in Xvfb using Mesa software OpenGL; scripted gameplay and title/map/garage captures, shader-error log check, framebuffer dimensions/variance/color/highlight checks.
- **Audio:** game initialized against a PulseAudio null sink; monitor recording checked for finite float samples, audible RMS, and absence of clipping.
- **Artifacts:** portable platform packages plus PNGs, captured audio, and logs, retained for 30 days.
- **Style:** clang-format 18 checks all project C++ on a hosted runner; the manual formatting workflow can apply style changes remotely.
- **Releases:** pushing a `v*` tag publishes packages, SHA-256 checksums, and four screenshots only after desktop, sanitizer, and style jobs succeed.

A passing framebuffer test proves that geometry, shader, HUD, and a graphics context produced a nonblank frame. It does not prove every visual detail is correct. Screenshots should also be inspected. Audio smoke verifies device startup and non-silent output on Linux; perceived mix quality and other platforms' device behavior still need human testing.

To reproduce remotely, use **Actions → CI → Run workflow** or submit a PR. To publish, push a version tag after reviewing the CI evidence. CMake fetches raylib 5.5 by immutable release commit, and the core-only sanitizer build makes no external dependency fetch.
