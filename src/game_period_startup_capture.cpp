#include "game_frame_interrupt_restore_capture.h"
#include "game_frame_interrupt_disable_capture.h"
#include "game_camera_frame_transform_capture.h"
#include "game_period_startup_capture.h"
#include "game_actor_resume_capture.h"
#include "game_ball_actor_contact_capture.h"
#include "game_actor_input_capture.h"
#include "game_camera_override_end_capture.h"
#include "game_period_startup_adapter.h"
#include "game_first_period_startup_adapter.h"
#include "game_late_period_limits_adapter.h"
#include "game_tipoff_announcement_capture.h"
#include "game_controller_frame_reset_capture.h"
#include "game_match_clocks_capture.h"
#include "game_clock_violations_capture.h"
#include "game_period_expiry_capture.h"
#include "game_match_service_publish_capture.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace nba97 {
namespace {
struct Fixture {
    std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x200000);
    Nba97GameTextRegion region{0x80000000u,bytes.data(),nullptr,bytes.size()};
    Nba97GamePeriodStartupContext context{};
    Nba97GamePeriodStartupProgress progress{};
    Nba97GamePeriodStartupAdapterProgress adapter{};
    Nba97GameFirstPeriodStartupBinding first{};
    GameTipoffAnnouncementCapture announcement;
    GameControllerFrameResetCapture reset;
    GameMatchClocksCapture clocks;
    GameClockViolationsCapture violations;
    GamePeriodExpiryCapture expiry;
    GameMatchServicePublishCapture publication;
    unsigned clock_phase=0;
    Nba97GameLatePeriodLimitsContext limits_context{};
    Nba97GameLatePeriodLimitsTickBinding limits{};
    std::vector<std::uint32_t> first_pcs;
    std::vector<std::uint32_t> pcs;
    unsigned previous_fixtures=0;
    std::uint32_t pre_pump_counter=0,post_pump_delta=0;
    void put(std::uint32_t a,std::uint32_t v,unsigned width=4) {for(unsigned i=0;i<width;++i)bytes.at(a-0x80000000u+i)=std::uint8_t(v>>(8*i));}
    std::uint32_t get(std::uint32_t a,unsigned width=4) const {std::uint32_t v=0;for(unsigned i=0;i<width;++i)v|=std::uint32_t(bytes.at(a-0x80000000u+i))<<(8*i);return v;}
    static int firstChild(void* user,const Nba97GameTextMemory* memory,const Nba97GameFirstPeriodStartupEvent* e,Nba97GameFirstPeriodStartupRegisters* r) {
        auto& f=*static_cast<Fixture*>(user);f.first_pcs.push_back(e->pc);
        if(e->pc==0x80067450u)return f.announcement.dispatch(memory,e,r,f.get(0x800eb680u,1)?1u:2u);
        if(e->pc==0x80067434u && r->gpr[4].word!=1)return 0;
        // Explicit full-GPR service fixtures: no frame renderer or tip-off child is claimed.
        r->gpr[2]={e->pc^0x24681357u,15};return 1;
    }
    static int child(void* user,const Nba97GameTextMemory* m,const Nba97GamePeriodStartupEvent* e,Nba97GamePeriodStartupRegisters* r) {
        auto& f=*static_cast<Fixture*>(user);f.pcs.push_back(e->pc);
        if(e->pc==0x80067494u)return nba97_game_first_period_startup_from_period_startup(&f.first,m,e,r);
        if(e->pc==0x800674a4u && r->gpr[4].word!=1)return 0;
        if(e->pc==0x800674b8u && (r->gpr[4].word!=1 || r->gpr[5].word!=0xffffffffu))return 0;
        if(e->pc==0x800674c0u) {if(r->gpr[4].word!=15)return 0;r->gpr[16]={0xabcd4321u,15};}
        if(e->pc==0x800674e0u) {
            f.pre_pump_counter=f.get(0x800fdb92u,2);
            if(f.get(0x800fdb92u,2)!=0x4321 || f.get(0x800fdc48u)!=0x80123400u)return 0;
            r->gpr[16]={0x12348765u,15};
        }
        r->gpr[2]={e->pc^0x13572468u,15};return 1;
    }
    static int service(void* user,const Nba97MatchTickCall* call,Nba97GamePeriodValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        if(call->pc==0x80068cecu)return nba97_game_late_period_limits_from_match_tick(&f.limits,call,nullptr);
        if(call->pc==0x80068cf4u)return f.reset.dispatch(&f.context.memory,call,&f.limits.progress);
        if(call->pc==0x80068d58u)return f.clocks.dispatch(&f.context.memory,call,f.clock_phase);
        if(call->pc==0x80068d64u)return f.violations.dispatch(&f.context.memory,call,&f.clocks.progress);
        if(call->pc==0x80068d6cu)return f.expiry.dispatch(&f.context.memory,call,value,&f.violations.progress);
        if(call->pc==0x80068d7cu)return f.publication.dispatch(&f.context.memory,call,&f.expiry.progress);
        // Explicit preceding-service fixtures; they do not establish live GPRs.
        if(call->pc==0x80068c24u || call->pc==0x80068c2cu){++f.previous_fixtures;return NBA97_BODY_OK;}
        if(call->pc!=0x80068c4cu)return NBA97_MATCH_TICK_SERVICE_REQUIRED;
        const Nba97GamePeriodStartupMatchTickContext c{&f.context,1};
        const auto status=nba97_game_period_startup_from_match_tick(call,&c,&f.progress,&f.adapter);
        f.post_pump_delta=f.get(0x800fdb6cu,2);return status;
    }
    static int access(void* user,std::uint32_t,std::uint32_t a,unsigned width,unsigned kind,Nba97PlayerFrameValue* v) {
        auto& f=*static_cast<Fixture*>(user);
        if(!v || a<0x80000000u || std::uint64_t(a-0x80000000u)+width>f.bytes.size())return NBA97_BODY_BOUNDS;
        if(kind==NBA97_FRAME_READ){*v={};v->word=f.get(a,width);v->known_mask=std::uint8_t((1u<<width)-1u);}
        else {if(v->is_reference)return NBA97_BODY_REFERENCE_REQUIRED;f.put(a,v->word,width);}
        return NBA97_BODY_OK;
    }
};
}
static std::string capturePeriodFixture(int first_flag) {
    Fixture f;f.clock_phase=first_flag<0?0u:(first_flag?0x82u:0x81u);f.context.memory={&f.region,1};f.context.operation_budget=100;f.context.io=Fixture::child;f.context.user=&f;
    f.first.operation_budget=30;f.first.io=Fixture::firstChild;f.first.user=&f;
    f.limits_context.memory=f.context.memory;f.limits_context.operation_budget=12;
    // Independent full-GPR entry fixture: the legacy tick does not expose its
    // actual intermediate register file. Memory is shared with period startup.
    for(unsigned i=0;i<32;++i)f.limits_context.registers.gpr[i]={i?0x33000000u+i:0u,15};
    f.limits_context.registers.gpr[29]={0x801fff00u,15};f.limits_context.registers.gpr[31]={0x80068cf4u,15};
    f.limits.limits=&f.limits_context;f.limits.entry_context_source_proven=1;
    for(unsigned i=0;i<32;++i)f.context.registers.gpr[i]={i?0x11000000u+i:0u,15};
    f.context.registers.gpr[29]={0x801fff00u,15};f.context.registers.gpr[31]={0x80068c54u,15};
    f.put(0x800fdb68u,first_flag<0?0x8000:0,2);f.put(0x80020c14u,0x80123400u);f.put(0x8001edecu,0,2);
    f.put(0x800eb680u,first_flag<0?0:unsigned(first_flag),1);f.put(0x800fdb4eu,0xbeef,2);
    f.put(0x8010606cu,0xbeef,2);
    Nba97MatchTickContext tick{};tick.access=Fixture::access;tick.service=Fixture::service;tick.user=&f;tick.operation_budget=100;
    tick.incoming_s6={f.limits_context.registers.gpr[22].word,1};
    Nba97MatchTickProgress tp{};const auto result=nba97_game_match_tick(&tick,&tp);
    if(result!=NBA97_MATCH_TICK_PLAYER_UPDATE_REQUIRED || !f.progress.completed || f.progress.operations!=23 ||
       f.pcs.size()!=13 || f.previous_fixtures!=2 || f.get(0x800fdc48u)!=0x80123400u ||
       f.pre_pump_counter!=0x4321u || f.post_pump_delta!=0x8765u ||
       f.progress.period_selector.word!=(first_flag<0?0xffff8000u:0u) || f.progress.restored_return_address.word!=0x80068c54u ||
       tp.stopped_pc!=0x80068d84u || tp.stopped_entry!=0x8006801cu ||
       f.limits.invocations!=1 || !f.limits.progress.completed || f.limits.progress.operations!=3 ||
       f.get(0x8010606cu,2)!=0)
        throw std::runtime_error("period-startup CPU fixture drifted");
    if(first_flag>=0 && (f.first.invocations!=1 || !f.first.progress.completed ||
       f.first.progress.operations!=(first_flag?12u:9u) || f.first_pcs.size()!=(first_flag?7u:5u) ||
       f.get(0x800fdb94u,2)!=0xffff || f.get(0x800fdb4eu,2)!=(first_flag?0u:0xbeefu) ||
       f.first.progress.restored_return_address.word!=0x8006749cu))
        throw std::runtime_error("first-period startup CPU fixture drifted");
    std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x80067468\",\"inclusive_end\":\"0x8006754F\",\"bytes\":232,\"instructions\":58,"
       "\"classification\":\"no direct visual effect\",\"scope\":\"production tick adapter with explicit synthetic full-GPR entry, preceding services and child services; not live tick stack continuation\","
       "\"completed\":true,\"operations\":"<<f.progress.operations<<",\"reads\":"<<f.progress.reads<<",\"stores\":"<<f.progress.stores<<
       ",\"calls\":"<<f.pcs.size()<<",\"call_pcs\":[";
    for(std::size_t i=0;i<f.pcs.size();++i){if(i)o<<',';o<<f.pcs[i];}
    o<<"],\"signed_selector\":"<<f.progress.period_selector.word<<",\"published_pointer\":"<<f.get(0x800fdc48u)<<
       ",\"pre_pump_counter\":"<<f.pre_pump_counter<<",\"post_pump_delta\":"<<f.post_pump_delta<<",\"restored_ra\":"<<f.progress.restored_return_address.word<<
       ",\"frame_stack_pointer\":"<<f.progress.frame_stack_pointer<<",\"next_pc\":"<<tp.stopped_pc<<",\"next_entry\":"<<tp.stopped_entry<<
       ",\"simulation_steps\":"<<tp.simulation_steps<<",\"frame_pumps\":"<<tp.frame_pumps;
    o<<",\"late_period_limits\":{\"program\":\"GAMEONLY\",\"address\":\"0x80067550\",\"inclusive_end\":\"0x800675E3\",\"bytes\":148,\"instructions\":37,"
      "\"classification\":\"no direct visual effect\",\"scope\":\"actual tick service adapter with independent synthetic full-GPR entry and shared period fixture memory\","
      "\"completed\":true,\"call_pc\":2147912940,\"operations\":"<<f.limits.progress.operations<<",\"reads\":"<<f.limits.progress.reads<<",\"stores\":"<<f.limits.progress.stores
     <<",\"clock\":"<<f.limits.progress.clock.word<<",\"period\":"<<f.limits.progress.period.word<<",\"limit_before\":48879,\"limit_after\":"<<f.get(0x8010606cu,2)
     <<",\"returned_ra\":"<<f.limits.progress.registers.gpr[31].word<<"}";
    o<<",\"controller_frame_reset\":"<<f.reset.receipt;
    o<<",\"match_clocks\":"<<f.clocks.receipt;
    o<<",\"clock_violations\":"<<f.violations.receipt;
    o<<",\"period_expiry\":"<<f.expiry.receipt;
    o<<",\"service_publication\":"<<f.publication.receipt;
    if(first_flag>=0) {
        const auto& p=f.first.progress;
        o<<",\"first_period_startup\":{\"program\":\"GAMEONLY\",\"address\":\"0x800673F0\",\"inclusive_end\":\"0x80067467\",\"bytes\":120,\"instructions\":30,"
          "\"classification\":\"no direct visual effect\",\"scope\":\"actual period startup and child adapter; explicit synthetic full-GPR entry and remaining service fixtures\","
          "\"completed\":true,\"flag\":"<<first_flag<<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"call_pcs\":[";
        for(std::size_t i=0;i<f.first_pcs.size();++i){if(i)o<<',';o<<f.first_pcs[i];}
        o<<"],\"marker\":"<<f.get(0x800fdb94u,2)<<",\"presentation_halfword\":"<<f.get(0x800fdb4eu,2)
         <<",\"frame_stack_pointer\":"<<p.frame_stack_pointer<<",\"restored_ra\":"<<p.restored_return_address.word<<",\"announcement\":"<<f.announcement.receipt<<"}";
    } else o<<",\"zero_period_cases\":["<<capturePeriodFixture(0)<<','<<capturePeriodFixture(255)<<']'
            <<",\"actor_resume_period_probe\":"<<captureGameActorResumePeriod()
            <<",\"ball_actor_contact_probe\":"<<captureGameBallActorContact()
            <<",\"contact_dispatch_probe\":"<<captureGameContactDispatch()
            <<",\"actor_contact_gate_probe\":"<<captureGameActorContactGate()
            <<",\"camera_frame_transform_probe\":"<<captureGameCameraFrameTransform()
            <<",\"frame_interrupt_disable_probe\":"<<captureGameFrameInterruptDisable()
            <<",\"frame_interrupt_restore_probe\":"<<captureGameFrameInterruptRestore()
            <<",\"camera_override_end_probe\":"<<captureGameCameraOverrideEnd()
            <<",\"opponent_contact_probe\":"<<captureGameOpponentContact()
            <<",\"actor_contact_eligibility_probe\":"<<captureGameActorContactEligibility()
            <<",\"ball_acquire_probe\":"<<captureGameBallAcquire()
            <<",\"actor_input_probe\":"<<captureGameActorInput();
    o<<"}";
    return o.str();
}
std::string captureGamePeriodStartup() {return capturePeriodFixture(-1);}
}
