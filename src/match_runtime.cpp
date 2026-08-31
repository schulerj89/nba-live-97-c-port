#include "match_runtime.hpp"
#include "recovered/game_player_bindings.h"
#include "recovered/game_team_roles.h"
#include "recovered/game_lineup_recovery.h"
#include "recovered/game_controller_selection.h"
#include "recovered/game_period_dependencies.h"
#include <algorithm>
#include <limits>

namespace nba97 {
namespace {
constexpr unsigned entity_fields[]={0,6,8,12,16,20,22,24,0xac,0xb4,0xba,0xd9};
constexpr unsigned team_fields[]={0x34,0x35,0x10};
constexpr unsigned animation_fields[]={0x46,0x48,0x4a,0x4c,0x4e,0x50,0x54,0x58,
                                      0x5c,0x60,0x64,0x70,0x78,0x94,0x96,0x9a};
std::uint32_t require(Nba97GamePeriodValue v) {
    if(v.known!=1) throw std::runtime_error("required original field is unknown");
    return v.word;
}
unsigned resolve(Nba97GamePeriodReference r,unsigned count) {
    if(!r.known || r.record>=count) throw std::runtime_error("unresolved owned reference");
    return r.record;
}
std::int32_t signed32(std::uint32_t v) {
    return v<=0x7fffffffu?std::int32_t(v):-1-std::int32_t(~v);
}
std::int16_t signed16(std::uint32_t v) {
    return std::int16_t(v<0x8000u?std::int32_t(v):std::int32_t(v)-65536);
}
Nba97GameAnimationState animation(const MatchRuntimeRecord<244>& e) {
    Nba97GameAnimationState a{};
    for(unsigned i=0;i<16;++i) {
        const auto v=e.read(animation_fields[i],2);
        a.field[i]=std::uint16_t(v.word);a.known|=std::uint16_t(v.known<<i);
    }
    const auto height=e.read(0x10,4);a.height_known=height.known;
    a.height10=signed32(height.word); return a;
}
Nba97GamePeriodState exportPeriod(const MatchRuntimeState& s) {
    Nba97GamePeriodState p{};
    std::copy(s.scalar.begin(),s.scalar.end(),p.scalar);
    for(unsigned i=0;i<2;++i) for(unsigned f=0;f<3;++f)
        p.team[i][f]=s.team[i].read(team_fields[f],f==2?4:1);
    for(unsigned i=0;i<11;++i) {
        for(unsigned f=0;f<NBA97_PERIOD_ENTITY_FIELD_COUNT;++f)
            p.entity[i][f]=s.entity[i].record.read(entity_fields[f],nba97_game_period_entity_width(f));
        p.entity_table[i]=s.entity_table[i];p.render_table[i]=s.render_table[i];
    }
    for(unsigned i=0;i<8;++i) {
        p.controller22[i]=s.controller[i].read(0x22,2);p.controller_table[i]=s.controller_table[i];
    }
    p.ball_fdc48=s.ball;p.reference_fdc34=s.reference34;p.incoming_s6=s.incoming_s6;
    return p;
}
void importPeriod(MatchRuntimeState& s,const Nba97GamePeriodState& p) {
    std::copy_n(p.scalar,NBA97_PERIOD_SCALAR_COUNT,s.scalar.begin());
    for(unsigned i=0;i<2;++i) for(unsigned f=0;f<3;++f)
        s.team[i].write(team_fields[f],f==2?4:1,p.team[i][f]);
    for(unsigned i=0;i<11;++i) {
        for(unsigned f=0;f<NBA97_PERIOD_ENTITY_FIELD_COUNT;++f)
            s.entity[i].record.write(entity_fields[f],nba97_game_period_entity_width(f),p.entity[i][f]);
        s.entity_table[i]=p.entity_table[i];s.render_table[i]=p.render_table[i];
    }
    for(unsigned i=0;i<8;++i) {
        s.controller[i].write(0x22,2,p.controller22[i]);s.controller_table[i]=p.controller_table[i];
    }
    s.ball=p.ball_fdc48;s.reference34=p.reference_fdc34;s.incoming_s6=p.incoming_s6;
}
struct Run {
    MatchRuntimeState& state;
    MatchRuntimePeriodResult& result;
    int bindings() {
        auto& s=state;
        std::vector<std::uint8_t> heights;
        for(const auto& p:s.players) heights.push_back(p.height_inches);
        Nba97GamePlayerBindingsInput in{};
        in.player_byte9=heights.data();in.player_count=heights.size();
        for(unsigned side=0;side<2;++side) for(unsigned i=0;i<12;++i) {
            in.lineup[side][i]=std::uint16_t(require(s.team[side].read(0x16+i*2,2)));
            in.player_reference[side][i]=s.player_alias[side][i];
        }
        for(unsigned i=0;i<10;++i) {
            const auto& e=s.entity[i].record;
            in.entity_table[i]=std::uint8_t(resolve(s.entity_table[i],10));
            in.entity[i].binding_index=require(e.read(0,4));
            in.entity[i].opponent_slot=std::uint16_t(require(e.read(0xd6,2)));
            in.entity[i].side_byte=std::uint8_t(require(e.read(0xd9,1)));
        }
        Nba97GamePlayerBindingsEffects out{};
        const int result_=nba97_game_player_bindings(&out,&in);
        result.dependency_result=result_;
        if(result_!=NBA97_PLAYER_BINDINGS_READY && result_!=NBA97_PLAYER_BINDINGS_DIVIDE_TRAP) return -1;
        for(unsigned i=0;i<10;++i) {
            s.active_player[i]={out.player_reference[i],1};s.active_status[i]={out.status_reference[i],1};
            auto& e=s.entity[i];const auto& effect=out.entity[i];
            if(effect.written&NBA97_BINDING_WORD38)e.record.put(0x38,2,effect.word38);
            if(effect.written&NBA97_BINDING_STATUS1C) {e.status={effect.status_reference,1};e.record.write(0x1c,4,{});}
            if(effect.written&NBA97_BINDING_PLAYER20) {e.player={effect.player_reference,1};e.record.write(0x20,4,{});}
            if(effect.written&NBA97_BINDING_SCALEC6)e.record.put(0xc6,2,effect.scale_c6);
            if(effect.written&NBA97_BINDING_INVERSEC8)e.record.put(0xc8,2,effect.inverse_c8);
            if(effect.written&NBA97_BINDING_OPPONENTCC) {
                e.opponent={effect.opponent_cc,1};e.record.put(0xcc,2,effect.opponent_cc);
            }
        }
        for(unsigned side=0;side<2;++side) for(unsigned i=0;i<12;++i)
            s.team[side].put(0x80+i*2,2,out.inverse_lineup[side][i]);
        // Preserve the original zero-height trap and its prefix. The outer
        // transaction refuses publication; it must not execute the four tails.
        if(result_==NBA97_PLAYER_BINDINGS_DIVIDE_TRAP) {
            result.detail="original646A8 zero-height divide trap"; return -1;
        }
        std::vector<Nba97GameRolePlayer> players;
        for(const auto& p:s.players) players.push_back({p.ratings[0],p.ratings[1],p.ratings[9]});
        Nba97GameTeamRolesInput roles{}; roles.players=players.data();roles.player_count=players.size();
        roles.incoming_t6=0x8001f984u;roles.incoming_t6_known=1; // Actual646A8 carried register.
        for(unsigned i=0;i<24;++i)roles.status_byte1e[i]=std::uint8_t(require(s.status[i].read(0x1e,1)));
        for(unsigned i=0;i<10;++i) {
            roles.active_player_reference[i]=std::uint16_t(require(s.active_player[i]));
            roles.entity_table[i]=in.entity_table[i];const auto& e=s.entity[i];
            roles.entity[i]={require(e.record.read(0,4)),std::uint16_t(require(e.record.read(0xd6,2))),
                std::uint16_t(require(e.player)),std::uint8_t(require(e.status))};
        }
        Nba97GameTeamRolesEffects effect{};
        result.dependency_result=nba97_game_team_roles(&effect,&roles);
        if(result.dependency_result!=NBA97_TEAM_ROLES_OK)return -1;
        for(unsigned side=0;side<2;++side) {
            auto& team=s.team[side];const auto& t=effect.team[side];
            for(unsigned i=0;i<5;++i) {team.put(0x5c+i,1,t.order5c[i]);team.put(0xbb+i,1,t.orderbb[i]);}
            team.put(0x61,1,t.field61);team.put(0xa6,2,t.fielda6);team.put(0xa8,2,t.fielda8);
        }
        for(unsigned i=0;i<10;++i) {
            const auto& e=effect.entity[i];
            if(e.written&NBA97_ROLE_D4)s.entity[i].record.put(0xd4,2,e.fieldd4);
            if(e.written&NBA97_ROLE_CB)s.entity[i].record.put(0xcb,1,e.fieldcb);
        }
        return 1;
    }
    int recovery(std::int32_t elapsed) {
        auto& s=state; Nba97GameLineupRecoveryState r{};
        for(unsigned side=0;side<2;++side) {
            const auto& t=s.team[side];auto& out=r.team[side];
            for(unsigned i=0;i<12;++i) {
                out.lineup[i]=std::uint16_t(require(t.read(0x16+i*2,2)));
                out.inverse[i]=std::uint16_t(require(t.read(0x80+i*2,2)));
            }
            for(unsigned i=0;i<5;++i)out.preferred[i]=std::uint16_t(require(t.read(0x98+i*2,2)));
            out.recovery_count=std::uint16_t(require(t.read(0x66,2)));
            out.human_count=std::uint16_t(require(t.read(0x42,2)));
            out.automatic=std::uint8_t(require(t.read(0x76,1)));
        }
        for(unsigned i=0;i<24;++i)r.status[i]=std::uint16_t(require(s.status[i].read(0x20,2)));
        r.marker=std::uint16_t(require(s.auxiliary[MatchRuntimeMarker8e]));
        r.substitution_lock=std::uint16_t(require(s.auxiliary[MatchRuntimeLock54]));
        // No callback success is fabricated. An actual requested substitution
        // stops here until its full live context/callees are composed.
        result.dependency_result=nba97_game_lineup_recover(&r,elapsed,nullptr,nullptr);
        if(result.dependency_result!=NBA97_RECOVERY_OK) {
            result.detail="lineup recovery requires an uncomposed substitution owner";return 0;
        }
        for(unsigned side=0;side<2;++side) {
            auto& t=s.team[side];const auto& in=r.team[side];
            for(unsigned i=0;i<12;++i) {
                t.put(0x16+i*2,2,in.lineup[i]);t.put(0x80+i*2,2,in.inverse[i]);
            }
            t.put(0x66,2,in.recovery_count);
        }
        for(unsigned i=0;i<24;++i)s.status[i].put(0x20,2,r.status[i]);
        s.auxiliary[MatchRuntimeMarker8e]={r.marker,1};
        s.auxiliary[MatchRuntimeLock54]={r.substitution_lock,1};return 1;
    }
    int initializePlayers(const Nba97GamePeriodCall& call) {
        auto& s=state; const unsigned side=call.side==0?0:1;
        const auto& t=s.team[side];Nba97GamePlayerInitializationInput in{};
        in.side_word=std::uint16_t(require(t.read(0x14,2)));
        in.period=signed16(require(s.scalar[NBA97_PERIOD_FDB68]));
        in.direction10=signed32(require(t.read(0x10,4)));in.special_center=call.argument;
        in.duration=require(s.scalar[NBA97_PERIOD_FDB58]);
        const auto cumulative=t.read(0x48,4),previous=t.read(0xb4,2),header32=t.read(0x32,2);
        in.previous_cumulative48=cumulative.word;in.cumulative_known=cumulative.known;
        in.sum_known=std::uint8_t(previous.known && header32.known);
        in.previous_b4=in.sum_known?std::uint16_t(previous.word):0;
        in.header32=in.sum_known?std::uint16_t(header32.word):0;
        const auto& formation=s.setup->formation(call.formation);
        for(unsigned i=0;i<5;++i) {
            std::copy(formation[i].begin(),formation[i].end(),in.formation[i]);
            if(in.side_word+i>=10)throw std::runtime_error("player initialization physical span");
            const auto& e=s.entity[in.side_word+i];const auto ref=require(e.player);
            if(ref>=s.players.size())throw std::runtime_error("player initialization binding");
            in.player_byte_d[i]=s.players[ref].hand();in.player_byte_d_known[i]=1;
            in.previous_animation[i]=animation(e.record);
        }
        for(unsigned channel=0;channel<2;++channel)in.motion0[channel]=s.setup->motionView(channel,0);
        Nba97GamePlayerInitializationEffects out{};
        result.dependency_result=nba97_game_player_initialize(&out,&in);
        if(result.dependency_result!=NBA97_PLAYER_INIT_OK)return -1;
        for(unsigned b=0;b<196;++b)if(out.header_written[b])s.team[side].put(b,1,out.header_value[b]);
        for(const auto& e:out.entity) {
            if(e.entity_index>=10 || e.table_slot>=11)throw std::runtime_error("initializer reference");
            auto& record=s.entity[e.entity_index].record;
            for(unsigned b=0;b<244;++b)if(e.written[b])record.write(b,1,{e.value[b],e.known[b]});
            s.render_table[e.table_slot]={e.entity_index,1};
        }
        return 1;
    }
    int selection(const Nba97GamePeriodCall& call) {
        auto& s=state;Nba97GameSelectionInput in{};
        for(unsigned i=0;i<8;++i) {
            in.controller_table[i]=std::uint8_t(resolve(s.controller_table[i],8));
            in.controller[i].team_base=signed16(require(s.controller[i].read(0x24,2)));
            const auto selected=s.controller[i].read(0x26,2);
            in.controller[i].selected={std::uint16_t(selected.word),selected.known};
        }
        for(unsigned i=0;i<11;++i) {
            in.entity_table[i]=std::uint8_t(resolve(s.entity_table[i],11));const auto& e=s.entity[i].record;
            in.entity[i]={signed16(require(e.read(4,2))),signed32(require(e.read(8,4))),signed32(require(e.read(12,4)))};
        }
        in.ball=std::uint8_t(resolve(s.ball,11));
        in.tail_state=signed16(require(s.scalar[NBA97_PERIOD_FE8CC]));
        // FE8CA is not consumed at all when FE8CC is0. The zero here is an
        // unused API argument, never a store or a claim about original state.
        if(in.tail_state)in.tail_entity=signed16(require(s.auxiliary[MatchRuntimeTail8ca]));
        in.incoming_s6={call.incoming_s6.word,call.incoming_s6.known};
        Nba97GameSelectionEffects out{};
        result.dependency_result=nba97_game_controller_selection(&out,&in);
        if(result.dependency_result!=NBA97_SELECTION_OK)return -1;
        if(out.call_7a36c) {result.detail="controller tail requires7A36C";return 0;}
        for(unsigned i=0;i<8;++i)if(out.selected_written[i])
            s.controller[i].write(0x26,2,{out.selected[i].word,out.selected[i].known});
        for(unsigned i=0;i<11;++i)if(out.claim_written[i])s.entity[i].record.put(4,2,std::uint16_t(out.claim[i]));
        if(out.tail_state_written)s.scalar[NBA97_PERIOD_FE8CC]={std::uint16_t(out.tail_state),1};return 1;
    }
    int sort() {
        auto& s=state;Nba97GameRenderSortState in{};
        for(unsigned i=0;i<11;++i) {in.render_table[i]=s.render_table[i];in.x[i]=s.entity[i].record.read(8,4);in.index06[i]=s.entity[i].record.read(6,2);}
        Nba97GameRenderSortEffects out{};
        result.dependency_result=nba97_game_period_sort_render(&out,&in);
        if(result.dependency_result!=NBA97_PERIOD_DEPENDENCY_OK)return -1;
        for(unsigned i=0;i<out.count;++i) {
            const auto& e=out.write[i];
            if(e.field==NBA97_RENDER_SORT_TABLE)s.render_table[e.record]={std::uint8_t(e.value),e.known};
            else s.entity[e.record].record.write(6,2,{e.value,e.known});
        }
        return 1;
    }
    int reset() {
        auto& s=state;Nba97GamePeriodResetEffects out{};
        result.dependency_result=nba97_game_period_reset_phase(&out,&s.scalar[NBA97_PERIOD_FDB90]);
        if(result.dependency_result!=NBA97_PERIOD_DEPENDENCY_OK)return -1;
        s.scalar[NBA97_PERIOD_FDB90]=out.phase;
        for(unsigned i=0;i<4;++i)s.auxiliary[MatchRuntimeResetD4+i]={out.field[i],1};return 1;
    }
    int motion(const Nba97GamePeriodCall& call) {
        if(call.entity>=11)throw std::runtime_error("motion entity reference");
        auto& record=state.entity[call.entity].record;Nba97GamePeriodMotionInput in{};
        in.previous=animation(record);in.request=std::uint32_t(call.argument);
        in.header_index=in.request&0x3fffffffu;
        // Out-of-directory requests remain unresolved and may still short-circuit
        // in the original setter. Do not reject an unconsumed header up front.
        for(unsigned channel=0;channel<2;++channel) {
            if(in.header_index<84)in.motion[channel]=state.setup->motionView(channel,in.header_index);
            else in.motion[channel].available=2;
        }
        Nba97GamePeriodMotionEffects out{};
        result.dependency_result=nba97_game_period_switch_motion(&out,&in);
        if(result.dependency_result!=NBA97_PERIOD_DEPENDENCY_OK)return -1;
        for(unsigned i=0;i<out.count;++i) {
            const auto& w=out.write[i];record.write(animation_fields[w.field],2,{w.value,w.known});
        }
        return 1;
    }
    static int callback(void* context,Nba97GamePeriodState* period,const Nba97GamePeriodCall* call) noexcept {
        auto& run=*static_cast<Run*>(context);
        try {
            importPeriod(run.state,*period);run.result.pending_owner=call->owner;int completed=0;
            switch(call->owner) {
            case NBA97_PERIOD_CALL_646A8:completed=run.bindings();break;
            case NBA97_PERIOD_CALL_65140:completed=run.recovery(call->argument);break;
            case NBA97_PERIOD_CALL_65B18:completed=run.initializePlayers(*call);break;
            case NBA97_PERIOD_CALL_653E8:completed=run.selection(*call);break;
            case NBA97_PERIOD_CALL_60EF8:completed=run.sort();break;
            case NBA97_PERIOD_CALL_5828C:completed=run.reset();break;
            case NBA97_PERIOD_CALL_56B78:completed=run.motion(*call);break;
            }
            *period=exportPeriod(run.state);
            if(completed==1)run.result.pending_owner=-1;return completed;
        } catch(const std::exception& e) {run.result.detail=e.what();return -1;}
        catch(...) {run.result.detail="native match dependency failure";return -1;}
    }
};
}

MatchRuntimeState prepareMatchRuntime(const MatchSnapshot& snapshot,GameplaySetupResource setup,const MatchRuntimeEntry& entry) {
    if(!setup || snapshot.team_initialization.stage!=MatchTeamStage::After655B0Before65328 ||
       !snapshot.controller_initialization.prepared)throw std::runtime_error("unprepared match snapshot/resources");
    MatchRuntimeState s{};s.accepted=std::make_shared<const MatchSnapshot>(snapshot);s.setup=std::move(setup);
    s.scalar=entry.scalar;s.auxiliary=entry.auxiliary;s.incoming_s6=entry.incoming_s6;
    s.render_flag21498=entry.render_flag21498;
    for(unsigned i=0;i<11;++i)s.entity[i].record=entry.entity[i];
    // Actual659F0 A3A74 clears cover bothC4 headers and24 status records.
    for(auto& t:s.team)t.clearFromSource();for(auto& status:s.status)status.clearFromSource();
    s.scalar[NBA97_PERIOD_21D73]={snapshot.request.setup[0],1};
    s.scalar[NBA97_PERIOD_1EDEC]={snapshot.launch_control,1};
    s.scalar[NBA97_PERIOD_1EDF2]={0,1}; // Explicit659F0 halfword store.
    for(unsigned side=0;side<2;++side) {
        const auto& accepted=snapshot.teams[side];auto& t=s.team[side];
        const unsigned base=unsigned(s.players.size());
        if(base+accepted.players.size()>65535)throw std::runtime_error("owned player index capacity");
        s.players.insert(s.players.end(),accepted.players.begin(),accepted.players.end());
        for(unsigned i=0;i<12;++i) {
            const auto alias=accepted.indices.alias[i];
            if(alias>=accepted.players.size())throw std::runtime_error("unowned accepted player alias");
            s.player_alias[side][i]=std::uint16_t(base+alias);
            t.put(0x16+i*2,2,accepted.indices.initial_lineup[i]);
        }
        t.put(0,2,accepted.id);t.put(0x14,2,side*5);
        const auto& e=snapshot.team_initialization.teams[side];
        s.team_word08[side]=e.word08;s.team_word0c[side]=e.word0c;
        // Address fields are represented by stable owner/side/index references,
        // never by pointer-shaped integers in the byte record.
        for(unsigned offset:{4u,8u,12u,0x6cu,0x7cu})t.write(offset,4,{});
        t.put(0x10,4,std::uint32_t(e.direction10));t.put(0x34,1,e.field34);
        t.put(0x38,1,e.field38);t.put(0x39,1,e.field39);t.put(0x62,2,e.field62);
        t.put(0x66,2,e.count66);t.put(0x68,2,e.count68);t.put(0x72,2,e.field72);t.put(0x74,2,e.field74);
        for(unsigned i=0;i<5;++i)t.put(0x98+i*2,2,e.saved_lineup[i]);
        for(unsigned i=0;i<12;++i)s.status[side*12+i].put(0x20,2,e.status[i]);
        for(const auto& entity:e.entity) {
            if(entity.table_slot>=10 || entity.entity_id>=10)throw std::runtime_error("team entity registration");
            s.entity_table[entity.table_slot]={std::uint8_t(entity.entity_id),1};
            s.entity[entity.entity_id].record.put(0xd6,2,entity.opponent_d6);
            // ORIGINAL655B0 does NOT store entity word00 or byteD9 here.
            // Keep the supplied preperiod state;65B18 establishes IDs later.
        }
    }
    const auto& c=snapshot.controller_initialization.effects;
    for(unsigned i=0;i<8;++i) {
        for(unsigned b=0;b<36;++b)s.controller[i].put(b,1,snapshot.controls.controls.stats[i][b]);
        for(unsigned b=0;b<59;++b)s.controller[i].put(0x3c+b,1,snapshot.controls.controls.map[i][b]);
        s.controller[i].put(0x24,2,std::uint16_t(c.team_base[i]));
        s.controller[i].write(0x26,2,{c.selected[i].word,c.selected[i].known});
        s.controller_table[i]={c.controller_binding[i],1};
    }
    for(unsigned i=0;i<10;++i)s.entity[i].record.put(4,2,std::uint16_t(c.player_claim[i]));
    s.auxiliary[MatchRuntimeResetD0]={std::uint16_t(c.marker),1};
    for(unsigned side=0;side<2;++side)s.team[side].put(0x42,2,c.human_count[side]);
    return s;
}

MatchRuntimePeriodResult initializeMatchRuntimePeriod(MatchRuntimeState& live) {
    MatchRuntimePeriodResult result;
    try {
        if(!live.accepted || !live.setup)throw std::runtime_error("missing owned match resources");
        if(live.completed_period_initializations==std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("native period receipt counter exhausted");
        auto candidate=live;auto period=exportPeriod(candidate);Nba97GamePeriodDurations durations{};
        for(unsigned overtime=0;overtime<2;++overtime)for(unsigned i=0;i<256;++i)
            durations.value[overtime][i]=candidate.setup->duration(overtime!=0,std::uint8_t(i));
        Run run{candidate,result};
        result.result=nba97_game_period_initialize(&period,&durations,Run::callback,&run,&result.receipt);
        if(result.result==NBA97_PERIOD_COMPLETE) {
            importPeriod(candidate,period);++candidate.completed_period_initializations;live=std::move(candidate);
        }
    } catch(const std::exception& e) {result.detail=e.what();}
    return result;
}

MatchRuntimeAttributesResult initializeMatchRuntimeAttributes(MatchRuntimeState& live) {
    MatchRuntimeAttributesResult result;
    try {
        if(!live.accepted)throw std::runtime_error("missing accepted match players");
        auto candidate=live;
        std::vector<Nba97GameAttributePlayer> players;
        players.reserve(candidate.players.size());
        for(const auto& p:candidate.players) {
            // Original player offsets: ratings begin at0E; metadata begins1F.
            // Byte20 is metadata[1], not the decimal20 rating field.
            players.push_back({p.height_inches,p.source_metadata[1],p.ratings[16],
                p.ratings[13],p.ratings[6],p.ratings[14],p.ratings[7]});
        }
        Nba97GamePlayerAttributesInput in{};
        in.players=players.data();in.player_count=players.size();
        const auto first=candidate.entity_table[0];
        in.first_entity=first.record;in.first_known=first.known;
        const auto divisor=candidate.scalar[NBA97_PERIOD_FDB64];
        in.divisor64=divisor.word;in.divisor_known=divisor.known;
        const auto flag=candidate.render_flag21498;
        if(flag.word>UINT16_MAX)throw std::runtime_error("render flag exceeds source halfword");
        in.flag21498=std::uint16_t(flag.word);in.flag_known=flag.known;
        for(unsigned i=0;i<11;++i) {
            const auto id=candidate.entity[i].record.read(0,4);
            const auto player=candidate.entity[i].player;
            if(player.word>UINT16_MAX)throw std::runtime_error("player reference exceeds owned index capacity");
            in.entity[i]={id.word,std::uint16_t(player.word),id.known,player.known};
        }
        result.result=nba97_game_player_attributes(&result.effects,&in);
        if(result.result!=NBA97_ATTRIBUTES_COMPLETE) {
            switch(result.result) {
            case NBA97_ATTRIBUTES_TAILS_REQUIRED:
                result.detail="63EDC render tails4D9EC/35A44/38A18 are not composed";break;
            case NBA97_ATTRIBUTES_RATING_DIVIDE_TRAP:
                result.detail="original63EDC rating divide trap; prefix retained in receipt";break;
            case NBA97_ATTRIBUTES_RATE_DIVIDE_TRAP:
                result.detail="original63EDC FDB64 divide trap; prefix retained in receipt";break;
            default:result.detail="63EDC needs valid known fields and owned references";break;
            }
            return result;
        }
        constexpr unsigned offsets[]={0x3a,0x3c,0x3e,0x40,0x42,0x44};
        for(unsigned i=0;i<11;++i) {
            const auto& e=result.effects.entity[i];
            for(unsigned f=0;f<6;++f)if(e.written&(1u<<f))
                candidate.entity[i].record.put(offsets[f],2,e.field[f]);
            if(result.effects.height_written&(1u<<i))
                candidate.player_height165f48[i]={result.effects.height165f48[i],1};
        }
        live=std::move(candidate);result.published=true;
    } catch(const std::exception& e) {result.detail=e.what();}
    return result;
}
}
