#include "roster_database.hpp"
#include "recovered/semantic_trace.h"
#include "sha256.hpp"

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

RosterBaseIdentity catalogueIdentity(const RosterDatabase& db,
        const std::array<std::uint16_t,25>& fallback,const RosterDatabase::SlotTable& original) {
    // Canonical v1: explicit logical fields, sorted stable IDs, no addresses,
    // pack offsets, path, packing version, STL padding or derived cache values.
    // Field order is a durable schema: see docs/roster_save_format.md.
    Sha256 hash;
    constexpr char domain[]="NBA97.ROSTER.BASE.V1";
    hash.update(domain,sizeof(domain));
    const auto scalar=[&](std::uint32_t v,unsigned n) {
        std::uint8_t bytes[4]{};
        for(unsigned i=0;i<n;++i) { bytes[i]=static_cast<std::uint8_t>(v); v>>=8; }
        hash.update(bytes,n);
    };
    const auto text=[&](std::string_view s) {
        if(s.size()>UINT32_MAX) throw std::runtime_error("catalogue string too large");
        scalar(static_cast<std::uint32_t>(s.size()),4); hash.update(s.data(),s.size());
    };
    const auto stats=[&](const StatLine& s) {
        scalar(s.valid ? 1 : 0,1);
        for(auto v : {s.field_goal_attempts,s.field_goals_made,s.three_point_attempts,s.three_pointers_made,
            s.free_throw_attempts,s.free_throws_made,s.minutes,s.offensive_rebounds,s.defensive_rebounds,
            s.assists,s.fouls,s.blocks}) scalar(v,2);
        for(auto v : {s.steals,s.games_played,s.games_started,s.ejections}) scalar(v,1);
    };
    std::vector<const PlayerRecord*> ordered;
    ordered.reserve(db.players().size());
    for(const auto& p:db.players()) ordered.push_back(&p);
    std::sort(ordered.begin(),ordered.end(),[](auto a,auto b){return a->id<b->id;});
    scalar(static_cast<std::uint32_t>(ordered.size()),4);
    for(const auto* p:ordered) {
        for(auto v : {p->id,p->school_index,p->regular_stats_index}) scalar(v,2);
        for(auto v : {p->postseason_stats_index,p->jersey_number,p->position,p->height_inches,
            p->weight_minus_100,p->source_byte_11}) scalar(v,1);
        scalar(p->source_word_12,2);
        hash.update(p->ratings.data(),p->ratings.size());
        hash.update(p->source_metadata.data(),p->source_metadata.size());
        scalar(p->games_played_1995_96,1); scalar(p->games_started_1995_96,1);
        stats(p->season_1995_96); stats(p->playoffs_1995_96);
        for(const auto* s : {&p->last_name,&p->first_name,&p->nickname,&p->birthdate,&p->birthplace,
            &p->school_name,&p->acquisition_method}) text(*s);
    }
    scalar(29,4);
    for(unsigned id=0;id<29;++id) {
        const auto* t=db.team(static_cast<std::uint16_t>(id));
        if(!t) throw std::runtime_error("canonical catalogue requires all 29 teams");
        scalar(t->id,2);
        for(auto s : {t->nickname,t->city,t->alternate_name,t->location,t->abbreviation}) text(s);
        hash.update(t->source_metadata.data(),t->source_metadata.size());
    }
    for(auto id:original) scalar(id,2);
    for(auto id:fallback) scalar(id,2);
    return hash.digest();
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

std::string PlayerRecord::jerseyNumberText() const {
    // FEONLY stores jersey 00 as the byte sentinel 0xff. Preserve that raw
    // representation in the recovered database, but reproduce the frontend's
    // two-character display instead of leaking unsigned decimal 255.
    return jersey_number == 0xff ? "00" : std::to_string(jersey_number);
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
    if ((version < 1 || version > 5) || u32(data, 12) != 0x12345678)
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
    if (play.count==0 || play.count>0x8000 || team_section.count!=29 ||
        play.stride != expected_player_stride || play.size != play.count * play.stride ||
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
        if(value.id>=0x8000) throw std::runtime_error("unsupported base player ID");
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
        } else if (version >= 3) {
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
        if (version >= 3) {
            value.school_name = poolString(data, strings, u32(data, string_fields + 20));
            value.acquisition_method = poolString(data, strings, u32(data, string_fields + 24));
            if (value.school_name.empty() || value.acquisition_method.empty())
                throw std::runtime_error("roster player attribute lookup string is empty");
        }
        if (!new_player_index.emplace(value.id, new_players.size()).second)
            throw std::runtime_error("duplicate player ID");
        new_players.push_back(std::move(value));
    }
    new_teams.reserve(team_section.count);
    std::unordered_set<std::uint16_t> assigned;
    for (std::size_t i = 0; i < team_section.count; ++i) {
        const std::size_t at = team_section.offset + i * team_section.stride;
        TeamRecord value({poolString(data, strings, u32(data, at + 4)),
            poolString(data, strings, u32(data, at + 8)),
            poolString(data, strings, u32(data, at + 12)),
            poolString(data, strings, u32(data, at + 16)),
            poolString(data, strings, u32(data, at + 20))});
        value.id = u16(data, at);
        if(value.id>=29) throw std::runtime_error("unsupported base team ID");
        const std::size_t roster_count = u16(data, at + 2);
        if (roster_count > 15) throw std::runtime_error("team roster exceeds 15 slots");
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
    std::array<std::uint16_t, 25> new_special_fallback_ids{};
    new_special_fallback_ids.fill(UINT16_MAX);
    if (version >= 4) {
        const Section& fallback = require("FALL");
        if (fallback.count != new_special_fallback_ids.size() ||
            fallback.stride != sizeof(std::uint16_t) ||
            fallback.size != fallback.count * fallback.stride)
            throw std::runtime_error("special roster fallback table shape mismatch");
        for (std::size_t i = 0; i < new_special_fallback_ids.size(); ++i) {
            const auto id = u16(data, fallback.offset + i * sizeof(std::uint16_t));
            if (!new_player_index.count(id))
                throw std::runtime_error("special roster fallback references unknown player ID");
            new_special_fallback_ids[i] = id;
        }
    }
    std::array<std::uint16_t, 100> new_free_agent_slots{};
    new_free_agent_slots.fill(UINT16_MAX);
    if (version >= 5) {
        const Section& free_agents = require("FREE");
        if (free_agents.count != new_free_agent_slots.size() ||
            free_agents.stride != sizeof(std::uint16_t) ||
            free_agents.size != free_agents.count * free_agents.stride)
            throw std::runtime_error("free-agent slot table shape mismatch");
        std::unordered_set<std::uint16_t> free_agent_ids;
        bool saw_hole = false;
        for (std::size_t i = 0; i < new_free_agent_slots.size(); ++i) {
            const auto raw = static_cast<std::int16_t>(
                u16(data, free_agents.offset + i * sizeof(std::uint16_t)));
            if (raw == -1) {
                saw_hole = true;
                continue;
            }
            if (raw < 0 || saw_hole ||
                !new_player_index.count(static_cast<std::uint16_t>(raw)) ||
                assigned.count(static_cast<std::uint16_t>(raw)) ||
                !free_agent_ids.insert(static_cast<std::uint16_t>(raw)).second)
                throw std::runtime_error("invalid free-agent slot assignment");
            new_free_agent_slots[i] = static_cast<std::uint16_t>(raw);
        }
    } else {
        // Legacy packs did not preserve the original thirtieth slot list.
        std::size_t slot = 0;
        for (const auto& value : new_players) {
            if (!assigned.count(value.id) && slot < new_free_agent_slots.size())
                new_free_agent_slots[slot++] = value.id;
        }
    }
    auto catalogue = std::make_shared<PlayerCatalogue>();
    catalogue->players = std::move(new_players);
    catalogue->index = std::move(new_player_index);
    // Parse, canonicalize and allocate every derived cache off to the side.
    // Failed reloads must retain both accepted data and the old immutable base.
    RosterDatabase candidate;
    candidate.source_path_ = path; candidate.version_ = version;
    candidate.catalogue_ = catalogue; candidate.teams_ = std::move(new_teams);
    candidate.team_index_ = std::move(new_team_index);
    candidate.special_fallback_player_ids_ = new_special_fallback_ids;
    candidate.free_agent_slots_ = new_free_agent_slots;
    catalogue->original_slots=candidate.slotTable();
    catalogue->identity=catalogueIdentity(candidate,new_special_fallback_ids,catalogue->original_slots);
    catalogue->base_ready=true;
    candidate.copySlotTable();
    swap(candidate);
}

Nba97ReorderResult RosterDatabase::reorderSlots(
    std::int16_t team_id, int source, int destination, std::uint16_t& session_changes) {
    if (team_id < 0 || team_id >= 29) return NBA97_REORDER_INVALID_ARGUMENT;
    const auto found = team_index_.find(static_cast<std::uint16_t>(team_id));
    if (found == team_index_.end()) return NBA97_REORDER_INVALID_ARGUMENT;
    auto& slots = teams_[found->second].roster;
    if (slots.size() != NBA97_TEAM_SLOTS) return NBA97_REORDER_INVALID_ARGUMENT;
    const auto result = nba97_reorder_swap(slots.data(), source, destination, &session_changes);
    if (result == NBA97_REORDER_CHANGED) copySlotTable();
    return result;
}

bool RosterDatabase::applyReorderSession(std::int16_t team_id, const Nba97ReorderSession& session) {
    if (team_id < 0 || team_id >= 29 || session.phase != NBA97_REORDER_CLOSED || !session.accepted)
        return false;
    const auto found = team_index_.find(static_cast<std::uint16_t>(team_id));
    if (found == team_index_.end()) return false;
    const auto& current = teams_[found->second].roster;
    if (current.size() != NBA97_TEAM_SLOTS ||
        !std::equal(current.begin(), current.end(), std::begin(session.original))) return false;
    auto before = current;
    std::vector<std::uint16_t> after(std::begin(session.slots), std::end(session.slots));
    std::sort(before.begin(), before.end());
    std::sort(after.begin(), after.end());
    if (before != after) return false; // Host guard: no insertion/deletion of IDs.
    // Build all derived indexes before publishing; preserve the live database
    // if allocating the new tables fails. This is a native transaction boundary.
    auto candidate = *this;
    candidate.teams_[found->second].roster.assign(std::begin(session.slots), std::end(session.slots));
    candidate.copySlotTable();
    swap(candidate);
    return true;
}

RosterDatabase::SlotTable RosterDatabase::slotTable() const {
    SlotTable result{};
    for (std::uint16_t id = 0; id < 29; ++id) {
        const auto* t = team(id);
        if (!t || t->roster.size() != 15) throw std::runtime_error("incomplete 535-slot database");
        std::copy(t->roster.begin(), t->roster.end(), result.begin() + id * 15);
    }
    std::copy(free_agent_slots_.begin(), free_agent_slots_.end(), result.begin() + 435);
    return result;
}

const RosterDatabase::SlotTable& RosterDatabase::originalSlots() const {
    if(!catalogue_->base_ready) throw std::runtime_error("roster base is not loaded");
    return catalogue_->original_slots;
}
const RosterBaseIdentity& RosterDatabase::baseIdentity() const {
    if(!catalogue_->base_ready) throw std::runtime_error("roster base is not loaded");
    return catalogue_->identity;
}
bool RosterDatabase::differsFromOriginal() const { return slotTable()!=originalSlots(); }

RosterDatabase RosterDatabase::prepareSlotTable(const SlotTable& proposed) const {
    auto before=originalSlots(),after=proposed;
    std::sort(before.begin(),before.end()); std::sort(after.begin(),after.end());
    if(before!=after) throw std::runtime_error("prepared roster changes the supported base population");
    for(unsigned list=0;list<30;++list) {
        bool hole=false;
        const unsigned count=list==29 ? 100 : 15;
        for(unsigned i=0;i<count;++i) {
            const auto id=proposed[list*15+i];
            if(id==UINT16_MAX) hole=true;
            else if(hole) throw std::runtime_error("prepared roster has noncontiguous occupied slots");
        }
    }
    auto candidate=*this;
    for(auto& t:candidate.teams_)
        t.roster.assign(proposed.begin()+t.id*15,proposed.begin()+(t.id+1)*15);
    std::copy(proposed.begin()+435,proposed.end(),candidate.free_agent_slots_.begin());
    candidate.copySlotTable();
    return candidate;
}

void RosterDatabase::swap(RosterDatabase& other) noexcept {
    using std::swap;
    swap(version_,other.version_); source_path_.swap(other.source_path_);
    catalogue_.swap(other.catalogue_); teams_.swap(other.teams_); team_index_.swap(other.team_index_);
    special_fallback_player_ids_.swap(other.special_fallback_player_ids_);
    free_agent_slots_.swap(other.free_agent_slots_); resolved_team_slots_.swap(other.resolved_team_slots_);
    player_roster_membership_.swap(other.player_roster_membership_); roster_counts_.swap(other.roster_counts_);
    swap(derived_team_ratings_dirty_,other.derived_team_ratings_dirty_);
}

bool RosterDatabase::applyReorderScreen(const Nba97ReorderScreen& s) {
    if (!s.selection.accepted || s.selection.phase != NBA97_REORDER_CLOSED) return false;
    const auto current = slotTable();
    if (!std::equal(current.begin(), current.end(), s.snapshot) ||
        !std::equal(current.begin() + 435, current.end(), s.working + 435)) return false;
    for (int team_id = 0; team_id < 29; ++team_id) {
        std::array<std::uint16_t, 15> before{}, after{};
        std::copy_n(s.snapshot + team_id * 15, 15, before.begin());
        std::copy_n(s.working + team_id * 15, 15, after.begin());
        std::sort(before.begin(), before.end()); std::sort(after.begin(), after.end());
        if (before != after) return false;
    }
    if (std::equal(current.begin(), current.end(), s.working)) return true;
    auto candidate = *this;
    for (auto& t : candidate.teams_)
        t.roster.assign(s.working + t.id * 15, s.working + (t.id+1) * 15);
    candidate.copySlotTable(); // Rebuild roster projections against the shared immutable catalogue.
    swap(candidate);
    return true;
}

RosterDatabase RosterDatabase::draftView(const Nba97ReorderScreen& s) const {
    if ((s.selection.phase != NBA97_REORDER_FIRST && s.selection.phase != NBA97_REORDER_REPLACEMENT) ||
        s.team < 0 || s.team >= 29 || s.selection.accepted)
        throw std::runtime_error("invalid Re-order draft lifecycle");
    const auto baseline = slotTable();
    if (!std::equal(baseline.begin(), baseline.end(), s.snapshot))
        throw std::runtime_error("stale Re-order draft baseline");
    SlotTable proposed{};
    std::copy_n(s.working, proposed.size(), proposed.begin());
    std::copy_n(s.selection.slots, 15, proposed.begin() + s.team * 15);
    if (!std::equal(baseline.begin()+435, baseline.end(), proposed.begin()+435))
        throw std::runtime_error("Re-order draft changed free-agent membership");
    for (int team_id=0;team_id<29;++team_id) {
        std::array<std::uint16_t,15> before{}, after{};
        std::copy_n(baseline.begin()+team_id*15,15,before.begin());
        std::copy_n(proposed.begin()+team_id*15,15,after.begin());
        std::sort(before.begin(),before.end()); std::sort(after.begin(),after.end());
        if(before!=after) throw std::runtime_error("Re-order draft changed team membership");
    }
    auto view = *this; // Only roster/derived state copied; player catalogue shared.
    for (auto& team : view.teams_)
        team.roster.assign(proposed.begin()+team.id*15,proposed.begin()+(team.id+1)*15);
    view.copySlotTable();
    return view;
}

void RosterDatabase::copySlotTable() {
    nba97_semantic_trace_record(0x80057864u);
    roster_counts_.fill(0);
    player_roster_membership_.assign(catalogue_->players.size(), -1);

    // FUN_80057864 copies fifteen signed IDs and immediately resolves each
    // of the 29 team lists before advancing to the next team.
    for (std::int16_t team_id = 0; team_id < 29; ++team_id) {
        resolved_team_slots_[static_cast<std::size_t>(team_id)] =
            resolveTeamSlots(team_id);
        const TeamRecord* selected_team = team(static_cast<std::uint16_t>(team_id));
        if (!selected_team) continue;
        for (const auto id : selected_team->roster) {
            if (id == UINT16_MAX) continue;
            const auto found = catalogue_->index.find(id);
            if (found != catalogue_->index.end()) {
                player_roster_membership_[found->second] = team_id;
                ++roster_counts_[static_cast<std::size_t>(team_id)];
            }
        }
    }

    // The second loop copies the exact 100-short thirtieth list. The
    // subsequent FUN_80054CBC call maps those players to owner 29 and counts
    // the non--1 entries; hidden players remain deliberately unlisted.
    for (const auto id : free_agent_slots_) {
        if (id == UINT16_MAX) continue;
        const auto found = catalogue_->index.find(id);
        if (found != catalogue_->index.end()) {
            player_roster_membership_[found->second] = 29;
            ++roster_counts_[29];
        }
    }

    // The original then clears +0x72E before calling FUN_8005DB34. Preserve
    // that invalidation/request boundary; the callee's ranking algorithm is
    // independently scoped recovery work and is not claimed here.
    derived_team_ratings_dirty_ = true;
}

const PlayerRecord* RosterDatabase::player(std::uint16_t id) const noexcept {
    nba97_semantic_trace_record(0x8005FE14u);
    const auto found = catalogue_->index.find(id);
    return found == catalogue_->index.end() ? nullptr : &catalogue_->players[found->second];
}
const TeamRecord* RosterDatabase::team(std::uint16_t id) const noexcept {
    const auto found = team_index_.find(id);
    return found == team_index_.end() ? nullptr : &teams_[found->second];
}
RosterDatabase::ResolvedTeamSlots RosterDatabase::resolveTeamSlots(
    std::int16_t team_id, bool special_roster_mode) const noexcept {
    ResolvedTeamSlots result{};
    nba97_semantic_trace_record(0x8005770Cu);
    if (team_id < 0 || team_id >= 29) return result;

    const TeamRecord* selected_team = team(static_cast<std::uint16_t>(team_id));
    if (!selected_team) return result;
    for (std::size_t slot = 0; slot < result.size(); ++slot) {
        const std::uint16_t id = slot < selected_team->roster.size()
            ? selected_team->roster[slot] : UINT16_MAX;
        if (id == UINT16_MAX) continue; // Original signed -1 roster sentinel.

        const PlayerRecord* resolved = player(id);
        if (special_roster_mode && resolved && resolved->regular_stats_index == 0) {
            // FUN_8005768C indexes a private 5-position by 5-rating-tier
            // replacement table. Keep those copyrighted IDs in roster.n97db.
            const unsigned rating_sum = resolved->ratings[0] + resolved->ratings[1] +
                resolved->ratings[2] + resolved->ratings[3];
            if (resolved->position < 5 && rating_sum >= 200) {
                const unsigned tier = (rating_sum - 200) / 40;
                if (tier < 5) {
                    const auto fallback_id = special_fallback_player_ids_[
                        static_cast<std::size_t>(resolved->position) * 5 + tier];
                    resolved = player(fallback_id);
                }
            }
        }
        result[slot] = resolved;
    }
    return result;
}
std::string RosterDatabase::playerAttribute(const PlayerRecord& player,
                                            std::size_t descriptor) const {
    const auto teamCity = [this](std::uint8_t id) -> std::string {
        const TeamRecord* value = team(id);
        return value ? std::string(value->city) : "n/a";
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
    for (std::size_t team_id = 0; team_id < 29; ++team_id)
        count += roster_counts_[team_id];
    return count;
}
std::size_t RosterDatabase::freeAgentCount() const noexcept {
    return roster_counts_[29];
}
std::size_t RosterDatabase::unlistedPlayerCount() const noexcept {
    return catalogue_->players.size() - (std::min)(catalogue_->players.size(),
        assignedPlayerCount() + freeAgentCount());
}
std::int16_t RosterDatabase::rosterOwner(std::uint16_t player_id) const noexcept {
    const auto found = catalogue_->index.find(player_id);
    if (found == catalogue_->index.end() || found->second >= player_roster_membership_.size())
        return -1;
    return player_roster_membership_[found->second];
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
