#include "neon/save.hpp"
#include "neon/simulation.hpp"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace neon;
namespace {
void check(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(float a, float b, float eps = 0.01f) {
    return std::abs(a - b) < eps;
}
void emptyCity(Simulation& s) {
    s.vehicles.clear();
    s.pedestrians.clear();
    s.events.clear();
}
void ticks(Simulation& s, int n, Input input = {}) {
    for (int i = 0; i < n; ++i)
        s.tick(input);
}
std::filesystem::path tempRoot;
using Test = std::pair<std::string, std::function<void()>>;
} // namespace
int main() {
    tempRoot = std::filesystem::temp_directory_path() /
               ("neon-borough-core-tests-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(tempRoot);
    std::vector<Test> tests{
        {"math: zero and unit normalization",
         [] {
             check(length(normalized({0, 0})) == 0, "zero normalization");
             check(near(length(normalized({3, 4})), 1), "normalization");
             check(near(wrapAngle(3 * pi), -pi) || near(wrapAngle(3 * pi), pi), "angle wrapping");
         }},
        {"math: segment intersection and circle clearance",
         [] {
             Rect r{10, 10, 20, 20};
             check(segmentRect({0, 20}, {100, 20}, r), "crossing");
             check(!segmentRect({0, 0}, {100, 0}, r), "parallel outside");
             check(segmentRect({15, 15}, {16, 16}, r), "inside");
             check(!circleRect({0, 20}, 9, r), "circle outside");
             check(circleRect({0, 20}, 11, r), "circle overlap");
         }},
        {"rng: reproducible and bounded",
         [] {
             Rng a(12), b(12);
             for (int i = 0; i < 10000; ++i)
                 check(a.next() == b.next(), "rng determinism");
             Rng c(0);
             for (int i = 0; i < 10000; ++i) {
                 float f = c.unit();
                 check(f >= 0 && f < 1, "unit range");
                 int n = c.integer(-5, 5);
                 check(n >= -5 && n <= 5, "integer range");
             }
         }},
        {"world: seeded city layout",
         [] {
             City a(5), b(5), c(6);
             check(a.buildings.size() == b.buildings.size(), "count");
             check(a.buildings.size() > 200 && a.parks.size() > 0, "city density");
             for (std::size_t i = 0; i < a.buildings.size(); ++i)
                 check(a.buildings[i].detail == b.buildings[i].detail, "seed details");
             check(a.buildings.front().detail != c.buildings.front().detail, "seed variation");
         }},
        {"world: service locations and spawn are accessible",
         [] {
             for (std::uint64_t seed : {1ULL, 42ULL, 2025ULL, 99999ULL}) {
                 City c(seed);
                 check(!c.blocked(c.spawn(), 9), "blocked spawn");
                 for (const auto& s : c.services)
                     check(!c.blocked(s.position, 9), "blocked service " + s.name);
                 for (const auto& b : c.buildings)
                     check(c.blocked(b.bounds.center(), 9), "building not solid");
             }
         }},
        {"world: all-pairs routes have shortest grid length",
         [] {
             City c;
             for (int from = 0; from < 64; ++from)
                 for (int to = 0; to < 64; ++to) {
                     auto route = c.route(c.node(from), c.node(to));
                     check(route.front() == from && route.back() == to, "route endpoints");
                     int expected = std::abs(from % 8 - to % 8) + std::abs(from / 8 - to / 8) + 1;
                     check(static_cast<int>(route.size()) == expected, "route optimality");
                     for (std::size_t i = 1; i < route.size(); ++i)
                         check(near(distance(c.node(route[i]), c.node(route[i - 1])), 384),
                               "nonneighbor path");
                 }
         }},
        {"world: swept movement cannot tunnel through buildings",
         [] {
             City c;
             const auto& b = c.buildings.front();
             Vec2 start{b.bounds.x - 20, b.bounds.y + 40};
             bool hit = false;
             Vec2 end = c.moveCircle(start, {300, 0}, 9, &hit);
             check(hit, "collision not reported");
             check(end.x < b.bounds.x - 8, "tunneled");
             check(!c.blocked(end, 9), "inside wall");
             check(!c.lineOfSight(start, {b.bounds.x + b.bounds.w + 20, start.y}), "los through wall");
         }},
        {"world: boundary and wall sliding",
         [] {
             City c;
             bool hit = false;
             Vec2 p = c.moveCircle({30, 30}, {-100, -100}, 9, &hit);
             check(hit && p.x >= 9 && p.y >= 9, "bounds escape");
             auto b = c.buildings.front();
             Vec2 start{b.bounds.x - 12, b.bounds.y + 30};
             Vec2 end = c.moveCircle(start, {50, 30}, 9);
             check(end.y > start.y + 20, "wall sliding");
         }},
        {"simulation: fixed-step determinism",
         [] {
             Simulation a(44), b(44);
             Input in;
             in.aim = {500, 64};
             for (int i = 0; i < 1800; ++i) {
                 in.movement = {i % 180 < 90 ? 1.0f : -1.0f, 0};
                 in.sprint = i % 120 < 60;
                 in.shoot = i % 90 == 0;
                 a.tick(in);
                 b.tick(in);
                 check(a.digest() == b.digest(), "divergence at tick " + std::to_string(i));
             }
         }},
        {"simulation: invalid time deltas are ignored",
         [] {
             Simulation s;
             auto d = s.digest();
             s.tick({}, 0);
             s.tick({}, -1);
             s.tick({}, 1);
             s.tick({}, NAN);
             check(s.digest() == d, "invalid dt mutated state");
         }},
        {"player: first session has reachable loft and identifiable sports car",
         [] {
             Simulation s;
             check(distance(s.player.position, s.city.services.front().position) < 86,
                   "loft is out of reach");
             bool starter = false;
             for (const auto& v : s.vehicles)
                 if (v.active && v.driver == 2 && v.kind == VehicleKind::Sport &&
                     distance(v.position, s.player.position) < 56) {
                     starter = v.color == 6 && !s.city.blocked(v.position, 19);
                 }
             check(starter, "no accessible pink starter car");
         }},
        {"player: sprint consumes stamina and regenerates",
         [] {
             Simulation s;
             emptyCity(s);
             s.player.position = {200, 64};
             Input in;
             in.movement = {1, 0};
             in.sprint = true;
             ticks(s, 90, in);
             check(s.player.stamina < 60, "sprint not consumed");
             float low = s.player.stamina;
             ticks(s, 60);
             check(s.player.stamina > low, "stamina not recovered");
         }},
        {"vehicle: enter, drive, brake, safe exit",
         [] {
             Simulation s;
             emptyCity(s);
             s.player.position = {200, 64};
             int id = s.spawnVehicle({230, 64}, VehicleKind::Sport);
             Input in;
             in.enter = true;
             s.tick(in);
             check(s.player.vehicle == id, "entry failed");
             in = {};
             in.throttle = 1;
             ticks(s, 120, in);
             float speed = length(s.vehicle(id)->velocity);
             check(speed > 200, "engine acceleration");
             in.throttle = -1;
             ticks(s, 30, in);
             check(length(s.vehicle(id)->velocity) < speed, "braking failed");
             in = {};
             in.enter = true;
             s.tick(in);
             check(s.player.vehicle == -1, "exit failed");
             check(!s.city.blocked(s.player.position, 9), "unsafe exit");
         }},
        {"vehicle: handbrake preserves measurable lateral drift",
         [] {
             Simulation a(12), b(12);
             emptyCity(a);
             emptyCity(b);
             a.city.buildings.clear();
             b.city.buildings.clear();
             int x = a.spawnVehicle({1500, 1500}, VehicleKind::Sport, 0),
                 y = b.spawnVehicle({1500, 1500}, VehicleKind::Sport, 0);
             a.player.vehicle = x;
             b.player.vehicle = y;
             a.vehicle(x)->velocity = {200, 0};
             b.vehicle(y)->velocity = {200, 0};
             Input normal;
             normal.throttle = 1;
             normal.steering = 0.45f;
             Input drift = normal;
             drift.handbrake = true;
             ticks(a, 60, normal);
             ticks(b, 60, drift);
             auto slip = [](const Vehicle& v) {
                 Vec2 f = direction(v.heading);
                 return std::abs(dot(v.velocity, Vec2{-f.y, f.x}));
             };
             check(slip(*b.vehicle(y)) > slip(*a.vehicle(x)) * 1.8f,
                   "handbrake did not increase lateral slip");
         }},
        {"vehicle: theft and police theft escalate wanted",
         [] {
             Simulation s;
             emptyCity(s);
             s.player.position = {200, 64};
             s.spawnVehicle({230, 64}, VehicleKind::Police, 1);
             Input in;
             in.enter = true;
             s.tick(in);
             check(s.wanted() >= 2, "police theft severity");
         }},
        {"vehicle: building impact damages at speed",
         [] {
             Simulation s;
             emptyCity(s);
             auto b = s.city.buildings.front();
             int id = s.spawnVehicle({b.bounds.x - 42, b.bounds.y + 40}, VehicleKind::Sport, 0);
             s.player.vehicle = id;
             s.vehicle(id)->velocity = {250, 0};
             Input in;
             in.throttle = 1;
             ticks(s, 20, in);
             check(s.vehicle(id)->health < 100, "no crash damage");
             check(!s.city.blocked(s.vehicle(id)->position, 19), "car in building");
         }},
        {"combat: bullet sweep hits a vehicle",
         [] {
             Simulation s;
             emptyCity(s);
             s.player.position = {400, 64};
             int id = s.spawnVehicle({500, 64}, VehicleKind::Sedan);
             Input in;
             in.aim = {700, 64};
             in.shoot = true;
             s.tick(in);
             ticks(s, 15);
             check(s.vehicle(id)->health < 100, "bullet missed");
             check(s.player.ammo == 15, "ammo consumption");
             check(s.wanted() > 0, "gunfire did not alert");
         }},
        {"combat: magazine reload transfers finite reserve",
         [] {
             Simulation s;
             emptyCity(s);
             s.player.ammo = 2;
             s.player.reserve = 5;
             Input in;
             in.reload = true;
             s.tick(in);
             ticks(s, 75);
             check(s.player.ammo == 7 && s.player.reserve == 0, "reload transfer");
             check(s.player.reloadTimer == 0, "reload unfinished");
         }},
        {"combat: hostile damage honors armor",
         [] {
             Simulation s;
             emptyCity(s);
             s.player.position = {400, 64};
             s.player.armor = 100;
             s.bullets.push_back({{388, 64}, {388, 64}, {600, 0}, 1, true});
             s.tick({});
             check(s.player.armor < 100 && s.player.health < 100 && s.player.health > 94, "armor absorption");
         }},
        {"police: crime is capped and units spawn",
         [] {
             Simulation s;
             s.crime(99);
             check(s.wanted() == 5 && s.heat == 5, "heat cap");
             s.tick({});
             int cops = 0;
             for (const auto& v : s.vehicles)
                 if (v.active && v.driver == 1)
                     ++cops;
             check(cops > 0, "police not dispatched");
         }},
        {"police: hidden wanted level decays",
         [] {
             Simulation s;
             emptyCity(s);
             s.heat = 0.1f;
             s.unseen = 9;
             ticks(s, 120);
             check(s.heat == 0 && s.wanted() == 0, "heat decay");
         }},
        {"missions: courier requires vehicle and three drops",
         [] {
             Simulation s;
             emptyCity(s);
             s.startMission(0);
             int money = s.player.cash;
             s.player.position = s.mission.target;
             s.tick({});
             check(s.mission.deliveries == 0, "foot courier completion");
             int id = s.spawnVehicle(s.mission.target, VehicleKind::Van, 0);
             s.player.vehicle = id;
             for (int i = 0; i < 3; ++i) {
                 s.vehicle(id)->position = s.mission.target;
                 s.vehicle(id)->velocity = {};
                 s.tick({});
             }
             check(!s.mission.active && s.progress.completed == 1, "courier completion");
             check(s.player.cash > money && s.saveRevision > 0, "reward/save");
         }},
        {"missions: acquisition requires correct vehicle and zero wanted",
         [] {
             Simulation s;
             emptyCity(s);
             s.startMission(1);
             int id = s.mission.targetVehicle;
             check(id > 0, "no acquisition target");
             s.player.vehicle = id;
             s.vehicle(id)->driver = 0;
             s.tick({});
             check(s.mission.stage == 1 && s.wanted() > 0, "tracker stage");
             s.vehicle(id)->position = s.mission.target;
             s.tick({});
             check(s.mission.active, "wanted delivery allowed");
             s.heat = 0;
             s.tick({});
             check(!s.mission.active && s.progress.completed == 1, "acquisition not completed");
         }},
        {"missions: extraction hold and escape",
         [] {
             Simulation s;
             emptyCity(s);
             s.startMission(2);
             s.player.position = s.mission.target;
             ticks(s, 185);
             check(s.mission.stage == 1 && s.heat > 2, "extraction hold");
             s.player.position = s.mission.target;
             s.tick({});
             check(s.mission.active, "extraction with wanted");
             s.heat = 0;
             s.tick({});
             check(!s.mission.active, "extraction success");
         }},
        {"missions: timeout and destroyed target fail safely",
         [] {
             Simulation s;
             emptyCity(s);
             s.startMission(0);
             s.mission.timeLeft = 0.001f;
             s.tick({});
             check(!s.mission.active, "timeout");
             s.missionCooldown = 0;
             s.startMission(1);
             s.vehicle(s.mission.targetVehicle)->health = 0;
             s.tick({});
             check(!s.mission.active, "destroyed target");
         }},
        {"missions: full vehicle pool handles unavailable targets",
         [] {
             Simulation s;
             emptyCity(s);
             for (int i = 0; i < 96; ++i)
                 check(s.spawnVehicle({64, 64}, VehicleKind::Van) > 0, "pool size");
             check(s.spawnVehicle({64, 64}, VehicleKind::Van) == -1, "pool unbounded");
             s.startMission(1);
             check(!s.mission.active, "invalid mission target");
         }},
        {"economy: garage purchases are gated and capped",
         [] {
             Simulation s;
             emptyCity(s);
             s.player.cash = 10000;
             Input in;
             in.shopChoice = 3;
             s.tick(in);
             check(s.progress.engineLevel == 0, "remote purchase");
             s.player.position = s.city.services[7].position;
             in = {};
             in.interact = true;
             s.tick(in);
             check(s.garageOpen, "garage did not open");
             in = {};
             in.shopChoice = 3;
             ticks(s, 4, in);
             check(s.progress.engineLevel == 3, "engine cap");
             int before = s.player.cash;
             in.shopChoice = 1;
             s.tick(in);
             check(s.player.cash == before, "repair charged without car");
         }},
        {"player: defeat recovers and penalizes once",
         [] {
             Simulation s;
             emptyCity(s);
             s.startMission(0);
             s.player.health = 0;
             s.heat = 3;
             s.tick({});
             check(s.player.health == 100 && s.player.cash == 400, "medical recovery");
             check(s.wanted() == 0 && !s.mission.active, "recovery reset");
             ticks(s, 2);
             check(s.player.cash == 400, "duplicate medical penalty");
         }},
        {"save: round trip and backup rotation",
         [] {
             auto path = tempRoot / "roundtrip.sav";
             Progress p;
             p.cash = 12345;
             p.completed = 4;
             p.bestChain = 3;
             p.engineLevel = 2;
             p.armorLevel = 1;
             p.reputation = {1, 2, 1};
             check(saveProgress(path, p).ok, "save failed");
             std::string error;
             auto loaded = loadProgress(path, &error);
             check(loaded.has_value(), "load failed: " + error);
             check(loaded->cash == p.cash && loaded->reputation == p.reputation && loaded->seed == p.seed,
                   "roundtrip mismatch");
             p.cash = 999;
             check(saveProgress(path, p).ok, "rotation failed");
             auto backup = path;
             backup += ".bak";
             check(loadProgress(backup)->cash == 12345 && loadProgress(path)->cash == 999, "backup mismatch");
         }},
        {"save: rejects corruption truncation and oversized input",
         [] {
             auto path = tempRoot / "bad.sav";
             saveProgress(path, {});
             {
                 std::fstream f(path, std::ios::binary | std::ios::in | std::ios::out);
                 f.seekp(20);
                 f.put('X');
             }
             check(!loadProgress(path), "corrupt save accepted");
             {
                 std::ofstream f(path);
                 f << "NEON_BOROUGH 1\n";
             }
             check(!loadProgress(path), "truncated save accepted");
             {
                 std::ofstream f(path);
                 f << std::string(5000, 'a');
             }
             check(!loadProgress(path), "oversized save accepted");
         }},
        {"save: validates ranges and does not overwrite valid progress",
         [] {
             auto path = tempRoot / "range.sav";
             Progress p;
             check(saveProgress(path, p).ok, "initial save");
             p.engineLevel = 4;
             check(!saveProgress(path, p).ok, "invalid upgrade saved");
             check(loadProgress(path)->engineLevel == 0, "valid save overwritten");
             p = {};
             p.cash = -5;
             check(!saveProgress(path, p).ok, "negative cash saved");
         }},
        {"progress: fresh world restores economy not transient pursuit",
         [] {
             Simulation s(88);
             Progress p;
             p.seed = 88;
             p.cash = 9000;
             p.engineLevel = 3;
             p.armorLevel = 1;
             p.completed = 2;
             p.bestChain = 2;
             p.reputation = {1, 1, 0};
             s.applyProgress(p);
             check(s.snapshot().cash == 9000 && s.player.armor == 100 && s.wanted() == 0, "restore");
         }},
        {"stress: long seeded traffic remains finite and bounded", [] {
             Simulation s(8675309);
             s.pedestrians.resize(20);
             for (int i = 0; i < 7200; ++i) {
                 Input in;
                 in.aim = {1200, 64};
                 in.movement = {i % 600 < 300 ? 1.0f : -1.0f, 0};
                 s.tick(in);
                 check(std::isfinite(s.playerPosition().x) && std::isfinite(s.playerPosition().y),
                       "nonfinite player");
                 check(s.vehicles.size() <= 96 && s.bullets.size() <= 128, "unbounded storage");
                 for (const auto& v : s.vehicles)
                     if (v.active)
                         check(std::isfinite(v.position.x) && std::isfinite(v.heading) &&
                                   length(v.velocity) < 500,
                               "nonfinite vehicle");
             }
         }}};
    int failed = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& e) {
            ++failed;
            std::cerr << "[FAIL] " << name << ": " << e.what() << '\n';
        }
    }
    std::filesystem::remove_all(tempRoot);
    std::cout << tests.size() - static_cast<std::size_t>(failed) << '/' << tests.size() << " tests passed\n";
    return failed ? 1 : 0;
}
