#include "roster_database.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace nba97 {
namespace {
constexpr std::size_t kPlayerStrideV1 = 61;
constexpr std::size_t kPlayerStrideV2 = 63;
constexpr std::size_t kPlayerStrideV3 = 127;
constexpr std::size_t kTeamStride = 74;
constexpr std::size_t kPackedStatLineSize = 29;

std::uint16_t u16(const std::vector<std::uint8_t>& data, std::size_t at) {
    if (at + 2 > data.size()) throw std::runtime_error("roster pack truncated u16");
    return static_cast<std::uint16_t>(data[at] | (data[at + 1] << 8));
}

std::uint32_t u32(const std::vector<std::uint8_t>& data, std::size_t at) {
    if (at + 4 > data.size()) throw std::runtime_error("roster pack truncated u32");
    return static_cast<std::uint32_t>(data[at]) |
           (static_cast<std::uint32_t>(data[at + 1]) << 8) |
           (static_cast<std::uint32_t>(data[at + 2]) << 16) |
           (static_cast<std::uint32_t>(data[at + 3]) << 24);
}

struct Section { std::size_t offset, size, count, stride; };

std::string poolString(const std::vector<std::uint8_t>& data, const Section& pool,
                       std::uint32_t relative) {
    if (relative >= pool.size) throw std::runtime_error("roster string offset out of bounds");
    const std::size_t begin = pool.offset + relative;
    const auto first = data.begin() + static_cast<std::ptrdiff_t>(begin);
    const auto end = data.begin() + static_cast<std::ptrdiff_t>(pool.offset + pool.size);
    const auto zero = std::find(first, end, 0);
    if (zero == end) throw std::runtime_error("unterminated roster string");
    return std::string(first, zero);
}

StatLine packedStatLine(const std::vector<std::uint8_t>& data, std::size_t at) {
    if (at + kPackedStatLineSize > data.size())
        throw std::runtime_error("roster pack truncated stat line");
    StatLine value;
    value.field_goal_attempts = u16(data, at + 0);
    value.field_goals_made = u16(data, at + 2);
    value.three_point_attempts = u16(data, at + 4);
    value.three_pointers_made = u16(data, at + 6);
    value.free_throw_attempts = u16(data, at + 8);
    value.free_throws_made = u16(data, at + 10);
    value.minutes = u16(data, at + 12);
    value.offensive_rebounds = u16(data, at + 14);
    value.defensive_rebounds = u16(data, at + 16);
    value.assists = u16(data, at + 18);
    value.fouls = u16(data, at + 20);
    value.blocks = u16(data, at + 22);
    value.steals = data[at + 24];
    value.games_played = data[at + 25];
    value.games_started = data[at + 26];
    value.ejections = data[at + 27];
    if (data[at + 28] > 1) throw std::runtime_error("invalid stat-line validity flag");
    value.valid = data[at + 28] != 0;
    if (value.valid && (value.field_goals_made > value.field_goal_attempts ||
        value.three_pointers_made > value.three_point_attempts ||
        value.free_throws_made > value.free_throw_attempts))
        throw std::runtime_error("roster stat line has impossible shooting totals");
    return value;
}

std::uint16_t scaled(std::uint32_t numerator, std::uint32_t denominator,
                     std::uint32_t multiplier) noexcept {
    if (denominator == 0) return kMissingStat;
    return static_cast<std::uint16_t>((numerator * multiplier) / denominator);
}

const char* ordinalSuffix(unsigned value) noexcept {
    const unsigned last_two = value % 100;
    if (last_two >= 11 && last_two <= 13) return "th";
    switch (value % 10) {
    case 1: return "st";
    case 2: return "nd";
    case 3: return "rd";
    default: return "th";
    }
}
}

