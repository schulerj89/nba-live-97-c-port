#include "game_match_hot_start_capture.h"
#include "game_match_hot_start_adapter.h"
#include "game_camera_startup_capture.h"
#include <algorithm>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace nba97 {
namespace {
struct Fixture {
    std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x200000);
    Nba97GameTextRegion region{0x80000000u,bytes.data(),nullptr,bytes.size()};
    Nba97GameMatchHotStartContext context{};
    Nba97GameMatchHotStartProgress progress{};
    Nba97GameMatchHotStartTickAdapter adapter{};
    GameCameraStartupCapture camera;
    unsigned loads=0,calls=0;
    void put(std::uint32_t a,std::uint32_t v,unsigned width=4) {
        for(unsigned i=0;i<width;++i)bytes.at(a-0x80000000u+i)=std::uint8_t(v>>(8*i));
    }
    std::uint32_t get(std::uint32_t a,unsigned width=4) const {
        std::uint32_t v=0;for(unsigned i=0;i<width;++i)v|=std::uint32_t(bytes.at(a-0x80000000u+i))<<(8*i);return v;
    }
    static int child(void* user,const Nba97GameTextMemory*,const Nba97GameMatchHotStartEvent* e,Nba97GameMatchInitializeRegisters* r) {
        auto& f=*static_cast<Fixture*>(user);++f.calls;
        if(e->entry==0x800a72bcu) {
            if(r->gpr[4].word!=0x800275b8u || r->gpr[5].word!=0x800c6400u)return 0;
            r->gpr[2]={++f.loads==1?0u:0x80130000u,15};return 1;
        }
        if(e->entry!=0x80051ed8u || r->gpr[4].word!=0x80122000u)return 0;
        if(r->gpr[5].word!=(e->pc==0x80067034u?0x4eu:0xf3u))return 0;
        r->gpr[2]={0x12345678u,15};return 1;
    }
    static int service(void* user,const Nba97MatchTickCall* call,Nba97GamePeriodValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        if(call->pc==0x80068c2cu){const Nba97GameTextMemory m{&f.region,1};return f.camera.dispatch(&m,call,&f.progress);}
        return nba97_game_match_hot_start_dispatch_tick(&f.adapter,call,value);
    }
    static int access(void*,std::uint32_t,std::uint32_t,unsigned,unsigned,Nba97PlayerFrameValue*) {return NBA97_BODY_ARGUMENT;}
};
}
std::string captureGameMatchHotStart() {
    Fixture f;
    f.context.memory={&f.region,1};f.context.operation_budget=1024;
    for(unsigned i=0;i<32;++i)f.context.registers.gpr[i]={i?0x11000000u+i:0u,15};
    f.context.registers.gpr[29]={0x801fff00u,15};f.context.registers.gpr[31]={0x80068c2cu,15};
    f.context.io=Fixture::child;f.context.user=&f;
    f.put(0x80020becu,0x80121000u);f.put(0x80121000u,0x80122000u);
    f.put(0x80121020u,0x80123000u);f.put(0x80123009u,0xf3,1);f.put(0x8002148cu,0xffff,2);
    std::vector<unsigned> expected;
    unsigned prefix=0;
    for(unsigned i=0;i<84;++i) {
        const auto left=(i%3)?0x80120000u+i*16u:0u;
        const auto right=(i%4)?0x80120800u+i*16u:0u;
        const auto a=(i*13u)&255u,b=(255u-i*3u)&255u;
        f.put(0x8001ec98u+i*4u,left);f.put(0x800170c8u+i*4u,right);
        if(left)f.put(left+7,a,1);if(right)f.put(right+7,b,1);
        expected.push_back(prefix&65535u);prefix+=std::max(left?a:0u,right?b:0u);
    }
    f.adapter.hot_start_context=&f.context;f.adapter.hot_start_progress=&f.progress;
    Nba97MatchTickContext tick{};tick.access=Fixture::access;tick.service=Fixture::service;tick.user=&f;tick.operation_budget=1024;
    Nba97MatchTickProgress tp{};const auto result=nba97_game_match_tick(&tick,&tp);
    if(result!=NBA97_MATCH_TICK_SERVICE_REQUIRED || tp.stopped_pc!=0x80068c4cu || tp.stopped_entry!=0x80067468u ||
       !f.progress.completed || f.loads!=2 || f.calls!=4 || f.progress.prefixes_written!=84 ||
       f.get(0x800fe91cu)!=0x80130000u || f.get(0x800d7af8u)!=1 || f.get(0x8002148cu,2)!=0)
        throw std::runtime_error("hot-start native CPU fixture drifted");
    for(unsigned i=0;i<84;++i)if(f.get(0x800fe920u+i*2u,2)!=expected[i])throw std::runtime_error("hot-start prefix drifted");
    std::ostringstream o;
    o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x80066F88\",\"inclusive_end\":\"0x800670A7\",\"bytes\":288,\"instructions\":72,"
       "\"classification\":\"no direct visual effect\",\"scope\":\"production tick adapter with explicit synthetic full-GPR entry and runtime-generated hot-data; not live tick stack continuation\","
       "\"completed\":true,\"operations\":"<<f.progress.operations<<",\"reads\":"<<f.progress.reads<<",\"stores\":"<<f.progress.stores<<
       ",\"calls\":"<<f.calls<<",\"retry_attempts\":"<<f.loads<<",\"prefixes_written\":84,\"prefixes\":[";
    for(unsigned i=0;i<84;++i){if(i)o<<',';o<<expected[i];}
    o<<"],\"hot_pointer\":"<<f.get(0x800fe91cu)<<",\"load_flag\":"<<f.get(0x800d7af8u)<<",\"cleared_halfword\":"<<f.get(0x8002148cu,2)<<
       ",\"frame_stack_pointer\":"<<f.progress.frame_stack_pointer<<",\"restored_ra\":"<<f.progress.restored_return_address.word<<
       ",\"final_v0\":"<<f.progress.registers.gpr[2].word<<",\"next_pc\":"<<tp.stopped_pc<<",\"next_entry\":"<<tp.stopped_entry<<
       ",\"simulation_steps\":"<<tp.simulation_steps<<",\"frame_pumps\":"<<tp.frame_pumps<<",\"camera_startup\":"<<f.camera.receipt<<"}";
    return o.str();
}
}
