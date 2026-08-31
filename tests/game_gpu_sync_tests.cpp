#include "recovered/game_gpu_sync.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {
unsigned checks=0,failures=0;
void check_at(bool value,unsigned line){++checks;if(!value){++failures;std::fprintf(stderr,"failed check %u at line %u\n",checks,line);}}
#define check(value) check_at((value),__LINE__)

constexpr std::uint32_t IMASK=0x1f801074u,DMA=0x1f8010a8u;
constexpr std::uint32_t GPU=0x1f801814u,GPUREAD=0x1f801810u;
constexpr std::uint32_t DPCR=0x1f8010f0u,TIMER_STATUS=0x1f801124u,TIMER_COUNT=0x1f801120u;

struct Fixture {
    Nba97GameGpuSyncState *state=nullptr;
    std::uint32_t dispatch=0x8009b9b4u;
    std::uint32_t imask=0x2468u,dma=0,gpu=0x04000000u,gpuread=0x11223344u;
    std::uint32_t dpcr=0x00000100u,timer_status=0x77u,timer_count=0x1234u;
    std::uint32_t resolve_known=0xffffffffu;
    std::uint32_t imask_known=0xffffu,dma_known=0xffffffffu,gpu_known=0xffffffffu;
    std::uint32_t gpuread_known=0xffffffffu,dpcr_known=0xffffffffu,timer_known=0xffffffffu;
    std::uint64_t submitted=0,completed=0;
    bool backend_idle=true;
    int refuse_read_at=0,refuse_write_at=0,refuse_call_at=0,refuse_resolve=0,refuse_observe_at=0;
    unsigned reads=0,writes=0,calls=0,resolves=0,observes=0;
    unsigned dma_busy_reads=0,gpu_not_ready_reads=0;
    bool debug_mutates_table=false,debug_submits=false,handler_mutates_index=false;
    bool handler_submits=false,completion_after_restore=false,complete_backend_on_gpu_ready=false;
    std::uint32_t tick_advance_on_timer=0;
    std::size_t poll_budget=64,source_budget=2000000;
    std::vector<Nba97GameGpuSyncAccess> read_events;
    std::vector<Nba97GameGpuSyncWrite> write_events;
    std::vector<Nba97GameGpuSyncCall> call_events;

