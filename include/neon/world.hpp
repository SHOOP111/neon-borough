#pragma once
#include "neon/math.hpp"
#include <array>
#include <string>
#include <vector>

namespace neon {
constexpr int tileSize = 32;
constexpr int blockTiles = 12;
constexpr int blockSize = blockTiles * tileSize;
constexpr int cityBlocks = 8;
constexpr int cityTiles = cityBlocks * blockTiles;
constexpr float citySize = static_cast<float>(cityTiles * tileSize);
constexpr float streetWidth = 128.0f;

enum class BuildingKind { Residential, Offices, Warehouse, Club };
struct Building {
    Rect bounds;
    BuildingKind kind{};
    int palette{}, floors{};
    std::uint32_t detail{};
};
struct Park {
    Rect bounds;
    std::vector<Vec2> trees;
};
enum class ServiceKind { Contact, Garage, Safehouse };
struct Service {
    Vec2 position;
    ServiceKind kind{};
    std::string name;
    int district{};
};
struct District {
    std::string name;
    Vec2 center;
    int color{};
};

class City {
  public:
    std::uint64_t seed;
    std::vector<Building> buildings;
    std::vector<Park> parks;
    std::vector<Service> services;
    std::array<District, 4> districts;
    explicit City(std::uint64_t seedValue = 2025);
    bool isStreet(Vec2 p) const;
    bool blocked(Vec2 p, float radius = 0) const;
    bool lineOfSight(Vec2 a, Vec2 b) const;
    Vec2 moveCircle(Vec2 p, Vec2 delta, float radius, bool* collided = nullptr) const;
    Vec2 node(int index) const;
    int nearestNode(Vec2 p) const;
    std::vector<int> neighbors(int index) const;
    std::vector<int> route(Vec2 start, Vec2 end) const;
    int districtAt(Vec2 p) const;
    Vec2 spawn() const;
};
} // namespace neon
