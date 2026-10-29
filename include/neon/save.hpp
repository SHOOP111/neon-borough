#pragma once
#include "neon/progress.hpp"
#include <filesystem>
#include <optional>

namespace neon {
struct SaveResult {
    bool ok{};
    std::string message;
};
SaveResult saveProgress(const std::filesystem::path& path, const Progress& progress);
std::optional<Progress> loadProgress(const std::filesystem::path& path, std::string* error = nullptr);
// Also recovers an interrupted rotation where only the backup remains.
std::optional<Progress> recoverProgress(const std::filesystem::path& path, std::string* error = nullptr);
std::filesystem::path defaultSavePath();
} // namespace neon
