#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace nba97 {

enum class PlayerRating : std::uint8_t {
    FieldGoals, ThreePoint, FreeThrows, Dunking, Stealing, Blocking,
    DefensiveAwareness, Agility, OffensiveRebounds, DefensiveRebounds,
    Jumping, Strength, BallControl, OffensiveAwareness, Speed, Dribbling,
    Endurance, Count
};

struct PlayerRecord {
    std::uint16_t id = 0;
    std::uint16_t art_index = 0;
    std::uint16_t portrait_index = 0;
    std::uint8_t appearance = 0;
    std::uint8_t jersey_number = 0;
    std::uint8_t position = 0;
    std::uint8_t height_inches = 0;
    std::uint8_t weight_minus_100 = 0;
    std::uint8_t metadata = 0;
    std::uint16_t school_id = 0;
    std::array<std::uint8_t, static_cast<std::size_t>(PlayerRating::Count)> ratings{};
    std::array<std::uint8_t, 10> source_metadata{};
    std::string last_name, first_name, nickname, birthdate, birthplace;

    [[nodiscard]] int weightPounds() const noexcept { return weight_minus_100 + 100; }
    [[nodiscard]] std::string displayName() const;
};

struct TeamRecord {
    std::uint16_t id = 0;
    std::string nickname, city, alternate_name, location, abbreviation;
    std::vector<std::uint16_t> roster;
    std::array<std::uint8_t, 20> source_metadata{};
};

class RosterDatabase final {
public:
    void load(const std::filesystem::path& path);
    [[nodiscard]] const PlayerRecord* player(std::uint16_t id) const noexcept;
    [[nodiscard]] const TeamRecord* team(std::uint16_t id) const noexcept;
    [[nodiscard]] const std::vector<PlayerRecord>& players() const noexcept { return players_; }
    [[nodiscard]] const std::vector<TeamRecord>& teams() const noexcept { return teams_; }
    [[nodiscard]] std::size_t assignedPlayerCount() const noexcept;
    [[nodiscard]] std::size_t freeAgentCount() const noexcept;
    [[nodiscard]] std::uint32_t version() const noexcept { return version_; }
    [[nodiscard]] const std::filesystem::path& sourcePath() const noexcept { return source_path_; }

private:
    std::uint32_t version_ = 0;
    std::filesystem::path source_path_;
    std::vector<PlayerRecord> players_;
    std::vector<TeamRecord> teams_;
    std::unordered_map<std::uint16_t, std::size_t> player_index_;
    std::unordered_map<std::uint16_t, std::size_t> team_index_;
};

[[nodiscard]] const char* playerRatingName(PlayerRating rating) noexcept;
[[nodiscard]] const char* positionName(std::uint8_t position) noexcept;

} // namespace nba97