std::uint16_t StatLine::value(std::int16_t field) const noexcept {
    if (!valid) return kMissingStat;
    const std::uint32_t points = field_goals_made * 2u + three_pointers_made + free_throws_made;
    const std::uint32_t rebounds = offensive_rebounds + defensive_rebounds;
    switch (field) {
    case 0: case 10: return field_goal_attempts;
    case 1: case 12: return three_point_attempts;
    case 2: case 14: return free_throw_attempts;
    case 6: return static_cast<std::uint16_t>(points);
    case 8: return minutes;
    case 9: return field_goals_made;
    case 11: return three_pointers_made;
    case 13: return free_throws_made;
    case 15: return offensive_rebounds;
    case 16: return defensive_rebounds;
    case 17: return static_cast<std::uint16_t>(rebounds);
    case 18: return blocks;
    case 19: return steals;
    case 20: return assists;
    case 21: return fouls;
    case 22: return games_played;
    case 23: return games_started;
    case 25: return ejections;
    case 32: return scaled(points, games_played, 10);
    case 34: return scaled(minutes, games_played, 10);
    case 37: return scaled(rebounds, games_played, 10);
    case 38: return scaled(blocks, games_played, 10);
    case 39: return scaled(steals, games_played, 10);
    case 40: return scaled(assists, games_played, 10);
    case 42: return scaled(fouls, games_played, 10);
    case 43: return scaled(field_goals_made, field_goal_attempts, 1000);
    case 44: return scaled(three_pointers_made, three_point_attempts, 1000);
    case 45: return scaled(free_throws_made, free_throw_attempts, 1000);
    default: return 0;
    }
}

std::string StatLine::format(std::int16_t field) const {
    if (!valid) return "-  ";
    if (field >= 0 && field <= 2) {
        const std::uint16_t made[] = {field_goals_made, three_pointers_made, free_throws_made};
        const std::uint16_t attempted[] = {field_goal_attempts, three_point_attempts, free_throw_attempts};
        return std::to_string(made[field]) + "/" + std::to_string(attempted[field]);
    }
    const auto result = value(field);
    if (result == kMissingStat) return "-  ";
    if (field >= 32 && field < 43)
        return std::to_string(result / 10) + "." + std::to_string(result % 10);
    if (field >= 43 && field < 52) {
        if (result == 1000) return "1.000";
        std::ostringstream out;
        out << '.' << std::setfill('0') << std::setw(3) << result;
        return out.str();
    }
    return std::to_string(result);
}

std::string PlayerRecord::displayName() const {
    return first_name.empty() ? last_name : first_name + " " + last_name;
}

int PlayerRecord::overallRating() const noexcept {
    unsigned total = 0;
    for (std::size_t i = 0; i < 16; ++i) total += ratings[i];
    return static_cast<int>(total / 16);
}

const StatLine& PlayerRecord::stats(PlayerStatPeriod period) const noexcept {
    switch (period) {
    case PlayerStatPeriod::Season1995_96: return season_1995_96;
    case PlayerStatPeriod::Playoffs1995_96: return playoffs_1995_96;
    case PlayerStatPeriod::CurrentSeason: return current_season;
    case PlayerStatPeriod::CurrentPlayoffs: return current_playoffs;
    }
    return current_season;
}

