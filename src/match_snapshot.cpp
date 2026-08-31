#include "match_snapshot.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace nba97 {
namespace {
MatchTeamInitialization initializeTeams(const MatchSnapshot& snapshot,
                                        const std::vector<UserProfile>& profiles) {
    MatchTeamInitialization result;
    // finalizeMatchControls has already validated unique fixed slots/IDs.
    // The native STAT schema is not the retail profile prefix, so table12
    // cannot be reconstructed from games/wins. Missing records also remain
    // unknown here: this stage does not adopt a cleared resident profile table.
    for(const auto& profile:profiles) {
        if(profile.slot!=0)continue;
        uint32_t word=0;
        for(unsigned byte=0;byte<4;++byte)
            word|=uint32_t(profile.controls[14+byte])<<(8*byte);
        result.table24.payload=word;
        result.table24.kind=word ? NBA97_TEAM_REF_OPAQUE_WORD:NBA97_TEAM_REF_NULL;
        break; // Validity/selected-controller defaults do not govern this read.
    }
    for(unsigned side=0;side<2;++side) {
        const auto& team=snapshot.teams[side];
        Nba97TeamHeaderInput input{};
        input.side_word=uint16_t(side*5);input.opponent_side_word=uint16_t((1-side)*5);
        input.count=team.indices.count;input.injury_slot=snapshot.injury_slot[side];
        input.difficulty=snapshot.request.setup[3];
        input.rank54=team.metadata[0];input.rank57=team.metadata[3];
        std::copy_n(team.indices.initial_lineup,5,input.lineup);
        input.table12=result.table12;input.table24=result.table24;
        if(!nba97_team_header_initialize(&result.teams[side],&input))
            throw std::runtime_error("match snapshot: team initialization refused");
    }
    // No controller assignments, period preparation or65820 strategy apply
    // has happened here. Those owners may change starters and direction.
    result.stage=MatchTeamStage::After655B0Before65328;
    return result;
}
}
MatchSnapshot buildMatchSnapshot(const MatchRequest& request,const MatchSourceView& source,
        const Nba97MatchControls& live,const std::array<uint8_t,59>& defaults,
        const std::array<uint32_t,6>& frontend_rng,const MatchStrategyState& strategy,
        const MatchControllerSelections& previous_selected) {
    if(!strategy.known)
        throw std::runtime_error("match snapshot pending: unknown persistent team strategy");
    if(request.users.result!=6 || nba97_user_setup_busy(&request.users))
        throw std::runtime_error("match snapshot requires accepted state5 result6");
    if(request.setup[0]>3 || request.setup[1]>2 || request.setup[2]>2 || request.setup[3]>2)
        throw std::runtime_error("match snapshot: invalid Setup choice");
    if(request.setup[1]!=0)
        throw std::runtime_error("match snapshot pending: season/playoff configuration");
    if(request.setup[2]!=source.settings.style())
        throw std::runtime_error("match snapshot: Setup/style source mismatch");
    for(unsigned c=0;c<8;++c) {
        const auto side=request.users.side[c];
        if(side>2 || request.users.assignment[c]!=(side==0 ? 2:side==2 ? 1:0))
            throw std::runtime_error("match snapshot: assignments do not describe accepted local sides");
        if(request.users.profile[c]<-2 || request.users.profile[c]>=20)
            throw std::runtime_error("match snapshot: selector outside state5 domain");
    }
    for(auto id:request.teams)
        if(id>=29)throw std::runtime_error(id<=30 ?
            "match snapshot pending: special-team roster/jersey/metadata":
            "match snapshot: invalid selected team");
    MatchSnapshot result;result.request=request;
    result.strategy=strategy;result.pending&=~MatchStrategyFields;
    result.base_identity=source.rosters.baseIdentity();
    result.accepted_slots=source.rosters.slotTable();
    result.roster_generation=source.roster_generation;
    result.profile_generation=source.profile_generation;
    result.created_generation=source.created_generation;
    result.created=source.created;
    for(unsigned i=0;i<NBA97_CREATED_PLAYER_CAPACITY;++i) {
        if(!nba97_created_player_occupied(&source.created.records[i]))continue;
        if(nba97_created_player_id(&source.created.records[i])!=NBA97_CREATED_PLAYER_FIRST_ID+i)
            throw std::runtime_error("match snapshot: malformed created-player identity");
        result.pending|=MatchCreatedMembership;
    }
    // Recompute from all accepted regular teams, never trust the previous screen
    // cache or overwrite immutable stock metadata used by the save identity.
    result.ranks=calculateRosterRanks(source.rosters,source.rating_adjustments);
    for(unsigned side=0;side<2;++side) {
        auto& out=result.teams[side];out.id=request.teams[side];
        const auto* team=source.rosters.team(out.id);
        if(!team || team->roster.size()!=15)
            throw std::runtime_error("match snapshot: missing ordinary team slots");
        std::copy(team->roster.begin(),team->roster.end(),out.roster.begin());
        out.names={std::string(team->nickname),std::string(team->city),std::string(team->alternate_name),
                   std::string(team->location),std::string(team->abbreviation)};
        out.metadata=team->source_metadata;
        for(unsigned category=0;category<5;++category)out.metadata[category]=result.ranks.value[category][out.id];
        bool empty=false;unsigned count=0;
        for(const auto id:out.roster) {
            if(id==0xffff) {empty=true;continue;}
            if(empty)throw std::runtime_error("match snapshot pending: interior roster hole");
            if(id>=NBA97_CREATED_PLAYER_FIRST_ID)
                throw std::runtime_error("match snapshot pending: accepted created-player membership resolver");
            const auto* player=source.rosters.player(id);
            if(!player)throw std::runtime_error("match snapshot: unresolved current player");
            out.players.push_back(*player);++count;
        }
        if(!nba97_match_roster_indices(&out.indices,count))
            throw std::runtime_error("match snapshot: invalid roster count");
    }
    std::array<int8_t,8> selectors;
    std::copy_n(request.users.profile,8,selectors.begin());
    result.controls=finalizeMatchControls(live,selectors,source.profiles,defaults);
    result.team_initialization=initializeTeams(result,source.profiles);
    // GAME659F0 calls65328 after BOTH655B0 calls. It does not initialize the
    // selected player for joined controllers; preserve raw value/provenance.
    // The later65DB0/653E8 producers remain separate, unexecuted boundaries.
    Nba97GameControllersInput controllers{};
    std::copy_n(request.users.assignment,8,controllers.assignment);
    std::copy(previous_selected.begin(),previous_selected.end(),controllers.previous_selected);
    if(!nba97_game_controllers_initialize(&result.controller_initialization.effects,&controllers))
        throw std::runtime_error("match snapshot: controller initialization refused");
    result.controller_initialization.previous_selected=previous_selected;
    result.controller_initialization.prepared=true;
    if(result.team_initialization.table12.kind!=NBA97_TEAM_REF_UNKNOWN &&
       result.team_initialization.table24.kind!=NBA97_TEAM_REF_UNKNOWN)
        result.pending&=~MatchTeamReferenceWords;
    // Accepted ordinary exhibition: 61674(0), then46D24, then3E7A8.
    // Preparation uses a copy; a refused native snapshot consumes no live RNG.
    result.frontend_rng_before=frontend_rng;
    result.frontend_rng_after=result.frontend_rng_before;
    if(!nba97_match_presentation(&result.presentation,result.frontend_rng_after.data(),0,0))
        throw std::runtime_error("match snapshot: presentation selection refused");
    result.pending&=~MatchPresentationVariant;
    result.rules=source.settings.effectiveRules();result.custom_rules=source.settings.customRules();
    for(unsigned i=0;i<11;++i)result.options[i]=source.settings.option(int(i));
    return result;
}

