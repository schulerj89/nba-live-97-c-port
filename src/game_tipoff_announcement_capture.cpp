#include "game_tipoff_announcement_capture.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace nba97 {
namespace {
struct Fixture {
    const Nba97GameTextMemory* memory;
    std::vector<std::uint32_t> pcs,args;
    unsigned selections=0,values=0;
    void put(std::uint32_t a,std::uint32_t v,unsigned width=4) {
        for(std::size_t n=0;n<memory->count;++n){const auto& r=memory->region[n];
            if(a<r.base || std::uint64_t(a-r.base)+width>r.size)continue;
            for(unsigned i=0;i<width;++i){r.data[a-r.base+i]=std::uint8_t(v>>(8*i));if(r.known)r.known[a-r.base+i]=1;}return;}
        throw std::runtime_error("announcement fixture mapping missing");
    }
    static int child(void* user,const Nba97GameTextMemory*,const Nba97GameTipoffAnnouncementEvent* e,Nba97GameTipoffAnnouncementRegisters* r) {
        auto& f=*static_cast<Fixture*>(user);f.pcs.push_back(e->pc);
        // Runtime-generated speech service results; no audible output is claimed.
        std::uint32_t result=e->pc^0x24681357u;
        if(e->entry==0x800887e8u)result=8;
        if(e->entry==0x8007eea8u)result=0x80190000u;
        if(e->entry==0x8007fa9cu)result=0x80180000u+(f.selections++)*0x100u;
        if(e->entry==0x80083748u)result=f.values++?0x30u:0xfffffff0u;
        if(e->entry==0x8007ececu || e->entry==0x8007e8c4u)
            for(unsigned i=0;i<e->argument_count;++i)f.args.push_back(r->gpr[4+i].word);
        r->gpr[2]={result,15};return 1;
    }
};
}
int GameTipoffAnnouncementCapture::dispatch(const Nba97GameTextMemory* memory,
    const Nba97GameFirstPeriodStartupEvent* e,Nba97GameFirstPeriodStartupRegisters* r,unsigned mode) {
    if(!memory || !e || !r || !receipt.empty() || (mode!=1 && mode!=2))return 0;
    Fixture f{memory};f.put(0x80021d70u,mode,1);f.put(0x8001ec94u,1);
    f.put(0x80021d74u,0x80111100u);f.put(0x80021d78u,0x80122200u);
    Nba97GameTipoffAnnouncementBinding binding{};binding.operation_budget=100;binding.io=Fixture::child;binding.user=&f;
    const int result=nba97_game_tipoff_announcement_from_first_period_startup(&binding,memory,e,r);
    const auto& p=binding.progress;
    const std::vector<std::uint32_t> expected=mode==2?std::vector<std::uint32_t>{0x80180100u,0x80180200u,0x20u,0x80190000u}:
        std::vector<std::uint32_t>{0x80180000u,0x80180100u,5};
    if(result!=1 || !p.completed || binding.invocations!=1 || f.args!=expected ||
       p.operations!=(mode==2?23u:16u) || f.pcs.size()!=(mode==2?12u:6u) ||
       p.restored_return_address.word!=0x80067458u)
        throw std::runtime_error("announcement native CPU fixture drifted");
    std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x8007EF4C\",\"inclusive_end\":\"0x8007F073\",\"bytes\":296,\"instructions\":74,"
      "\"classification\":\"no direct visual effect\",\"scope\":\"actual first-period caller and announcement adapter; synthetic speech service results, no audible playback\","
      "\"completed\":true,\"call_pc\":"<<e->pc<<",\"mode\":"<<mode<<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"call_pcs\":[";
    for(std::size_t i=0;i<f.pcs.size();++i){if(i)o<<',';o<<f.pcs[i];}
    o<<"],\"announcement_args\":[";for(std::size_t i=0;i<f.args.size();++i){if(i)o<<',';o<<f.args[i];}
    o<<"],\"frame_stack_pointer\":"<<p.frame_stack_pointer<<",\"restored_ra\":"<<p.restored_return_address.word<<"}";
    receipt=o.str();return 1;
}
}
