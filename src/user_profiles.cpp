#include "user_profiles.hpp"

#define WIN32_LEAN_AND_MEAN
#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>
#include <random>
#include <stdexcept>

namespace nba97 {
namespace {
constexpr std::array<std::uint8_t, 8> kMagic{'N','9','7','P','R','O','F',0};
constexpr std::uint16_t kMajorVersion = 1;
constexpr std::uint16_t kMinorVersion = 0;
constexpr std::uint32_t kHeaderSize = 40;
constexpr std::uint32_t kDirectoryEntrySize = 16;
constexpr std::uint32_t kProfileRecordSize = 48;
constexpr std::uint32_t kStatsRecordSize = 8 + 16 * 4;
constexpr std::size_t kNoIndex = (std::numeric_limits<std::size_t>::max)();

void appendU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
}

void appendU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<std::uint8_t>(value >> shift));
}

void appendU64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8)
        out.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::uint16_t readU16(const std::vector<std::uint8_t>& in, std::size_t at) {
    if (at + 2 > in.size()) throw std::runtime_error("truncated u16");
    return static_cast<std::uint16_t>(in[at] | (in[at + 1] << 8));
}

std::uint32_t readU32(const std::vector<std::uint8_t>& in, std::size_t at) {
    if (at + 4 > in.size()) throw std::runtime_error("truncated u32");
    std::uint32_t value = 0;
    for (int shift = 0; shift < 32; shift += 8) value |= std::uint32_t(in[at++]) << shift;
    return value;
}

std::uint64_t readU64(const std::vector<std::uint8_t>& in, std::size_t at) {
    if (at + 8 > in.size()) throw std::runtime_error("truncated u64");
    std::uint64_t value = 0;
    for (int shift = 0; shift < 64; shift += 8) value |= std::uint64_t(in[at++]) << shift;
    return value;
}

void patchU32(std::vector<std::uint8_t>& out, std::size_t at, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) out[at++] = static_cast<std::uint8_t>(value >> shift);
}

std::uint32_t crc32(const std::vector<std::uint8_t>& bytes) {
    std::uint32_t value = 0xffffffffu;
    for (std::uint8_t byte : bytes) {
        value ^= byte;
        for (int bit = 0; bit < 8; ++bit)
            value = (value >> 1) ^ (0xedb88320u & (0u - (value & 1u)));
    }
    return ~value;
}

std::uint64_t unixNow() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

std::uint64_t newId(const std::vector<UserProfile>& existing) {
    std::random_device source;
    std::mt19937_64 generator((std::uint64_t(source()) << 32) ^ source() ^ unixNow());
    for (;;) {
        const std::uint64_t candidate = generator();
        if (candidate != 0 && std::none_of(existing.begin(), existing.end(),
            [candidate](const UserProfile& profile) { return profile.id == candidate; }))
            return candidate;
    }
}

void appendStats(std::vector<std::uint8_t>& out, const UserCareerStats& stats) {
    for (const std::uint32_t value : std::array<std::uint32_t, 16>{
        stats.games, stats.wins, stats.losses, stats.points,
        stats.field_goals_made, stats.field_goals_attempted,
        stats.three_pointers_made, stats.three_pointers_attempted,
        stats.free_throws_made, stats.free_throws_attempted,
        stats.rebounds, stats.assists, stats.steals, stats.blocks,
        stats.turnovers, stats.fouls}) appendU32(out, value);
}

UserCareerStats readStats(const std::vector<std::uint8_t>& in, std::size_t at) {
    std::array<std::uint32_t, 16> values{};
    for (auto& value : values) { value = readU32(in, at); at += 4; }
    UserCareerStats result;
    result.games = values[0]; result.wins = values[1]; result.losses = values[2]; result.points = values[3];
    result.field_goals_made = values[4]; result.field_goals_attempted = values[5];
    result.three_pointers_made = values[6]; result.three_pointers_attempted = values[7];
    result.free_throws_made = values[8]; result.free_throws_attempted = values[9];
    result.rebounds = values[10]; result.assists = values[11]; result.steals = values[12];
    result.blocks = values[13]; result.turnovers = values[14]; result.fouls = values[15];
    return result;
}
} // namespace

