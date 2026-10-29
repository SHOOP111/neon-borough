# Changelog

## 0.2.0 — deep hardening and navigation

- Free-roam map waypoints with service snapping, arrival clearing, and contract-route priority.
- Keyboard title/pause navigation, persistent F1 help, focus-loss pause, visible garage cursor, and suppression of menu-click gunfire.
- Shared progression bounds, saturation at endgame caps, valid-seed restores, no-op/remote/wreck purchase rejection, and reset recovery timers.
- First-impact swept projectile collision with actor ordering, wall/boundary entry points, source-vehicle exclusion, hostile vehicle cover, and immediate wreck resolution.
- Prevented contract rewards after defeat, invalid contact assignment, continued pursuit by cleared patrols, permanently depleted traffic, and abandoned mission-target pool leaks.
- Locale-independent bounded save reads/parsing, strict unsigned fields, aggregate reputation checks, missing/damaged-primary backup recovery, backup preservation, non-file write-target rejection, temporary cleanup, and autosave backoff.
- Full dynamic-state replay digests, finite controller guards, clamped analog values, safe graph/movement boundaries, and overflow-safe unbiased integer RNG ranges.
- Master audio mute applies to SFX and music; graphics/audio resource checks and render-target resize failure reporting.
- Expanded hosted coverage: 33 original + 29 review C++ cases, CLI validation, negative-path evidence tests, static analysis, warnings-as-errors, core and renderer ASan/UBSan, multi-size and resize/overlay/effects lifecycle captures, audible/muted output checks.
- raylib source archive verified by SHA-256; release version matching and every required CI job gate publication.

## 0.1.0 — first playable

- Original seeded four-district city with navigable streets, alleys, parks, and services.
- Four vehicle archetypes, arcade driving, real handbrake slip, tire trails, traffic signals, damage, and wrecks.
- Pedestrians, aimed sidearm combat, armor/reload, five wanted levels, road-routed patrols, and occlusion-based escape.
- Courier, marked-car acquisition, and informant extraction contracts with timers, rewards, and chain bonuses.
- Garages, engine upgrades, recovery, and checksummed economy/progression saves with backup rotation.
- Procedural artwork, night lighting, rain, post-processing, synth music/SFX, HUD, maps, and mouse/keyboard menus.
- GitHub-hosted Linux, Windows, and macOS builds; 33 headless test cases; ASan/UBSan; gameplay/title/map/garage framebuffer checks; virtual-device audio capture.
- Fixed defects found by remote CI and screenshot review: single-shot alert threshold, screenshot directory export, boundary camera framing, starter placement/paint, and title-button label fitting.

This is single-player desktop software with documented limitations, not a completed AAA game. See `docs/DESIGN.md`.