    static int read_device(void *user,const Nba97GameGpuSyncAccess *access,
            Nba97GameGpuSyncWord *value){
        auto& f=*static_cast<Fixture*>(user);++f.reads;f.read_events.push_back(*access);
        if(f.refuse_read_at&&static_cast<int>(f.reads)==f.refuse_read_at)return -71;
        value->known_mask=access->width==2?0xffffu:0xffffffffu;
        if(access->address==IMASK){value->word=f.imask;value->known_mask=f.imask_known;}
        else if(access->address==DMA){
            value->known_mask=f.dma_known;
            if(f.dma_busy_reads){value->word=0x01000000u;--f.dma_busy_reads;}
            else {value->word=f.dma;f.dma&=~0x01000000u;}
        }else if(access->address==GPU){
            value->known_mask=f.gpu_known;
            if(f.gpu_not_ready_reads){value->word=f.gpu&~0x04000000u;--f.gpu_not_ready_reads;}
            else {value->word=f.gpu|0x04000000u;if(f.complete_backend_on_gpu_ready){f.completed=f.submitted;f.backend_idle=true;}}
        }else if(access->address==GPUREAD){value->word=f.gpuread;value->known_mask=f.gpuread_known;}
        else if(access->address==DPCR){value->word=f.dpcr;value->known_mask=f.dpcr_known;}
        else if(access->address==TIMER_STATUS){value->word=f.timer_status;value->known_mask=f.timer_known;}
        else if(access->address==TIMER_COUNT){
            value->word=f.timer_count;value->known_mask=f.timer_known;
            if(f.tick_advance_on_timer&&f.state)f.state->c5574_tick+=f.tick_advance_on_timer;
        }else return NBA97_GAME_GPU_SYNC_ARGUMENT;
        return NBA97_GAME_GPU_SYNC_OK;
    }
    static int write_device(void *user,const Nba97GameGpuSyncWrite *write){
        auto& f=*static_cast<Fixture*>(user);++f.writes;f.write_events.push_back(*write);
        if(f.refuse_write_at&&static_cast<int>(f.writes)==f.refuse_write_at)return -72;
        if(write->address==IMASK)f.imask=write->value.word&0xffffu;
        else if(write->address==DMA)f.dma=write->value.word;
        else if(write->address==GPU)f.gpu=write->value.word;
        else if(write->address==DPCR)f.dpcr=write->value.word;
        else return NBA97_GAME_GPU_SYNC_ARGUMENT;
        return NBA97_GAME_GPU_SYNC_OK;
    }
    static int resolve(void *user,std::uint32_t pc,std::uint32_t table,
            std::uint32_t offset,Nba97GameGpuSyncWord *value){
        auto& f=*static_cast<Fixture*>(user);++f.resolves;
        if(f.refuse_resolve)return -73;
        check(pc==0x8009953cu&&offset==0x3cu);
        check(f.state&&table==f.state->c55b8_dispatch_table);
        value->word=f.dispatch;value->known_mask=f.resolve_known;return 1;
    }
    static int invoke(void *user,const Nba97GameGpuSyncCall *call,
            Nba97GameGpuSyncState *state){
        auto& f=*static_cast<Fixture*>(user);++f.calls;f.call_events.push_back(*call);
        if(f.refuse_call_at&&static_cast<int>(f.calls)==f.refuse_call_at)return -74;
        if(call->kind==NBA97_GAME_GPU_SYNC_CALL_DEBUG){
            if(f.debug_mutates_table){state->c55b8_dispatch_table=0x80124000u;f.dispatch=0x80123456u;}
            if(f.debug_submits){++f.submitted;f.backend_idle=false;}
        }else if(call->kind==NBA97_GAME_GPU_SYNC_CALL_QUEUE_HANDLER){
            if(f.handler_mutates_index)state->c56c8_queue_read=(state->c56c8_queue_read+1u)&63u;
            if(f.handler_submits){++f.submitted;f.backend_idle=false;f.dma_busy_reads=2;f.gpu_not_ready_reads=2;f.complete_backend_on_gpu_ready=true;}
        }else if(call->kind==NBA97_GAME_GPU_SYNC_CALL_COMPLETION){
            f.completion_after_restore=f.imask==0x2468u;
        }
        return 1;
    }
    static int observe(void *user,Nba97GameGpuSyncBackend *backend){
        auto& f=*static_cast<Fixture*>(user);++f.observes;
        if(f.refuse_observe_at&&static_cast<int>(f.observes)==f.refuse_observe_at)return -75;
        *backend={f.submitted,f.completed,static_cast<std::uint8_t>(f.backend_idle),1};return 1;
    }
    Nba97GameGpuSyncContext context(){return {read_device,write_device,resolve,invoke,observe,this,poll_budget,source_budget};}
};

Nba97GameGpuSyncState base_state(){
    Nba97GameGpuSyncState s{};
    s.c55bc_debug_callback=0x80110000u;s.c55b8_dispatch_table=0x800c5578u;
    s.c55cc_completion_callback=0x80111000u;s.c5534_i_mask_ptr=IMASK;
    s.c5694_gpu_status_ptr=GPU;s.c5698_gpu_read_ptr=GPUREAD;
    s.c56a0_dma2_chcr_ptr=DMA;s.c56b0_dpcr_ptr=DPCR;
    s.c5714_timer_status_ptr=TIMER_STATUS;s.c5718_timer_counter_ptr=TIMER_COUNT;
    s.c5574_tick=100;s.c571c_timer_origin=0x1000u;s.c5720_timer_base=0xa5a5a5a5u;
    for(unsigned i=0;i<64;++i){
        s.queue[i].handler={0x80100000u+i,0xffffffffu};
        s.queue[i].a0={0x100u+i,0xffffffffu};s.queue[i].a1={0x200u+i,0xffffffffu};
    }
    return s;
}
int run(Fixture& f,Nba97GameGpuSyncState& state,std::uint32_t mode,
        Nba97GameGpuSyncWord& result,Nba97GameGpuSyncProgress& progress){
    f.state=&state;auto context=f.context();return nba97_game_gpu_sync(&context,&state,mode,&result,&progress);
}
}

