#include "neon/save.hpp"
#include <array>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <locale>
#include <sstream>
#include <system_error>

namespace neon {
namespace {
std::uint64_t checksum(const std::string& data) {
    // Preserve the v1 checksum algorithm for existing saves.
    std::uint64_t result = 1469598103934665603ULL;
    for (char c : data) {
        result ^= static_cast<unsigned char>(c);
        result *= 1099511628211ULL;
    }
    return result;
}
template <class T> bool readNumber(std::istream& in, T& value) {
    std::string token;
    if (!(in >> token))
        return false;
    auto [end, ec] = std::from_chars(token.data(), token.data() + token.size(), value);
    return ec == std::errc{} && end == token.data() + token.size();
}
bool unsafeTarget(const std::filesystem::path& path, std::error_code& ec) {
    auto status = std::filesystem::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory) {
        ec.clear();
        return false;
    }
    return ec || (std::filesystem::exists(status) && !std::filesystem::is_regular_file(status));
}
struct TemporarySave {
    std::filesystem::path path;
    ~TemporarySave() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
};
} // namespace
SaveResult saveProgress(const std::filesystem::path& path, const Progress& p) {
    if (!validProgress(p))
        return {false, "Invalid progress; save not written."};
    if (path.empty() || path.filename().empty())
        return {false, "Invalid save path."};
    std::error_code ec;
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path(), ec);
    if (ec)
        return {false, "Cannot create save directory: " + ec.message()};
    auto temporary = path;
    temporary += ".tmp";
    auto backup = path;
    backup += ".bak";
    for (const auto& target : {path, temporary, backup})
        if (unsafeTarget(target, ec))
            return {false, "Save target is inaccessible or is not a regular file."};
    TemporarySave cleanup{temporary};
    std::ostringstream body;
    body.imbue(std::locale::classic());
    body << "NEON_BOROUGH 1\n"
         << p.seed << ' ' << p.cash << ' ' << p.completed << ' ' << p.bestChain << ' ' << p.engineLevel << ' '
         << p.armorLevel << ' ' << p.reputation[0] << ' ' << p.reputation[1] << ' ' << p.reputation[2] << '\n';
    std::string data = body.str();
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file)
            return {false, "Cannot open temporary save file."};
        file.imbue(std::locale::classic());
        file << data << checksum(data) << '\n';
        file.flush();
        if (!file)
            return {false, "Cannot flush save file."};
        file.close();
        if (file.fail())
            return {false, "Cannot close save file."};
    }
    bool rotated = false;
    if (std::filesystem::exists(path, ec)) {
        // A damaged primary must never replace the last known-good backup.
        if (loadProgress(path)) {
            std::filesystem::remove(backup, ec);
            if (ec)
                return {false, "Cannot remove previous backup: " + ec.message()};
            std::filesystem::rename(path, backup, ec);
            if (ec)
                return {false, "Cannot rotate previous save: " + ec.message()};
            rotated = true;
        } else {
            std::filesystem::remove(path, ec);
            if (ec)
                return {false, "Cannot replace damaged save: " + ec.message()};
        }
    }
    if (ec)
        return {false, "Cannot inspect previous save: " + ec.message()};
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        if (rotated) {
            std::error_code recovery;
            std::filesystem::rename(backup, path, recovery);
        }
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
    bool exists = std::filesystem::exists(path, ec);
    if (ec)
        return fail("Save is inaccessible: " + ec.message());
    if (!exists)
        return fail("No save found.");
    if (!std::filesystem::is_regular_file(path, ec) || ec)
        return fail("Save is not a readable regular file.");
    // Bound the actual read, not just a racy file_size query.
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return fail("Cannot open save.");
    std::array<char, 4097> buffer{};
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    if (file.bad() || file.gcount() > 4096)
        return fail("Save is unreadable or oversized.");
    std::istringstream lines(std::string(buffer.data(), static_cast<std::size_t>(file.gcount())));
    std::string header, payload, hashLine, extra;
    if (!std::getline(lines, header) || !std::getline(lines, payload) || !std::getline(lines, hashLine))
        return fail("Truncated save.");
    if (std::getline(lines, extra))
        return fail("Unexpected trailing save data.");
    if (header != "NEON_BOROUGH 1")
        return fail("Unsupported save version.");
    std::uint64_t stored{};
    std::istringstream hash(hashLine);
    hash.imbue(std::locale::classic());
    if (!readNumber(hash, stored))
        return fail("Malformed save checksum.");
    hash >> std::ws;
    if (!hash.eof() || stored != checksum(header + '\n' + payload + '\n'))
        return fail("Save checksum mismatch.");
    Progress p;
    std::istringstream in(payload);
    in.imbue(std::locale::classic());
    if (!readNumber(in, p.seed) || !readNumber(in, p.cash) || !readNumber(in, p.completed) ||
        !readNumber(in, p.bestChain) || !readNumber(in, p.engineLevel) || !readNumber(in, p.armorLevel) ||
        !readNumber(in, p.reputation[0]) || !readNumber(in, p.reputation[1]) || !readNumber(in, p.reputation[2]))
        return fail("Malformed progress.");
    in >> std::ws;
    if (!in.eof() || !validProgress(p))
        return fail("Out-of-range save data.");
    if (error)
        error->clear();
    return p;
}
std::optional<Progress> recoverProgress(const std::filesystem::path& path, std::string* error) {
    std::string primaryError;
    if (auto p = loadProgress(path, &primaryError)) {
        if (error) error->clear();
        return p;
    }
    auto backup = path;
    backup += ".bak";
    if (auto p = loadProgress(backup)) {
        if (error) *error = "Recovered the previous save backup.";
        return p;
    }
    if (error) *error = primaryError;
    return std::nullopt;
}
std::filesystem::path defaultSavePath() {
    auto environmentPath = [](const char* key) {
        const char* value = std::getenv(key);
        auto path = value && *value ? std::filesystem::path(value) : std::filesystem::path{};
        return path.is_absolute() ? path : std::filesystem::path{};
    };
#ifdef _WIN32
    auto root = environmentPath("LOCALAPPDATA");
    if (!root.empty()) return root / "NeonBorough" / "progress.sav";
#elif defined(__APPLE__)
    auto root = environmentPath("HOME");
    if (!root.empty()) return root / "Library" / "Application Support" / "NeonBorough" / "progress.sav";
#else
    auto root = environmentPath("XDG_DATA_HOME");
    if (!root.empty()) return root / "neon-borough" / "progress.sav";
    root = environmentPath("HOME");
    if (!root.empty()) return root / ".local" / "share" / "neon-borough" / "progress.sav";
#endif
    return std::filesystem::path("neon-borough-progress.sav");
}
} // namespace neon
