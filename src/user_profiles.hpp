#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace nba97 {

// FEONLY FUN_80036D48/FUN_8005CD88 recover a 20-entry table with a 0x6c
// stride. The name starts at +0x5d and the editor allows 13 visible bytes.
constexpr std::size_t kMaximumUserProfiles = 20;
constexpr std::size_t kMaximumUserNameLength = 13;

struct UserCareerStats {
    std::uint32_t games = 0;
    std::uint32_t wins = 0;
    std::uint32_t losses = 0;
    std::uint32_t points = 0;
    std::uint32_t field_goals_made = 0;
    std::uint32_t field_goals_attempted = 0;
    std::uint32_t three_pointers_made = 0;
    std::uint32_t three_pointers_attempted = 0;
    std::uint32_t free_throws_made = 0;
    std::uint32_t free_throws_attempted = 0;
    std::uint32_t rebounds = 0;
    std::uint32_t assists = 0;
    std::uint32_t steals = 0;
    std::uint32_t blocks = 0;
    std::uint32_t turnovers = 0;
    std::uint32_t fouls = 0;
};

struct UserProfile {
    std::uint64_t id = 0;
    std::uint64_t created_unix_seconds = 0;
    std::uint64_t updated_unix_seconds = 0;
    std::string name;
    UserCareerStats stats;
};

enum class ProfileLoadStatus {
    NewStore,
    Loaded,
    RecoveredBackup
};

class UserProfileStore final {
public:
    ProfileLoadStatus load(const std::filesystem::path& path);
    void save();

    [[nodiscard]] const std::vector<UserProfile>& profiles() const noexcept { return profiles_; }
    [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }
    [[nodiscard]] const std::string& lastError() const noexcept { return last_error_; }

    bool create(std::string_view name, std::size_t* created_index = nullptr);
    bool rename(std::size_t index, std::string_view name);
    bool erase(std::size_t index);

    static bool isAllowedNameCharacter(char value) noexcept;
    static std::string normalizeName(std::string_view name);

private:
    bool readFile(const std::filesystem::path& path);
    std::vector<std::uint8_t> serialize(std::uint64_t next_generation) const;
    void writeAtomically(const std::vector<std::uint8_t>& bytes);
    bool nameExists(std::string_view normalized, std::size_t except_index) const noexcept;

    std::filesystem::path path_;
    std::vector<UserProfile> profiles_;
    std::uint64_t generation_ = 0;
    std::string last_error_;
};

class UserProfileMenu final {
public:
    void open(std::size_t profile_count) noexcept;
    bool move(int direction, std::size_t profile_count) noexcept;
    void beginEdit(const UserProfileStore& store);
    bool append(char value);
    bool backspace();
    bool commit(UserProfileStore& store);
    bool requestDelete(UserProfileStore& store);
    void cancel() noexcept;

    [[nodiscard]] std::size_t selected() const noexcept { return selected_; }
    [[nodiscard]] bool editing() const noexcept { return editing_; }
    [[nodiscard]] bool deletePending() const noexcept { return delete_pending_; }
    [[nodiscard]] const std::string& draft() const noexcept { return draft_; }
    [[nodiscard]] const std::string& message() const noexcept { return message_; }

private:
    std::size_t selected_ = 0;
    bool editing_ = false;
    bool editing_existing_ = false;
    bool delete_pending_ = false;
    std::string draft_;
    std::string message_;
};

} // namespace nba97
