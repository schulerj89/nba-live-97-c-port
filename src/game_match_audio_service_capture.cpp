#include "game_match_audio_service_capture.h"
#include "game_stream_readiness_adapter.h"
#include "game_stream_queue_count_adapter.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace nba97 {
namespace {
struct Fixture {
    const Nba97GameTextMemory* memory;
    std::vector<std::uint32_t> pcs;
    Nba97GameStreamReadinessProgress readiness{};
    Nba97GameStreamQueueCountProgress queue{};
    std::vector<std::uint32_t> queue_pcs;
    unsigned readiness_calls=0,readiness_children=0;
    void put(std::uint32_t a,std::uint32_t v,unsigned width=4) {
        for(std::size_t n=0;n<memory->count;++n){const auto& r=memory->region[n];
            if(a<r.base || std::uint64_t(a-r.base)+width>r.size)continue;
            for(unsigned i=0;i<width;++i){r.data[a-r.base+i]=std::uint8_t(v>>(8*i));if(r.known)r.known[a-r.base+i]=1;}return;}
        throw std::runtime_error("match audio fixture mapping missing");
    }
    std::uint32_t get(std::uint32_t a,unsigned width=4) const {
        for(std::size_t n=0;n<memory->count;++n){const auto& r=memory->region[n];
            if(a<r.base || std::uint64_t(a-r.base)+width>r.size)continue;
            std::uint32_t v=0;for(unsigned i=0;i<width;++i){if(r.known && r.known[a-r.base+i]!=1)throw std::runtime_error("match audio unknown fixture");v|=std::uint32_t(r.data[a-r.base+i])<<(8*i);}return v;}
        throw std::runtime_error("match audio fixture mapping missing");
    }
    static int queueChild(void* user,const Nba97GameTextMemory*,const Nba97GameStreamQueueCountEvent* e,Nba97GameStreamQueueCountMachine* m) {
        auto& f=*static_cast<Fixture*>(user);
        if(e->argument_count || !((e->pc==0x8008447cu && e->entry==0x80093d94u) ||
            (e->pc==0x8008455cu && e->entry==0x80093dd4u)))return 0;
        // Explicit critical-section backend fixture; no scheduler is claimed.
        f.queue_pcs.push_back(e->pc);m->registers.gpr[2]={e->pc^0x13572468u,15};return 1;
    }
    static int readinessChild(void* user,const Nba97GameTextMemory* memory,const Nba97GameStreamReadinessEvent* e,Nba97GameStreamReadinessMachine* m) {
        auto& f=*static_cast<Fixture*>(user);
        if(e->pc!=0x80088d30u || e->entry!=0x80084448u || e->argument_count)return 0;
        ++f.readiness_children;
        Nba97GameStreamQueueCountContext c{};c.operation_budget=30;c.io=queueChild;c.user=&f;
        return nba97_game_stream_queue_count_from_stream_readiness(memory,e,m,&c,&f.queue)==NBA97_TEXT_COMPLETE;
    }
    static int child(void* user,const Nba97GameTextMemory*,const Nba97GameMatchAudioServiceEvent* e,Nba97GameMatchAudioServiceMachine* m) {
        auto& f=*static_cast<Fixture*>(user);
        if(e->entry==0x80088d0cu){
            Nba97GameStreamReadinessContext c{};c.operation_budget=6;c.io=readinessChild;c.user=&f;
            ++f.readiness_calls;
            return nba97_game_stream_readiness_from_match_audio_service(f.memory,e,m,&c,&f.readiness)==NBA97_TEXT_COMPLETE;
        }
        f.pcs.push_back(e->pc);
        // Explicit readiness and backend responses, not audible playback.
        std::uint32_t value=e->pc^0x13572468u;
        if(e->entry==0x8008847cu)value=1;
        else if(e->entry==0x80084588u || e->entry==0x800ad9fcu)value=0;
        else if(e->entry==0x8009dc10u){if(m->registers.gpr[4].word!=9 || m->registers.gpr[5].word || m->registers.gpr[6].word)return 0;}
        else if(e->entry!=0x8009f8d8u)return 0;
        m->registers.gpr[2]={value,15};return 1;
    }
};
}
int GameMatchAudioServiceCapture::dispatch(const Nba97GameTextMemory* memory,
    const Nba97GameMatchServicePublishEvent* event,Nba97GameMatchServicePublishMachine* machine,unsigned mode) {
    if(!memory || !event || !machine || !receipt.empty() || mode<1 || mode>3)return 0;
    Fixture f{memory};const auto phase=f.get(0x800170bcu);
    // Mode coverage is explicitly seeded by this diagnostic. The enclosing
    // publication and prior clock/rule/expiry machine remain live.
    f.put(0x800fda0cu,mode,2);f.put(0x800fda0eu,mode==1?480u:1u,2);f.put(0x800fda10u,40,2);
    f.put(0x800e430cu,1000);f.put(0x800d7a70u,1022);
    f.put(0x8002149cu,1);f.put(0x80021ee0u,0x80172000u);f.put(0x80021ee8u,0xffff,2);
    f.put(0x800c43b0u,7,1);f.put(0x800c43b1u,0,1);
    f.put(0x800f0fdcu,1,2); // Explicit enabled-stream fixture for the readiness owner.
    f.put(0x800c43a0u,0x80173000u);f.put(0x80173000u,0x80173020u);f.put(0x80173020u,0);
    f.put(0x800c4410u,0xffffffffu); // Two synthetic nodes, one nonterminal link.
    Nba97GameMatchAudioServiceContext service{};service.operation_budget=100;service.io=Fixture::child;service.user=&f;
    Nba97GameClockReadContext clock{};clock.operation_budget=1;
    Nba97GameAudioStreamStatusContext status{};status.operation_budget=20;
    Nba97GameMatchAudioServiceProgress p{};Nba97GameMatchAudioServiceAdapterProgress a{};
    const int result=nba97_game_match_audio_service_from_match_service_publish(memory,event,machine,&service,&clock,&status,&p,&a);
    if(result!=NBA97_TEXT_COMPLETE || !p.completed || a.caller_completions!=1 || a.clock_read_completions!=1 ||
       p.clock_delta.word!=22 || f.get(0x800e430cu)!=1022 || p.restored_return_address.word!=0x8002de64u ||
       f.get(0x800170bcu)!=phase || a.stream_status_completions!=(mode==3?1u:0u))
        throw std::runtime_error("match audio service native CPU fixture drifted");
    std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x8002A264\",\"inclusive_end\":\"0x8002A463\",\"bytes\":512,\"instructions\":128,"
        "\"classification\":\"no direct visual effect\",\"scope\":\"actual publication child and clock/status owners; explicit mode, counter, readiness and backend fixtures\","
        "\"completed\":true,\"call_pc\":"<<event->pc<<",\"mode_before\":"<<mode<<",\"mode_after\":"<<f.get(0x800fda0cu,2)
       <<",\"timer_before\":"<<(mode==1?480:1)<<",\"timer_after\":"<<f.get(0x800fda0eu,2)<<",\"phase\":"<<phase
       <<",\"clock_before\":1000,\"clock_after\":"<<f.get(0x800e430cu)<<",\"delta\":"<<p.clock_delta.word
       <<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"child_calls\":"<<p.callbacks_completed
       <<",\"clock_read\":{\"address\":\"0x800A5810\",\"call_pc\":"<<a.clock_read_event.pc<<",\"completed\":"<<unsigned(a.clock_read.completed)
       <<",\"reads\":"<<a.clock_read.reads<<",\"value\":"<<a.clock_read.return_v0.word<<",\"returned_sp\":"<<a.clock_read.machine.registers.gpr[29].word
       <<",\"returned_ra\":"<<a.clock_read.machine.registers.gpr[31].word<<"},\"status_calls\":"<<a.stream_status_completions
       <<",\"status_value\":"<<a.stream_status.returned_value.word<<",\"unresolved_call_pcs\":[";
    for(std::size_t i=0;i<f.pcs.size();++i){if(i)o<<',';o<<f.pcs[i];}
    o<<"],\"stream_readiness\":";
    if(!f.readiness_calls)o<<"null";
    else {
        const auto& d=f.readiness;
        if(f.readiness_calls!=1 || !d.completed || f.readiness_children!=1)throw std::runtime_error("stream readiness native CPU fixture drifted");
        o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x80088D0C\",\"inclusive_end\":\"0x80088D7B\",\"span_bytes\":112,\"span_words\":28,\"body_bytes\":104,\"instructions\":26,"
            "\"classification\":\"no direct visual effect\",\"scope\":\"actual match-audio event and queue owner; explicit enabled flag, nodes and critical-section services\","
            "\"completed\":true,\"call_pc\":2147656428,\"operations\":"<<d.operations<<",\"reads\":"<<d.reads<<",\"stores\":"<<d.stores
         <<",\"flag\":"<<d.loaded_flag.word<<",\"child_calls\":"<<f.readiness_children<<",\"child_pc\":2148044080,\"child_value\":1"
         <<",\"returned_value\":"<<d.machine.registers.gpr[2].word<<",\"frame_stack_pointer\":"<<d.frame_stack_pointer
         <<",\"returned_sp\":"<<d.machine.registers.gpr[29].word<<",\"restored_ra\":"<<d.restored_return_address.word;
        const auto& q=f.queue;
        if(!q.completed || q.returned_count.word!=1 || f.queue_pcs.size()!=2 || f.get(0x800c4410u)!=0xffffffffu)
            throw std::runtime_error("stream queue count native CPU fixture drifted");
        o<<",\"queue_count\":{\"program\":\"GAMEONLY\",\"address\":\"0x80084448\",\"inclusive_end\":\"0x80084587\",\"bytes\":320,\"instructions\":80,"
            "\"classification\":\"no direct visual effect\",\"scope\":\"actual readiness call; two synthetic nodes and explicit critical-section services\","
            "\"completed\":true,\"call_pc\":2148044080,\"operations\":"<<q.operations<<",\"reads\":"<<q.reads<<",\"stores\":"<<q.stores
         <<",\"head\":"<<q.initial_head.word<<",\"links\":"<<q.links_counted<<",\"iterations\":"<<q.loop_iterations
         <<",\"counter_before\":4294967295,\"counter_incremented\":"<<q.counter_after_increment.word
         <<",\"counter_after\":"<<f.get(0x800c4410u)<<",\"returned_value\":"<<q.returned_count.word
         <<",\"call_pcs\":["<<f.queue_pcs[0]<<','<<f.queue_pcs[1]<<"],\"frame_stack_pointer\":"<<q.frame_stack_pointer
         <<",\"returned_sp\":"<<q.machine.registers.gpr[29].word<<",\"restored_ra\":"<<q.restored_return_address.word<<"}}";
    }
    o<<",\"returned_v0\":"<<machine->registers.gpr[2].word<<",\"returned_v1\":"<<machine->registers.gpr[3].word
     <<",\"frame_stack_pointer\":"<<p.frame_stack_pointer<<",\"restored_ra\":"<<p.restored_return_address.word<<"}";
    receipt=o.str();return 1;
}
}
