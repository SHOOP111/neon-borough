#include "neon/save.hpp"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>

namespace neon {
namespace {
std::uint64_t checksum(const std::string& data) {
    std::uint64_t result = 1469598103934665603ULL;
    for (char c : data) {
        result ^= static_cast<unsigned char>(c);
        result *= 1099511628211ULL;
    }
    return result;
}
bool valid(const Progress& p) {
    if (p.cash < 0 || p.cash > 100000000 || p.completed < 0 || p.completed > 100000 || p.bestChain < 0 ||
        p.bestChain > p.completed || p.engineLevel < 0 || p.engineLevel > 3 || p.armorLevel < 0 ||
        p.armorLevel > 1)
        return false;
    for (int rep : p.reputation)
        if (rep < 0 || rep > p.completed)
            return false;
    return true;
}
} // namespace
SaveResult saveProgress(const std::filesystem::path& path, const Progress& p) {
    if (!valid(p))
        return {false, "Invalid progress; save not written."};
    std::ostringstream body;
    body << "NEON_BOROUGH 1\n"
         << p.seed << ' ' << p.cash << ' ' << p.completed << ' ' << p.bestChain << ' ' << p.engineLevel << ' '
         << p.armorLevel << ' ' << p.reputation[0] << ' ' << p.reputation[1] << ' ' << p.reputation[2]
         << '\n';
    std::string data = body.str();
    std::error_code ec;
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path(), ec);
    if (ec)
        return {false, "Cannot create save directory: " + ec.message()};
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file)
            return {false, "Cannot open temporary save file."};
        file << data << checksum(data) << '\n';
        file.flush();
        if (!file)
            return {false, "Cannot flush save file."};
        file.close();
        if (file.fail())
            return {false, "Cannot close save file."};
    }
    // Keep a recoverable previous save. Windows rename cannot replace an existing file.
    auto backup = path;
    backup += ".bak";
    if (std::filesystem::exists(path, ec)) {
        std::filesystem::remove(backup, ec);
        ec.clear();
        std::filesystem::rename(path, backup, ec);
        if (ec)
            return {false, "Cannot rotate previous save: " + ec.message()};
    }
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        std::error_code recovery;
        if (std::filesystem::exists(backup, recovery))
            std::filesystem::rename(backup, path, recovery);
        return {false, "Cannot commit save: " + ec.message()};
    }
    return {true, "Progress saved."};
}
std::optional<Progress> loadProgress(const std::filesystem::path& path, std::string* error) {
    auto fail = [error](const std::string& text) -> std::optional<Progress> {
        if (error)
            *error = text;
        return std::nullopt;
    };
    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
        return fail("No save found.");
    auto size = std::filesystem::file_size(path, ec);
    if (ec || size > 4096)
        return fail("Save is unreadable or oversized.");
    std::ifstream file(path, std::ios::binary);
    std::string header, payload, hashLine, extra;
    if (!std::getline(file, header) || !std::getline(file, payload) || !std::getline(file, hashLine))
        return fail("Truncated save.");
    if (std::getline(file, extra))
        return fail("Unexpected trailing save data.");
    if (header != "NEON_BOROUGH 1")
        return fail("Unsupported save version.");
    std::uint64_t stored{};
    std::istringstream hash(hashLine);
    if (!(hash >> stored))
        return fail("Malformed save checksum.");
    hash >> std::ws;
    if (!hash.eof() || stored != checksum(header + '\n' + payload + '\n'))
        return fail("Save checksum mismatch.");
    Progress p;
    std::istringstream in(payload);
    if (!(in >> p.seed >> p.cash >> p.completed >> p.bestChain >> p.engineLevel >> p.armorLevel >>
          p.reputation[0] >> p.reputation[1] >> p.reputation[2]))
        return fail("Malformed progress.");
    in >> std::ws;
    if (!in.eof() || !valid(p))
        return fail("Out-of-range save data.");
    if (error)
        error->clear();
    return p;
}
std::filesystem::path defaultSavePath() {
#ifdef _WIN32
    if (const char* appdata = std::getenv("LOCALAPPDATA"))
        return std::filesystem::path(appdata) / "NeonBorough" / "progress.sav";
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME"))
        return std::filesystem::path(home) / "Library" / "Application Support" / "NeonBorough" /
               "progress.sav";
#else
    if (const char* xdg = std::getenv("XDG_DATA_HOME"))
        return std::filesystem::path(xdg) / "neon-borough" / "progress.sav";
    if (const char* home = std::getenv("HOME"))
        return std::filesystem::path(home) / ".local" / "share" / "neon-borough" / "progress.sav";
#endif
    return std::filesystem::path("neon-borough-progress.sav");
}
} // namespace neon