void MatchSession::initializeFresh(const std::array<uint8_t,59>& defaults) {
    if(initialized_) {
        if(defaults_!=defaults)throw std::runtime_error("match controls: defaults changed after initialization");
        return;
    }
    std::array<int8_t,8> none;none.fill(-2);
    const auto initial=finalizeMatchControls({},none,{},defaults,true);
    MatchStrategyState strategy;
    // FE35D80 checks the full resident word21EE4. This host adopts only a
    // fresh native epoch here; no imported/warm state is inferred as zero.
    if(!nba97_match_strategy_cold(&strategy.values,0))
        throw std::runtime_error("match strategy: fresh initialization refused");
    strategy.known=true;
    defaults_=defaults;live_=initial.controls;strategy_=strategy;initialized_=true;
}
const MatchSnapshot& MatchSession::capture(const MatchRequest& request,const MatchSourceView& source,
                                         std::array<uint32_t,6>& frontend_rng) {
    if(!initialized_)throw std::runtime_error("match controls: cold defaults not initialized");
    if(revision_==(std::numeric_limits<uint64_t>::max)())throw std::runtime_error("match snapshot revision exhausted");
    auto next=std::make_unique<MatchSnapshot>(buildMatchSnapshot(request,source,live_,defaults_,frontend_rng,strategy_,selected_));
    // All validation and allocation finished. Publication cannot fail.
    frontend_rng=next->frontend_rng_after;
    std::copy_n(next->controller_initialization.effects.selected,8,selected_.begin());
    live_=next->controls.controls;snapshot_=std::move(next);++revision_;return *snapshot_;
}
void MatchSession::writebackStrategy(uint64_t snapshot_revision,uint16_t live_launch_control,
                                    const std::array<Nba97MatchTeamStrategy,2>& current) {
    if(!initialized_ || !snapshot_ || snapshot_revision!=revision_)
        throw std::runtime_error("match strategy: writeback requires the current accepted match revision");
    auto next=strategy_;
    if(!nba97_match_strategy_writeback(&next.values,current.data(),live_launch_control))
        throw std::runtime_error("match strategy: writeback refused");
    if(live_launch_control==0) {
        next.known=true;next.writeback_revision=snapshot_revision;
    }
    strategy_=next; // Snapshot, controls, revision and external RNG stay intact.
}
std::string matchSnapshotReceipt(const MatchSnapshot& s) {
    std::ostringstream out;
    auto array=[&](const auto& values) {
        out<<'[';bool comma=false;
        for(const auto n:values) {if(comma)out<<',';out<<+n;comma=true;}
        out<<']';
    };
    out<<"{\"scope\":\"partial ordinary exhibition snapshot\",\"pending\":"<<s.pending<<
        ",\"venue\":"<<unsigned(s.venue_selector)<<",\"launch_control\":"<<unsigned(s.launch_control)<<
        ",\"presentation\":{\"value\":"<<unsigned(s.presentation.value)<<
        ",\"from_schedule\":"<<unsigned(s.presentation.from_schedule)<<
        ",\"rng_draws\":"<<s.presentation.rng_draws<<",\"rejected_draws\":"<<s.presentation.rejected_draws<<
        ",\"rng_before\":";array(s.frontend_rng_before);
    out<<",\"rng_after\":";array(s.frontend_rng_after);
    out<<"},\"strategy\":{\"known\":"<<(s.strategy.known ? "true":"false")<<
        ",\"writeback_revision\":"<<s.strategy.writeback_revision<<",\"side\":[";
    array(s.strategy.values.side[0]);out<<',';array(s.strategy.values.side[1]);out<<"]},\"setup\":";
    array(s.request.setup);out<<",\"assignments\":";array(s.request.users.assignment);
    out<<",\"selectors\":";array(s.request.users.profile);
    out<<",\"controls_source\":";array(s.controls.provenance);
    out<<",\"profile_ids\":";array(s.controls.profile_ids);
    out<<",\"rules\":";array(s.rules);out<<",\"custom_rules\":";array(s.custom_rules);
    out<<",\"options\":[";
    for(unsigned i=0;i<11;++i) {
        if(i)out<<',';
        out<<"{\"address\":"<<nba97_match_option_address(i)<<",\"value\":"<<unsigned(s.options[i])<<'}';
    }
    out<<"],\"roster_generation\":"<<s.roster_generation<<",\"profile_generation\":"<<s.profile_generation<<
        ",\"created_generation\":"<<s.created_generation<<",\"created_count\":"<<nba97_created_count(&s.created)<<
        ",\"base_identity\":";array(s.base_identity);
    out<<",\"teams\":[";
    for(unsigned side=0;side<2;++side) {
        if(side)out<<',';
        const auto& t=s.teams[side];
        out<<"{\"id\":"<<t.id<<",\"count\":"<<unsigned(t.indices.count)<<
            ",\"active\":"<<unsigned(t.indices.active_count)<<",\"ids\":";array(t.roster);
        out<<",\"aliases\":";array(t.indices.alias);out<<",\"lineup\":";array(t.indices.initial_lineup);
        out<<",\"metadata\":";array(t.metadata);out<<'}';
    }
    out<<"],\"team_initialization\":{\"stage\":\""<<
        (s.team_initialization.stage==MatchTeamStage::After655B0Before65328 ?
            "after_655B0_before_65328":"unprepared")<<'"';
    auto reference=[&](const Nba97TeamHeaderRef& ref) {
        out<<"{\"kind\":"<<unsigned(ref.kind)<<",\"value\":"<<ref.payload<<'}';
    };
    out<<",\"table12\":";reference(s.team_initialization.table12);
    out<<",\"table24\":";reference(s.team_initialization.table24);
    out<<",\"teams\":[";
    for(unsigned side=0;side<2;++side) {
        if(side)out<<',';
        const auto& t=s.team_initialization.teams[side];
        out<<"{\"opponent_side\":"<<unsigned(t.opponent_side)<<
            ",\"metadata_side\":"<<unsigned(t.metadata_side)<<",\"alias_side\":"<<unsigned(t.alias_side)<<
            ",\"word08\":";reference(t.word08);out<<",\"word0c\":";reference(t.word0c);
        out<<",\"direction10\":"<<t.direction10<<",\"field34\":"<<unsigned(t.field34)<<
            ",\"field38\":"<<unsigned(t.field38)<<",\"field39\":"<<unsigned(t.field39)<<
            ",\"field62\":"<<t.field62<<",\"count66\":"<<t.count66<<",\"count68\":"<<t.count68<<
            ",\"field72\":"<<t.field72<<",\"field74\":"<<t.field74<<",\"saved_lineup\":";
        array(t.saved_lineup);out<<",\"status\":";array(t.status);out<<",\"entity\":[";
        for(unsigned i=0;i<5;++i) {
            if(i)out<<',';
            out<<'['<<t.entity[i].table_slot<<','<<t.entity[i].entity_id<<','<<t.entity[i].opponent_d6<<']';
        }
        out<<"]}";
    }
    out<<"]},\"controller_initialization\":{\"stage\":\""<<
        (s.controller_initialization.prepared ? "after_65328_before_65DB0":"unprepared")<<'"';
    auto selections=[&](const auto& values) {
        out<<'[';bool comma=false;
        for(const auto& value:values) {
            if(comma)out<<',';comma=true;
            out<<"{\"known\":"<<unsigned(value.known)<<",\"word\":"<<value.word<<'}';
        }
        out<<']';
    };
    const auto& c=s.controller_initialization.effects;
    out<<",\"previous_selected\":";selections(s.controller_initialization.previous_selected);
    out<<",\"selected\":";selections(c.selected);
    out<<",\"selected_written\":";array(c.selected_written);
    out<<",\"team_base\":";array(c.team_base);
    out<<",\"player_claim\":";array(c.player_claim);
    out<<",\"controller_binding\":";array(c.controller_binding);
    out<<",\"human_count\":";array(c.human_count);
    out<<",\"marker\":"<<c.marker<<"}}";return out.str();
}
}
