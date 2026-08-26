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

inline constexpr std::uint16_t kMissingStat = 0xffff;

enum class PlayerStatPeriod : std::uint8_t {
    Season1995_96,
    Playoffs1995_96,
    CurrentSeason,
    CurrentPlayoffs,
};

// Raw totals recovered from the two column-major FEONLY stat databases.
// value()/format() preserve FUN_80060094 and FUN_80060874 field semantics.
struct StatLine {
    std::uint16_t field_goal_attempts = 0;
    std::uint16_t field_goals_made = 0;
    std::uint16_t three_point_attempts = 0;
    std::uint16_t three_pointers_made = 0;
    std::uint16_t free_throw_attempts = 0;
    std::uint16_t free_throws_made = 0;
    std::uint16_t minutes = 0;
    std::uint16_t offensive_rebounds = 0;
    std::uint16_t defensive_rebounds = 0;
    std::uint16_t assists = 0;
    std::uint16_t fouls = 0;
    std::uint16_t blocks = 0;
    std::uint8_t steals = 0;
    std::uint8_t games_played = 0;
    std::uint8_t games_started = 0;
    std::uint8_t ejections = 0;
    bool valid = false;

    [[nodiscard]] std::uint16_t value(std::int16_t field) const noexcept;
    [[nodiscard]] std::string format(std::int16_t field) const;
};

struct PlayerRecord {
    std::uint16_t id = 0;
    std::uint16_t school_index = 0;
    std::uint16_t regular_stats_index = 0;
    std::uint8_t postseason_stats_index = 0;
    std::uint8_t jersey_number = 0;
    std::uint8_t position = 0;
    std::uint8_t height_inches = 0;
    std::uint8_t weight_minus_100 = 0;
    std::uint8_t source_byte_11 = 0;
    std::uint16_t source_word_12 = 0;
    std::array<std::uint8_t, static_cast<std::size_t>(PlayerRating::Count)> ratings{};
    std::array<std::uint8_t, 10> source_metadata{};
    std::uint8_t games_played_1995_96 = 0xff;
    std::uint8_t games_started_1995_96 = 0xff;
    StatLine season_1995_96;
    StatLine playoffs_1995_96;
    StatLine current_season;
    StatLine current_playoffs;
    std::string last_name, first_name, nickname, birthdate, birthplace;
    std::string school_name, acquisition_method;

    [[nodiscard]] int weightPounds() const noexcept { return weight_minus_100 + 100; }
    [[nodiscard]] std::uint8_t hand() const noexcept {
        return static_cast<std::uint8_t>(source_word_12 >> 8);
    }
    [[nodiscard]] int overallRating() const noexcept;
    [[nodiscard]] std::uint8_t yearsPro() const noexcept { return source_metadata[0]; }
    [[nodiscard]] std::uint8_t draftedByTeamId() const noexcept { return source_metadata[2]; }
    [[nodiscard]] std::uint8_t draftRound() const noexcept { return source_metadata[3]; }
    [[nodiscard]] std::uint8_t overallPick() const noexcept { return source_metadata[4]; }
    [[nodiscard]] int draftYear() const noexcept { return 1900 + source_metadata[5]; }
    [[nodiscard]] std::uint8_t acquiredFromTeamId() const noexcept { return source_metadata[7]; }
    [[nodiscard]] const StatLine& stats(PlayerStatPeriod period) const noexcept;
    [[nodiscard]] std::string displayName() const;
    [[nodiscard]] std::string jerseyNumberText() const;
};

struct TeamRecord {
    std::uint16_t id = 0;
    std::string nickname, city, alternate_name, location, abbreviation;
    std::vector<std::uint16_t> roster;
    std::array<std::uint8_t, 20> source_metadata{};
};

class RosterDatabase final {
public:
    using ResolvedTeamSlots = std::array<const PlayerRecord*, 15>;

    void load(const std::filesystem::path& path);
    [[nodiscard]] const PlayerRecord* player(std::uint16_t id) const noexcept;
    [[nodiscard]] const TeamRecord* team(std::uint16_t id) const noexcept;
    // Native representation of FEONLY FUN_8005770C. Empty signed roster
    // slots resolve to nullptr. The optional special mode applies the exact
    // position/rating-tier fallback recovered from FUN_8005768C.
    [[nodiscard]] ResolvedTeamSlots resolveTeamSlots(
        std::int16_t team_id, bool special_roster_mode = false) const noexcept;
    [[nodiscard]] std::string playerAttribute(const PlayerRecord& player,
                                              std::size_t descriptor) const;
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
    std::array<std::uint16_t, 25> special_fallback_player_ids_{};
};

[[nodiscard]] const char* playerRatingName(PlayerRating rating) noexcept;
[[nodiscard]] const char* positionName(std::uint8_t position) noexcept;

} // namespace nba97
