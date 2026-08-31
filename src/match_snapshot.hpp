#pragma once
#include "match_controls.hpp"
#include "roster_ratings.hpp"
#include "frontend_settings.hpp"
#include "recovered/match_setup.h"
#include "recovered/match_strategy.h"
#include "recovered/team_header.h"
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
    MatchExtensionSettings=1, MatchPresentationVariant=2, MatchCreatedMembership=4,
    MatchStrategyFields=8, MatchTeamReferenceWords=16
};
struct MatchStrategyState {
    Nba97MatchStrategy values{};
    bool known=false; // Whole group: cold init/writeback owns all fourteen bytes.
    uint64_t writeback_revision=0; // Native provenance, not a source field.
};
enum class MatchTeamStage { Unprepared, After655B0Before65328 };
struct MatchTeamInitialization {
    MatchTeamStage stage=MatchTeamStage::Unprepared;
    // Source table20BEC[12]/[24] overlap fixed profile slot0. They are not
    // roster bounds. Unknown words remain unknown; opaque words never become
    // native pointers. Side/entity IDs resolve only against this snapshot.
    Nba97TeamHeaderRef table12{},table24{};
    std::array<Nba97TeamHeaderEffects,2> teams{};
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
    Nba97MatchPresentation presentation{}; // FEONLY46D24 -> resident80021DF4.
    std::array<uint32_t,6> frontend_rng_before{},frontend_rng_after{};
    MatchControlResult controls;
    MatchStrategyState strategy;
    MatchTeamInitialization team_initialization;
    Nba97CreatedPlayerCatalog created{}; // retained, never treated as accepted membership.
    uint64_t roster_generation=0,profile_generation=0,created_generation=0;
    uint32_t pending=MatchExtensionSettings|MatchPresentationVariant|MatchStrategyFields|MatchTeamReferenceWords;
};
// Pure preparation: owns every published value, no I/O or source mutation.
// Unsupported branches throw explicitly; a captured subset is not launch-ready.
MatchSnapshot buildMatchSnapshot(const MatchRequest&,const MatchSourceView&,
    const Nba97MatchControls& live,const std::array<uint8_t,59>& defaults,
    const std::array<uint32_t,6>& frontend_rng,const MatchStrategyState& strategy);
std::string matchSnapshotReceipt(const MatchSnapshot&);

class MatchSession {
public:
    // Once per fresh native host. Repeating identical initialization is a no-op;
    // it must not replace maps retained from a previous successful handoff.
    void initializeFresh(const std::array<uint8_t,59>& defaults);
    bool initialized() const noexcept {return initialized_;}
    // Consumes the caller's existing frontend RNG only after preparation and
    // allocation succeed. Never installs a new seed on entry or confirmation.
    const MatchSnapshot& capture(const MatchRequest&,const MatchSourceView&,
                                std::array<uint32_t,6>& frontend_rng);
    const MatchSnapshot* snapshot() const noexcept {return snapshot_.get();}
    const Nba97MatchControls& liveControls() const noexcept {return live_;}
    const MatchStrategyState& liveStrategy() const noexcept {return strategy_;}
    // A future warm-state importer must invalidate an unproven group. Calling
    // initializeFresh again cannot silently replace it with cold defaults.
    void invalidateStrategy() noexcept {strategy_.known=false;}
    // Field-only 67930 boundary for actual post-gameplay headers. No current
    // host gameplay return calls this; stats/resource cleanup remain separate.
    // Native revision guard refuses values from a different accepted match.
    // Pass the actual live launch halfword: 67930 rereads it on exit, rather
    // than assuming it still equals the original accepted snapshot value.
    void writebackStrategy(uint64_t snapshot_revision,uint16_t live_launch_control,
                           const std::array<Nba97MatchTeamStrategy,2>& current);
    uint64_t revision() const noexcept {return revision_;}
private:
    bool initialized_=false;
    uint64_t revision_=0;
    std::array<uint8_t,59> defaults_{};
    Nba97MatchControls live_{};
    MatchStrategyState strategy_;
    std::unique_ptr<MatchSnapshot> snapshot_;
};
}
