#include "game_audio_stream_pump_capture.h"
#include "game_audio_stream_status_adapter.h"
#include "game_audio_stream_service_adapter.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>
namespace nba97 {
namespace {
struct Fixture {
    const Nba97GameTextMemory* memory;
    unsigned mode,queries=0,handlers=0;
    unsigned busy=0;
    Nba97GameAudioStreamStatusAdapterProgress status{};
    std::vector<std::uint32_t> pcs;
    std::vector<Nba97GameAudioStreamServiceAdapterProgress> services;
    unsigned worker_calls=0;
    void put(std::uint32_t a,std::uint32_t v,unsigned width=4) {
        for(std::size_t n=0;n<memory->count;++n){const auto& r=memory->region[n];
            if(a<r.base || std::uint64_t(a-r.base)+width>r.size)continue;
            for(unsigned i=0;i<width;++i){r.data[a-r.base+i]=std::uint8_t(v>>(8*i));if(r.known)r.known[a-r.base+i]=1;}return;}
        throw std::runtime_error("stream pump fixture mapping missing");
    }
    static int serviceChild(void* user,const Nba97GameTextMemory*,const Nba97GameAudioStreamServiceEvent* e,Nba97GameAudioStreamServiceRegisters* r) {
        auto& f=*static_cast<Fixture*>(user);
        if(e->pc!=0x800861c4u || e->entry!=0x800861e4u || e->argument_count)return 0;
        // Explicit synthetic adjacent-worker effect; no recovered worker claim.
        ++f.worker_calls;f.put(0x80171024u,1);r->gpr[2]={0x13572468u,15};return 1;
    }
    static int child(void* user,const Nba97GameTextMemory*,const Nba97GameAudioStreamPumpEvent* e,Nba97GameAudioStreamPumpRegisters* r) {
        auto& f=*static_cast<Fixture*>(user);f.pcs.push_back(e->pc);
        // Explicit synthetic stream-service results; no audible playback.
        if(e->entry==0x8008472cu){
            Nba97GameAudioStreamStatusContext c{};c.operation_budget=12;
            return nba97_game_audio_stream_status_from_stream_pump(f.memory,e,r,&c,&f.status)==NBA97_TEXT_COMPLETE;
        }
        else if(e->entry==0x80086190u){
            Nba97GameAudioStreamServiceContext c{};c.operation_budget=16;c.io=serviceChild;c.user=&f;
            Nba97GameAudioStreamServiceAdapterProgress service{};
            const int result=nba97_game_audio_stream_service_from_stream_pump(f.memory,e,r,&c,&service);
            f.services.push_back(service);return result==NBA97_TEXT_COMPLETE;
        }
        else if(e->entry==0x80088018u){
            if(r->gpr[4].word!=0x80170000u)return 0;
            r->gpr[2]={f.queries++?0u:(f.mode==6?0xfffffff6u:1u),15};
        }else if(e->entry==0x800840f0u){
            if(r->gpr[4].word!=1)return 0;
            ++f.handlers;r->gpr[2]={0x12345678u,15};
        }else return 0;
        return 1;
    }
    void initialize(){put(0x800c43b0u,mode==5?7u:6u,1);put(0x800c43b1u,busy,1);put(0x800c438cu,0x80170000u);put(0x8010473cu,0x80171000u);put(0x80171024u,0);}
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
        const auto& gate=status.status;
        const unsigned expected=busy?4u:(mode==5?3u:1u);
        if(status.status_invocations!=1 || !gate.completed || gate.returned_value.word!=expected ||
           gate.registers.gpr[31].word!=0x80083f08u || gate.registers.gpr[29].word!=p.frame_stack_pointer)
            throw std::runtime_error("stream status native CPU fixture drifted");
        auto prefix=o.str();prefix.pop_back();o.str("");o.clear();o<<prefix<<",\"stream_status\":{"
          "\"program\":\"GAMEONLY\",\"address\":\"0x8008472C\",\"inclusive_end\":\"0x8008480F\",\"body_bytes\":196,\"body_instructions\":49,\"span_bytes\":228,\"span_instructions\":57,"
          "\"classification\":\"no direct visual effect\",\"scope\":\"actual stream-pump event and full-GPR leaf adapter; explicit raw flag fixture\","
          "\"completed\":true,\"call_pc\":2148024064,\"flags\":"<<(mode==5?7u:6u)<<",\"busy\":"<<busy
         <<",\"operations\":"<<gate.operations<<",\"reads\":"<<gate.reads<<",\"stores\":"<<gate.stores
         <<",\"returned_value\":"<<gate.returned_value.word<<",\"frame_stack_pointer\":"<<gate.frame_stack_pointer<<",\"returned_ra\":"<<gate.registers.gpr[31].word<<"}}";
        if(services.size()!=2 || worker_calls!=1)throw std::runtime_error("stream service native count drifted");
        prefix=o.str();prefix.pop_back();o.str("");o.clear();o<<prefix<<",\"stream_services\":[";
        for(std::size_t i=0;i<services.size();++i){
            if(i)o<<',';const auto& a=services[i];const auto& v=a.service;
            if(a.service_invocations!=1 || a.service_completions!=1 || !v.completed ||
               v.header_state.word!=i || v.returned_value.word!=(i?1u:0x13572468u) ||
               v.registers.gpr[29].word!=p.frame_stack_pointer || v.restored_return_address.word!=a.service_event.pc+8u)
                throw std::runtime_error("stream service native CPU fixture drifted");
            o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x80086190\",\"inclusive_end\":\"0x800861E3\",\"bytes\":84,\"instructions\":21,"
              "\"classification\":\"no direct visual effect\",\"scope\":\"actual stream pump full-GPR adapter; explicit synthetic header and adjacent-worker effect\","
              "\"completed\":true,\"call_pc\":"<<a.service_event.pc<<",\"header\":2148995072,\"header_state\":"<<v.header_state.word
             <<",\"operations\":"<<v.operations<<",\"reads\":"<<v.reads<<",\"stores\":"<<v.stores
             <<",\"child_calls\":"<<v.call_count[NBA97_GAME_AUDIO_STREAM_SERVICE_CHILD_800861E4]
             <<",\"child_pc\":"<<(i?0u:0x800861c4u)<<",\"returned_value\":"<<v.returned_value.word
             <<",\"frame_stack_pointer\":"<<v.frame_stack_pointer<<",\"restored_ra\":"<<v.restored_return_address.word<<"}";
        }
        o<<"]}";return o.str();
    }
};
}
int GameAudioStreamPumpCapture::fromSpeech(const Nba97GameTextMemory* memory,const Nba97GameSpeechStartupEvent* e,Nba97GameSpeechStartupRegisters* r) {
    if(!memory || !e || !r)return 0;
    Fixture f{memory,e->pc==0x800801e4u?5u:6u};f.busy=e->pc==0x800801e4u?255u:0u;f.initialize();
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
