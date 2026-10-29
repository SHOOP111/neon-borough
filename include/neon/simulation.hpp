#pragma once
#include "neon/progress.hpp"
#include "neon/world.hpp"
#include <string>
#include <vector>

namespace neon {
constexpr float fixedStep = 1.0f / 60.0f;
enum class VehicleKind { Sedan, Sport, Van, Police };
struct Vehicle {
    int id{};
    VehicleKind kind{};
    Vec2 position{}, velocity{};
    float heading{}, health{100}, aiTimer{}, fireTimer{}, stuckTimer{};
    int color{}, nodeFrom{}, nodeTo{}, driver{-1}; // -1 traffic, 0 player, 1 police, 2 parked
    bool active{true}, retired{};
    float topSpeed() const;
};
struct Pedestrian {
    Vec2 position{}, target{};
    float heading{}, panic{}, health{100}, wait{};
    int color{}, corner{}, nextCorner{};
};
struct Bullet {
    Vec2 position{}, previous{}, velocity{};
    float life{1.2f};
    bool hostile{};
    int sourceVehicle{-1};
};
enum class EventKind { Notice, Shot, Impact, Explosion, Reward, Siren, Skid };
struct Event {
    EventKind kind{};
    Vec2 position{};
    std::string text;
    float value{};
};
struct Player {
    Vec2 position{}, aim{1, 0};
    float health{100}, armor{}, stamina{100}, shotTimer{}, reloadTimer{}, invulnerable{};
    int vehicle{-1}, cash{750}, ammo{16}, reserve{80};
};
enum class MissionKind { Courier, Acquisition, Extraction };
struct Mission {
    bool active{};
    MissionKind kind{};
    int stage{}, targetVehicle{-1}, deliveries{};
    Vec2 target{};
    float timeLeft{}, hold{};
    int reward{};
    std::string title, objective;
};
struct Input {
    Vec2 movement{}, aim{};
    float throttle{}, steering{};
    bool shoot{}, sprint{}, handbrake{}, enter{}, interact{}, reload{}, honk{};
    int shopChoice{};
};

class Simulation {
    Rng rng_;
    int nextVehicleId_{1};
    float policeTimer_{}, populationTimer_{}, honkTimer_{};
    void updatePlayer(const Input& in, float dt);
    void updateVehicles(const Input& in, float dt);
    void updatePedestrians(float dt);
    void updateBullets(float dt);
    void updatePolice(float dt);
    void updateMission(float dt);
    void interact();
    void purchase(int choice);
    void defeat();
    void drive(Vehicle& car, float throttle, float steer, bool handbrake, float dt);
    bool nearGarage() const;
    void wreckVehicles();
    void retireMissionTarget();
    void addBullet(Vec2 position, Vec2 direction, bool hostile, int sourceVehicle = -1);
    Vec2 deliveryPoint();
    void notice(std::string text);

  public:
    City city;
    Player player;
    Progress progress;
    Mission mission;
    std::vector<Vehicle> vehicles;
    std::vector<Pedestrian> pedestrians;
    std::vector<Bullet> bullets;
    std::vector<Event> events;
    float time{}, heat{}, unseen{}, trauma{}, missionCooldown{};
    int chain{}, saveRevision{};
    bool garageOpen{};
    explicit Simulation(std::uint64_t seed = 2025);
    void tick(const Input& input, float dt = fixedStep);
    bool applyProgress(const Progress& value);
    Progress snapshot() const;
    Vehicle* vehicle(int id);
    const Vehicle* vehicle(int id) const;
    Vec2 playerPosition() const;
    int wanted() const;
    void crime(float severity);
    int spawnVehicle(Vec2 position, VehicleKind kind, int driver = 2);
    void startMission(int contactIndex);
    void finishMission();
    void failMission(const std::string& reason);
    std::uint64_t digest() const;
};
} // namespace neon
