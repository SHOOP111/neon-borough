# NEON BOROUGH

**The streets don't sleep.** An original, playable C++20 top-down urban sandbox inspired by the driving-and-pursuit format of classic GTA 2. All artwork, effects, and audio are generated in code. No GTA names, maps, music, or assets are included.

[![CI](https://github.com/SHOOP111/neon-borough/actions/workflows/ci.yml/badge.svg)](https://github.com/SHOOP111/neon-borough/actions/workflows/ci.yml)

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

On a car, W is throttle, S brakes before reversing, and A/D steer. Steering needs motion; this is not tank-style movement. The sidearm can also be fired from a vehicle. Do not run over civilians: it costs money and raises heat.

## Implemented systems

- **Seeded city:** 3,072 × 3,072 world units, 64 connected blocks, four districts, alleys, parks, detailed roofs, lit signs, services, and crosswalks.
- **Driving:** acceleration/braking/reverse, lateral grip, speed-sensitive steering, handbrake, capsule-style building clearance, stepped collision, car-to-car impulses, damage and wrecks.
- **Population:** 48 initial traffic vehicles, 100 sidewalk pedestrians, alternating traffic lights, collision avoidance, panic, and distant wreck recycling.
- **Pursuit:** five heat levels, up to ten patrol cars, road routing, direct line-of-sight pursuit, ramming, hostile fire, and sight-based escape.
- **Combat:** aimed sidearm, swept bullets, magazine and finite reserve, reload time, armor, civilian penalties, and medical recovery.
- **Contracts:** three-drop courier runs, marked-car acquisition, and informant pickup/extraction; timers, failure states, escalating rewards, chain bonuses, and persistent contract reputation counters.
- **Progression:** cash, completed contracts, best chain, armor unlock, three engine upgrades, garages, and autosaves.
- **Presentation:** chase camera, speed zoom, procedural vehicle details and headlights, neon lighting, particles, rain, bloom/vignette/grain shader, minimap, route overlay, menus, settings, HUD, notifications, and synthesized soundtrack/SFX.

## CI-only build and test policy

**Development builds and tests are run on GitHub-hosted runners, not on the author's PC.** The repository includes no script that automatically compiles locally.

[The workflow](.github/workflows/ci.yml) builds release binaries on Ubuntu, Windows, and macOS, runs the headless core test suite on all three, and runs ASan/UBSan separately on Linux. The Linux game is launched under Xvfb with software OpenGL. CI checks the captured framebuffer and uploads the screenshot and renderer log as evidence. Version tags publish packages only after all required jobs pass.

To build or test without using your PC:

1. Fork the public repository, or push a branch if you are a collaborator.
2. Open **Actions → CI → Run workflow**, or open a pull request.
3. Download platform binaries and renderer evidence from the finished run.

See [CI and architecture](docs/ARCHITECTURE.md) for details. Standard CMake files are supplied for portability, but no local compilation is needed to obtain binaries.

## Saves and command-line options

Progress is saved after contracts, purchases, rest, recovery, and normal exit. A versioned checksum and previous-save backup guard against damaged writes. Saves restore the economy and upgrades into a fresh city; they do **not** resume an in-progress mission or chase.

- Windows: `%LOCALAPPDATA%/NeonBorough/progress.sav`
- Linux: `$XDG_DATA_HOME/neon-borough/progress.sav` or `~/.local/share/neon-borough/progress.sav`
- macOS: `~/Library/Application Support/NeonBorough/progress.sav`

`--seed N` selects a city seed. `--no-audio` avoids initializing the audio device. `--no-save` runs without disk persistence. `--save-dir PATH` isolates saves. `--help` lists options. Delete or move the save to reset progress.

CI uses `--smoke --frames 240 --screenshot artifacts/gameplay.png`; this disables audio and saving and runs scripted input. It is an automated smoke check, not a substitute for human playtesting.

## License

Original code and procedural artwork/audio: [MIT](LICENSE). raylib and its bundled dependencies retain their own licenses; see [THIRD_PARTY.md](THIRD_PARTY.md). This project is not affiliated with or endorsed by Rockstar Games or Take-Two Interactive.