bool UserProfileStore::isAllowedNameCharacter(char value) noexcept {
    const unsigned char byte = static_cast<unsigned char>(value);
    return (byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9') ||
           byte == ' ' || byte == '-' || byte == '.';
}

std::string UserProfileStore::normalizeName(std::string_view name) {
    std::string result;
    result.reserve((std::min)(name.size(), kMaximumUserNameLength));
    bool previous_space = true;
    for (char value : name) {
        const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(value)));
        if (!isAllowedNameCharacter(upper)) continue;
        if (upper == ' ' && previous_space) continue;
        if (result.size() == kMaximumUserNameLength) break;
        result.push_back(upper);
        previous_space = upper == ' ';
    }
    while (!result.empty() && result.back() == ' ') result.pop_back();
    return result;
}

ProfileLoadStatus UserProfileStore::load(const std::filesystem::path& path) {
    path_ = path;
    profiles_.clear();
    generation_ = 0;
    last_error_.clear();
    if (!std::filesystem::exists(path_)) return ProfileLoadStatus::NewStore;
    if (readFile(path_)) return ProfileLoadStatus::Loaded;
    const std::string primary_error = last_error_;
    const auto backup = std::filesystem::path(path_.wstring() + L".bak");
    profiles_.clear();
    generation_ = 0;
    if (std::filesystem::exists(backup) && readFile(backup)) {
        last_error_ = primary_error;
        return ProfileLoadStatus::RecoveredBackup;
    }
    throw std::runtime_error("profile save is invalid: " + primary_error +
                             (last_error_.empty() ? "" : "; backup: " + last_error_));
}

bool UserProfileStore::readFile(const std::filesystem::path& path) {
    try {
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("cannot open " + path.string());
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
        if (bytes.size() < kHeaderSize || !std::equal(kMagic.begin(), kMagic.end(), bytes.begin()))
            throw std::runtime_error("bad magic or truncated header");
        if (readU16(bytes, 8) != kMajorVersion)
            throw std::runtime_error("unsupported major version " + std::to_string(readU16(bytes, 8)));
        const std::uint32_t file_size = readU32(bytes, 24);
        const std::uint32_t stored_crc = readU32(bytes, 28);
        if (file_size != bytes.size()) throw std::runtime_error("file-size field mismatch");
        patchU32(bytes, 28, 0);
        if (crc32(bytes) != stored_crc) throw std::runtime_error("CRC32 mismatch");
        const std::uint32_t section_count = readU32(bytes, 20);
        if (section_count > 64 || kHeaderSize + section_count * kDirectoryEntrySize > bytes.size())
            throw std::runtime_error("invalid section directory");
        generation_ = readU64(bytes, 12);
        std::size_t prof_offset = 0, prof_size = 0, prof_count = 0;
        std::size_t stat_offset = 0, stat_size = 0, stat_count = 0;
        for (std::uint32_t section = 0; section < section_count; ++section) {
            const std::size_t at = kHeaderSize + section * kDirectoryEntrySize;
            const std::string tag(reinterpret_cast<const char*>(bytes.data() + at), 4);
            const std::size_t offset = readU32(bytes, at + 4);
            const std::size_t size = readU32(bytes, at + 8);
            const std::size_t count = readU32(bytes, at + 12);
            if (offset > bytes.size() || size > bytes.size() - offset)
                throw std::runtime_error("section outside file");
            if (tag == "PROF") { prof_offset = offset; prof_size = size; prof_count = count; }
            if (tag == "STAT") { stat_offset = offset; stat_size = size; stat_count = count; }
        }
        if (!prof_offset || prof_count > kMaximumUserProfiles || prof_size != prof_count * kProfileRecordSize)
            throw std::runtime_error("invalid PROF section");
        profiles_.reserve(prof_count);
        for (std::size_t i = 0; i < prof_count; ++i) {
            const std::size_t at = prof_offset + i * kProfileRecordSize;
            UserProfile profile;
            profile.id = readU64(bytes, at);
            profile.created_unix_seconds = readU64(bytes, at + 8);
            profile.updated_unix_seconds = readU64(bytes, at + 16);
            const std::size_t name_length = bytes[at + 24];
            if (!profile.id || name_length == 0 || name_length > kMaximumUserNameLength)
                throw std::runtime_error("invalid profile record");
            profile.name.assign(reinterpret_cast<const char*>(bytes.data() + at + 25), name_length);
            if (normalizeName(profile.name) != profile.name || nameExists(profile.name, kNoIndex))
                throw std::runtime_error("invalid or duplicate profile name");
            profiles_.push_back(std::move(profile));
        }
        if (stat_offset) {
            if (stat_size != stat_count * kStatsRecordSize)
                throw std::runtime_error("invalid STAT section");
            for (std::size_t i = 0; i < stat_count; ++i) {
                const std::size_t at = stat_offset + i * kStatsRecordSize;
                const std::uint64_t id = readU64(bytes, at);
                const auto found = std::find_if(profiles_.begin(), profiles_.end(),
                    [id](const UserProfile& profile) { return profile.id == id; });
                if (found != profiles_.end()) found->stats = readStats(bytes, at + 8);
            }
        }
        last_error_.clear();
        return true;
    } catch (const std::exception& error) {
        last_error_ = error.what();
        profiles_.clear();
        generation_ = 0;
        return false;
    }
}

