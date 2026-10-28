#pragma once
#include "neon/simulation.hpp"
#include <filesystem>
#include <optional>

namespace neon {
struct SaveResult { bool ok{}; std::string message; };
SaveResult saveProgress(const std::filesystem::path& path,const Progress& progress);
std::optional<Progress> loadProgress(const std::filesystem::path& path,std::string* error=nullptr);
std::filesystem::path defaultSavePath();
}
