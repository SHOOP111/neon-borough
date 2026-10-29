#include "neon/save.hpp"
#include "neon/simulation.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

using namespace neon;
namespace {
void check(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
void clean(Simulation& s) {
    s.vehicles.clear();
    s.pedestrians.clear();
    s.events.clear();
    s.player.position = {400, 64};
}
void ticks(Simulation& s, int count, Input in = {}) {
    for (int i = 0; i < count; ++i)
        s.tick(in);
}
void rawSave(const std::filesystem::path& path, const std::string& payload,
             const std::string& header = "NEON_BOROUGH 1") {
    std::string body = header + '\n' + payload + '\n';
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned char c : body) {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    std::ofstream file(path, std::ios::binary);
    file << body << hash << '\n';
}
struct TemporaryDirectory {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("neon-borough-review-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    TemporaryDirectory() { std::filesystem::create_directories(path); }
    ~TemporaryDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};
} // namespace
int main() {
    TemporaryDirectory temporary;
    const auto& root = temporary.path;
    using Test = std::pair<std::string, std::function<void()>>;
    std::vector<Test> tests{
        {"math: earliest intersection, tangent and degenerate segments",
         [] {
             check(std::abs(segmentCircleTime({0, 0}, {100, 0}, {50, 0}, 10) - 0.4f) < 0.001f, "entry time");
             check(std::isfinite(segmentCircleTime({0, 10}, {100, 10}, {50, 0}, 10)), "tangent missed");
             check(segmentCircleTime({50, 0}, {50, 0}, {50, 0}, 10) == 0, "inside point");
             check(!std::isfinite(segmentCircleTime({0, 0}, {0, 0}, {50, 0}, 10)), "outside point");
             check(segmentRectTime({0, 0}, {100, 0}, {60, -10, 20, 20}) == 0.6f, "rectangle time");
         }},
        {"world: rejects nonfinite and excessive movement safely",
         [] {
             City city;
             float nan = std::numeric_limits<float>::quiet_NaN();
             check(city.blocked({nan, 64}, 9) && !city.isStreet({64, nan}), "nonfinite queries");
             check(city.nearestNode({nan, 0}) == 0, "unsafe node conversion");
             check(city.neighbors(-1).empty() && city.neighbors(64).empty(), "invalid graph index");
             Vec2 start{64, 64};
             Vec2 end = city.moveCircle(start, {1e30f, 0}, 9);
             check(end.x == start.x && end.y == start.y, "unbounded movement");
             check(!city.lineOfSight({64, 64}, {-1, 64}), "out-of-bounds visibility");
         }},
        {"simulation: nonfinite inputs leave all state unchanged",
         [] {
             Simulation s;
             auto digest = s.digest();
             Input in;
             in.throttle = std::numeric_limits<float>::infinity();
             s.tick(in);
             in = {};
             in.steering = std::numeric_limits<float>::quiet_NaN();
             s.tick(in);
             in = {};
             in.aim.x = std::numeric_limits<float>::infinity();
             s.tick(in);
             in = {};
             in.movement.y = std::numeric_limits<float>::quiet_NaN();
             s.tick(in);
             check(s.digest() == digest, "invalid input mutated state");
             s.crime(std::numeric_limits<float>::quiet_NaN());
             s.crime(-1);
             check(s.digest() == digest, "invalid crime mutated state");
         }},
        {"simulation: analog controls are clamped",
         [] {
             Simulation a, b;
             clean(a);
             clean(b);
             a.city.buildings.clear();
             b.city.buildings.clear();
             int x = a.spawnVehicle({1500, 1500}, VehicleKind::Sport, 0);
             int y = b.spawnVehicle({1500, 1500}, VehicleKind::Sport, 0);
             a.player.vehicle = x;
             b.player.vehicle = y;
             Input full;
             full.throttle = full.steering = 1;
             Input excessive = full;
             excessive.throttle = excessive.steering = 1000;
             ticks(a, 120, full);
             ticks(b, 120, excessive);
             check(a.digest() == b.digest(), "out-of-range controller strength");
         }},
        {"digest: covers mission, ammunition, velocity and controller state",
         [] {
             Simulation a(9), b(9);
             check(a.digest() == b.digest(), "initial equality");
             b.player.ammo--;
             check(a.digest() != b.digest(), "ammo omitted");
             b.player.ammo++;
             b.mission.timeLeft = 1;
             check(a.digest() != b.digest(), "mission omitted");
             b.mission.timeLeft = 0;
             b.vehicles.front().velocity.x += 1;
             check(a.digest() != b.digest(), "velocity omitted");
             b.vehicles.front().velocity.x -= 1;
             b.vehicles.front().driver = 2;
             check(a.digest() != b.digest(), "controller omitted");
         }},
        {"combat: nearer car wins regardless of storage order",
         [] {
             Simulation s;
             clean(s);
             int far = s.spawnVehicle({510, 64}, VehicleKind::Sedan);
             int near = s.spawnVehicle({450, 64}, VehicleKind::Sedan);
             s.bullets.push_back({{400, 64}, {400, 64}, {1800, 0}, 1, false});
             s.tick({}, 0.1f);
             check(s.vehicle(near)->health == 82 && s.vehicle(far)->health == 100, "wrong first target");
             check(s.bullets.empty(), "hit bullet not retired");
         }},
        {"combat: pedestrian before car receives first impact",
         [] {
             Simulation s;
             clean(s);
             int id = s.spawnVehicle({510, 64}, VehicleKind::Sedan);
             Pedestrian p;
             p.position = p.target = {450, 64};
             s.pedestrians.push_back(p);
             s.bullets.push_back({{400, 64}, {400, 64}, {1800, 0}, 1, false});
             s.tick({}, 0.1f);
             check(s.pedestrians.front().health == 45 && s.vehicle(id)->health == 100,
                   "entity-type priority");
         }},
        {"combat: target before wall is hit, wall protects target behind it",
         [] {
             Simulation s;
             clean(s);
             s.city.buildings.clear();
             s.city.buildings.push_back({{520, 30, 30, 68}});
             int near = s.spawnVehicle({470, 64}, VehicleKind::Sedan);
             int far = s.spawnVehicle({590, 64}, VehicleKind::Sedan);
             s.bullets.push_back({{400, 64}, {400, 64}, {2000, 0}, 1, false});
             s.tick({}, 0.1f);
             check(s.vehicle(near)->health == 82, "wall hid earlier impact");
             s.bullets.push_back({{500, 64}, {500, 64}, {2000, 0}, 1, false});
             s.tick({}, 0.1f);
             check(s.vehicle(far)->health == 100, "shot crossed cover");
             auto impact = std::find_if(s.events.begin(), s.events.end(),
                                        [](const Event& e) { return e.kind == EventKind::Impact; });
             check(impact != s.events.end() && std::abs(impact->position.x - 520) < 0.01f, "impact location");
         }},
        {"combat: traffic provides cover against hostile shots",
         [] {
             Simulation s;
             clean(s);
             s.player.position = {550, 64};
             int cover = s.spawnVehicle({470, 64}, VehicleKind::Van);
             s.bullets.push_back({{400, 64}, {400, 64}, {1800, 0}, 1, true});
             s.tick({}, 0.1f);
             check(s.player.health == 100 && s.vehicle(cover)->health == 96, "hostile shot ignored cover");
         }},
        {"combat: source vehicle is not hit by its own bullet",
         [] {
             Simulation s;
             clean(s);
             s.player.position = {600, 64};
             int shooter = s.spawnVehicle({400, 64}, VehicleKind::Police, 2);
             s.bullets.push_back({{400, 64}, {400, 64}, {1800, 0}, 1, true, shooter});
             s.tick({}, 0.1f);
             check(s.vehicle(shooter)->health == 100, "self damage");
         }},
        {"combat: invulnerable player still blocks a bullet",
         [] {
             Simulation s;
             clean(s);
             s.player.invulnerable = 1;
             s.bullets.push_back({{388, 64}, {388, 64}, {600, 0}, 1, true});
             s.tick({});
             check(s.player.health == 100 && s.bullets.empty(), "bullet passed through recovery shield");
         }},
        {"vehicle: gunfire wrecks occupied vehicle in the same tick",
         [] {
             Simulation s;
             clean(s);
             int id = s.spawnVehicle({450, 64}, VehicleKind::Sport, 0);
             s.player.vehicle = id;
             s.vehicle(id)->health = 3;
             s.bullets.push_back({{420, 64}, {420, 64}, {600, 0}, 1, true});
             s.tick({});
             check(s.player.vehicle == -1 && s.vehicle(id)->health <= -999, "delayed wreck");
             check(!s.city.blocked(s.player.position, 9), "unsafe ejection");
             check(std::count_if(s.events.begin(), s.events.end(),
                                 [](const Event& e) { return e.kind == EventKind::Explosion; }) == 1,
                   "duplicate explosion");
         }},
        {"missions: death cannot collect completion reward",
         [] {
             Simulation s;
             clean(s);
             s.startMission(0);
             int id = s.spawnVehicle(s.mission.target, VehicleKind::Van, 0);
             s.player.vehicle = id;
             s.mission.deliveries = 2;
             s.player.health = 0;
             s.tick({});
             check(s.progress.completed == 0 && s.player.cash == 400 && !s.mission.active,
                   "posthumous reward");
         }},
        {"missions: negative contact index is rejected",
         [] {
             Simulation s;
             clean(s);
             s.startMission(-1);
             check(!s.mission.active, "invalid mission enum");
             s.startMission(6);
             check(!s.mission.active, "invalid contact");
         }},
        {"missions: capped progression stays serializable after repeated rewards",
         [&] {
             Simulation s;
             clean(s);
             Progress p;
             p.cash = maxCash - 10;
             p.completed = maxCompleted;
             p.bestChain = maxCompleted;
             p.reputation = {maxCompleted, 0, 0};
             check(s.applyProgress(p), "valid cap restore");
             s.chain = maxCompleted;
             for (int i = 0; i < 10; ++i) {
                 s.missionCooldown = 0;
                 s.startMission(i % 3);
                 s.finishMission();
                 check(validProgress(s.snapshot()), "unsavable endgame");
             }
             check(s.player.cash == maxCash && s.progress.completed == maxCompleted, "cap overflow");
             check(saveProgress(root / "endgame.sav", s.snapshot()).ok, "cap save");
         }},
        {"missions: abandoned target is recycled only while distant and unoccupied",
         [] {
             Simulation s;
             clean(s);
             s.startMission(1);
             int id = s.mission.targetVehicle;
             s.failMission("test");
             check(s.vehicle(id)->retired, "orphan target not marked");
             s.vehicle(id)->position = {2500, 2368};
             ticks(s, 481);
             check(!s.vehicle(id), "distant target leaked");
         }},
        {"population: destroyed distant traffic is replenished",
         [] {
             Simulation s;
             clean(s);
             int id = s.spawnVehicle({2500, 2368}, VehicleKind::Sedan, -1);
             s.vehicle(id)->health = 0;
             ticks(s, 481);
             check(!s.vehicle(id), "wreck not recycled");
             check(std::count_if(
                       s.vehicles.begin(), s.vehicles.end(),
                       [](const Vehicle& v) { return v.active && v.health > 0 && v.driver == -1; }) > 0,
                   "traffic permanently depleted");
             check(s.vehicles.size() <= 96, "unbounded replenishment");
         }},
        {"police: nearby cleared patrol returns to traffic instead of chasing",
         [] {
             Simulation s;
             clean(s);
             int id = s.spawnVehicle({500, 64}, VehicleKind::Police, 1);
             s.tick({});
             check(s.vehicle(id)->driver == -1, "patrol kept pursuing after heat cleared");
             check(s.player.health == 100 && s.wanted() == 0, "unprovoked hostility");
         }},
        {"economy: stale garage flag does not permit remote purchase",
         [] {
             Simulation s;
             clean(s);
             s.garageOpen = true;
             Input in;
             in.shopChoice = 3;
             s.tick(in);
             check(s.progress.engineLevel == 0 && s.player.cash == 750 && !s.garageOpen,
                   "stale shop exploit");
         }},
        {"economy: full supplies and wrecks never charge for a no-op",
         [] {
             Simulation s;
             clean(s);
             s.player.position = s.city.services[7].position;
             s.garageOpen = true;
             s.player.armor = 100;
             s.player.reserve = maxReserve;
             Input in;
             in.shopChoice = 2;
             s.tick(in);
             in.shopChoice = 4;
             s.tick(in);
             check(s.player.cash == 750, "charged for full supply");
             int id = s.spawnVehicle(s.city.services[7].position + Vec2{0, -30}, VehicleKind::Van, 0);
             s.player.vehicle = id;
             in.shopChoice = 1;
             s.tick(in);
             check(s.player.cash == 750, "charged for full vehicle");
             s.vehicle(id)->health = 0;
             s.tick(in);
             check(s.player.cash == 750 && s.player.vehicle == -1, "wreck repaired / charged");
         }},
        {"recovery: reload and stamina are reset",
         [] {
             Simulation s;
             clean(s);
             s.player.health = 0;
             s.player.reloadTimer = 1;
             s.player.shotTimer = 1;
             s.player.stamina = 0;
             s.tick({});
             check(s.player.reloadTimer == 0 && s.player.shotTimer == 0 && s.player.stamina == 100,
                   "stale recovery state");
         }},
        {"progress: invalid and wrong-seed restore is atomic",
         [] {
             Simulation s;
             auto digest = s.digest();
             Progress p;
             p.cash = -1;
             check(!s.applyProgress(p) && s.digest() == digest, "invalid progress applied");
             p = {};
             p.seed = 123;
             check(!s.applyProgress(p) && s.digest() == digest, "mismatched city seed");
             p = {};
             p.completed = 1;
             p.reputation = {1, 1, 1};
             check(!validProgress(p), "impossible reputation total");
         }},
        {"save: missing primary recovers interrupted backup rotation",
         [&] {
             auto path = root / "missing.sav";
             auto backup = path;
             backup += ".bak";
             Progress p;
             p.cash = 1234;
             check(saveProgress(backup, p).ok, "backup fixture");
             std::string error;
             auto recovered = recoverProgress(path, &error);
             check(recovered && recovered->cash == 1234 && error.find("Recovered") != std::string::npos,
                   "missing primary recovery");
         }},
        {"save: damaged primary does not replace good backup",
         [&] {
             auto path = root / "preserved.sav";
             Progress p;
             p.cash = 1000;
             check(saveProgress(path, p).ok, "first fixture");
             p.cash = 2000;
             check(saveProgress(path, p).ok, "second fixture");
             {
                 std::ofstream file(path);
                 file << "damaged";
             }
             auto recovered = recoverProgress(path);
             check(recovered && recovered->cash == 1000, "damaged primary recovery");
             p.cash = 3000;
             check(saveProgress(path, p).ok, "recovery save");
             auto backup = path;
             backup += ".bak";
             auto previous = loadProgress(backup);
             check(previous && previous->cash == 1000, "good backup overwritten by corruption");
             check(loadProgress(path)->cash == 3000, "new save missing");
         }},
        {"save: strict numbers and semantic checks despite a valid checksum",
         [&] {
             auto path = root / "strict.sav";
             for (const std::string payload :
                  {"-1 750 0 0 0 0 0 0 0", "2025 750 1 1 0 0 1 1 0", "2025 750 0 0 0 0 0 0 0 junk",
                   "18446744073709551616 750 0 0 0 0 0 0 0", "2025 +750 0 0 0 0 0 0 0",
                   "2025 750 0 0 0 0 0 0", "2025 750 0 1 0 0 0 0 0"}) {
                 rawSave(path, payload);
                 check(!loadProgress(path), "accepted malformed payload: " + payload);
             }
             rawSave(path, "2025 750 0 0 0 0 0 0 0", "NEON_BOROUGH 2");
             check(!loadProgress(path), "future format accepted");
             rawSave(path, "2025 750 0 0 0 0 0 0 0");
             {
                 std::ofstream file(path, std::ios::app);
                 file << '\n';
             }
             check(!loadProgress(path), "extra line accepted");
         }},
        {"save: filesystem failures are reported without destroying targets",
         [&] {
             auto path = root / "directory.sav";
             std::filesystem::create_directory(path);
             check(!saveProgress(path, {}).ok && std::filesystem::is_directory(path),
                   "directory overwritten");
             auto backup = root / "blocked.sav.bak";
             std::filesystem::create_directory(backup);
             check(!saveProgress(root / "blocked.sav", {}).ok, "non-file backup accepted");
             check(!loadProgress(path), "directory loaded");
             check(!saveProgress({}, {}).ok, "empty path accepted");
             check(!std::filesystem::exists(root / "blocked.sav.tmp"), "temporary file leaked");
         }},
        {"save: invalid reputation never overwrites valid primary",
         [&] {
             auto path = root / "reputation.sav";
             check(saveProgress(path, {}).ok, "fixture");
             Progress p;
             p.completed = 1;
             p.reputation = {1, 1, 0};
             check(!saveProgress(path, p).ok && loadProgress(path)->completed == 0,
                   "invalid aggregate saved");
         }},
        {"stress: multiple seeds under driving, pursuit and gunfire", [] {
             for (std::uint64_t seed : {0ULL, 1ULL, 42ULL, 2025ULL, 99999ULL}) {
                 Simulation s(seed);
                 s.pedestrians.resize(30);
                 Input enter;
                 enter.enter = true;
                 s.tick(enter);
                 s.crime(2);
                 for (int i = 0; i < 1800; ++i) {
                     Input in;
                     in.throttle = 1;
                     in.steering = i % 360 < 180 ? 0.4f : -0.4f;
                     in.handbrake = i % 240 < 40;
                     in.shoot = i % 80 == 0;
                     in.aim = s.playerPosition() + Vec2{200, -20};
                     s.tick(in);
                     check(finite(s.playerPosition()) &&
                               !s.city.blocked(s.playerPosition(), s.player.vehicle == -1 ? 9.0f : 19.0f),
                           "invalid player position");
                     check(validProgress(s.snapshot()) && s.vehicles.size() <= 96 && s.bullets.size() <= 128,
                           "state invariants");
                     for (const auto& car : s.vehicles)
                         if (car.active)
                             check(finite(car.position) && finite(car.velocity) && std::isfinite(car.heading),
                                   "invalid car state");
                 }
             }
         }}};
    int failed = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& error) {
            ++failed;
            std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
        }
    }
    std::cout << tests.size() - static_cast<std::size_t>(failed) << '/' << tests.size()
              << " review tests passed\n";
    return failed ? 1 : 0;
}
