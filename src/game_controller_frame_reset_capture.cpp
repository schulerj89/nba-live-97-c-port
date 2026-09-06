#include "game_controller_frame_reset_capture.h"
#include "game_audio_stream_pump_capture.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>
namespace nba97 {
namespace {
struct Fixture {
    const Nba97GameTextMemory* memory;
    unsigned calls=0;
    GameAudioStreamPumpCapture stream;
    void put(std::uint32_t a,std::uint32_t v,unsigned width=4) {
        for(std::size_t n=0;n<memory->count;++n){const auto& r=memory->region[n];
            if(a<r.base || std::uint64_t(a-r.base)+width>r.size)continue;
            for(unsigned i=0;i<width;++i){r.data[a-r.base+i]=std::uint8_t(v>>(8*i));if(r.known)r.known[a-r.base+i]=1;}return;}
        throw std::runtime_error("controller reset fixture mapping missing");
    }
    std::uint32_t get(std::uint32_t a,unsigned width=4) const {
        for(std::size_t n=0;n<memory->count;++n){const auto& r=memory->region[n];
            if(a<r.base || std::uint64_t(a-r.base)+width>r.size)continue;
            std::uint32_t v=0;for(unsigned i=0;i<width;++i){if(r.known && r.known[a-r.base+i]!=1)throw std::runtime_error("controller reset unknown fixture word");v|=std::uint32_t(r.data[a-r.base+i])<<(8*i);}return v;}
        throw std::runtime_error("controller reset fixture mapping missing");
    }
    static int child(void* user,const Nba97GameTextMemory*,const Nba97GameControllerFrameResetEvent* e,Nba97GameControllerFrameResetRegisters* r) {
        auto& f=*static_cast<Fixture*>(user);++f.calls;
        if(e->pc!=0x8006764cu || e->entry!=0x80083eecu || r->gpr[4].word!=8 || r->gpr[3].word!=0x800fdc70u)return 0;
        return f.stream.fromController(f.memory,e,r);
    }
};
}
int GameControllerFrameResetCapture::dispatch(const Nba97GameTextMemory* memory,
    const Nba97MatchTickCall* call,const Nba97GameLatePeriodLimitsProgress* previous) {
    if(!memory || !call || !previous || !previous->completed || !receipt.empty() ||
       previous->registers.gpr[31].known_mask!=15 || previous->registers.gpr[31].word!=0x80068cf4u)return NBA97_BODY_ARGUMENT;
    Fixture f{memory};f.put(0x800fe90eu,1,2);
    for(unsigned i=0;i<8;++i){const auto a=0x80140000u+i*64u;f.put(0x800fdc50u+i*4u,a);f.put(a+0x28u,0xbeef,2);}
    Nba97GameControllerFrameResetTickBinding binding{};binding.memory=*memory;binding.entry_registers=previous->registers;
    // Recovered leaf returns directly to 68CF4 JAL; 68CF8 is NOP. This
    // projects only the source return-address change on the explicit root fixture.
    binding.entry_registers.gpr[31]={0x80068cfcu,15};binding.entry_context_source_proven=1;
    binding.operation_budget=50;binding.io=Fixture::child;binding.user=&f;
    const int result=nba97_game_controller_frame_reset_from_match_tick(&binding,call,nullptr);
    const auto& p=binding.progress;
    if(result!=NBA97_BODY_OK || !p.completed || binding.invocations!=1 || p.operations!=23 || f.calls!=1 ||
       f.get(0x800fe90eu,2)!=0 || p.initial_timer.word!=1 || p.delta.word!=2 || !p.timer_clamped ||
       p.controller_slots_cleared!=8 || p.restored_return_address.word!=0x80068cfcu)
        throw std::runtime_error("controller reset native CPU fixture drifted");
    for(unsigned i=0;i<8;++i)if(f.get(0x80140028u+i*64u,2)!=0)throw std::runtime_error("controller reset retained field drifted");
    std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x800675E4\",\"inclusive_end\":\"0x80067663\",\"bytes\":128,\"instructions\":32,"
      "\"classification\":\"no direct visual effect\",\"scope\":\"recovered limit-leaf output plus adjacent JAL/NOP; explicit root and nested stream-service fixtures, no live tick prologue claim\","
      "\"completed\":true,\"call_pc\":"<<call->pc<<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"calls\":"<<f.calls
     <<",\"child_pc\":2147907148,\"timer_before\":"<<p.initial_timer.word<<",\"delta\":"<<p.delta.word<<",\"timer_after\":"<<f.get(0x800fe90eu,2)
     <<",\"cleared_slots\":"<<unsigned(p.controller_slots_cleared)<<",\"controller_fields\":[";
    for(unsigned i=0;i<8;++i){if(i)o<<',';o<<f.get(0x80140028u+i*64u,2);}
    o<<"],\"frame_stack_pointer\":"<<p.frame_stack_pointer<<",\"restored_ra\":"<<p.restored_return_address.word<<"}";
    auto prefix=o.str();prefix.pop_back();o.str("");o.clear();o<<prefix<<",\"audio_stream_pump\":"<<f.stream.receipts.at(0)<<"}";
    receipt=o.str();return result;
}
}
