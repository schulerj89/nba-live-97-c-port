#include "roster_database.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace nba97 {
namespace {
constexpr std::size_t kPlayerStride = 61;
constexpr std::size_t kTeamStride = 74;

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
}

std::string PlayerRecord::displayName() const {
    return first_name.empty() ? last_name : first_name + " " + last_name;
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
    if (version != 1 || u32(data, 12) != 0x12345678)
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
    if (play.stride != kPlayerStride || play.size != play.count * play.stride ||
        team_section.stride != kTeamStride || team_section.size != team_section.count * team_section.stride)
        throw std::runtime_error("roster record stride/count mismatch");

    std::vector<PlayerRecord> new_players;
    std::vector<TeamRecord> new_teams;
    std::unordered_map<std::uint16_t, std::size_t> new_player_index, new_team_index;
    new_players.reserve(play.count);
    for (std::size_t i = 0; i < play.count; ++i) {
        const std::size_t at = play.offset + i * play.stride;
        PlayerRecord value;
        value.id = u16(data, at); value.art_index = u16(data, at + 2);
        value.portrait_index = u16(data, at + 4); value.appearance = data[at + 6];
        value.jersey_number = data[at + 7]; value.position = data[at + 8];
        value.height_inches = data[at + 9]; value.weight_minus_100 = data[at + 10];
        value.metadata = data[at + 11]; value.school_id = u16(data, at + 12);
        std::copy_n(data.begin() + static_cast<std::ptrdiff_t>(at + 14), 17, value.ratings.begin());
        std::copy_n(data.begin() + static_cast<std::ptrdiff_t>(at + 31), 10, value.source_metadata.begin());
        value.last_name = poolString(data, strings, u32(data, at + 41));
        value.first_name = poolString(data, strings, u32(data, at + 45));
        value.nickname = poolString(data, strings, u32(data, at + 49));
        value.birthdate = poolString(data, strings, u32(data, at + 53));
        value.birthplace = poolString(data, strings, u32(data, at + 57));
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
std::size_t RosterDatabase::assignedPlayerCount() const noexcept {
    std::size_t count = 0; for (const auto& value : teams_) count += value.roster.size(); return count;
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