void RosterDatabase::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("missing private roster database: " + path.string() +
        " (run scripts/extract_assetpacks.ps1)");
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(input)), {});
    static constexpr char magic[8] = {'N','9','7','R','D','B','\0','\0'};
    if (data.size() < 24 || std::memcmp(data.data(), magic, 8) != 0)
        throw std::runtime_error("invalid roster database magic");
    const auto version = u32(data, 8);
    if ((version != 1 && version != 2 && version != 3) || u32(data, 12) != 0x12345678)
        throw std::runtime_error("unsupported roster database version/endianness");
    const auto section_count = u32(data, 16);
    if (section_count == 0 || section_count > 32 || u32(data, 20) != data.size())
        throw std::runtime_error("invalid roster database directory");

    std::unordered_map<std::string, Section> sections;
    for (std::uint32_t i = 0; i < section_count; ++i) {
        const std::size_t at = 24 + i * 20;
        if (at + 20 > data.size()) throw std::runtime_error("truncated roster section directory");
        const std::string tag(reinterpret_cast<const char*>(data.data() + at), 4);
        Section section{u32(data, at + 4), u32(data, at + 8),
                        u32(data, at + 12), u32(data, at + 16)};
        if (section.offset > data.size() || section.size > data.size() - section.offset)
            throw std::runtime_error("roster section outside file: " + tag);
        if (!sections.emplace(tag, section).second)
            throw std::runtime_error("duplicate roster section: " + tag);
    }
    const auto require = [&sections](const char* tag) -> const Section& {
        const auto found = sections.find(tag);
        if (found == sections.end()) throw std::runtime_error(std::string("missing roster section: ") + tag);
        return found->second;
    };
    const Section& play = require("PLAY");
    const Section& team_section = require("TEAM");
    const Section& strings = require("STRS");
    const std::size_t expected_player_stride = version == 1 ? kPlayerStrideV1 :
        (version == 2 ? kPlayerStrideV2 : kPlayerStrideV3);
    if (play.stride != expected_player_stride || play.size != play.count * play.stride ||
        team_section.stride != kTeamStride || team_section.size != team_section.count * team_section.stride)
        throw std::runtime_error("roster record stride/count mismatch");

    std::vector<PlayerRecord> new_players;
    std::vector<TeamRecord> new_teams;
    std::unordered_map<std::uint16_t, std::size_t> new_player_index, new_team_index;
    new_players.reserve(play.count);
    for (std::size_t i = 0; i < play.count; ++i) {
        const std::size_t at = play.offset + i * play.stride;
        PlayerRecord value;
        value.id = u16(data, at); value.school_index = u16(data, at + 2);
        value.regular_stats_index = u16(data, at + 4);
        value.postseason_stats_index = data[at + 6];
        value.jersey_number = data[at + 7]; value.position = data[at + 8];
        value.height_inches = data[at + 9]; value.weight_minus_100 = data[at + 10];
        value.source_byte_11 = data[at + 11]; value.source_word_12 = u16(data, at + 12);
        std::copy_n(data.begin() + static_cast<std::ptrdiff_t>(at + 14), 17, value.ratings.begin());
        std::copy_n(data.begin() + static_cast<std::ptrdiff_t>(at + 31), 10, value.source_metadata.begin());
        const std::size_t string_fields = version == 1 ? at + 41 :
            (version == 2 ? at + 43 : at + 99);
        if (version == 2) {
            value.games_played_1995_96 = data[at + 41];
            value.games_started_1995_96 = data[at + 42];
        } else if (version == 3) {
            value.season_1995_96 = packedStatLine(data, at + 41);
            value.playoffs_1995_96 = packedStatLine(data, at + 70);
            if (value.regular_stats_index >= 391 || value.postseason_stats_index >= 180 ||
                value.season_1995_96.valid != (value.regular_stats_index != 0) ||
                value.playoffs_1995_96.valid != (value.postseason_stats_index != 0))
                throw std::runtime_error("player stat index/validity mismatch");
            value.games_played_1995_96 = value.season_1995_96.valid ?
                value.season_1995_96.games_played : 0xff;
            value.games_started_1995_96 = value.season_1995_96.valid ?
                value.season_1995_96.games_started : 0xff;
        }
        value.last_name = poolString(data, strings, u32(data, string_fields));
        value.first_name = poolString(data, strings, u32(data, string_fields + 4));
        value.nickname = poolString(data, strings, u32(data, string_fields + 8));
        value.birthdate = poolString(data, strings, u32(data, string_fields + 12));
        value.birthplace = poolString(data, strings, u32(data, string_fields + 16));
        if (version == 3) {
            value.school_name = poolString(data, strings, u32(data, string_fields + 20));
            value.acquisition_method = poolString(data, strings, u32(data, string_fields + 24));
            if (value.school_name.empty() || value.acquisition_method.empty())
                throw std::runtime_error("v3 player attribute lookup string is empty");
        }
        if (!new_player_index.emplace(value.id, new_players.size()).second)
            throw std::runtime_error("duplicate player ID");
        new_players.push_back(std::move(value));
    }
    new_teams.reserve(team_section.count);
    std::unordered_set<std::uint16_t> assigned;
    for (std::size_t i = 0; i < team_section.count; ++i) {
        const std::size_t at = team_section.offset + i * team_section.stride;
        TeamRecord value;
        value.id = u16(data, at);
        const std::size_t roster_count = u16(data, at + 2);
        if (roster_count > 15) throw std::runtime_error("team roster exceeds 15 slots");
        value.nickname = poolString(data, strings, u32(data, at + 4));
        value.city = poolString(data, strings, u32(data, at + 8));
        value.alternate_name = poolString(data, strings, u32(data, at + 12));
        value.location = poolString(data, strings, u32(data, at + 16));
        value.abbreviation = poolString(data, strings, u32(data, at + 20));
        for (std::size_t slot = 0; slot < roster_count; ++slot) {
            const auto id = static_cast<std::int16_t>(u16(data, at + 24 + slot * 2));
            if (id < 0 || !new_player_index.count(static_cast<std::uint16_t>(id)))
                throw std::runtime_error("team references unknown player ID");
            if (!assigned.insert(static_cast<std::uint16_t>(id)).second)
                throw std::runtime_error("player assigned to multiple teams");
            value.roster.push_back(static_cast<std::uint16_t>(id));
        }
        // The recovered frontend consumes a fixed fifteen-short roster and
        // preserves -1 holes instead of collapsing them.
        value.roster.resize(15, UINT16_MAX);
        std::copy_n(data.begin() + static_cast<std::ptrdiff_t>(at + 54), 20,
                    value.source_metadata.begin());
        if (!new_team_index.emplace(value.id, new_teams.size()).second)
            throw std::runtime_error("duplicate team ID");
        new_teams.push_back(std::move(value));
    }
    source_path_ = path; version_ = version;
    players_ = std::move(new_players); teams_ = std::move(new_teams);
    player_index_ = std::move(new_player_index); team_index_ = std::move(new_team_index);
}

