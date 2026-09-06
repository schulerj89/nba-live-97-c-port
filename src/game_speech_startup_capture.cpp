#include "game_speech_startup_capture.h"
#include "game_audio_stream_pump_capture.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace nba97 {
namespace {
struct Fixture {
    const Nba97GameTextMemory* memory;
    std::vector<std::uint32_t> pcs;
    GameAudioStreamPumpCapture stream;
    unsigned clocks=0,ready=0,pumps=0;
    std::uint32_t filename=0,stack_argument=0;
    std::uint32_t get(std::uint32_t a) const {
        for(std::size_t n=0;n<memory->count;++n){const auto& r=memory->region[n];
            if(a<r.base || std::uint64_t(a-r.base)+4>r.size)continue;
            std::uint32_t v=0;for(unsigned i=0;i<4;++i){if(r.known && r.known[a-r.base+i]!=1)throw std::runtime_error("speech startup unknown fixture word");v|=std::uint32_t(r.data[a-r.base+i])<<(8*i);}return v;}
        throw std::runtime_error("speech startup fixture mapping missing");
    }
    static int child(void* user,const Nba97GameTextMemory*,const Nba97GameSpeechStartupEvent* e,Nba97GameSpeechStartupRegisters* r) {
        auto& f=*static_cast<Fixture*>(user);f.pcs.push_back(e->pc);
        // Explicit synthetic audio/time services; no audible playback is claimed.
        std::uint32_t result=e->pc^0x24681357u;
        if(e->entry==0x800853f4u){if(r->gpr[4].word!=0x6000 || r->gpr[5].word!=0x2000 || r->gpr[6].word!=0x20)return 0;result=0x80170000u;}
        if(e->entry==0x800859c8u){if(r->gpr[4].word!=0x80170000u)return 0;f.filename=r->gpr[5].word;}
        if(e->entry==0x80029ca0u){f.stack_argument=f.get(r->gpr[29].word+0x10u);if(r->gpr[4].word!=0x80170000u || r->gpr[5].word!=5 || r->gpr[6].word!=10000 || r->gpr[7].word!=0x6000 || f.stack_argument!=1)return 0;}
        if(e->entry==0x80083d38u){if(r->gpr[4].word!=15 || r->gpr[5].word!=0xffffffffu)return 0;result=0x80170100u;}
        if(e->entry==0x800abfbcu && (r->gpr[4].word!=0x80170100u || r->gpr[5].word!=0))return 0;
        if(e->entry==0x800a5810u){result=f.clocks==0?1000u:(f.clocks==1?1240u:1241u);++f.clocks;}
        if(e->entry==0x8008847cu){result=0;++f.ready;}
        if(e->entry==0x80083eecu){++f.pumps;return f.stream.fromSpeech(f.memory,e,r);}
        if(e->entry==0x8002abb4u && (r->gpr[4].word!=0 || r->gpr[5].word!=0))return 0;
        r->gpr[2]={result,15};return 1;
    }
};
}
int GameSpeechStartupCapture::dispatch(const Nba97GameTextMemory* memory,
    const Nba97GameSceneRandomWarmupEvent* e,Nba97GameSceneRandomWarmupRegisters* r) {
    if(!memory || !e || !r || !receipt.empty())return 0;
    Fixture f{memory};Nba97GameSpeechStartupContext c{};c.operation_budget=100;c.io=Fixture::child;c.user=&f;
    Nba97GameSpeechStartupAdapterProgress adapter{};
    const int result=nba97_game_speech_startup_from_warmup(memory,e,r,&c,&adapter);
    const auto& p=adapter.speech;
    if(result!=NBA97_TEXT_COMPLETE || !p.completed || adapter.speech_invocations!=1 ||
       f.clocks!=3 || f.ready!=2 || f.pumps!=2 || f.stack_argument!=1 ||
       f.get(0x80103fb0u)!=0 || f.get(0x800c4568u)!=0 || f.get(0x8002149cu)!=0x80170000u ||
       f.get(0x800dc7e8u)!=0x80170100u || p.restored_return_address.word!=0x800802bcu)
        throw std::runtime_error("speech startup native CPU fixture drifted");
    std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x800800F8\",\"inclusive_end\":\"0x80080247\",\"bytes\":336,\"instructions\":84,"
      "\"classification\":\"no direct visual effect\",\"scope\":\"actual warm-up owner and speech startup adapter; explicit synthetic audio/time services, no audible playback\","
      "\"completed\":true,\"call_pc\":"<<e->pc<<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"calls\":"<<f.pcs.size()<<",\"call_pcs\":[";
    for(std::size_t i=0;i<f.pcs.size();++i){if(i)o<<',';o<<f.pcs[i];}
    o<<"],\"language\":"<<p.language.word<<",\"filename\":"<<f.filename<<",\"handle\":"<<f.get(0x8002149cu)<<",\"voice\":"<<f.get(0x800dc7e8u)
     <<",\"fifth_argument\":"<<f.stack_argument<<",\"clock_samples\":[1000,1240,1241],\"deadline\":"<<p.deadline.word
     <<",\"ready_polls\":"<<f.ready<<",\"service_pumps\":"<<f.pumps<<",\"cleared_globals\":["<<f.get(0x80103fb0u)<<','<<f.get(0x800c4568u)
     <<"],\"frame_stack_pointer\":"<<p.frame_stack_pointer<<",\"restored_ra\":"<<p.restored_return_address.word<<"}";
    auto prefix=o.str();prefix.pop_back();o.str("");o.clear();o<<prefix<<",\"audio_stream_pumps\":[";
    for(std::size_t i=0;i<f.stream.receipts.size();++i){if(i)o<<',';o<<f.stream.receipts[i];}
    o<<"]}";
    receipt=o.str();return 1;
}
}
