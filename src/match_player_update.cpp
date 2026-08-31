#include "match_player_update.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace nba97 {
namespace {
Nba97GamePlayerUpdateState updateView(const MatchRuntimeState& s) {
    Nba97GamePlayerUpdateState out{};
    for(unsigned i=0;i<11;++i) {
        out.entity_table[i]=s.entity_table[i];
        for(unsigned f=0;f<NBA97_UPDATE_FIELD_COUNT;++f)
            out.entity[i][f]=s.entity[i].record.read(nba97_game_player_update_offset(f),
                                                   nba97_game_player_update_width(f));
    }
    out.current_fdc3c=s.current_fdc3c;out.team_fdc40=s.team_fdc40;
    out.flags_fe8c4=s.scalar[NBA97_PERIOD_FE8C4];return out;
}
Nba97GamePlayerPhysicsState physicsView(const MatchRuntimeState& s,unsigned entity) {
    Nba97GamePlayerPhysicsState out{};
    for(unsigned f=0;f<NBA97_PHYSICS_ENTITY_COUNT;++f)
        out.entity[f]=s.entity[entity].record.read(nba97_game_physics_entity_offset(f),
                                                 nba97_game_physics_entity_width(f));
    const int period[]={NBA97_PERIOD_FDBCC,NBA97_PERIOD_FE8E2,-1,NBA97_PERIOD_FE8E0,
        NBA97_PERIOD_FDB90,NBA97_PERIOD_FE8CC,NBA97_PERIOD_FE8C4,NBA97_PERIOD_FE8BC,
        -1,NBA97_PERIOD_FDB58,-1,NBA97_PERIOD_FDB94,NBA97_PERIOD_FE882,-1};
    static_assert(sizeof(period)/sizeof(*period)==NBA97_PHYSICS_GLOBAL_COUNT);
    for(unsigned f=0;f<NBA97_PHYSICS_GLOBAL_COUNT;++f)
        if(period[f]>=0)out.global[f]=s.scalar[unsigned(period[f])];
    out.global[NBA97_PHYSICS_FDB6C]=s.simulation_tick6c;
    out.global[NBA97_PHYSICS_FDBD4]=s.auxiliary[MatchRuntimeResetD4];
    out.global[NBA97_PHYSICS_21D8F]=s.rule_three_seconds21d8f;
    out.global[NBA97_PHYSICS_FE910]=s.flags_fe910;
    //6801C sets this actual reference at entry and slot5. It does not derive
    // the context from entity+D9, even when table entries alias the other side.
    if(s.team_fdc40.known==1 && s.team_fdc40.record<2)
        out.team_direction10=s.team[s.team_fdc40.record].read(0x10,4);
    return out;
}
struct Run {
    MatchRuntimeState& state;
    const GameplayAnimationResource& resources;
    MatchPlayerUpdateResult& result;
    unsigned imported=0;
    void importWrites() {
        // Import stores rather than the entire typed view. Unwritten partially
        // known fields must retain their original byte provenance.
        while(imported<result.receipt.count) {
            const auto& e=result.receipt.event[imported++];
            switch(e.kind) {
            case NBA97_UPDATE_ENTITY_WRITE:
                state.entity[e.entity].record.write(nba97_game_player_update_offset(e.field),
                    nba97_game_player_update_width(e.field),e.value);break;
            case NBA97_UPDATE_CURRENT_REFERENCE:state.current_fdc3c=e.reference;break;
            case NBA97_UPDATE_TEAM_REFERENCE:state.team_fdc40=e.reference;break;
            case NBA97_UPDATE_FLAGS_WRITE:state.scalar[NBA97_PERIOD_FE8C4]=e.value;break;
            default:break;
            }
        }
    }
    int physics(unsigned slot,unsigned entity) {
        auto view=physicsView(state,entity);
        auto& receipt=result.physics[slot];
        result.dependency_result=nba97_game_player_physics(&view,&resources->physicsView(),nullptr,nullptr,&receipt);
        for(unsigned i=0;i<receipt.count;++i) {
            const auto& e=receipt.event[i];
            if(e.kind==0)state.entity[entity].record.write(nba97_game_physics_entity_offset(e.field),
                nba97_game_physics_entity_width(e.field),e.value);
            else if(e.kind==1) {
                if(e.field!=NBA97_PHYSICS_FE882)throw std::runtime_error("unowned physics global write");
                state.scalar[NBA97_PERIOD_FE882]=e.value;
            } else if(!e.completed) {
                result.pending_owner=int(e.call.owner);
                result.detail="6CFE0 requires its actual synchronous rule/audio owner";
            }
        }
        if(result.dependency_result==NBA97_PHYSICS_OK)return 1;
        return result.dependency_result==NBA97_PHYSICS_CALLBACK_PENDING?0:-1;
    }
    static int callback(void* context,Nba97GamePlayerUpdateState* view,
                        const Nba97GamePlayerUpdateCall* call) {
        auto& run=*static_cast<Run*>(context);
        run.importWrites();int completed;
        // The original saved S0 survives both calls. A reread of current_fdc3c
        // or entity_table here would change source behavior after a mutation.
        if(call->owner==NBA97_UPDATE_CALL_579FC) {
            auto& animation=run.result.animation[call->slot];
            animation=advanceMatchRuntimeAnimation(run.state,call->entity,run.resources);
            run.result.dependency_result=animation.result;
            completed=animation.published?1:-1;
            if(completed<0)run.result.detail=animation.detail;
        } else completed=run.physics(call->slot,call->entity);
        *view=updateView(run.state);return completed;
    }
};
}
MatchPlayerUpdateResult updateMatchRuntimePlayers(MatchRuntimeState& live,
                                                  const GameplayAnimationResource& resources) {
    MatchPlayerUpdateResult result;
    try {
        if(!live.accepted || !live.setup || !resources || resources->setup()!=live.setup)
            throw std::runtime_error("player update resource generation differs from owned match setup");
        // Stage only the mutable records/globals consumed by these owners.
        // Accepted rosters and immutable assets are retained, never recopied
        // ten times (or even once) for each simulation update.
        MatchRuntimeState candidate;
        candidate.accepted=live.accepted;candidate.setup=live.setup;
        candidate.entity=live.entity;candidate.team=live.team;
        candidate.entity_table=live.entity_table;candidate.scalar=live.scalar;
        candidate.auxiliary=live.auxiliary;candidate.simulation_tick6c=live.simulation_tick6c;
        candidate.current_fdc3c=live.current_fdc3c;candidate.team_fdc40=live.team_fdc40;
        candidate.flags_fe910=live.flags_fe910;candidate.rule_three_seconds21d8f=live.rule_three_seconds21d8f;
        auto view=updateView(candidate);Run run{candidate,resources,result};
        result.result=nba97_game_player_update(&view,Run::callback,&run,&result.receipt);
        run.importWrites();
        if(result.result!=NBA97_PLAYER_UPDATE_OK) {
            if(result.detail.empty())result.detail="player update needs a known source field or owned table reference";
            return result;
        }
        live.entity=std::move(candidate.entity);live.scalar=candidate.scalar;
        live.current_fdc3c=candidate.current_fdc3c;live.team_fdc40=candidate.team_fdc40;
        result.published=true;
    } catch(const std::exception& e) {result.detail=e.what();}
    return result;
}
}
