#include "game_camera_startup_capture.h"
#include "game_camera_select_capture.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>
namespace nba97 {
namespace {
struct Fixture {
    const Nba97GameTextMemory* memory;
    unsigned calls=0;
    std::uint32_t child_pc=0;
    GameCameraSelectCapture selector;
    void put(std::uint32_t a,std::uint32_t v,unsigned width=4) {
        for(std::size_t n=0;n<memory->count;++n){const auto& r=memory->region[n];
            if(a<r.base || std::uint64_t(a-r.base)+width>r.size)continue;
            for(unsigned i=0;i<width;++i){r.data[a-r.base+i]=std::uint8_t(v>>(8*i));if(r.known)r.known[a-r.base+i]=1;}return;}
        throw std::runtime_error("camera capture fixture mapping missing");
    }
    std::uint32_t get(std::uint32_t a,unsigned width=4) const {
        for(std::size_t n=0;n<memory->count;++n){const auto& r=memory->region[n];
            if(a<r.base || std::uint64_t(a-r.base)+width>r.size)continue;
            std::uint32_t v=0;for(unsigned i=0;i<width;++i){if(r.known && r.known[a-r.base+i]!=1)throw std::runtime_error("camera capture unknown publication");v|=std::uint32_t(r.data[a-r.base+i])<<(8*i);}return v;}
        throw std::runtime_error("camera capture publication mapping missing");
    }
    static int child(void* user,const Nba97GameTextMemory* memory,const Nba97GameCameraStartupEvent* e,Nba97GameCameraStartupRegisters* r) {
        auto& f=*static_cast<Fixture*>(user);++f.calls;
        f.child_pc=e->pc;
        if(e->pc!=0x800796b8u || e->entry!=0x800799ccu || r->gpr[4].word!=12 || r->gpr[5].word!=0)return 0;
        return f.selector.dispatch(memory,e,r);
    }
};
}
int GameCameraStartupCapture::dispatch(const Nba97GameTextMemory* memory,
    const Nba97MatchTickCall* call,const Nba97GameMatchHotStartProgress* previous) {
    if(!memory || !call || !previous || !previous->completed || !receipt.empty() ||
       previous->restored_return_address.known_mask!=15 || previous->restored_return_address.word!=0x80068c2cu)
        return NBA97_BODY_ARGUMENT;
    Fixture f{memory};f.put(0x80021ed9u,0xe7,1);f.put(0x80021edau,0x91,1);
    Nba97GameCameraStartupTickBinding binding{};binding.memory=*memory;
    binding.entry_registers=previous->registers;
    // Original tick 68C2C JAL and 68C30 delay are the only instructions
    // between this recovered hot-start return and camera entry. This shares
    // the explicit synthetic root fixture; it does not model tick's prologue.
    binding.entry_registers.gpr[31]={0x80068c34u,15};
    binding.entry_registers.gpr[4]={0,15};
    binding.operation_budget=32;binding.io=Fixture::child;binding.user=&f;
    const auto result=nba97_game_camera_startup_from_match_tick(&binding,call,nullptr);
    const auto& p=binding.progress;
    if(result!=NBA97_BODY_OK || !p.completed || p.operations!=23 || f.calls!=1 ||
       f.get(0x800fa378u,1)!=0xe7 || f.get(0x800fabc4u,1)!=0x91 ||
       f.get(0x801029bcu,1)!=1 || f.get(0x800dce00u)!=0 ||
       f.get(0x80104744u)!=0xffffffffu || f.get(0x800bc258u)!=256 || f.get(0x800fc9b4u)!=256 ||
       f.get(0x800bc1f4u)!=0xffffffffu || p.restored_return_address.word!=0x80068c34u)
        throw std::runtime_error("camera startup native CPU fixture drifted");
    for(const auto a:{0x801042acu,0x801042b0u,0x801042b4u,0x80106074u})if(f.get(a)!=0)throw std::runtime_error("camera reset drifted");
    std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x80079664\",\"inclusive_end\":\"0x80079757\",\"bytes\":244,\"instructions\":61,"
       "\"classification\":\"no direct visual effect\",\"scope\":\"recovered hot-start output plus source JAL/delay register projection and recovered camera selector; explicit synthetic root and remaining camera services, no live tick prologue claim\","
       "\"completed\":true,\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<
       ",\"calls\":"<<f.calls<<",\"call_pc\":"<<call->pc<<",\"child_pc\":"<<f.child_pc<<",\"child_args\":[12,0],\"camera_bytes\":["<<f.get(0x800fa378u,1)<<','<<f.get(0x800fabc4u,1)<<
       "],\"vector\":["<<f.get(0x8010607cu)<<','<<f.get(0x80106080u)<<','<<f.get(0x80106084u)<<"],\"frame_stack_pointer\":"<<p.frame_stack_pointer<<
       ",\"restored_ra\":"<<p.restored_return_address.word<<",\"final_v0\":"<<p.registers.gpr[2].word<<",\"camera_select\":"<<f.selector.receipt<<"}";
    receipt=o.str();return result;
}
}
