#include "neon/simulation.hpp"
#include <bit>
#include <cstddef>
#include <utility>

namespace neon {
namespace {
constexpr int maxVehicles = 96;
Vec2 rightOf(Vec2 directionValue) {
    return {-directionValue.y, directionValue.x};
}
Vec2 sidewalkCorner(int block, int corner) {
    float x = static_cast<float>((block % cityBlocks) * blockSize) + 136;
    float y = static_cast<float>((block / cityBlocks) * blockSize) + 136;
    constexpr Vec2 offsets[]{{0, 0}, {236, 0}, {236, 236}, {0, 236}};
    return Vec2{x, y} + offsets[corner % 4];
}
} // namespace
float Vehicle::topSpeed() const {
    switch (kind) {
    case VehicleKind::Sport:
        return 310;
    case VehicleKind::Van:
        return 170;
    case VehicleKind::Police:
        return 270;
    default:
        return 220;
    }
}
Simulation::Simulation(std::uint64_t seed) : rng_(seed ^ 0x43495459ULL), city(seed) {
    progress.seed = seed;
    player.position = city.spawn();
    player.aim = {1, 0};
    vehicles.reserve(maxVehicles);
    for (int i = 0; i < 48; ++i) {
        int n = rng_.integer(0, cityBlocks * cityBlocks - 1);
        auto neighbors = city.neighbors(n);
        int next =
            neighbors[static_cast<std::size_t>(rng_.integer(0, static_cast<int>(neighbors.size()) - 1))];
        Vec2 dir = normalized(city.node(next) - city.node(n));
        Vec2 pos = lerp(city.node(n), city.node(next), rng_.range(0.12f, 0.85f)) + rightOf(dir) * 22;
        int id = spawnVehicle(pos, static_cast<VehicleKind>(rng_.integer(0, 2)), -1);
        auto* car = vehicle(id);
        car->nodeFrom = n;
        car->nodeTo = next;
        car->heading = angle(dir);
        car->velocity = dir * 80;
    }
    int starter = spawnVehicle({210, 484}, VehicleKind::Sport, 2);
    vehicle(starter)->heading = 0;
    vehicle(starter)->color = 6;
    for (int i = 0; i < 100; ++i) {
        int block = rng_.integer(0, cityBlocks * cityBlocks - 1), corner = rng_.integer(0, 3);
        Vec2 a = sidewalkCorner(block, corner), b = sidewalkCorner(block, (corner + 1) % 4);
        pedestrians.push_back({lerp(a, b, rng_.unit()), b, angle(b - a), 0, 100, 0, rng_.integer(0, 5),
                               block * 4 + corner, block * 4 + (corner + 1) % 4});
    }
    notice("Welcome to Neon Borough. E: take a car. F: contacts / services.");
}
void Simulation::notice(std::string text) {
    events.push_back({EventKind::Notice, playerPosition(), std::move(text), 0});
}
Vehicle* Simulation::vehicle(int id) {
    for (auto& v : vehicles)
        if (v.id == id && v.active)
            return &v;
    return nullptr;
}
const Vehicle* Simulation::vehicle(int id) const {
    for (const auto& v : vehicles)
        if (v.id == id && v.active)
            return &v;
    return nullptr;
}
Vec2 Simulation::playerPosition() const {
    if (const auto* car = vehicle(player.vehicle))
        return car->position;
    return player.position;
}
int Simulation::wanted() const {
    return heat < 0.15f ? 0 : std::min(5, static_cast<int>(std::ceil(heat)));
}
void Simulation::crime(float severity) {
    if (!std::isfinite(severity) || severity <= 0)
        return;
    heat = std::clamp(heat + severity, 0.0f, 5.0f);
    unseen = 0;
}
int Simulation::spawnVehicle(Vec2 position, VehicleKind kind, int driver) {
    if (city.blocked(position, 19) || nextVehicleId_ == std::numeric_limits<int>::max() ||
        (vehicles.size() >= maxVehicles &&
         std::none_of(vehicles.begin(), vehicles.end(), [](const Vehicle& v) { return !v.active; })))
        return -1;
    Vehicle v;
    v.id = nextVehicleId_++;
    v.position = position;
    v.kind = kind;
    v.driver = driver;
    v.color = rng_.integer(0, 7);
    v.nodeFrom = city.nearestNode(position);
    auto choices = city.neighbors(v.nodeFrom);
    v.nodeTo = choices[static_cast<std::size_t>(rng_.integer(0, static_cast<int>(choices.size()) - 1))];
    for (auto& slot : vehicles)
        if (!slot.active) {
            slot = v;
            return v.id;
        }
    if (vehicles.size() >= maxVehicles)
        return -1;
    vehicles.push_back(v);
    return v.id;
}
void Simulation::tick(const Input& input, float dt) {
    if (!std::isfinite(dt) || dt <= 0 || dt > 0.1f || !finite(input.movement) || !finite(input.aim) ||
        !std::isfinite(input.throttle) || !std::isfinite(input.steering))
        return;
    Input safe = input;
    safe.throttle = std::clamp(safe.throttle, -1.0f, 1.0f);
    safe.steering = std::clamp(safe.steering, -1.0f, 1.0f);
    events.clear();
    time += dt;
    trauma = approach(trauma, 0, dt * 1.8f);
    missionCooldown = std::max(0.0f, missionCooldown - dt);
    player.invulnerable = std::max(0.0f, player.invulnerable - dt);
    honkTimer_ = std::max(0.0f, honkTimer_ - dt);
    if (garageOpen && !nearGarage())
        garageOpen = false;
    if (player.health <= 0) {
        defeat();
        return;
    }
    if (safe.shopChoice)
        purchase(safe.shopChoice);
    updatePlayer(safe, dt);
    updatePolice(dt);
    updateVehicles(safe, dt);
    updatePedestrians(dt);
    updateBullets(dt);
    wreckVehicles();
    if (player.health <= 0)
        defeat();
    else
        updateMission(dt);
    populationTimer_ += dt;
    if (populationTimer_ > 8) {
        populationTimer_ = 0;
        // Recycle destroyed vehicles only after their wreck has been visible for a while.
        for (auto& car : vehicles)
            if (car.active && (car.health <= 0 || car.retired) && car.id != player.vehicle &&
                distance(car.position, playerPosition()) > 700)
                car.active = false;
        int traffic = static_cast<int>(std::count_if(vehicles.begin(), vehicles.end(), [](const Vehicle& v) {
            return v.active && v.health > 0 && v.driver == -1;
        }));
        for (int attempt = 0; traffic < 48 && attempt < 64; ++attempt) {
            int n = rng_.integer(0, cityBlocks * cityBlocks - 1);
            auto options = city.neighbors(n);
            int next =
                options[static_cast<std::size_t>(rng_.integer(0, static_cast<int>(options.size()) - 1))];
            Vec2 dir = normalized(city.node(next) - city.node(n));
            Vec2 pos = city.node(n) + dir * 130 + rightOf(dir) * 22;
            if (distance(pos, playerPosition()) < 700 ||
                std::any_of(vehicles.begin(), vehicles.end(),
                            [pos](const Vehicle& v) { return v.active && distance(v.position, pos) < 60; }))
                continue;
            int id = spawnVehicle(pos, static_cast<VehicleKind>(rng_.integer(0, 2)), -1);
            if (auto* car = vehicle(id)) {
                car->nodeFrom = n;
                car->nodeTo = next;
                car->heading = angle(dir);
                car->velocity = dir * 80;
                ++traffic;
            } else
                break;
        }
    }
}
void Simulation::updatePlayer(const Input& in, float dt) {
    player.shotTimer = std::max(0.0f, player.shotTimer - dt);
    if (player.reloadTimer > 0) {
        player.reloadTimer = std::max(0.0f, player.reloadTimer - dt);
        if (player.reloadTimer == 0) {
            int amount = std::min(16 - player.ammo, player.reserve);
            player.ammo += amount;
            player.reserve -= amount;
        }
    }
    if (in.reload && player.ammo < 16 && player.reserve > 0 && player.reloadTimer == 0)
        player.reloadTimer = 1.15f;
    if (length(in.aim - playerPosition()) > 2)
        player.aim = normalized(in.aim - playerPosition());
    if (in.enter) {
        if (auto* car = vehicle(player.vehicle)) {
            Vec2 right = rightOf(direction(car->heading));
            Vec2 candidates[]{car->position + right * 34, car->position - right * 34,
                              car->position - direction(car->heading) * 42};
            bool exited = false;
            for (Vec2 p : candidates)
                if (!city.blocked(p, 9)) {
                    player.position = p;
                    player.vehicle = -1;
                    car->driver = 2;
                    exited = true;
                    break;
                }
            if (!exited)
                notice("Door blocked. Move the vehicle away from the wall.");
        } else {
            Vehicle* closest = nullptr;
            float nearest = 56;
            for (auto& v : vehicles)
                if (v.active && v.health > 0 && distance(v.position, player.position) < nearest) {
                    nearest = distance(v.position, player.position);
                    closest = &v;
                }
            if (closest) {
                if (closest->driver != 2)
                    crime(closest->kind == VehicleKind::Police ? 1.5f : 0.55f);
                closest->driver = 0;
                player.vehicle = closest->id;
                garageOpen = false;
                notice(closest->kind == VehicleKind::Sport ? "APEX GT // rear-wheel drive"
                                                           : "Vehicle acquired. Space: handbrake. E: exit.");
            } else
                notice("No vehicle in reach.");
        }
    }
    if (!vehicle(player.vehicle)) {
        player.vehicle = -1;
        bool sprint = in.sprint && player.stamina > 2 && length(in.movement) > 0.1f;
        float speed = sprint ? 155.0f : 96.0f;
        player.position = city.moveCircle(player.position, normalized(in.movement) * speed * dt, 9);
        player.stamina = std::clamp(player.stamina + (sprint ? -30.0f : 22.0f) * dt, 0.0f, 100.0f);
    } else
        player.stamina = std::min(100.0f, player.stamina + 25 * dt);
    if (in.shoot && player.shotTimer <= 0 && player.reloadTimer == 0 && player.ammo > 0) {
        --player.ammo;
        player.shotTimer = 0.18f;
        Vec2 dir = direction(angle(player.aim) + rng_.range(-0.035f, 0.035f));
        addBullet(playerPosition() + dir * (player.vehicle == -1 ? 14.0f : 26.0f), dir, false,
                  player.vehicle);
        crime(0.18f);
        trauma = std::max(trauma, 0.08f);
    }
    if (in.shoot && player.ammo == 0 && player.reserve > 0 && player.reloadTimer == 0)
        player.reloadTimer = 1.15f;
    if (in.interact)
        interact();
    if (in.honk && player.vehicle != -1 && honkTimer_ == 0) {
        honkTimer_ = 1;
        events.push_back({EventKind::Siren, playerPosition(), "", 0});
        for (auto& p : pedestrians)
            if (distance(p.position, playerPosition()) < 150)
                p.panic = 2;
    }
    if (garageOpen && !nearGarage())
        garageOpen = false;
}
void Simulation::drive(Vehicle& car, float throttle, float steer, bool handbrake, float dt) {
    const float previousHeading = car.heading;
    Vec2 forward = direction(car.heading), side = rightOf(forward);
    float longitudinal = dot(car.velocity, forward), lateral = dot(car.velocity, side);
    float engine = car.driver == 0 ? 1.0f + 0.12f * static_cast<float>(progress.engineLevel) : 1.0f;
    float acceleration = car.kind == VehicleKind::Van ? 115.0f : 185.0f;
    if (throttle * longitudinal < 0)
        acceleration = 320;
    longitudinal += throttle * acceleration * engine * dt;
    longitudinal = approach(longitudinal, 0, (std::abs(throttle) < 0.01f ? 32.0f : 7.0f) * dt);
    longitudinal = std::clamp(longitudinal, -85.0f, car.topSpeed() * engine);
    float grip = handbrake ? 1.6f : 9.0f;
    // Preserve world-space momentum through yaw; grip then pulls velocity toward
    // the new heading. This gives the handbrake real lateral slip, not tank turns.
    Vec2 momentum = forward * longitudinal + side * lateral;
    float turning = std::clamp(longitudinal / 65.0f, -1.0f, 1.0f);
    car.heading = wrapAngle(car.heading + steer * turning * (handbrake ? 3.4f : 2.0f) * dt);
    Vec2 newForward = direction(car.heading), newSide = rightOf(newForward);
    float slip = dot(momentum, newSide) * std::exp(-grip * dt);
    car.velocity = newForward * dot(momentum, newForward) + newSide * slip;
    if (handbrake)
        car.velocity *= std::exp(-0.8f * dt);
    if (car.driver == 0 && std::abs(slip) > 32 && length(car.velocity) > 80)
        events.push_back({EventKind::Skid, car.position, "", car.heading});
    bool hit = false;
    Vec2 previous = car.position;
    car.position = city.moveCircle(car.position, car.velocity * dt, 19, &hit);
    // Additional front/rear clearance approximates a capsule rather than a point car.
    if (city.blocked(car.position + direction(car.heading) * 12, 10) ||
        city.blocked(car.position - direction(car.heading) * 12, 10)) {
        car.position = previous;
        car.heading = previousHeading;
        hit = true;
    }
    if (hit) {
        float impact = length(car.velocity);
        car.velocity *= 0.28f;
        if (impact > 55) {
            car.health -= impact * 0.04f;
            car.aiTimer = 0.25f;
            events.push_back({EventKind::Impact, car.position, "", impact});
            if (car.driver == 0)
                trauma = std::max(trauma, std::min(0.65f, impact / 400));
        }
    }
}
void Simulation::updateVehicles(const Input& in, float dt) {
    for (auto& car : vehicles) {
        if (!car.active || car.health <= 0)
            continue;
        car.aiTimer = std::max(0.0f, car.aiTimer - dt);
        car.fireTimer = std::max(0.0f, car.fireTimer - dt);
        if (car.driver == 0) {
            drive(car, in.throttle, in.steering, in.handbrake, dt);
            continue;
        }
        if (car.driver == 2) {
            drive(car, 0, 0, false, dt);
            continue;
        }
        Vec2 target;
        float desired = car.driver == 1 ? 205.0f : 100.0f;
        if (car.driver == 1 && distance(car.position, playerPosition()) < 380 &&
            city.lineOfSight(car.position, playerPosition())) {
            target = playerPosition();
            desired = player.vehicle == -1 ? 130.0f : 245.0f;
        } else {
            Vec2 edge = normalized(city.node(car.nodeTo) - city.node(car.nodeFrom));
            target = city.node(car.nodeTo) + rightOf(edge) * 22;
            if (distance(car.position, target) < 48) {
                int old = car.nodeFrom;
                car.nodeFrom = car.nodeTo;
                auto options = city.neighbors(car.nodeFrom);
                if (car.driver == 1) {
                    auto path = city.route(city.node(car.nodeFrom), playerPosition());
                    car.nodeTo = path.size() > 1 ? path[1] : options.front();
                } else {
                    if (options.size() > 1)
                        std::erase(options, old);
                    car.nodeTo = options[static_cast<std::size_t>(
                        rng_.integer(0, static_cast<int>(options.size()) - 1))];
                }
                edge = normalized(city.node(car.nodeTo) - city.node(car.nodeFrom));
                target = city.node(car.nodeTo) + rightOf(edge) * 22;
            }
        }
        float error = wrapAngle(angle(target - car.position) - car.heading);
        float steering = std::clamp(error * 2.5f, -1.0f, 1.0f);
        desired *= std::max(0.2f, 1.0f - std::abs(error) * 0.5f);
        for (const auto& other : vehicles)
            if (other.active && other.id != car.id && other.health > 0) {
                Vec2 delta = other.position - car.position;
                if (dot(delta, direction(car.heading)) > 0 && length(delta) < 70 &&
                    std::abs(dot(delta, rightOf(direction(car.heading)))) < 26) {
                    if (car.driver != 1 || other.driver != 0)
                        desired = std::min(desired, length(other.velocity) * 0.8f);
                }
            }
        bool waitingForLight = false;
        if (car.driver == -1) {
            // Alternating right of way at intersections; never stops cars in the middle.
            Vec2 n = city.node(car.nodeTo);
            float d = distance(car.position, n);
            bool vertical = std::abs(std::sin(car.heading)) > 0.7f;
            bool green = (static_cast<int>(std::fmod(time / 7, 2.0f)) == 0) == vertical;
            if (!green && d > 62 && d < 112) {
                desired = 0;
                waitingForLight = true;
            }
        }
        float speed = dot(car.velocity, direction(car.heading));
        float throttle = std::clamp((desired - speed) * 0.045f, -1.0f, 1.0f);
        if (length(car.velocity) < 8 && !waitingForLight)
            car.stuckTimer += dt;
        else
            car.stuckTimer = 0;
        if (car.stuckTimer > 2.0f) {
            throttle = -0.6f;
            steering = -steering;
        }
        if (car.stuckTimer > 4.0f) {
            car.stuckTimer = 0;
            car.nodeFrom = city.nearestNode(car.position);
            auto ns = city.neighbors(car.nodeFrom);
            car.nodeTo = ns.front();
        }
        drive(car, throttle, steering, false, dt);
        if (car.driver == 1 && car.fireTimer <= 0 && wanted() >= 2 &&
            distance(car.position, playerPosition()) < 230 &&
            city.lineOfSight(car.position, playerPosition())) {
            Vec2 dir = direction(angle(playerPosition() - car.position) + rng_.range(-0.16f, 0.16f));
            addBullet(car.position + dir * 30, dir, true, car.id);
            car.fireTimer = rng_.range(0.7f, 1.3f);
        }
    }
    for (std::size_t i = 0; i < vehicles.size(); ++i)
        for (std::size_t j = i + 1; j < vehicles.size(); ++j) {
            auto& a = vehicles[i];
            auto& b = vehicles[j];
            if (!a.active || !b.active || a.health <= 0 || b.health <= 0)
                continue;
            Vec2 delta = b.position - a.position;
            float d = length(delta);
            if (d >= 37)
                continue;
            Vec2 n = d > 0.01f ? delta / d : Vec2{1, 0};
            float impulse = dot(a.velocity - b.velocity, n);
            a.position = city.moveCircle(a.position, n * (-(37 - d) * 0.5f), 19);
            b.position = city.moveCircle(b.position, n * ((37 - d) * 0.5f), 19);
            if (impulse > 0) {
                a.velocity -= n * (impulse * 0.65f);
                b.velocity += n * (impulse * 0.65f);
            }
            if (impulse > 50) {
                a.health -= impulse * 0.035f;
                b.health -= impulse * 0.035f;
                events.push_back({EventKind::Impact, (a.position + b.position) * 0.5f, "", impulse});
                if (a.driver == 0 || b.driver == 0) {
                    trauma = std::max(trauma, 0.25f);
                    if (a.driver == 1 || b.driver == 1)
                        crime(0.1f);
                }
            }
        }
    wreckVehicles();
}
void Simulation::wreckVehicles() {
    for (auto& car : vehicles)
        if (car.active && car.health <= 0 && car.health > -999) {
            car.health = -1000;
            car.velocity = {};
            events.push_back({EventKind::Explosion, car.position, "", 1});
            if (car.driver == 0) {
                player.vehicle = -1;
                Vec2 exit = car.position + rightOf(direction(car.heading)) * 35;
                player.position = city.blocked(exit, 9) ? city.moveCircle(car.position, {0, 45}, 9) : exit;
                player.health -= 30;
                player.invulnerable = 1;
                trauma = 1;
                notice("Vehicle lost. Get clear!");
            }
        }
}
void Simulation::updatePedestrians(float dt) {
    Vec2 playerPos = playerPosition();
    for (auto& p : pedestrians) {
        if (p.health <= 0) {
            p.wait += dt;
            if (p.wait > 30 && distance(p.position, playerPos) > 600) {
                p.health = 100;
                p.wait = 0;
                p.position = sidewalkCorner(p.corner / 4, p.corner % 4);
            }
            continue;
        }
        p.panic = std::max(0.0f, p.panic - dt);
        if (wanted() > 0 && distance(p.position, playerPos) < 120 && city.lineOfSight(p.position, playerPos))
            p.panic = 2;
        Vec2 dir;
        if (p.panic > 0)
            dir = normalized(p.position - playerPos);
        else {
            if (distance(p.position, p.target) < 5) {
                p.corner = p.nextCorner;
                p.nextCorner = (p.corner / 4) * 4 + (p.corner + 1) % 4;
                p.target = sidewalkCorner(p.nextCorner / 4, p.nextCorner % 4);
            }
            dir = normalized(p.target - p.position);
        }
        p.heading = angle(dir);
        p.position = city.moveCircle(p.position, dir * (p.panic > 0 ? 90.0f : 32.0f) * dt, 6);
        for (const auto& car : vehicles)
            if (car.active && car.health > 0 && distance(p.position, car.position) < 23 &&
                length(car.velocity) > 50) {
                p.health = 0;
                p.wait = 0;
                events.push_back({EventKind::Impact, p.position, "", 30});
                if (car.driver == 0) {
                    crime(0.65f);
                    player.cash = std::max(0, player.cash - 100);
                    notice("Civilian injured. $100 liability + police alert.");
                }
                break;
            }
    }
}
void Simulation::addBullet(Vec2 pos, Vec2 dir, bool hostile, int sourceVehicle) {
    if (bullets.size() < 128)
        bullets.push_back({pos, pos, dir * (hostile ? 600.0f : 850.0f), 1.2f, hostile, sourceVehicle});
    events.push_back({EventKind::Shot, pos, "", hostile ? 0.0f : 1.0f});
}
void Simulation::updateBullets(float dt) {
    for (auto& b : bullets) {
        b.previous = b.position;
        Vec2 end = b.position + b.velocity * dt;
        b.life -= dt;
        float first = city.obstruction(b.previous, end);
        Vehicle* hitCar = nullptr;
        Pedestrian* hitPed = nullptr;
        bool hitPlayer = false;
        for (auto& car : vehicles) {
            if (!car.active || car.health <= 0 || car.id == b.sourceVehicle ||
                (!b.hostile && car.id == player.vehicle))
                continue;
            float t = segmentCircleTime(b.previous, end, car.position, 21);
            if (t < first) {
                first = t;
                hitCar = &car;
            }
        }
        if (b.hostile && player.vehicle == -1) {
            float t = segmentCircleTime(b.previous, end, player.position, 10);
            if (t < first) {
                first = t;
                hitCar = nullptr;
                hitPlayer = true;
            }
        } else if (!b.hostile) {
            for (auto& p : pedestrians) {
                if (p.health <= 0)
                    continue;
                float t = segmentCircleTime(b.previous, end, p.position, 8);
                if (t < first) {
                    first = t;
                    hitCar = nullptr;
                    hitPed = &p;
                }
            }
        }
        b.position = std::isfinite(first) ? lerp(b.previous, end, first) : end;
        if (hitCar) {
            if (!b.hostile || hitCar->id != player.vehicle || player.invulnerable == 0)
                hitCar->health -= b.hostile ? 4.0f : 18.0f;
            if (!b.hostile && hitCar->kind == VehicleKind::Police)
                crime(0.35f);
            if (b.hostile && hitCar->id == player.vehicle) {
                player.invulnerable = std::max(player.invulnerable, 0.12f);
                trauma = std::max(trauma, 0.2f);
            }
        } else if (hitPlayer && player.invulnerable == 0) {
            float absorbed = std::min(player.armor, 6.0f);
            player.armor -= absorbed;
            player.health -= 8 - absorbed;
            player.invulnerable = 0.12f;
            trauma = std::max(trauma, 0.2f);
        } else if (hitPed) {
            hitPed->health -= 55;
            hitPed->panic = 8;
            crime(0.5f);
        }
        if (std::isfinite(first)) {
            b.life = 0;
            events.push_back({EventKind::Impact, b.position, "", 12});
        }
    }
    std::erase_if(bullets, [](const Bullet& b) { return b.life <= 0; });
}
void Simulation::updatePolice(float dt) {
    bool seen = false;
    int cops = 0;
    for (auto& v : vehicles)
        if (v.active && v.driver == 1 && v.health > 0) {
            ++cops;
            float d = distance(v.position, playerPosition());
            if (wanted() == 0) {
                if (d > 550)
                    v.active = false;
                else {
                    v.driver = -1;
                    v.nodeFrom = city.nearestNode(v.position);
                    v.nodeTo = city.neighbors(v.nodeFrom).front();
                    v.stuckTimer = 0;
                }
                --cops;
            } else if (d < 480 && city.lineOfSight(v.position, playerPosition()))
                seen = true;
        }
    unseen = seen ? 0 : unseen + dt;
    if (heat > 0 && unseen > 8)
        heat = std::max(
            0.0f, heat - dt * (mission.active && mission.kind == MissionKind::Extraction ? 0.1f : 0.075f));
    policeTimer_ -= dt;
    if (wanted() > 0 && cops < wanted() * 2 && policeTimer_ <= 0) {
        policeTimer_ = std::max(1.8f, 5.0f - static_cast<float>(wanted()) * 0.6f);
        std::vector<int> candidates;
        for (int n = 0; n < cityBlocks * cityBlocks; ++n) {
            float d = distance(city.node(n), playerPosition());
            if (d > 520 && d < 1100)
                candidates.push_back(n);
        }
        if (!candidates.empty()) {
            int n = candidates[static_cast<std::size_t>(
                rng_.integer(0, static_cast<int>(candidates.size()) - 1))];
            Vec2 spawn = city.node(n) + Vec2{22, 0};
            if (std::any_of(vehicles.begin(), vehicles.end(), [spawn](const Vehicle& v) {
                    return v.active && distance(v.position, spawn) < 60;
                }))
                return;
            int id = spawnVehicle(spawn, VehicleKind::Police, 1);
            if (auto* car = vehicle(id)) {
                auto path = city.route(car->position, playerPosition());
                car->nodeFrom = n;
                car->nodeTo = path.size() > 1 ? path[1] : city.neighbors(n).front();
                car->heading = angle(city.node(car->nodeTo) - city.node(n));
            }
        }
    }
}
Vec2 Simulation::deliveryPoint() {
    for (int attempt = 0; attempt < 40; ++attempt) {
        const auto& s = city.services[static_cast<std::size_t>(
            rng_.integer(0, static_cast<int>(city.services.size()) - 1))];
        if (distance(s.position, playerPosition()) > 600)
            return s.position + Vec2{0, -30};
    }
    return city.services.back().position + Vec2{0, -30};
}
void Simulation::startMission(int contactIndex) {
    if (contactIndex < 0 || contactIndex >= 6) {
        notice("Invalid contact.");
        return;
    }
    if (mission.active || missionCooldown > 0) {
        notice("Finish the current contract first, or wait for the next dispatch.");
        return;
    }
    mission = {};
    mission.active = true;
    mission.kind = static_cast<MissionKind>(contactIndex % 3);
    mission.reward = 1200 + progress.completed * 100;
    mission.timeLeft = 180;
    if (mission.kind == MissionKind::Courier) {
        mission.title = "NIGHT SHIFT";
        mission.objective = "Deliver the package. Reach the marked drop in any vehicle.";
        mission.target = deliveryPoint();
        mission.reward += 300;
    } else if (mission.kind == MissionKind::Acquisition) {
        mission.title = "BORROWED CHROME";
        mission.objective = "Acquire the marked APEX GT. Keep it intact.";
        mission.target = deliveryPoint();
        mission.targetVehicle = spawnVehicle(mission.target, VehicleKind::Sport, 2);
        if (mission.targetVehicle == -1) {
            mission.active = false;
            notice("No target available. Try again shortly.");
            return;
        }
        vehicle(mission.targetVehicle)->color = 6;
        mission.reward += 650;
        mission.timeLeft = 210;
    } else {
        mission.title = "DEAD AIR";
        mission.objective = "Reach the informant. Hold position for 3 seconds.";
        mission.target = deliveryPoint();
        mission.reward += 900;
        mission.timeLeft = 240;
    }
    garageOpen = false;
    notice("CONTRACT // " + mission.title);
}
void Simulation::updateMission(float dt) {
    if (!mission.active)
        return;
    mission.timeLeft -= dt;
    if (mission.timeLeft <= 0) {
        failMission("Dispatch window expired.");
        return;
    }
    Vec2 pos = playerPosition();
    if (mission.kind == MissionKind::Courier) {
        if (player.vehicle != -1 && distance(pos, mission.target) < 58) {
            ++mission.deliveries;
            if (mission.deliveries >= 3) {
                finishMission();
                return;
            }
            mission.target = deliveryPoint();
            mission.timeLeft += 35;
            notice("Package delivered. Next drop uploaded.");
        }
    } else if (mission.kind == MissionKind::Acquisition) {
        const auto* targetCar = vehicle(mission.targetVehicle);
        if (!targetCar || targetCar->health <= 0) {
            failMission("The target vehicle was destroyed.");
            return;
        }
        if (mission.stage == 0) {
            mission.target = targetCar->position;
            if (player.vehicle == mission.targetVehicle) {
                mission.stage = 1;
                mission.target = city.services[7].position + Vec2{0, -30};
                crime(1.3f);
                mission.objective = "Deliver the marked car to PITSTOP NORTH. Lose all wanted stars first.";
                notice("Tracker activated. Shake the police and deliver the car.");
            }
        } else if (player.vehicle == mission.targetVehicle && distance(pos, mission.target) < 70 &&
                   wanted() == 0)
            finishMission();
    } else {
        if (mission.stage == 0) {
            if (distance(pos, mission.target) < 48)
                mission.hold += dt;
            else
                mission.hold = std::max(0.0f, mission.hold - dt);
            if (mission.hold >= 3) {
                mission.stage = 1;
                mission.target = city.services.front().position + Vec2{0, -30};
                crime(2.3f);
                mission.objective = "Escort the informant to THE LOFT. Lose all wanted stars before arrival.";
                notice("Informant secured. Police dispatched. Break line of sight!");
            }
        } else if (distance(pos, mission.target) < 60 && wanted() == 0)
            finishMission();
    }
}
void Simulation::finishMission() {
    if (!mission.active || player.health <= 0)
        return;
    chain = std::min(chain + 1, maxCompleted);
    if (progress.completed < maxCompleted) {
        ++progress.completed;
        ++progress.reputation[static_cast<std::size_t>(mission.kind)];
    }
    progress.bestChain = std::max(progress.bestChain, std::min(chain, progress.completed));
    int bonus = std::min(chain - 1, 5) * 150;
    int earned = std::min(mission.reward + bonus, maxCash - player.cash);
    player.cash += earned;
    events.push_back(
        {EventKind::Reward, playerPosition(), "CONTRACT COMPLETE // +$" + std::to_string(earned), 1});
    retireMissionTarget();
    mission.active = false;
    missionCooldown = 3;
    ++saveRevision;
}
void Simulation::retireMissionTarget() {
    if (mission.kind == MissionKind::Acquisition)
        if (auto* car = vehicle(mission.targetVehicle))
            car->retired = true;
}
void Simulation::failMission(const std::string& reason) {
    if (!mission.active)
        return;
    retireMissionTarget();
    mission.active = false;
    chain = 0;
    missionCooldown = 5;
    notice("CONTRACT FAILED // " + reason);
}
void Simulation::interact() {
    const Service* closest = nullptr;
    float d = 86;
    int contact = 0, selected = 0;
    for (const auto& s : city.services) {
        if (s.kind == ServiceKind::Contact) {
            if (distance(s.position, playerPosition()) < d)
                selected = contact;
            ++contact;
        }
        float next = distance(s.position, playerPosition());
        if (next < d) {
            d = next;
            closest = &s;
        }
    }
    if (!closest) {
        notice("Move closer to a cyan contact, green garage, or violet safehouse.");
        return;
    }
    if (closest->kind == ServiceKind::Contact)
        startMission(selected);
    else if (closest->kind == ServiceKind::Garage) {
        garageOpen = !garageOpen;
        notice("PITSTOP // 1 repair, 2 armor, 3 engine, 4 ammo. F to close.");
    } else {
        if (wanted() > 0)
            notice("Safehouse locked while wanted. Break line of sight to lose police.");
        else {
            player.health = 100;
            player.stamina = 100;
            ++saveRevision;
            notice("Rested and progress saved. The city is yours.");
        }
    }
}
bool Simulation::nearGarage() const {
    return std::any_of(city.services.begin(), city.services.end(), [this](const Service& s) {
        return s.kind == ServiceKind::Garage && distance(s.position, playerPosition()) < 100;
    });
}
void Simulation::purchase(int choice) {
    if (!garageOpen || !nearGarage())
        return;
    int cost = choice == 1   ? 150
               : choice == 2 ? 250
               : choice == 3 ? 650 + progress.engineLevel * 350
               : choice == 4 ? 120
                             : 0;
    if (cost == 0)
        return;
    if (choice == 1 && (!vehicle(player.vehicle) || vehicle(player.vehicle)->health <= 0)) {
        notice("Bring a vehicle for repairs.");
        return;
    }
    if ((choice == 1 && vehicle(player.vehicle)->health >= 100) || (choice == 2 && player.armor >= 100) ||
        (choice == 4 && player.reserve >= maxReserve)) {
        notice("Already full. No purchase needed.");
        return;
    }
    if (choice == 3 && progress.engineLevel >= maxEngineLevel) {
        notice("Engine tuning already maxed.");
        return;
    }
    if (player.cash < cost) {
        notice("Insufficient funds.");
        return;
    }
    player.cash -= cost;
    if (choice == 1)
        vehicle(player.vehicle)->health = 100;
    if (choice == 2) {
        player.armor = 100;
        progress.armorLevel = 1;
    }
    if (choice == 3)
        ++progress.engineLevel;
    if (choice == 4)
        player.reserve = std::min(maxReserve, player.reserve + 64);
    ++saveRevision;
    notice("Upgrade installed // -$" + std::to_string(cost));
}
void Simulation::defeat() {
    failMission("You were taken out.");
    if (auto* car = vehicle(player.vehicle))
        car->driver = 2;
    player.vehicle = -1;
    player.position = city.spawn();
    player.health = 100;
    player.armor = 0;
    player.ammo = 16;
    player.reserve = std::max(32, player.reserve);
    player.cash = std::max(0, player.cash - 350);
    player.invulnerable = 3;
    player.reloadTimer = player.shotTimer = 0;
    player.stamina = 100;
    heat = 0;
    unseen = 0;
    garageOpen = false;
    chain = 0;
    for (auto& car : vehicles)
        if (car.driver == 1)
            car.active = false;
    bullets.clear();
    ++saveRevision;
    notice("RECOVERED AT THE LOFT // $350 medical bill");
}
Progress Simulation::snapshot() const {
    Progress p = progress;
    p.cash = player.cash;
    return p;
}
bool Simulation::applyProgress(const Progress& p) {
    if (!validProgress(p) || p.seed != city.seed)
        return false;
    progress = p;
    player.cash = p.cash;
    player.armor = p.armorLevel ? 100.0f : 0.0f;
    return true;
}
std::uint64_t Simulation::digest() const {
    std::uint64_t h = 1469598103934665603ULL;
    auto mix = [&h](std::uint64_t v) {
        h ^= v;
        h *= 1099511628211ULL;
    };
    auto scalar = [&mix](float v) { mix(std::bit_cast<std::uint32_t>(v)); };
    auto vector = [&scalar](Vec2 v) {
        scalar(v.x);
        scalar(v.y);
    };
    auto integer = [&mix](int v) { mix(static_cast<std::uint64_t>(v)); };
    auto string = [&mix](const std::string& v) {
        mix(v.size());
        for (unsigned char c : v)
            mix(c);
    };
    mix(city.seed);
    mix(rng_.state());
    integer(nextVehicleId_);
    scalar(policeTimer_);
    scalar(populationTimer_);
    scalar(honkTimer_);
    scalar(time);
    scalar(heat);
    scalar(unseen);
    scalar(trauma);
    scalar(missionCooldown);
    integer(chain);
    integer(saveRevision);
    mix(garageOpen);
    vector(player.position);
    vector(player.aim);
    scalar(player.health);
    scalar(player.armor);
    scalar(player.stamina);
    scalar(player.shotTimer);
    scalar(player.reloadTimer);
    scalar(player.invulnerable);
    integer(player.vehicle);
    integer(player.cash);
    integer(player.ammo);
    integer(player.reserve);
    mix(progress.seed);
    integer(progress.cash);
    integer(progress.completed);
    integer(progress.bestChain);
    integer(progress.engineLevel);
    integer(progress.armorLevel);
    for (int rep : progress.reputation)
        integer(rep);
    mix(mission.active);
    integer(static_cast<int>(mission.kind));
    integer(mission.stage);
    integer(mission.targetVehicle);
    integer(mission.deliveries);
    vector(mission.target);
    scalar(mission.timeLeft);
    scalar(mission.hold);
    integer(mission.reward);
    string(mission.title);
    string(mission.objective);
    mix(vehicles.size());
    for (const auto& v : vehicles) {
        integer(v.id);
        integer(static_cast<int>(v.kind));
        vector(v.position);
        vector(v.velocity);
        scalar(v.heading);
        scalar(v.health);
        scalar(v.aiTimer);
        scalar(v.fireTimer);
        scalar(v.stuckTimer);
        integer(v.color);
        integer(v.nodeFrom);
        integer(v.nodeTo);
        integer(v.driver);
        mix(v.active);
        mix(v.retired);
    }
    mix(pedestrians.size());
    for (const auto& p : pedestrians) {
        vector(p.position);
        vector(p.target);
        scalar(p.heading);
        scalar(p.panic);
        scalar(p.health);
        scalar(p.wait);
        integer(p.color);
        integer(p.corner);
        integer(p.nextCorner);
    }
    mix(bullets.size());
    for (const auto& b : bullets) {
        vector(b.position);
        vector(b.previous);
        vector(b.velocity);
        scalar(b.life);
        mix(b.hostile);
        integer(b.sourceVehicle);
    }
    mix(events.size());
    for (const auto& e : events) {
        integer(static_cast<int>(e.kind));
        vector(e.position);
        string(e.text);
        scalar(e.value);
    }
    return h;
}
} // namespace neon
