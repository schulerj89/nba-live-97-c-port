#include "game_clock_violations_capture.h"
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
        throw std::runtime_error("clock violations fixture mapping missing");
    }
    std::uint32_t get(std::uint32_t a,unsigned width=4) const {
        for(std::size_t n=0;n<memory->count;++n){const auto& r=memory->region[n];
            if(a<r.base || std::uint64_t(a-r.base)+width>r.size)continue;
            std::uint32_t v=0;for(unsigned i=0;i<width;++i){if(r.known && r.known[a-r.base+i]!=1)throw std::runtime_error("clock violations unknown fixture");v|=std::uint32_t(r.data[a-r.base+i])<<(8*i);}return v;}
        throw std::runtime_error("clock violations fixture mapping missing");
    }
    static int child(void* user,const Nba97GameTextMemory*,const Nba97GameClockViolationsEvent* e,Nba97GameClockViolationsMachine* m) {
        auto& f=*static_cast<Fixture*>(user);f.pcs.push_back(e->pc);f.args.push_back(e->argument_count?m->registers.gpr[4].word:0u);
        // Explicit remaining effect services, including the narrow legacy
        // audio owner's unavailable full-machine return contract.
        m->registers.gpr[2]={e->pc^0x13572468u,15};return 1;
    }
};
}
int GameClockViolationsCapture::dispatch(const Nba97GameTextMemory* memory,const Nba97MatchTickCall* call,const Nba97GameMatchClocksProgress* previous) {
    if(!memory || !call || !previous || !previous->completed || !receipt.empty() ||
       previous->machine.registers.gpr[31].known_mask!=15 || previous->machine.registers.gpr[31].word!=0x80068d60u)return NBA97_BODY_ARGUMENT;
    Fixture f{memory};const auto phase=f.get(0x800fdb90u,2);
    // Additional rule-field fixtures share the clock owner's retained output.
    f.put(0x800fdbccu,0,2);f.put(0x800fdc34u,0x80150000u);f.put(0x801500a0u,0,2);
    f.put(0x800fe884u,2,2);f.put(0x800fe88eu,0,2);f.put(0x800fe8e0u,0,2);
    f.put(0x800fdba8u,1,2);f.put(0x800fdbaau,1,2);f.put(0x80021d91u,255,1);
    Nba97GameClockViolationsBinding b{};b.memory=*memory;b.operation_budget=100;b.entry_machine_ready=1;b.io=Fixture::child;b.user=&f;
    b.entry_machine=previous->machine;
    b.entry_machine.registers.gpr[4]=b.entry_machine.registers.gpr[16]; // 68D60 MOVE
    b.entry_machine.registers.gpr[31]={0x80068d6cu,15}; // 68D64 JAL, 68D68 NOP
    const int result=nba97_game_clock_violations_from_match_tick(&b,call,nullptr);const auto& p=b.progress;
    if(result!=NBA97_BODY_OK || !p.completed || b.invocations!=1 || p.restored_return_address.word!=0x80068d6cu ||
       f.get(0x800fdba8u,2)!=(phase==0x82?0u:1u) || f.get(0x800fdbaau,2)!=(phase==0x81?1u:0u) ||
       f.get(0x800fe882u,2)!=(phase==0x81?0u:4u))throw std::runtime_error("clock violations native CPU fixture drifted");
    std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x80067D38\",\"inclusive_end\":\"0x8006801B\",\"bytes\":740,\"instructions\":185,"
      "\"classification\":\"no direct visual effect\",\"scope\":\"recovered clock output through adjacent MOVE/JAL/NOP; explicit initial machine, rule fields and effect fixtures\","
      "\"completed\":true,\"call_pc\":"<<call->pc<<",\"phase_before\":"<<phase<<",\"phase_after\":"<<f.get(0x800fdb90u,2)<<",\"delta\":"<<call->args[0]
     <<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"call_pcs\":[";
    for(std::size_t i=0;i<f.pcs.size();++i){if(i)o<<',';o<<f.pcs[i];}
    o<<"],\"call_args\":[";for(std::size_t i=0;i<f.args.size();++i){if(i)o<<',';o<<f.args[i];}
    o<<"],\"timer_before\":[1,1],\"timer_after\":["<<f.get(0x800fdba8u,2)<<','<<f.get(0x800fdbaau,2)<<"],\"violation_state\":"<<f.get(0x800fe882u,2)
     <<",\"triggers\":["<<unsigned(p.first_violation_triggered)<<','<<unsigned(p.phase_82_violation_triggered)<<','<<unsigned(p.final_violation_triggered)
     <<"],\"frame_stack_pointer\":"<<p.frame_stack_pointer<<",\"restored_ra\":"<<p.restored_return_address.word<<"}";
    receipt=o.str();return result;
}
}
