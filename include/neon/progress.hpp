#pragma once
#include <array>
#include <cstdint>

namespace neon {
constexpr int maxCash = 100000000;
constexpr int maxCompleted = 100000;
constexpr int maxEngineLevel = 3;
constexpr int maxReserve = 240;
struct Progress {
    std::uint64_t seed{2025};
    int cash{750}, completed{}, bestChain{}, engineLevel{}, armorLevel{};
    std::array<int, 3> reputation{};
};
inline bool validProgress(const Progress& p) {
    if (p.cash < 0 || p.cash > maxCash || p.completed < 0 || p.completed > maxCompleted ||
        p.bestChain < 0 || p.bestChain > p.completed || p.engineLevel < 0 ||
        p.engineLevel > maxEngineLevel || p.armorLevel < 0 || p.armorLevel > 1)
        return false;
    int total = 0;
    for (int rep : p.reputation) {
        if (rep < 0 || rep > p.completed)
            return false;
        total += rep;
    }
    return total <= p.completed;
}
} // namespace neon
