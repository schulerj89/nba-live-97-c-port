#include "game_match_service_publish_capture.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>
namespace nba97 {
namespace {
struct Fixture {
    const Nba97GameTextMemory* memory;
    unsigned calls=0;
    std::uint32_t phase=0;
    void put(std::uint32_t a,std::uint32_t v,unsigned width=4) {
        for(std::size_t n=0;n<memory->count;++n){const auto& r=memory->region[n];
            if(a<r.base || std::uint64_t(a-r.base)+width>r.size)continue;
            for(unsigned i=0;i<width;++i){r.data[a-r.base+i]=std::uint8_t(v>>(8*i));if(r.known)r.known[a-r.base+i]=1;}return;}
        throw std::runtime_error("match service publication fixture mapping missing");
    }
    std::uint32_t get(std::uint32_t a,unsigned width=4) const {
        for(std::size_t n=0;n<memory->count;++n){const auto& r=memory->region[n];
            if(a<r.base || std::uint64_t(a-r.base)+width>r.size)continue;
            std::uint32_t v=0;for(unsigned i=0;i<width;++i){if(r.known && r.known[a-r.base+i]!=1)throw std::runtime_error("publication unknown fixture");v|=std::uint32_t(r.data[a-r.base+i])<<(8*i);}return v;}
        throw std::runtime_error("match service publication fixture mapping missing");
    }
    static int child(void* user,const Nba97GameTextMemory*,const Nba97GameMatchServicePublishEvent* e,Nba97GameMatchServicePublishMachine* m) {
        auto& f=*static_cast<Fixture*>(user);
        if(e->pc!=0x8002de5cu || e->entry!=0x8002a264u || e->argument_count ||
           f.get(0x80015028u,2)!=0xffffu || f.get(0x800170bcu)!=f.phase)return 0;
        ++f.calls;
        // Explicit remaining audio-state service, not a recovered child claim.
        m->registers.gpr[2]={0x13572468u,15};m->registers.gpr[3]={0x24681357u,15};return 1;
    }
};
}
int GameMatchServicePublishCapture::dispatch(const Nba97GameTextMemory* memory,const Nba97MatchTickCall* call,const Nba97GamePeriodExpiryProgress* previous) {
    if(!memory || !call || !previous || !previous->completed || !receipt.empty() ||
       previous->machine.registers.gpr[31].known_mask!=15 || previous->machine.registers.gpr[31].word!=0x80068d74u ||
       previous->machine.registers.gpr[2].known_mask!=15 || previous->machine.registers.gpr[2].word!=0)return NBA97_BODY_ARGUMENT;
    Fixture f{memory};f.phase=f.get(0x800fdb90u,2);
    // Only this status source and publication destinations are additional
    // fixtures; the prior clock/violation/expiry output and machine stay live.
    f.put(0x800f9ffeu,0xffffu,2);f.put(0x80015028u,0xbeefu,2);f.put(0x800170bcu,0xdeadbeefu);
    Nba97GameMatchServicePublishBinding b{};b.memory=*memory;b.operation_budget=20;b.entry_machine_ready=1;b.io=Fixture::child;b.user=&f;
    b.entry_machine=previous->machine;
    // 68D74 BNE falls through with its NOP; 68D7C JAL then 68D80 NOP.
    b.entry_machine.registers.gpr[31]={0x80068d84u,15};
    const int result=nba97_game_match_service_publish_from_match_tick(&b,call,nullptr);const auto& p=b.progress;
    if(result!=NBA97_BODY_OK || !p.completed || b.invocations!=1 || p.operations!=7 || p.reads!=3 || p.stores!=3 || f.calls!=1 ||
       p.restored_return_address.word!=0x80068d84u || f.get(0x80015028u,2)!=0xffffu || f.get(0x800170bcu)!=f.phase)
        throw std::runtime_error("match service publication native CPU fixture drifted");
    std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x8002DE34\",\"inclusive_end\":\"0x8002DE73\",\"bytes\":64,\"instructions\":16,"
      "\"classification\":\"no direct visual effect\",\"scope\":\"actual period-expiry output through branch/JAL/NOP; explicit status and audio-service fixtures\","
      "\"completed\":true,\"call_pc\":"<<call->pc<<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores
     <<",\"child_calls\":"<<f.calls<<",\"child_pc\":2147671644,\"status_before\":48879,\"status_after\":"<<f.get(0x80015028u,2)
     <<",\"phase_before\":3735928559,\"phase_after\":"<<f.get(0x800170bcu)<<",\"child_v0\":"<<p.child_return_v0.word
     <<",\"child_v1\":"<<p.child_return_v1.word<<",\"frame_stack_pointer\":"<<p.frame_stack_pointer<<",\"restored_ra\":"<<p.restored_return_address.word<<"}";
    progress=p;receipt=o.str();return result;
}
}
