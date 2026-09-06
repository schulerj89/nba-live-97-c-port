#include "game_audio_stream_pump_capture.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>
namespace nba97 {
namespace {
struct Fixture {
    const Nba97GameTextMemory* memory;
    unsigned mode,queries=0,handlers=0;
    std::vector<std::uint32_t> pcs;
    void put(std::uint32_t a,std::uint32_t v,unsigned width=4) {
        for(std::size_t n=0;n<memory->count;++n){const auto& r=memory->region[n];
            if(a<r.base || std::uint64_t(a-r.base)+width>r.size)continue;
            for(unsigned i=0;i<width;++i){r.data[a-r.base+i]=std::uint8_t(v>>(8*i));if(r.known)r.known[a-r.base+i]=1;}return;}
        throw std::runtime_error("stream pump fixture mapping missing");
    }
    static int child(void* user,const Nba97GameTextMemory*,const Nba97GameAudioStreamPumpEvent* e,Nba97GameAudioStreamPumpRegisters* r) {
        auto& f=*static_cast<Fixture*>(user);f.pcs.push_back(e->pc);
        // Explicit synthetic stream-service results; no audible playback.
        if(e->entry==0x8008472cu)r->gpr[2]={0,15};
        else if(e->entry==0x80086190u)r->gpr[2]={0x13572468u,15};
        else if(e->entry==0x80088018u){
            if(r->gpr[4].word!=0x80170000u)return 0;
            r->gpr[2]={f.queries++?0u:(f.mode==6?0xfffffff6u:1u),15};
        }else if(e->entry==0x800840f0u){
            if(r->gpr[4].word!=1)return 0;
            ++f.handlers;r->gpr[2]={0x12345678u,15};
        }else return 0;
        return 1;
    }
    void initialize(){put(0x800c43b0u,mode,1);put(0x800c438cu,0x80170000u);}
    std::string receipt(const Nba97GameAudioStreamPumpProgress& p,std::uint32_t caller,std::uint32_t sp,std::uint32_t s8) const {
        if(!p.completed || p.returned_value.word!=0 || p.restored_return_address.word!=caller+8u ||
           p.registers.gpr[29].word!=sp || p.restored_s8.word!=s8 || queries!=2 || handlers!=(mode==5?1u:0u))
            throw std::runtime_error("stream pump native CPU fixture drifted");
        std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x80083EEC\",\"inclusive_end\":\"0x800840EF\",\"bytes\":516,\"instructions\":129,"
          "\"classification\":\"no direct visual effect\",\"scope\":\"actual parent owner and full-GPR adapter; explicit synthetic stream services, no audible playback\","
          "\"completed\":true,\"call_pc\":"<<caller<<",\"mode\":"<<mode<<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"call_pcs\":[";
        for(std::size_t i=0;i<pcs.size();++i){if(i)o<<',';o<<pcs[i];}
        o<<"],\"status_queries\":"<<queries<<",\"handler_calls\":"<<handlers<<",\"handler_value\":"<<(mode==5?0x12345678u:0u)
         <<",\"returned_value\":"<<p.returned_value.word<<",\"frame_stack_pointer\":"<<p.frame_stack_pointer<<",\"restored_ra\":"<<p.restored_return_address.word<<"}";
        return o.str();
    }
};
}
int GameAudioStreamPumpCapture::fromSpeech(const Nba97GameTextMemory* memory,const Nba97GameSpeechStartupEvent* e,Nba97GameSpeechStartupRegisters* r) {
    if(!memory || !e || !r)return 0;
    Fixture f{memory,e->pc==0x800801e4u?5u:6u};f.initialize();
    const auto sp=r->gpr[29].word,s8=r->gpr[30].word;
    Nba97GameAudioStreamPumpContext c{};c.operation_budget=100;c.io=Fixture::child;c.user=&f;
    Nba97GameAudioStreamPumpAdapterProgress p{};
    if(nba97_game_audio_stream_pump_from_speech_startup(memory,e,r,&c,&p)!=NBA97_TEXT_COMPLETE)return 0;
    receipts.push_back(f.receipt(p.pump,e->pc,sp,s8));return 1;
}
int GameAudioStreamPumpCapture::fromController(const Nba97GameTextMemory* memory,const Nba97GameControllerFrameResetEvent* e,Nba97GameControllerFrameResetRegisters* r) {
    if(!memory || !e || !r)return 0;
    Fixture f{memory,5};f.initialize();
    const auto sp=r->gpr[29].word,s8=r->gpr[30].word;
    Nba97GameAudioStreamPumpContext c{};c.operation_budget=100;c.io=Fixture::child;c.user=&f;
    Nba97GameAudioStreamPumpProgress p{};
    if(nba97_game_audio_stream_pump_from_controller_reset(memory,e,r,&c,&p)!=NBA97_TEXT_COMPLETE)return 0;
    receipts.push_back(f.receipt(p,e->pc,sp,s8));return 1;
}
}