bool UserProfileStore::nameExists(std::string_view normalized, std::size_t except_index) const noexcept {
    for (std::size_t i = 0; i < profiles_.size(); ++i)
        if (i != except_index && profiles_[i].name == normalized) return true;
    return false;
}

bool UserProfileStore::create(std::string_view name, std::size_t* created_index) {
    last_error_.clear();
    const std::string normalized = normalizeName(name);
    if (normalized.empty()) { last_error_ = "name is empty"; return false; }
    if (profiles_.size() >= kMaximumUserProfiles) { last_error_ = "20-profile limit reached"; return false; }
    if (nameExists(normalized, kNoIndex)) { last_error_ = "name already exists"; return false; }
    UserProfile profile;
    profile.id = newId(profiles_);
    profile.created_unix_seconds = profile.updated_unix_seconds = unixNow();
    profile.name = normalized;
    profiles_.push_back(std::move(profile));
    try { save(); }
    catch (...) { profiles_.pop_back(); throw; }
    if (created_index) *created_index = profiles_.size() - 1;
    return true;
}

bool UserProfileStore::rename(std::size_t index, std::string_view name) {
    last_error_.clear();
    if (index >= profiles_.size()) { last_error_ = "profile index out of range"; return false; }
    const std::string normalized = normalizeName(name);
    if (normalized.empty()) { last_error_ = "name is empty"; return false; }
    if (nameExists(normalized, index)) { last_error_ = "name already exists"; return false; }
    const auto previous = profiles_[index];
    profiles_[index].name = normalized;
    profiles_[index].updated_unix_seconds = unixNow();
    try { save(); }
    catch (...) { profiles_[index] = previous; throw; }
    return true;
}

bool UserProfileStore::erase(std::size_t index) {
    last_error_.clear();
    if (index >= profiles_.size()) { last_error_ = "profile index out of range"; return false; }
    const auto previous = profiles_;
    profiles_.erase(profiles_.begin() + static_cast<std::ptrdiff_t>(index));
    try { save(); }
    catch (...) { profiles_ = previous; throw; }
    return true;
}

std::vector<std::uint8_t> UserProfileStore::serialize(std::uint64_t next_generation) const {
    std::vector<std::uint8_t> out;
    out.insert(out.end(), kMagic.begin(), kMagic.end());
    appendU16(out, kMajorVersion); appendU16(out, kMinorVersion);
    appendU64(out, next_generation); appendU32(out, 2); appendU32(out, 0);
    appendU32(out, 0); appendU32(out, 0); appendU32(out, 0); // file size, CRC, reserved
    const std::size_t directory = out.size();
    out.resize(out.size() + 2 * kDirectoryEntrySize, 0);
    const std::size_t prof_offset = out.size();
    for (const auto& profile : profiles_) {
        appendU64(out, profile.id); appendU64(out, profile.created_unix_seconds);
        appendU64(out, profile.updated_unix_seconds); out.push_back(static_cast<std::uint8_t>(profile.name.size()));
        out.insert(out.end(), profile.name.begin(), profile.name.end());
        out.resize(prof_offset + ((out.size() - prof_offset + kProfileRecordSize - 1) / kProfileRecordSize) * kProfileRecordSize, 0);
    }
    const std::size_t stat_offset = out.size();
    for (const auto& profile : profiles_) { appendU64(out, profile.id); appendStats(out, profile.stats); }
    const auto writeDirectory = [&](std::size_t at, const char tag[4], std::size_t offset,
                                    std::size_t size, std::size_t count) {
        std::copy(tag, tag + 4, out.begin() + static_cast<std::ptrdiff_t>(at));
        patchU32(out, at + 4, static_cast<std::uint32_t>(offset));
        patchU32(out, at + 8, static_cast<std::uint32_t>(size));
        patchU32(out, at + 12, static_cast<std::uint32_t>(count));
    };
    writeDirectory(directory, "PROF", prof_offset, profiles_.size() * kProfileRecordSize, profiles_.size());
    writeDirectory(directory + kDirectoryEntrySize, "STAT", stat_offset,
                   profiles_.size() * kStatsRecordSize, profiles_.size());
    patchU32(out, 24, static_cast<std::uint32_t>(out.size()));
    patchU32(out, 28, 0);
    patchU32(out, 28, crc32(out));
    return out;
}

