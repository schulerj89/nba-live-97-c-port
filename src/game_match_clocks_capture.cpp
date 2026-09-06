#include "game_match_clocks_capture.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace nba97 {
namespace {
struct Fixture {
    const Nba97GameTextMemory* memory;
    std::vector<std::uint32_t> pcs,args;
    void put(std::uint32_t a,std::uint32_t v,unsigned width=4) {
        for(std::size_t n=0;n<memory->count;++n){const auto& r=memory->region[n];
            if(a<r.base || std::uint64_t(a-r.base)+width>r.size)continue;
            for(unsigned i=0;i<width;++i){r.data[a-r.base+i]=std::uint8_t(v>>(8*i));if(r.known)r.known[a-r.base+i]=1;}return;}
        throw std::runtime_error("match clocks fixture mapping missing");
    }
    std::uint32_t get(std::uint32_t a,unsigned width=4) const {
        for(std::size_t n=0;n<memory->count;++n){const auto& r=memory->region[n];
            if(a<r.base || std::uint64_t(a-r.base)+width>r.size)continue;
            std::uint32_t v=0;for(unsigned i=0;i<width;++i){if(r.known && r.known[a-r.base+i]!=1)throw std::runtime_error("match clocks unknown fixture");v|=std::uint32_t(r.data[a-r.base+i])<<(8*i);}return v;}
        throw std::runtime_error("match clocks fixture mapping missing");
    }
    static int child(void* user,const Nba97GameTextMemory*,const Nba97GameMatchClocksEvent* e,Nba97GameMatchClocksMachine* m) {
        auto& f=*static_cast<Fixture*>(user);f.pcs.push_back(e->pc);f.args.push_back(m->registers.gpr[4].word);
        if(e->entry!=0x80029258u && e->entry!=0x8007f9c4u)return 0;
        // Explicit effect-service fixture. No audio or render work is implied.
        m->registers.gpr[2]={e->pc^0x13572468u,15};return 1;
    }
};
}
int GameMatchClocksCapture::dispatch(const Nba97GameTextMemory* memory,const Nba97MatchTickCall* call,unsigned phase) {
    if(!memory || !call || !receipt.empty() || call->args[0]!=22)return NBA97_BODY_ARGUMENT;
    Fixture f{memory};
    f.put(0x800fdb58u,7200);f.put(0x800fdba4u,180);f.put(0x800fdb90u,phase,2);
    f.put(0x800fe882u,0,2);f.put(0x80021d90u,255,1);f.put(0x800fdb5cu,0);f.put(0x800fdb60u,0);
    f.put(0x80021d92u,1,1);f.put(0x8001eeb4u,1,2);f.put(0x8001ef78u,0,2);
    f.put(0x8001eeb6u,0xaaaa,2);f.put(0x8001ef7au,0xbbbb,2);f.put(0x800fdb86u,0xbeef,2);
    Nba97GameMatchClocksBinding b{};b.memory=*memory;b.operation_budget=100;b.entry_machine_ready=1;b.io=Fixture::child;b.user=&f;
    // The legacy tick does not expose all intermediate GPR/HI/LO state.
    // This independently supplied fixture checks its real argument and JAL RA.
    for(unsigned i=0;i<32;++i)b.entry_machine.registers.gpr[i]={i?0x44000000u+i:0u,15};
    b.entry_machine.registers.gpr[4]={call->args[0],15};b.entry_machine.registers.gpr[29]={0x801fff00u,15};
    // 68D54 sign-extends s0; the 68D58 JAL delay copies that s0 to a0.
    b.entry_machine.registers.gpr[16]={call->args[0],15};
    b.entry_machine.registers.gpr[31]={call->pc+8u,15};
    b.entry_machine.hi={0,0};b.entry_machine.lo={0,0};
    const int result=nba97_game_match_clocks_from_match_tick(&b,call,nullptr);const auto& p=b.progress;
    const bool paused=phase==0x81;
    if(result!=NBA97_BODY_OK || !p.completed || b.invocations!=1 || p.restored_return_address.word!=call->pc+8u ||
       f.get(0x800fdb58u)!=(paused?7200u:7178u) || f.get(0x800fdba4u)!=(phase==0?158u:180u) ||
       f.get(0x8001eeb4u,2)!=(paused?1u:0xffebu) || f.get(0x8001ef7au,2)!=(paused?0xbbbbu:2u))
        throw std::runtime_error("match clocks native CPU fixture drifted");
    std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x80067A60\",\"inclusive_end\":\"0x80067D37\",\"bytes\":728,\"instructions\":182,"
      "\"classification\":\"no direct visual effect\",\"scope\":\"actual tick adapter with independent synthetic machine and clock fixture; no live tick prologue or advancing simulation\","
      "\"completed\":true,\"call_pc\":"<<call->pc<<",\"phase\":"<<phase<<",\"delta\":"<<call->args[0]<<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"call_pcs\":[";
    for(std::size_t i=0;i<f.pcs.size();++i){if(i)o<<',';o<<f.pcs[i];}
    o<<"],\"call_args\":[";for(std::size_t i=0;i<f.args.size();++i){if(i)o<<',';o<<f.args[i];}
    o<<"],\"main_before\":7200,\"main_after\":"<<f.get(0x800fdb58u)<<",\"shot_before\":180,\"shot_after\":"<<f.get(0x800fdba4u)
     <<",\"team_timers\":["<<f.get(0x8001eeb4u,2)<<','<<f.get(0x8001ef78u,2)<<"],\"team_states\":["<<f.get(0x8001eeb6u,2)<<','<<f.get(0x8001ef7au,2)
     <<"],\"signal\":"<<f.get(0x800fdb86u,2)<<",\"multiply_count\":"<<p.multiply_count<<",\"frame_stack_pointer\":"<<p.frame_stack_pointer<<",\"restored_ra\":"<<p.restored_return_address.word<<"}";
    progress=p;receipt=o.str();return result;
}
}
