#pragma once
#include "match_controls.hpp"
#include "roster_ratings.hpp"
#include "frontend_settings.hpp"
#include "recovered/match_setup.h"
#include "recovered/user_setup.h"
#include "recovered/create_player.h"
#include <memory>

namespace nba97 {
struct MatchRequest {
    std::array<uint16_t,2> teams{}; // home,away;29/30 are not free-agent owner29.
    std::array<uint8_t,4> setup{}; // quarter,mode,style,difficulty indices.
    Nba97UserSetup users{};
};
struct MatchSourceView {
    const RosterDatabase& rosters;
    const FrontendSettings& settings;
    const std::vector<UserProfile>& profiles;
    const Nba97CreatedPlayerCatalog& created;
    const std::array<int16_t,29>& rating_adjustments;
    uint64_t roster_generation=0,profile_generation=0,created_generation=0;
};
struct MatchTeamSnapshot {
    uint16_t id=0;
    std::array<uint16_t,15> roster{};
    Nba97MatchRosterIndices indices{};
    std::vector<PlayerRecord> players; // owned valid prefix, including bench13..15.
    std::array<std::string,5> names;
    std::array<uint8_t,20> metadata{};
};
enum MatchPending : uint32_t {
    MatchExtensionSettings=1, MatchPresentationVariant=2, MatchCreatedMembership=4
};
struct MatchSnapshot {
    MatchRequest request;
    std::array<MatchTeamSnapshot,2> teams;
    RosterBaseIdentity base_identity{};
    RosterDatabase::SlotTable accepted_slots{};
    Nba97TeamRanks ranks{};
    std::array<uint8_t,11> options{};
    std::array<uint8_t,14> rules{},custom_rules{};
    std::array<uint8_t,2> injury_slot{{255,255}}; // ordinary exhibition source branch.
    uint32_t venue_selector=0; // resident8001EC94; ordinary exhibition only.
    uint16_t launch_control=0; // resident8001EDEC; other launch branches pending.
    MatchControlResult controls;
    Nba97CreatedPlayerCatalog created{}; // retained, never treated as accepted membership.
    uint64_t roster_generation=0,profile_generation=0,created_generation=0;
    uint32_t pending=MatchExtensionSettings|MatchPresentationVariant;
};
// Pure preparation: owns every published value, no I/O or source mutation.
// Unsupported branches throw explicitly; a captured subset is not launch-ready.
MatchSnapshot buildMatchSnapshot(const MatchRequest&,const MatchSourceView&,
    const Nba97MatchControls& live,const std::array<uint8_t,59>& defaults);
std::string matchSnapshotReceipt(const MatchSnapshot&);

class MatchSession {
public:
    // Once per fresh native host. Repeating identical initialization is a no-op;
    // it must not replace maps retained from a previous successful handoff.
    void initialize(const std::array<uint8_t,59>& defaults);
    bool initialized() const noexcept {return initialized_;}
    const MatchSnapshot& capture(const MatchRequest&,const MatchSourceView&);
    const MatchSnapshot* snapshot() const noexcept {return snapshot_.get();}
    const Nba97MatchControls& liveControls() const noexcept {return live_;}
    uint64_t revision() const noexcept {return revision_;}
private:
    bool initialized_=false;
    uint64_t revision_=0;
    std::array<uint8_t,59> defaults_{};
    Nba97MatchControls live_{};
    std::unique_ptr<MatchSnapshot> snapshot_;
};
}