void UserProfileStore::save() {
    if (path_.empty()) throw std::runtime_error("profile store has no path");
    const std::uint64_t next_generation = generation_ + 1;
    writeAtomically(serialize(next_generation));
    generation_ = next_generation;
}

void UserProfileStore::writeAtomically(const std::vector<std::uint8_t>& bytes) {
    std::filesystem::create_directories(path_.parent_path());
    const auto temp = std::filesystem::path(path_.wstring() + L".tmp");
    const auto backup = std::filesystem::path(path_.wstring() + L".bak");
    {
        std::ofstream output(temp, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("cannot create profile temp file");
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        output.flush();
        if (!output) throw std::runtime_error("cannot write profile temp file");
    }
#ifdef _WIN32
    if (std::filesystem::exists(path_)) {
        if (!ReplaceFileW(path_.c_str(), temp.c_str(), backup.c_str(), REPLACEFILE_WRITE_THROUGH, nullptr, nullptr)) {
            std::filesystem::remove(temp);
            throw std::runtime_error("atomic profile replacement failed: " + std::to_string(GetLastError()));
        }
    } else if (!MoveFileExW(temp.c_str(), path_.c_str(), MOVEFILE_WRITE_THROUGH)) {
        std::filesystem::remove(temp);
        throw std::runtime_error("atomic profile creation failed: " + std::to_string(GetLastError()));
    }
#else
    std::error_code ignored;
    if (std::filesystem::exists(path_)) std::filesystem::copy_file(path_, backup,
        std::filesystem::copy_options::overwrite_existing, ignored);
    std::filesystem::rename(temp, path_);
#endif
}

void UserProfileMenu::open(std::size_t profile_count) noexcept {
    selected_ = profile_count; // The recovered -1 sentinel is Start New.
    editing_ = false;
    editing_existing_ = false;
    delete_pending_ = false;
    draft_.clear();
    message_.clear();
}

bool UserProfileMenu::move(int direction, std::size_t profile_count) noexcept {
    if (editing_ || delete_pending_ || !direction) return false;
    const std::size_t previous = selected_;
    if (direction < 0 && selected_ > 0) --selected_;
    if (direction > 0 && selected_ < profile_count) ++selected_;
    return selected_ != previous;
}

void UserProfileMenu::beginEdit(const UserProfileStore& store) {
    delete_pending_ = false;
    message_.clear();
    editing_existing_ = selected_ < store.profiles().size();
    draft_ = editing_existing_ ? store.profiles()[selected_].name : std::string{};
    editing_ = true;
}

bool UserProfileMenu::append(char value) {
    if (!editing_ || draft_.size() >= kMaximumUserNameLength) return false;
    const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(value)));
    if (!UserProfileStore::isAllowedNameCharacter(upper)) return false;
    if (upper == ' ' && (draft_.empty() || draft_.back() == ' ')) return false;
    draft_.push_back(upper);
    message_.clear();
    return true;
}

bool UserProfileMenu::backspace() {
    if (!editing_ || draft_.empty()) return false;
    draft_.pop_back();
    message_.clear();
    return true;
}

bool UserProfileMenu::commit(UserProfileStore& store) {
    if (!editing_) return false;
    std::size_t created = 0;
    const bool changed = editing_existing_ ? store.rename(selected_, draft_)
                                           : store.create(draft_, &created);
    if (!changed) {
        message_ = store.lastError();
        return false;
    }
    if (!editing_existing_) selected_ = created;
    editing_ = false;
    editing_existing_ = false;
    draft_.clear();
    message_ = "profile saved";
    return true;
}

bool UserProfileMenu::requestDelete(UserProfileStore& store) {
    if (editing_ || selected_ >= store.profiles().size()) return false;
    if (!delete_pending_) {
        delete_pending_ = true;
        message_ = "press delete again to confirm";
        return false;
    }
    const bool changed = store.erase(selected_);
    if (selected_ > store.profiles().size()) selected_ = store.profiles().size();
    delete_pending_ = false;
    message_ = changed ? "profile deleted" : store.lastError();
    return changed;
}

void UserProfileMenu::cancel() noexcept {
    editing_ = false;
    editing_existing_ = false;
    delete_pending_ = false;
    draft_.clear();
    message_.clear();
}

} // namespace nba97
