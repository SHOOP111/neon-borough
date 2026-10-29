# NEON BOROUGH

**The streets don't sleep.** An original, playable C++20 top-down urban sandbox inspired by the driving-and-pursuit format of classic GTA 2. All artwork, effects, and audio are generated in code. No GTA names, maps, music, or assets are included.

[![CI](https://github.com/SHOOP111/neon-borough/actions/workflows/ci.yml/badge.svg)](https://github.com/SHOOP111/neon-borough/actions/workflows/ci.yml)

![Neon Borough gameplay captured on a GitHub-hosted runner](https://github.com/SHOOP111/neon-borough/releases/download/v0.1.0/gameplay.png)

> This is a substantial first playable release, not a claim of AAA completeness. Single-player desktop only. See [scope and limitations](docs/DESIGN.md#scope-and-limitations).

## Play

Download the **Windows x64**, **Linux x64**, or **macOS Apple Silicon** package from [Releases](https://github.com/SHOOP111/neon-borough/releases), once a verified release has been published. The latest CI run also supplies portable binaries under **Actions → CI → Artifacts** (GitHub login required). Extract the package and run `neon_borough.exe` on Windows or `neon_borough` on Linux/macOS.

There are no asset downloads or installation steps. OpenGL 3.3 and a working desktop display are required. Linux requires the usual X11/OpenGL/audio runtime libraries. Windows packages use the MSVC runtime available on most current Windows installations; if missing, install Microsoft's official Visual C++ 2015–2022 x64 Redistributable. macOS binaries are unsigned and may require approval in System Settings → Privacy & Security.

### First five minutes

1. Press **Enter** at the title screen.
2. Take the pink **APEX GT** parked beside the loft with **E**.
3. Visit a cyan contact on the minimap and press **F** to accept a contract.
4. Follow the gold target markers and cyan road route. Open **M** for the full city map.
5. If wanted, turn corners and use buildings to break sight. Stars decay after eight seconds out of view. Avoid nearby patrols until your heat clears.
6. Buy repairs, armor, ammunition, or permanent engine tuning at green garages. Rest and save at the violet safehouse when not wanted.

**Free-roam navigation:** open **M**, click a service to route there or click a road to set a waypoint. Right-click clears it; arriving clears it automatically. Active contracts take route priority without deleting your waypoint. The garage remains live—traffic, pursuit, and contract timers do not pause while shopping.

### Controls

| Action | Input |
|---|---|
| Walk / drive | WASD or arrow keys |
| Aim / fire | Mouse / left mouse button |
| Sprint on foot | Left Shift |
| Handbrake while driving | Space |
| Enter / exit vehicle | E |
| Contact / garage / safehouse | F |
| Reload | R |
| Horn | H |
| Garage purchases | 1–4 |
| City map (pauses simulation) | M |
| Pause / close overlay | Escape |
| Toggle help | F1 |
| Fullscreen | F11 |
| Title/pause menu selection | Up / Down / Tab, then Enter |
| Quit / return to title from a menu | Q |
| Settings toggles / volume / back | R / V / N, Left / Right or + / −, Escape |

Help stays available with F1 throughout a session. Losing window focus automatically pauses gameplay. Menu clicks are suppressed until mouse release so entering the game cannot accidentally fire the sidearm.

On a car, W is throttle, S brakes before reversing, and A/D steer. Steering needs motion; this is not tank-style movement. The handbrake preserves lateral momentum and leaves fading tire marks. The sidearm can also be fired from a vehicle. Do not run over civilians: it costs money and raises heat.

## Implemented systems

- **Seeded city:** 3,072 × 3,072 world units, 64 connected blocks, four districts, alleys, parks, detailed roofs, lit signs, services, and crosswalks.
- **Driving:** acceleration/braking/reverse, lateral grip, speed-sensitive steering, handbrake, capsule-style building clearance, stepped collision, car-to-car impulses, damage and wrecks.
- **Population:** 48 initial traffic vehicles, replenished away from the player, 100 sidewalk pedestrians, alternating traffic lights, collision avoidance, panic, and distant wreck/abandoned-target recycling.
- **Pursuit:** five heat levels, up to ten patrol cars, road routing, direct line-of-sight pursuit, ramming, hostile fire, and sight-based escape.
- **Combat:** aimed sidearm, first-impact swept bullets respecting walls and vehicle cover, magazine and finite reserve, reload time, armor, civilian penalties, and medical recovery.
- **Contracts:** three-drop courier runs, marked-car acquisition, and informant pickup/extraction; timers, failure states, escalating rewards, chain bonuses, and persistent contract reputation counters.
- **Progression:** bounded cash, completed contracts, best chain, armor unlock, three engine upgrades, garages that reject no-op purchases, and autosaves.
- **Presentation:** chase camera, speed zoom, procedural vehicle details and headlights, neon lighting, particles, rain, bloom/vignette/grain shader, minimap, route overlay, menus, settings, HUD, notifications, and synthesized soundtrack/SFX.

## CI-only build and test policy

**Development builds and tests are run on GitHub-hosted runners, not on the author's PC.** The repository includes no script that automatically compiles locally.

[The workflow](.github/workflows/ci.yml) builds release binaries on Ubuntu, Windows, and macOS with project warnings treated as errors, runs both headless C++ suites and CLI contracts, and runs ASan/UBSan separately on the core and desktop renderer. Static analysis and negative-path tests also check the evidence validators. Linux launches use Xvfb and software OpenGL. CI validates gameplay, title, map, garage, pause, settings, and waypoint screenshots at multiple window sizes, including an overlay/resize lifecycle run. A PulseAudio null sink supplies captured audible and muted audio. Screenshots, recordings, and logs are uploaded as evidence. Version tags publish packages and SHA-256 checksums only after every required job passes.

See [the deep-review record](docs/REVIEW.md) for findings, regression coverage, and remaining limitations.

To build or test without using your PC:

1. Fork the public repository, or push a branch if you are a collaborator.
2. Open **Actions → CI → Run workflow**, or open a pull request.
3. Download platform binaries and renderer evidence from the finished run.

See [CI and architecture](docs/ARCHITECTURE.md) for details. Standard CMake files are supplied for portability, but no local compilation is needed to obtain binaries.

## Saves and command-line options

Progress is saved after contracts, purchases, rest, recovery, and normal exit. The version-1 format remains compatible with existing valid saves. Bounded numeric parsing, a checksum, and a previous-save backup guard against damaged writes. Missing or damaged primary files recover from a valid `.bak`; a damaged primary never replaces a known-good backup. Saves restore the economy and upgrades into a fresh city; they do **not** resume an in-progress mission or chase. Save failures are shown, logged, and retried with backoff; failure to save blocks returning to the title, though closing the application still exits.

- Windows: `%LOCALAPPDATA%/NeonBorough/progress.sav`
- Linux: `$XDG_DATA_HOME/neon-borough/progress.sav` or `~/.local/share/neon-borough/progress.sav`
- macOS: `~/Library/Application Support/NeonBorough/progress.sav`

`--seed N` selects a city seed, retaining loaded economy/upgrades; use `--no-save` or a separate `--save-dir` for a fresh isolated run. `--no-audio` avoids initializing the audio device. `--no-save` disables disk persistence. `--save-dir PATH` isolates saves. `--size WIDTHxHEIGHT` selects the initial window size (minimum 1100 × 700). `--help` lists options without creating a window or save. Delete or move both `progress.sav` **and** `progress.sav.bak` to reset progress. Empty/relative platform data-directory environment variables are ignored; if no absolute platform directory is available, the fallback is `neon-borough-progress.sav` in the working directory.

CI uses `--smoke --frames 240 --screenshot artifacts/gameplay.png`; this disables audio and saving and runs scripted input. `--smoke-view title|map|garage|paused|settings|waypoint` captures overlays. `--smoke-view lifecycle` switches screens, resizes the window three times, and toggles effects in one process. `--smoke-audio` enables audio; `--smoke-mute` initializes it with the master mix muted. These are automated smoke checks, not substitutes for human playtesting.

## License

Original code and procedural artwork/audio: [MIT](LICENSE). raylib and its bundled dependencies retain their own licenses; see [THIRD_PARTY.md](THIRD_PARTY.md). This project is not affiliated with or endorsed by Rockstar Games or Take-Two Interactive.