int main(){
    for(unsigned write=0;write<64;++write)for(unsigned read=0;read<64;++read){
        auto s=base_state();s.c56c4_queue_write=write;s.c56c8_queue_read=read;
        Fixture f;f.dma=0x01000000u;Nba97GameGpuSyncWord result{};Nba97GameGpuSyncProgress p{};
        const auto delta=(write-read)&63u;
        check(run(f,s,1,result,p)==1&&result.word==(delta?delta:1u));
    }
    for(std::uint8_t debug: {std::uint8_t{0},std::uint8_t{1},std::uint8_t{2},std::uint8_t{255}}){
        auto s=base_state();s.c55c2_debug_level=debug;Fixture f;
        Nba97GameGpuSyncWord result{};Nba97GameGpuSyncProgress p{};
        check(run(f,s,1,result,p)==1&&f.calls==(debug>=2?1u:0u));
    }
    {
        auto s=base_state();Fixture f;Nba97GameGpuSyncWord result{7,0};Nba97GameGpuSyncProgress p{};
        check(run(f,s,0,result,p)==1);check(result.word==0&&result.known_mask==0xffffffffu);
        check(p.source_completed&&p.synchronized&&!p.source_timed_out);
        check(p.backend_observations==2&&p.dispatch_resolutions==1);
        check(s.c56d8_deadline==340&&s.c56dc_poll_count==0);
        check(s.c5720_timer_base==0xa5a5a5a5u&&s.c571c_timer_origin==0x1000u);
        check(f.read_events[0].pc==0x8009bdd4u&&f.read_events[1].pc==0x8009bdd8u);
    }
    {
        auto s=base_state();s.c56c4_queue_write=2;Fixture f;Nba97GameGpuSyncWord result{};Nba97GameGpuSyncProgress p{};
        check(run(f,s,1,result,p)==1);check(result.word==2&&result.known_mask==0xffffffffu);
        check(s.c56c8_queue_read==2&&f.calls==2);check(f.imask==0x2468u);
        check(f.call_events[0].kind==NBA97_GAME_GPU_SYNC_CALL_QUEUE_HANDLER);
        check(f.call_events[0].arguments[0]==0x100u&&f.call_events[1].arguments[0]==0x101u);
        check(!p.synchronized&&p.backend_observations==0);
    }
    {
        auto s=base_state();s.c56c4_queue_write=2;Fixture f;f.handler_mutates_index=true;
        Nba97GameGpuSyncWord result{};Nba97GameGpuSyncProgress p{};
        check(run(f,s,1,result,p)==1&&result.word==2);
        check(s.c56c8_queue_read==2&&f.calls==1);
        check(s.c56b4_last_handler==s.queue[1].handler.word);
        check(s.c56b8_last_a0==s.queue[1].a0.word&&s.c56bc_last_a1==s.queue[1].a1.word);
    }
    {
        auto s=base_state();s.c56c4_queue_write=1;s.c55c8_completion_pending=1;
        Fixture f;Nba97GameGpuSyncWord result{};Nba97GameGpuSyncProgress p{};
        check(run(f,s,1,result,p)==1&&result.word==1);
        check(f.completion_after_restore&&s.c55c8_completion_pending==0);
        check(f.call_events.back().kind==NBA97_GAME_GPU_SYNC_CALL_COMPLETION);
    }
    {
        auto s=base_state();s.c56c4_queue_write=1;s.c55cc_completion_callback=0;
        Fixture f;Nba97GameGpuSyncWord result{};Nba97GameGpuSyncProgress p{};
        check(run(f,s,1,result,p)==1&&f.calls==2);
        check(f.call_events[0].kind==NBA97_GAME_GPU_SYNC_CALL_WAIT_9863C);
        check(f.call_events[0].pc==0x8009b62cu&&f.call_events[0].arguments[0]==2&&f.call_events[0].arguments[1]==0);
    }
    {
        auto s=base_state();Fixture f;f.dma=0x01000000u;f.gpu=0;
        Nba97GameGpuSyncWord result{};Nba97GameGpuSyncProgress p{};
        check(run(f,s,3,result,p)==1&&result.word==1);check(f.calls==0);
    }
    {
        auto s=base_state();s.c55c2_debug_level=2;Fixture f;f.debug_mutates_table=true;
        Nba97GameGpuSyncWord result{0xabcdef01u,0x55u};Nba97GameGpuSyncProgress p{};
        check(run(f,s,0,result,p)==NBA97_GAME_GPU_SYNC_DYNAMIC_DISPATCH);
        check(f.calls==1&&f.call_events[0].kind==NBA97_GAME_GPU_SYNC_CALL_DEBUG);
        check(f.call_events[0].arguments[0]==0x800282c0u&&f.call_events[0].arguments[1]==0);
        check(s.c55b8_dispatch_table==0x80124000u&&result.word==0xabcdef01u&&!p.source_completed);
    }
    {
        auto s=base_state();s.c56c4_queue_write=3;s.c56c8_queue_read=2;
        s.c56b4_last_handler=0xaaaau;s.c56b8_last_a0=0xbbbbu;s.c56bc_last_a1=0xccccu;
        Fixture f;f.dma=0x01000000u;f.dma_busy_reads=1;f.tick_advance_on_timer=300;
        Nba97GameGpuSyncWord result{};Nba97GameGpuSyncProgress p{};
        check(run(f,s,0,result,p)==1&&result.word==0xffffffffu&&p.source_timed_out);
        check(s.c56c4_queue_write==0&&s.c56c8_queue_read==0&&s.c56d4_reset_i_mask==0x2468u);
        check(f.call_events.size()==2&&f.call_events[0].kind==NBA97_GAME_GPU_SYNC_CALL_DIAGNOSTIC);
        check(f.call_events[0].argument_count==5&&f.call_events[0].arguments[0]==0x800283b8u);
        check(f.call_events[1].arguments[0]==0x800283ecu&&f.call_events[1].arguments[1]==0xaaaau);
        check(f.write_events.size()==6);
        const std::uint32_t addresses[]={IMASK,DMA,DPCR,GPU,GPU,IMASK};
        check(f.write_events[1].address==addresses[1]&&f.write_events[1].value.word==0x401u);
        check(f.write_events[2].address==addresses[2]&&f.write_events[2].value.word==0x900u);
        check(f.write_events[3].value.word==0x02000000u&&f.write_events[4].value.word==0x01000000u);
        check(f.imask==0x2468u&&!p.synchronized);
    }
    {
        auto s=base_state();s.c56c4_queue_write=1;Fixture f;f.handler_submits=true;
        Nba97GameGpuSyncWord result{};Nba97GameGpuSyncProgress p{};
        check(run(f,s,0,result,p)==1&&result.word==0&&p.synchronized);
        check(f.submitted==1&&f.completed==1&&f.backend_idle);
        check(p.gpu_polls>=1&&p.backend_observations==2);
    }
    {
        auto s=base_state();Fixture f;f.submitted=3;f.completed=1;f.backend_idle=false;
        Nba97GameGpuSyncWord result{9,0};Nba97GameGpuSyncProgress p{};
        check(run(f,s,0,result,p)==NBA97_GAME_GPU_SYNC_DEVICE_INCOMPLETE);
        check(result.word==0&&result.known_mask==0xffffffffu&&p.source_completed&&!p.synchronized);
    }
    {
        auto s=base_state();Fixture f;f.resolve_known=0;Nba97GameGpuSyncWord result{9,7};Nba97GameGpuSyncProgress p{};
        check(run(f,s,0,result,p)==NBA97_GAME_GPU_SYNC_UNKNOWN&&result.word==9);
        f.resolve_known=0xffffffffu;f.refuse_resolve=1;check(run(f,s,0,result,p)==-73);
    }
    {
        auto s=base_state();Fixture f;f.dma_known=0x01000000u;f.gpu_known=0x04000000u;
        Nba97GameGpuSyncWord result{};Nba97GameGpuSyncProgress p{};
        check(run(f,s,1,result,p)==1&&result.word==0);
    }
    {
        auto s=base_state();s.c56c4_queue_write=1;Fixture f;f.imask_known=0xffu;
        Nba97GameGpuSyncWord result{9,7};Nba97GameGpuSyncProgress p{};
        check(run(f,s,1,result,p)==NBA97_GAME_GPU_SYNC_UNKNOWN&&result.word==9&&f.writes==0);
    }
    {
        auto s=base_state();s.c56c4_queue_write=1;Fixture f;f.gpu_not_ready_reads=1000;f.poll_budget=3;
        Nba97GameGpuSyncWord result{9,7};Nba97GameGpuSyncProgress p{};
        check(run(f,s,1,result,p)==NBA97_GAME_GPU_SYNC_POLL_BUDGET&&p.gpu_polls==4);
        check(f.imask==0&&result.word==9&&!p.source_completed);
    }
    {
        auto s=base_state();s.c56c4_queue_write=1;s.queue[0].handler.known_mask=0;Fixture f;
        Nba97GameGpuSyncWord result{9,7};Nba97GameGpuSyncProgress p{};
        check(run(f,s,1,result,p)==NBA97_GAME_GPU_SYNC_UNKNOWN&&p.stopped_pc==0x8009b6ccu);
        check(f.imask==0&&s.c56c8_queue_read==0);
    }
    {
        auto s=base_state();s.c56c4_queue_write=1;Fixture f;f.refuse_write_at=1;
        Nba97GameGpuSyncWord result{9,7};Nba97GameGpuSyncProgress p{};
        check(run(f,s,1,result,p)==-72&&result.word==9&&f.writes==1);
    }
    {
        auto s=base_state();s.c56c4_queue_write=1;s.c55c8_completion_pending=1;Fixture f;f.refuse_call_at=2;
        Nba97GameGpuSyncWord result{9,7};Nba97GameGpuSyncProgress p{};
        check(run(f,s,1,result,p)==-74&&s.c55c8_completion_pending==0);
        check(f.imask==0x2468u&&s.c56c8_queue_read==1&&result.word==9);
    }
    {
        auto s=base_state();Fixture f;f.source_budget=1;auto c=f.context();f.state=&s;
        Nba97GameGpuSyncWord result{9,7};Nba97GameGpuSyncProgress p{};
        check(nba97_game_gpu_sync(&c,&s,0,&result,&p)==NBA97_GAME_GPU_SYNC_SOURCE_BUDGET);
        check(result.word==9&&!p.source_completed&&p.source_steps==2);
    }
    {
        auto s=base_state();Fixture f;f.refuse_read_at=1;Nba97GameGpuSyncWord result{9,7};Nba97GameGpuSyncProgress p{};
        check(run(f,s,0,result,p)==-71&&result.word==9);
        f={};f.refuse_observe_at=1;check(run(f,s,0,result,p)==-75);
        f={};s.c55c2_debug_level=2;f.refuse_call_at=1;check(run(f,s,0,result,p)==-74);
    }
    {
        auto s=base_state();Fixture f;auto c=f.context();Nba97GameGpuSyncWord result{};Nba97GameGpuSyncProgress p{};
        f.state=&s;c.resolve_dispatch=nullptr;check(nba97_game_gpu_sync(&c,&s,0,&result,&p)==NBA97_GAME_GPU_SYNC_RESOLVE_REQUIRED);
        c=f.context();c.observe_backend=nullptr;check(nba97_game_gpu_sync(&c,&s,0,&result,&p)==NBA97_GAME_GPU_SYNC_OBSERVE_REQUIRED);
        c=f.context();c.read_device=nullptr;check(nba97_game_gpu_sync(&c,&s,0,&result,&p)==NBA97_GAME_GPU_SYNC_READ_REQUIRED);
        check(nba97_game_gpu_sync(nullptr,&s,0,&result,&p)==NBA97_GAME_GPU_SYNC_ARGUMENT);
        check(nba97_game_gpu_sync(&c,nullptr,0,&result,&p)==NBA97_GAME_GPU_SYNC_ARGUMENT);
        check(nba97_game_gpu_sync(&c,&s,0,nullptr,&p)==NBA97_GAME_GPU_SYNC_ARGUMENT);
        check(nba97_game_gpu_sync(&c,&s,0,&result,nullptr)==NBA97_GAME_GPU_SYNC_ARGUMENT);
    }
    std::printf("%u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
