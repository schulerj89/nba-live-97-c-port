#include "game_period_startup_capture.h"
#include "game_period_startup_adapter.h"
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
    std::vector<std::uint32_t> pcs;
    unsigned previous_fixtures=0;
    std::uint32_t pre_pump_counter=0,post_pump_delta=0;
    void put(std::uint32_t a,std::uint32_t v,unsigned width=4) {for(unsigned i=0;i<width;++i)bytes.at(a-0x80000000u+i)=std::uint8_t(v>>(8*i));}
    std::uint32_t get(std::uint32_t a,unsigned width=4) const {std::uint32_t v=0;for(unsigned i=0;i<width;++i)v|=std::uint32_t(bytes.at(a-0x80000000u+i))<<(8*i);return v;}
    static int child(void* user,const Nba97GameTextMemory*,const Nba97GamePeriodStartupEvent* e,Nba97GamePeriodStartupRegisters* r) {
        auto& f=*static_cast<Fixture*>(user);f.pcs.push_back(e->pc);
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
    static int service(void* user,const Nba97MatchTickCall* call,Nba97GamePeriodValue*) {
        auto& f=*static_cast<Fixture*>(user);
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
std::string captureGamePeriodStartup() {
    Fixture f;f.context.memory={&f.region,1};f.context.operation_budget=100;f.context.io=Fixture::child;f.context.user=&f;
    for(unsigned i=0;i<32;++i)f.context.registers.gpr[i]={i?0x11000000u+i:0u,15};
    f.context.registers.gpr[29]={0x801fff00u,15};f.context.registers.gpr[31]={0x80068c54u,15};
    f.put(0x800fdb68u,0x8000,2);f.put(0x80020c14u,0x80123400u);f.put(0x8001edecu,0,2);
    Nba97MatchTickContext tick{};tick.access=Fixture::access;tick.service=Fixture::service;tick.user=&f;tick.operation_budget=100;
    Nba97MatchTickProgress tp{};const auto result=nba97_game_match_tick(&tick,&tp);
    if(result!=NBA97_MATCH_TICK_SERVICE_REQUIRED || !f.progress.completed || f.progress.operations!=23 ||
       f.pcs.size()!=13 || f.previous_fixtures!=2 || f.get(0x800fdc48u)!=0x80123400u ||
       f.pre_pump_counter!=0x4321u || f.post_pump_delta!=0x8765u ||
       f.progress.period_selector.word!=0xffff8000u || f.progress.restored_return_address.word!=0x80068c54u ||
       tp.stopped_pc!=0x80068cecu || tp.stopped_entry!=0x80067550u)
        throw std::runtime_error("period-startup CPU fixture drifted");
    std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x80067468\",\"inclusive_end\":\"0x8006754F\",\"bytes\":232,\"instructions\":58,"
       "\"classification\":\"no direct visual effect\",\"scope\":\"production tick adapter with explicit synthetic full-GPR entry, preceding services and child services; not live tick stack continuation\","
       "\"completed\":true,\"operations\":"<<f.progress.operations<<",\"reads\":"<<f.progress.reads<<",\"stores\":"<<f.progress.stores<<
       ",\"calls\":"<<f.pcs.size()<<",\"call_pcs\":[";
    for(std::size_t i=0;i<f.pcs.size();++i){if(i)o<<',';o<<f.pcs[i];}
    o<<"],\"signed_selector\":"<<f.progress.period_selector.word<<",\"published_pointer\":"<<f.get(0x800fdc48u)<<
       ",\"pre_pump_counter\":"<<f.pre_pump_counter<<",\"post_pump_delta\":"<<f.post_pump_delta<<",\"restored_ra\":"<<f.progress.restored_return_address.word<<
       ",\"frame_stack_pointer\":"<<f.progress.frame_stack_pointer<<",\"next_pc\":"<<tp.stopped_pc<<",\"next_entry\":"<<tp.stopped_entry<<
       ",\"simulation_steps\":"<<tp.simulation_steps<<",\"frame_pumps\":"<<tp.frame_pumps<<"}";
    return o.str();
}
}
