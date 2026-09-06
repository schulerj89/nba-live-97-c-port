#include "game_camera_select_capture.h"
#include "game_camera_elapsed_dispatch_capture.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace nba97 {
namespace {
struct Fixture {
    const Nba97GameTextMemory* memory;
    std::vector<std::uint32_t> pcs;
    GameCameraElapsedDispatchCapture elapsed;
    void put(std::uint32_t a,std::uint32_t v,unsigned width=4) {
        for(std::size_t n=0;n<memory->count;++n){const auto& r=memory->region[n];
            if(a<r.base || std::uint64_t(a-r.base)+width>r.size)continue;
            for(unsigned i=0;i<width;++i){r.data[a-r.base+i]=std::uint8_t(v>>(8*i));if(r.known)r.known[a-r.base+i]=1;}return;}
        throw std::runtime_error("camera select fixture mapping missing");
    }
    std::uint32_t get(std::uint32_t a,unsigned width=4) const {
        for(std::size_t n=0;n<memory->count;++n){const auto& r=memory->region[n];
            if(a<r.base || std::uint64_t(a-r.base)+width>r.size)continue;
            std::uint32_t v=0;for(unsigned i=0;i<width;++i){if(r.known && r.known[a-r.base+i]!=1)throw std::runtime_error("camera select unknown fixture word");v|=std::uint32_t(r.data[a-r.base+i])<<(8*i);}return v;}
        throw std::runtime_error("camera select fixture mapping missing");
    }
    static int child(void* user,const Nba97GameTextMemory*,const Nba97GameCameraSelectEvent* e,Nba97GameCameraSelectRegisters* r) {
        auto& f=*static_cast<Fixture*>(user);f.pcs.push_back(e->pc);
        // Explicit camera service fixtures; these do not render or advance the match.
        if(e->entry==0x80079f78u && r->gpr[4].word!=255)return 0;
        if(e->entry==0x800798b4u)return f.elapsed.dispatch(f.memory,e,r);
        if(e->entry==0x8007a3a0u){f.put(0x800bc3d4u,0xffff1234u);f.put(0x800bc3d8u,0x12345678u);f.put(0x800bc3dcu,0x87654321u);}
        r->gpr[2]={e->pc^0x24681357u,15};return 1;
    }
};
}
int GameCameraSelectCapture::dispatch(const Nba97GameTextMemory* memory,
    const Nba97GameCameraStartupEvent* e,Nba97GameCameraStartupRegisters* r) {
    if(!memory || !e || !r || !receipt.empty())return 0;
    Fixture f{memory};f.put(0x80021ed7u,7,1);f.put(0x80021ed8u,255,1);f.put(0x800bc298u,0x80124000u);
    for(unsigned i=0;i<6;++i)f.put(0x80109aa8u+4*i,i==5?256u:0x70000000u+i*16u);
    // Explicit threshold/cache contract for the newly composed timing owner.
    f.put(0x800bc1f8,10);f.put(0x800bc1fc,100);f.put(0x800bc200,1);f.put(0x800bc1f4,0xffffffff);
    Nba97GameCameraSelectStartupBinding binding{};binding.operation_budget=100;binding.io=Fixture::child;binding.user=&f;
    const int result=nba97_game_camera_select_from_camera_startup(&binding,memory,e,r);
    const auto& p=binding.progress;
    if(result!=1 || !p.completed || binding.invocations!=1 || p.operations!=37 || f.pcs.size()!=4 ||
       f.get(0x800fc99cu)!=12 || f.get(0x800fc9d0u)!=0x80124000u || f.get(0x800fa62cu)!=1 ||
       f.get(0x801029f8u,1)!=0 || f.get(0x800bc1f4u)!=0xffffffffu || p.restored_return_address.word!=0x800796c0u)
        throw std::runtime_error("camera select native CPU fixture drifted");
    for(unsigned i=0;i<6;++i)if(f.get(0x800fc9a0u+4*i)!=f.get(0x80109aa8u+4*i) || f.get(0x800fc9b8u+4*i)!=0)throw std::runtime_error("camera select copy/reset drifted");
    std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x800799CC\",\"inclusive_end\":\"0x80079D37\",\"bytes\":876,\"instructions\":219,"
      "\"classification\":\"no direct visual effect\",\"scope\":\"actual camera startup and selector adapter with explicit synthetic camera child services\","
      "\"completed\":true,\"call_pc\":"<<e->pc<<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"call_pcs\":[";
    for(std::size_t i=0;i<f.pcs.size();++i){if(i)o<<',';o<<f.pcs[i];}
    o<<"],\"mode\":"<<f.get(0x800fc99cu)<<",\"selected_pointer\":"<<f.get(0x800fc9d0u)<<",\"force_flag\":"<<f.get(0x800fa62cu)<<",\"busy\":"<<f.get(0x801029f8u,1)<<",\"copied_words\":[";
    for(unsigned i=0;i<6;++i){if(i)o<<',';o<<f.get(0x800fc9a0u+4*i);}
    o<<"],\"cleared_words\":[";for(unsigned i=0;i<6;++i){if(i)o<<',';o<<f.get(0x800fc9b8u+4*i);}
    o<<"],\"frame_stack_pointer\":"<<p.frame_stack_pointer<<",\"restored_ra\":"<<p.restored_return_address.word<<",\"elapsed_dispatch\":"<<f.elapsed.receipt<<"}";
    receipt=o.str();return 1;
}
}