const PlayerRecord* RosterDatabase::player(std::uint16_t id) const noexcept {
    const auto found = player_index_.find(id);
    return found == player_index_.end() ? nullptr : &players_[found->second];
}
const TeamRecord* RosterDatabase::team(std::uint16_t id) const noexcept {
    const auto found = team_index_.find(id);
    return found == team_index_.end() ? nullptr : &teams_[found->second];
}
std::string RosterDatabase::playerAttribute(const PlayerRecord& player,
                                            std::size_t descriptor) const {
    const auto teamCity = [this](std::uint8_t id) -> std::string {
        const TeamRecord* value = team(id);
        return value ? value->city : "n/a";
    };
    switch (descriptor) {
    case 0: return player.first_name;
    case 1: return player.nickname;
    case 2: return player.birthdate;
    case 3: return player.birthplace;
    case 4: return std::to_string(player.height_inches / 12) + "'" +
                   std::to_string(player.height_inches % 12) + "\"";
    case 5: return std::to_string(player.weightPounds()) + " lbs.";
    case 6: return player.hand() == 0 ? "right" : "left";
    case 7: return player.school_name.empty() ? "n/a" : player.school_name;
    case 8:
        if (player.yearsPro() == 0) return "rookie";
        return std::to_string(player.yearsPro()) +
               (player.yearsPro() == 1 ? " year" : " years");
    case 9: return std::to_string(player.draftYear());
    case 10: return teamCity(player.draftedByTeamId());
    case 11:
        if (player.draftRound() == 0xff) return "not picked";
        return std::to_string(player.draftRound()) + ordinalSuffix(player.draftRound()) + " round";
    case 12:
        if (player.overallPick() == 0xff) return "not picked";
        return std::to_string(player.overallPick()) + ordinalSuffix(player.overallPick()) + " pick";
    case 13: return player.acquisition_method.empty() ? "n/a" : player.acquisition_method;
    case 14: return teamCity(player.acquiredFromTeamId());
    default: return {};
    }
}
std::size_t RosterDatabase::assignedPlayerCount() const noexcept {
    std::size_t count = 0;
    for (const auto& team : teams_)
        for (const auto id : team.roster)
            if (player(id)) ++count;
    return count;
}
std::size_t RosterDatabase::freeAgentCount() const noexcept {
    return players_.size() - (std::min)(players_.size(), assignedPlayerCount());
}

const char* playerRatingName(PlayerRating rating) noexcept {
    static constexpr const char* names[]{"field goals", "3 point FGs", "free throws",
        "dunking", "stealing", "blocking", "def. awareness", "agility",
        "off. rebounds", "def. rebounds", "jumping", "strength", "ball control",
        "off. awareness", "speed", "dribbling", "endurance"};
    const auto index = static_cast<std::size_t>(rating);
    return index < std::size(names) ? names[index] : "unknown";
}
const char* positionName(std::uint8_t position) noexcept {
    static constexpr const char* names[]{"center", "power forward", "small forward",
                                         "shooting guard", "point guard"};
    return position < std::size(names) ? names[position] : "unknown";
}
} // namespace nba97
