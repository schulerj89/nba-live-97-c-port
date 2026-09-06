#include "game_actor_resume_capture.h"
#include "game_actor_resume_adapter.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace nba97 {
namespace {
constexpr std::uint32_t Actor=0x80160000u,Ball=0x80161000u,Nested=0x80162000u;
struct Fixture {
    std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x200000);
    std::vector<std::uint8_t> known=std::vector<std::uint8_t>(0x200000,1);
    Nba97GameTextRegion region{0x80000000u,bytes.data(),known.data(),bytes.size()};
    std::vector<std::uint32_t> pcs;
    void put(std::uint32_t a,std::uint32_t v,unsigned w=4){
        for(unsigned i=0;i<w;++i)bytes.at(a-0x80000000u+i)=std::uint8_t(v>>(8*i));
    }
    std::uint32_t get(std::uint32_t a,unsigned w=4)const{
        std::uint32_t v=0;for(unsigned i=0;i<w;++i)v|=std::uint32_t(bytes.at(a-0x80000000u+i))<<(8*i);return v;
    }
    static int child(void* u,const Nba97GameTextMemory*,const Nba97GameActorResumeEvent* e,Nba97GameActorResumeMachine* m){
        auto& f=*static_cast<Fixture*>(u);f.pcs.push_back(e->pc);
        if(e->pc==0x80058374u){
            if(e->entry!=0x80056ffcu || e->argument_count!=2 || m->registers.gpr[4].word!=Actor || m->registers.gpr[5].word!=1)return 0;
        }else if(e->pc==0x8005837cu){
            if(e->entry!=0x8005703cu || e->argument_count!=1 || m->registers.gpr[4].word!=Actor)return 0;
        }else if(e->pc==0x800583e0u){
            if(e->entry!=0x800582ccu || e->argument_count || f.get(Actor+0xb8,2)!=47 || f.get(Actor+0xa6,2)!=0x1234)return 0;
        }else return 0;
        // Explicit animation/reset backend response, not an animation renderer.
        m->registers.gpr[2]={e->pc^0x13572468u,15};return 1;
    }
};
}
std::string captureGameActorResumePeriod(){
    Fixture f;Nba97GamePeriodExpiryContext z{};z.memory={&f.region,1};z.operation_budget=100;
    Nba97GameActorResumeBinding a{};a.operation_budget=22;a.io=Fixture::child;a.user=&f;
    z.io=nba97_game_actor_resume_from_period_expiry;z.user=&a;
    // Independent zero-clock fixture exercises the actual parent call; it
    // does not overwrite the primary clock/violation chain or resume live play.
    for(unsigned i=0;i<32;++i)z.machine.registers.gpr[i]={i?0x44000000u+i:0u,15};
    z.machine.registers.gpr[29]={0x801fff00u,15};z.machine.registers.gpr[31]={0x80068d74u,15};
    z.machine.hi={0x12345678u,15};z.machine.lo={0x9abcdef0u,15};
    f.put(0x800fdb58u,0);f.put(0x800fdbccu,0,2);f.put(0x800fdc34u,Actor);
    f.put(0x800fdb90u,0x82,2);f.put(0x800fe880u,1,2);f.put(0x800fdb94u,0,2);
    f.put(Actor+0x1a,27,1);f.put(Actor+0xd9,1,1);f.put(Actor+0x46,37,2);f.put(Actor+0x4a,36,2);
    f.put(Actor+0x4e,0xbeef,2);f.put(Actor+0x60,0,2);f.put(Actor+0x64,0,2);
    f.put(Actor+0x20,Nested);f.put(Nested+0xd,1,1);f.put(Actor+0x9a,0xbeef,2);
    f.put(Actor+0xa2,0x1234,2);f.put(Actor+0xa6,0xbeef,2);f.put(Actor+0xb8,0xbeef,2);
    f.put(0x800fdc48u,Ball);f.put(Ball+0x10,49u<<8);f.put(Ball+0x18,0,2);
    Nba97GamePeriodExpiryProgress p{};const int rc=nba97_game_period_expiry(&z,&p);const auto& actor=a.progress;
    if(rc!=NBA97_TEXT_COMPLETE || !p.completed || a.invocations!=1 || !actor.completed || f.pcs.size()!=3 ||
       f.get(Actor+0x1a,1)!=1 || f.get(Actor+0x4e,2)!=0 || f.get(Actor+0x9a,2)!=3 ||
       f.get(Actor+0xb8,2)!=47 || f.get(Actor+0xa6,2)!=0x1234 || f.get(Actor+0xb4,2)!=30 ||
       f.get(0x800fdbccu,2)!=0xffff || f.get(0x800fdb90u,2)!=0 || f.get(0x800fdc34u)!=Ball)
        throw std::runtime_error("actor resume native CPU fixture drifted");
    std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x800582DC\",\"inclusive_end\":\"0x800583FB\",\"bytes\":288,\"instructions\":72,"
        "\"classification\":\"no direct visual effect\",\"scope\":\"actual period-expiry call; independent zero-clock actor fixture and explicit animation services\","
        "\"completed\":true,\"call_pc\":"<<a.event.pc<<",\"operations\":"<<actor.operations<<",\"reads\":"<<actor.reads<<",\"stores\":"<<actor.stores
     <<",\"actor\":"<<Actor<<",\"state_before\":27,\"state_after\":"<<f.get(Actor+0x1a,1)<<",\"animation_before\":[37,36],\"cleared_4e\":"<<f.get(Actor+0x4e,2)
     <<",\"flags_9a\":"<<f.get(Actor+0x9a,2)<<",\"field_b8\":"<<f.get(Actor+0xb8,2)<<",\"copied_a6\":"<<f.get(Actor+0xa6,2)<<",\"call_pcs\":[";
    for(std::size_t i=0;i<f.pcs.size();++i){if(i)o<<',';o<<f.pcs[i];}
    o<<"],\"frame_stack_pointer\":"<<actor.frame_stack_pointer<<",\"returned_sp\":"<<actor.machine.registers.gpr[29].word
     <<",\"restored_ra\":"<<actor.restored_return_address.word<<",\"parent_completed\":true,\"parent_returned_value\":"<<p.machine.registers.gpr[2].word
     <<",\"parent_restored_ra\":"<<p.restored_return_address.word<<",\"parent_phase\":"<<f.get(0x800fdb90u,2)<<",\"parent_owner\":"<<f.get(0x800fdbccu,2)
     <<",\"parent_actor\":"<<f.get(0x800fdc34u)<<",\"parent_actor_timer\":"<<f.get(Actor+0xb4,2)<<"}";
    return o.str();
}
}
