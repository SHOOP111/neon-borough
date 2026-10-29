# Design

## Identity

Neon Borough is an original nocturnal city sandbox built around getting a car, taking a contract, and escaping a pursuing police network. The emphasis is fast access to systemic driving and clear feedback, rather than recreating another game's copyrighted content.

Four palette-coded neighborhoods share a connected 8 × 8 street graph. Building footprints leave real alleys. Contacts are cyan, garages green, the loft violet, and mission targets gold. The starting sports car is beside the safehouse so there is no grind before the first drive.

## Vehicle model

Each vehicle has a position, heading, velocity, health, and controller. Velocity is decomposed into forward and lateral components. Throttle changes forward speed; grip exponentially damps lateral velocity. Steering scales with signed forward speed. The handbrake reduces grip and increases yaw rate. Braking is stronger than throttle when velocity opposes the input.

Movement is subdivided into six-unit maximum steps and resolved per axis against building rectangles and the city boundary. A central clearance circle plus front/rear samples approximates a capsule. Vehicle contacts use circular overlap separation and normal impulses. This favors a responsive arcade sandbox over a full rigid-body solver.

## Contracts

- **Night Shift / courier:** three distant drops in any working vehicle. Each delivery extends the deadline. Being on foot does not count.
- **Borrowed Chrome / acquisition:** take a specific marked sports car, triggering its tracker, then bring that exact car to the north garage with no wanted stars. Destruction fails the contract.
- **Dead Air / extraction:** hold near an informant for three seconds, triggering a larger pursuit, then reach the loft with zero wanted stars.

Rewards scale with completed contracts. A capped chain bonus rewards consecutive successes. Timers, destroyed targets, and medical recovery terminate a contract. Contact assignment repeats across six contact locations. Reputation is currently a saved per-contract-type counter, not a branching gang diplomacy system.

## Police and escape

Crimes increase fractional heat, capped at five. Displayed stars are rounded up above a small dead zone. Patrols spawn at road nodes outside the immediate view and follow shortest grid routes. Nearby visible suspects trigger direct pursuit. Higher heat permits passenger gunfire. Sight is determined by building occlusion, not just distance.

Heat decays only after eight seconds without a visible patrol. Extraction contracts decay slightly faster to keep their deadline practical. When heat clears, distant patrols leave and nearby patrols resume traffic rather than pursuing indefinitely. A safehouse cannot clear wanted levels for free. There is no instant teleporting arrest mechanic; loss of health triggers medical recovery and a cash penalty before contract completion can award a reward.

## Navigation and services

The full map pauses simulation and supports clicking a service or road waypoint. Routes prefer an active contract; free-roam waypoints resume afterward and clear on arrival. This is guidance along the connected street graph, not autonomous driving or shortest geometric navigation through alleys.

Garages stay live. Purchases require proximity and eligibility: wrecks cannot be repaired, full supplies do not consume money, and engine tuning caps at level three. Cash and counters saturate at save-format limits so long-term progression remains loadable. Saves recover a known-good backup but are not a multiplayer/shared-writer database.

## Scope and limitations

This is a first playable **single-player desktop** game. It does not contain multiplayer, interiors, a story campaign, a weapon shop, gamepad support, destructible buildings, articulated pedestrian animation, or a streamed open-world engine. Music is synthesized, not a licensed soundtrack. The NPC controller is a lightweight road/sidewalk controller and can get stuck, especially in dense collisions. Vehicle contacts use approximations rather than exact oriented rigid bodies. Parks and trees are decorative; buildings and boundaries provide collision.

The city is deterministic for a seed, not modeled on a real place. Saves persist progress rather than every transient entity. Settings are session-only. Packages are unsigned. Automated CI verifies mechanics and the renderer startup path; subjective driving feel, long-session behavior, every GPU/audio driver, and accessibility need human playtesting.

Future work should be driven by playtest evidence: traffic intersection reservation, richer pursuit tactics, improved collision recovery, audio-device coverage, and controller/accessibility support before cosmetic feature expansion.
