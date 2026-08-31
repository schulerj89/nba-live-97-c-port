#include "match_snapshot.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace nba97 {
MatchSnapshot buildMatchSnapshot(const MatchRequest& request,const MatchSourceView& source,
        const Nba97MatchControls& live,const std::array<uint8_t,59>& defaults,
        const std::array<uint32_t,6>& frontend_rng) {
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

void MatchSession::initialize(const std::array<uint8_t,59>& defaults) {
    if(initialized_) {
        if(defaults_!=defaults)throw std::runtime_error("match controls: defaults changed after initialization");
        return;
    }
    std::array<int8_t,8> none;none.fill(-2);
    const auto initial=finalizeMatchControls({},none,{},defaults,true);
    defaults_=defaults;live_=initial.controls;initialized_=true;
}
const MatchSnapshot& MatchSession::capture(const MatchRequest& request,const MatchSourceView& source,
                                         std::array<uint32_t,6>& frontend_rng) {
    if(!initialized_)throw std::runtime_error("match controls: cold defaults not initialized");
    if(revision_==(std::numeric_limits<uint64_t>::max)())throw std::runtime_error("match snapshot revision exhausted");
    auto next=std::make_unique<MatchSnapshot>(buildMatchSnapshot(request,source,live_,defaults_,frontend_rng));
    // All validation and allocation finished. Publication cannot fail.
    frontend_rng=next->frontend_rng_after;
    live_=next->controls.controls;snapshot_=std::move(next);++revision_;return *snapshot_;
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
    out<<",\"rng_after\":";array(s.frontend_rng_after);out<<"},\"setup\":";
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
    out<<"]}";return out.str();
}
}
