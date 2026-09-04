#include "game_gpu_sync.h"

#include <limits.h>
#include <string.h>

#define TARGET_9B9B4 UINT32_C(0x8009b9b4)
#define DEBUG_TEXT UINT32_C(0x800282c0)
#define DIAGNOSTIC UINT32_C(0x8009cb2c)
#define DIAG_WAIT UINT32_C(0x8009863c)
#define DMA_BUSY UINT32_C(0x01000000)
#define GPU_READY UINT32_C(0x04000000)

static int validate_abi(const Nba97GameGpuSyncAbi *abi)
{
    size_t i,j;
    if(!abi)return NBA97_GAME_GPU_SYNC_OK;
    if(!abi->memory.region&&abi->memory.count)return NBA97_GAME_GPU_SYNC_ARGUMENT;
    for(i=0;i<abi->memory.count;++i){
        const Nba97GameTextRegion *a=&abi->memory.region[i];
        if(!a->data||!a->size||a->size>UINT64_C(0x100000000)||
                (uint64_t)a->base+a->size>UINT64_C(0x100000000))
            return NBA97_GAME_GPU_SYNC_ARGUMENT;
        for(j=0;j<i;++j){
            const Nba97GameTextRegion *b=&abi->memory.region[j];
            if((uint64_t)a->base<(uint64_t)b->base+b->size&&
                    (uint64_t)b->base<(uint64_t)a->base+a->size)
                return NBA97_GAME_GPU_SYNC_ARGUMENT;
        }
    }
    return NBA97_GAME_GPU_SYNC_OK;
}

static int locate_stack(Nba97GameGpuSyncContext *context,
    Nba97GameGpuSyncProgress *progress,uint32_t pc,uint32_t address,
    uint8_t **data,uint8_t **known)
{
    size_t i;
    unsigned j;
    Nba97GameGpuSyncAbi *abi=context->abi;
    progress->stopped_pc=pc;progress->stopped_address=address;
    if(address&3u)return NBA97_GAME_GPU_SYNC_STACK_ALIGNMENT;
    for(i=0;i<abi->memory.count;++i){
        Nba97GameTextRegion *region=&abi->memory.region[i];
        uint64_t offset=(uint64_t)address-region->base;
        if(address<region->base||offset>region->size||
                4u>region->size-(size_t)offset)continue;
        *data=region->data+(size_t)offset;
        *known=region->known?region->known+(size_t)offset:0;
        if(*known)for(j=0;j<4;++j)
            if((*known)[j]>1u)return NBA97_GAME_GPU_SYNC_ARGUMENT;
        return NBA97_GAME_GPU_SYNC_OK;
    }
    return NBA97_GAME_GPU_SYNC_STACK_RESOURCE;
}

static int write_stack(Nba97GameGpuSyncContext *context,
    Nba97GameGpuSyncProgress *progress,uint32_t pc,uint32_t address,
    uint32_t word)
{
    uint8_t *data,*known;
    unsigned i;
    int status=locate_stack(context,progress,pc,address,&data,&known);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    for(i=0;i<4;++i){
        data[i]=(uint8_t)(word>>(i*8u));
        if(known)known[i]=1;
    }
    ++progress->stack_writes;
    return NBA97_GAME_GPU_SYNC_OK;
}

static int read_stack(Nba97GameGpuSyncContext *context,
    Nba97GameGpuSyncProgress *progress,uint32_t pc,uint32_t address,
    uint32_t *word)
{
    uint8_t *data,*known;
    uint32_t result=0;
    unsigned i;
    int status=locate_stack(context,progress,pc,address,&data,&known);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    if(known)for(i=0;i<4;++i)
        if(!known[i])return NBA97_GAME_GPU_SYNC_STACK_UNKNOWN;
    for(i=0;i<4;++i)result|=(uint32_t)data[i]<<(i*8u);
    *word=result;++progress->stack_reads;
    return NBA97_GAME_GPU_SYNC_OK;
}

static uint32_t width_mask(uint8_t width)
{
    if(width==1u)return UINT32_C(0xff);
    if(width==2u)return UINT32_C(0xffff);
    return UINT32_MAX;
}

static int step(Nba97GameGpuSyncContext *context,
    Nba97GameGpuSyncProgress *progress,uint32_t pc)
{
    size_t budget=context->source_step_budget?context->source_step_budget:((size_t)1u<<20);
    progress->stopped_pc=pc;
    progress->stopped_address=0;
    ++progress->source_steps;
    return progress->source_steps>budget?NBA97_GAME_GPU_SYNC_SOURCE_BUDGET:
        NBA97_GAME_GPU_SYNC_OK;
}

static int read_device(Nba97GameGpuSyncContext *context,
    Nba97GameGpuSyncProgress *progress,uint32_t pc,uint32_t address,
    uint8_t width,uint32_t required_mask,Nba97GameGpuSyncWord *value)
{
    Nba97GameGpuSyncAccess access;
    uint32_t allowed=width_mask(width);
    int status;
    progress->stopped_pc=pc;
    progress->stopped_address=address;
    if(!context->read_device)return NBA97_GAME_GPU_SYNC_READ_REQUIRED;
    access.pc=pc;access.address=address;access.width=width;
    memset(value,0,sizeof *value);
    status=context->read_device(context->user,&access,value);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    ++progress->device_reads;
    if(value->known_mask&~allowed)return NBA97_GAME_GPU_SYNC_ARGUMENT;
    value->word&=allowed;
    if((value->known_mask&required_mask)!=required_mask)
        return NBA97_GAME_GPU_SYNC_UNKNOWN;
    return NBA97_GAME_GPU_SYNC_OK;
}

static int write_device(Nba97GameGpuSyncContext *context,
    Nba97GameGpuSyncProgress *progress,uint32_t pc,uint32_t address,
    uint8_t width,uint32_t word)
{
    Nba97GameGpuSyncWrite write;
    int status;
    progress->stopped_pc=pc;
    progress->stopped_address=address;
    if(!context->write_device)return NBA97_GAME_GPU_SYNC_WRITE_REQUIRED;
    memset(&write,0,sizeof write);
    write.pc=pc;write.address=address;write.width=width;
    write.value.word=word&width_mask(width);
    write.value.known_mask=width_mask(width);
    status=context->write_device(context->user,&write);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    ++progress->device_writes;
    return NBA97_GAME_GPU_SYNC_OK;
}

static int invoke(Nba97GameGpuSyncContext *context,
    Nba97GameGpuSyncState *state,Nba97GameGpuSyncProgress *progress,
    uint32_t pc,uint32_t entry,uint8_t kind,const uint32_t *arguments,
    uint8_t argument_count)
{
    Nba97GameGpuSyncCall call;
    int status;
    progress->stopped_pc=pc;
    progress->stopped_address=entry;
    if(!context->invoke)return NBA97_GAME_GPU_SYNC_INVOKE_REQUIRED;
    memset(&call,0,sizeof call);
    call.pc=pc;call.entry=entry;call.kind=kind;call.argument_count=argument_count;
    if(argument_count)memcpy(call.arguments,arguments,(size_t)argument_count*sizeof(uint32_t));
    status=context->invoke(context->user,&call,state);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    ++progress->calls;
    return NBA97_GAME_GPU_SYNC_OK;
}

static int observe_backend(Nba97GameGpuSyncContext *context,
    Nba97GameGpuSyncProgress *progress,uint32_t pc,
    Nba97GameGpuSyncBackend *backend)
{
    int status;
    progress->stopped_pc=pc;progress->stopped_address=0;
    if(!context->observe_backend)return NBA97_GAME_GPU_SYNC_OBSERVE_REQUIRED;
    memset(backend,0,sizeof *backend);
    status=context->observe_backend(context->user,backend);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    ++progress->backend_observations;
    if(backend->known>1u||backend->idle>1u||backend->completed>backend->submitted)
        return NBA97_GAME_GPU_SYNC_ARGUMENT;
    if(!backend->known)return NBA97_GAME_GPU_SYNC_UNKNOWN;
    return NBA97_GAME_GPU_SYNC_OK;
}

static int require_word(Nba97GameGpuSyncProgress *progress,uint32_t pc,
    const Nba97GameGpuSyncWord *value,uint32_t *word)
{
    progress->stopped_pc=pc;progress->stopped_address=0;
    if(value->known_mask!=UINT32_MAX)return NBA97_GAME_GPU_SYNC_UNKNOWN;
    *word=value->word;
    return NBA97_GAME_GPU_SYNC_OK;
}

/* Reached negative path of 8009BDB4. The two device reads and C571C load are
 * intentionally retained although their derived 16-bit value is unused. */
static int read_tick(Nba97GameGpuSyncContext *context,
    Nba97GameGpuSyncState *state,Nba97GameGpuSyncProgress *progress,
    uint32_t *tick)
{
    Nba97GameGpuSyncWord timer_status,timer_counter;
    uint32_t unused;
    int status=step(context,progress,UINT32_C(0x8009bdb8));
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    status=read_device(context,progress,UINT32_C(0x8009bdd4),
        state->c5714_timer_status_ptr,4,0,&timer_status);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    status=read_device(context,progress,UINT32_C(0x8009bdd8),
        state->c5718_timer_counter_ptr,4,0,&timer_counter);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    unused=(timer_counter.word-state->c571c_timer_origin)&UINT32_C(0xffff);
    (void)timer_status;(void)unused;
    *tick=state->c5574_tick;
    return NBA97_GAME_GPU_SYNC_OK;
}

/* 800986F8 exchanges the low I_MASK halfword and returns the old value. */
static int exchange_i_mask(Nba97GameGpuSyncContext *context,
    Nba97GameGpuSyncState *state,Nba97GameGpuSyncProgress *progress,
    uint32_t new_mask,uint32_t *old_mask)
{
    Nba97GameGpuSyncWord old;
    int status=step(context,progress,UINT32_C(0x800986fc));
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    status=read_device(context,progress,UINT32_C(0x80098704),
        state->c5534_i_mask_ptr,2,UINT32_C(0xffff),&old);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    status=write_device(context,progress,UINT32_C(0x80098708),
        state->c5534_i_mask_ptr,2,new_mask);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    *old_mask=old.word;
    return NBA97_GAME_GPU_SYNC_OK;
}

static int read_dma(Nba97GameGpuSyncContext *context,
    Nba97GameGpuSyncState *state,Nba97GameGpuSyncProgress *progress,
    uint32_t pc,uint32_t *word)
{
    Nba97GameGpuSyncWord value;
    int status=read_device(context,progress,pc,state->c56a0_dma2_chcr_ptr,
        4,DMA_BUSY,&value);
    if(status==NBA97_GAME_GPU_SYNC_OK)*word=value.word;
    return status;
}

static int read_gpu(Nba97GameGpuSyncContext *context,
    Nba97GameGpuSyncState *state,Nba97GameGpuSyncProgress *progress,
    uint32_t pc,uint32_t required_mask,uint32_t *word)
{
    Nba97GameGpuSyncWord value;
    int status=read_device(context,progress,pc,state->c5694_gpu_status_ptr,
        4,required_mask,&value);
    if(status==NBA97_GAME_GPU_SYNC_OK)*word=value.word;
    return status;
}

static int drain_queue(Nba97GameGpuSyncContext *context,
    Nba97GameGpuSyncState *state,Nba97GameGpuSyncProgress *progress,
    uint32_t *remaining)
{
    uint32_t dma,gpu,old_mask,index,next,handler,a0,a1,args[2];
    size_t polls=0;
    int status=step(context,progress,UINT32_C(0x8009b57c));
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    status=read_dma(context,state,progress,UINT32_C(0x8009b590),&dma);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    if(dma&DMA_BUSY){*remaining=1;return NBA97_GAME_GPU_SYNC_OK;}

    status=exchange_i_mask(context,state,progress,0,&old_mask);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    state->c56d0_saved_i_mask=old_mask;

    if(state->c56c4_queue_write!=state->c56c8_queue_read){
        status=step(context,progress,UINT32_C(0x8009b5cc));
        if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
        status=read_dma(context,state,progress,UINT32_C(0x8009b5d8),&dma);
        if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    }
    while(state->c56c4_queue_write!=state->c56c8_queue_read&&!(dma&DMA_BUSY)){

        index=state->c56c8_queue_read&63u;
        next=(index+1u)&63u;
        if(next==state->c56c4_queue_write&&state->c55cc_completion_callback==0){
            args[0]=2;args[1]=0;
            status=invoke(context,state,progress,UINT32_C(0x8009b62c),
                DIAG_WAIT,NBA97_GAME_GPU_SYNC_CALL_WAIT_9863C,args,2);
            if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
        }

        status=read_gpu(context,state,progress,UINT32_C(0x8009b640),GPU_READY,&gpu);
        if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
        while(!(gpu&GPU_READY)){
            ++polls;++progress->gpu_polls;
            if(polls>(context->poll_budget?context->poll_budget:((size_t)1u<<20))){
                progress->stopped_pc=UINT32_C(0x8009b658);
                progress->stopped_address=state->c5694_gpu_status_ptr;
                return NBA97_GAME_GPU_SYNC_POLL_BUDGET;
            }
            status=read_gpu(context,state,progress,UINT32_C(0x8009b658),GPU_READY,&gpu);
            if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
        }

        index=state->c56c8_queue_read&63u;
        status=require_word(progress,UINT32_C(0x8009b69c),&state->queue[index].a0,&a0);
        if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
        status=require_word(progress,UINT32_C(0x8009b6c0),&state->queue[index].a1,&a1);
        if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
        status=require_word(progress,UINT32_C(0x8009b6cc),&state->queue[index].handler,&handler);
        if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
        args[0]=a0;args[1]=a1;
        status=invoke(context,state,progress,UINT32_C(0x8009b6d4),handler,
            NBA97_GAME_GPU_SYNC_CALL_QUEUE_HANDLER,args,2);
        if(status!=NBA97_GAME_GPU_SYNC_OK)return status;

        /* Original reloads C56C8 independently after the handler for each
         * diagnostic snapshot; callback mutation therefore changes sources. */
        index=state->c56c8_queue_read&63u;
        status=require_word(progress,UINT32_C(0x8009b6fc),&state->queue[index].handler,&handler);
        if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
        state->c56b4_last_handler=handler;
        index=state->c56c8_queue_read&63u;
        status=require_word(progress,UINT32_C(0x8009b728),&state->queue[index].a0,&a0);
        if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
        state->c56b8_last_a0=a0;
        index=state->c56c8_queue_read&63u;
        status=require_word(progress,UINT32_C(0x8009b754),&state->queue[index].a1,&a1);
        if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
        state->c56bc_last_a1=a1;
        state->c56c8_queue_read=(state->c56c8_queue_read+1u)&63u;

        if(state->c56c4_queue_write==state->c56c8_queue_read)break;
        status=read_dma(context,state,progress,UINT32_C(0x8009b7a4),&dma);
        if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
        if(dma&DMA_BUSY)break;
    }

    status=exchange_i_mask(context,state,progress,state->c56d0_saved_i_mask,&old_mask);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    (void)old_mask;

    if(state->c56c4_queue_write==state->c56c8_queue_read){
        status=read_dma(context,state,progress,UINT32_C(0x8009b7f0),&dma);
        if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
        if(!(dma&DMA_BUSY)&&state->c55c8_completion_pending&&
                state->c55cc_completion_callback){
            state->c55c8_completion_pending=0;
            status=invoke(context,state,progress,UINT32_C(0x8009b840),
                state->c55cc_completion_callback,
                NBA97_GAME_GPU_SYNC_CALL_COMPLETION,0,0);
            if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
        }
    }
    *remaining=(state->c56c4_queue_write-state->c56c8_queue_read)&63u;
    return NBA97_GAME_GPU_SYNC_OK;
}

static int timeout_reset(Nba97GameGpuSyncContext *context,
    Nba97GameGpuSyncState *state,Nba97GameGpuSyncProgress *progress,
    uint32_t *timed_out)
{
    Nba97GameGpuSyncWord value;
    uint32_t now,old_count,gpu_discard,gpu_read,gpu,dma,old_mask,dpcr,args[5];
    int status=read_tick(context,state,progress,&now);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    *timed_out=0;
    if((int32_t)state->c56d8_deadline>=(int32_t)now){
        old_count=state->c56dc_poll_count;
        state->c56dc_poll_count=old_count+1u;
        if((int32_t)UINT32_C(0x000f0000)>=(int32_t)old_count)
            return NBA97_GAME_GPU_SYNC_OK;
    }

    status=read_gpu(context,state,progress,UINT32_C(0x8009bb90),UINT32_MAX,&gpu_discard);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    status=read_device(context,progress,UINT32_C(0x8009bbac),
        state->c5698_gpu_read_ptr,4,UINT32_MAX,&value);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    gpu_read=value.word;
    status=read_gpu(context,state,progress,UINT32_C(0x8009bbc0),UINT32_MAX,&gpu);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    status=read_device(context,progress,UINT32_C(0x8009bbc4),
        state->c56a0_dma2_chcr_ptr,4,UINT32_MAX,&value);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    dma=value.word;
    (void)gpu_discard;
    args[0]=UINT32_C(0x800283b8);
    args[1]=(state->c56c4_queue_write-state->c56c8_queue_read)&63u;
    args[2]=gpu;args[3]=dma;args[4]=gpu_read;
    status=invoke(context,state,progress,UINT32_C(0x8009bbc8),DIAGNOSTIC,
        NBA97_GAME_GPU_SYNC_CALL_DIAGNOSTIC,args,5);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    args[0]=UINT32_C(0x800283ec);args[1]=state->c56b4_last_handler;
    args[2]=state->c56b8_last_a0;args[3]=state->c56bc_last_a1;
    status=invoke(context,state,progress,UINT32_C(0x8009bbf4),DIAGNOSTIC,
        NBA97_GAME_GPU_SYNC_CALL_DIAGNOSTIC,args,4);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;

    status=exchange_i_mask(context,state,progress,0,&old_mask);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    state->c56c8_queue_read=0;
    state->c56d4_reset_i_mask=old_mask;
    state->c56c4_queue_write=state->c56c8_queue_read;
    status=write_device(context,progress,UINT32_C(0x8009bc30),
        state->c56a0_dma2_chcr_ptr,4,UINT32_C(0x00000401));
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    status=read_device(context,progress,UINT32_C(0x8009bc40),
        state->c56b0_dpcr_ptr,4,UINT32_MAX,&value);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    dpcr=value.word|UINT32_C(0x00000800);
    status=write_device(context,progress,UINT32_C(0x8009bc4c),
        state->c56b0_dpcr_ptr,4,dpcr);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    status=write_device(context,progress,UINT32_C(0x8009bc5c),
        state->c5694_gpu_status_ptr,4,UINT32_C(0x02000000));
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    status=write_device(context,progress,UINT32_C(0x8009bc6c),
        state->c5694_gpu_status_ptr,4,UINT32_C(0x01000000));
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    status=exchange_i_mask(context,state,progress,state->c56d4_reset_i_mask,&old_mask);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    *timed_out=1;progress->source_timed_out=1;
    return NBA97_GAME_GPU_SYNC_OK;
}

static int setup_timeout(Nba97GameGpuSyncContext *context,
    Nba97GameGpuSyncState *state,Nba97GameGpuSyncProgress *progress)
{
    uint32_t tick;
    int status=read_tick(context,state,progress,&tick);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    state->c56d8_deadline=tick+UINT32_C(0xf0);
    state->c56dc_poll_count=0;
    return NBA97_GAME_GPU_SYNC_OK;
}

static int default_dispatch(Nba97GameGpuSyncContext *context,
    Nba97GameGpuSyncState *state,Nba97GameGpuSyncProgress *progress,
    uint32_t mode,uint32_t *source_word)
{
    uint32_t remaining,dma,gpu,timed_out,captured;
    int status=step(context,progress,UINT32_C(0x8009b9b4));
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    if(mode==0){
        status=setup_timeout(context,state,progress);
        if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
        while(state->c56c4_queue_write!=state->c56c8_queue_read){
            status=drain_queue(context,state,progress,&remaining);
            if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
            status=timeout_reset(context,state,progress,&timed_out);
            if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
            if(timed_out){*source_word=UINT32_MAX;return NBA97_GAME_GPU_SYNC_OK;}
        }
        for(;;){
            status=read_dma(context,state,progress,UINT32_C(0x8009ba2c),&dma);
            if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
            if(!(dma&DMA_BUSY))break;
            status=timeout_reset(context,state,progress,&timed_out);
            if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
            if(timed_out){*source_word=UINT32_MAX;return NBA97_GAME_GPU_SYNC_OK;}
        }
        for(;;){
            status=read_gpu(context,state,progress,UINT32_C(0x8009ba4c),GPU_READY,&gpu);
            if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
            if(gpu&GPU_READY)break;
            ++progress->gpu_polls;
            status=timeout_reset(context,state,progress,&timed_out);
            if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
            if(timed_out){*source_word=UINT32_MAX;return NBA97_GAME_GPU_SYNC_OK;}
        }
        *source_word=0;
        return NBA97_GAME_GPU_SYNC_OK;
    }

    captured=(state->c56c4_queue_write-state->c56c8_queue_read)&63u;
    if(captured){
        status=drain_queue(context,state,progress,&remaining);
        if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    }
    status=read_dma(context,state,progress,UINT32_C(0x8009baa0),&dma);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    if(!(dma&DMA_BUSY)){
        status=read_gpu(context,state,progress,UINT32_C(0x8009bac0),GPU_READY,&gpu);
        if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
        if(gpu&GPU_READY){*source_word=captured;return NBA97_GAME_GPU_SYNC_OK;}
    }
    *source_word=captured?captured:1u;
    return NBA97_GAME_GPU_SYNC_OK;
}

int nba97_game_gpu_sync(Nba97GameGpuSyncContext *context,
    Nba97GameGpuSyncState *state,uint32_t mode,
    Nba97GameGpuSyncWord *source_v0,Nba97GameGpuSyncProgress *progress)
{
    Nba97GameGpuSyncWord target;
    Nba97GameGpuSyncBackend before,after;
    uint32_t args[2],source_word;
    int status;
    if(!progress)return NBA97_GAME_GPU_SYNC_ARGUMENT;
    memset(progress,0,sizeof *progress);
    if(!context||!state||!source_v0)return NBA97_GAME_GPU_SYNC_ARGUMENT;

    status=validate_abi(context->abi);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;

    status=step(context,progress,UINT32_C(0x800994f8));
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    if(context->abi){
        /* 800994FC..80099510: sp adjustment and both live o32 saves. The ra
         * store is the branch-delay instruction and therefore happens on
         * both debug paths before any callback can inspect/alias the frame. */
        progress->frame_stack_pointer=context->abi->stack_pointer-UINT32_C(0x18);
        progress->stack_pointer=progress->frame_stack_pointer;
        status=write_stack(context,progress,UINT32_C(0x80099500),
            progress->frame_stack_pointer+UINT32_C(0x10),
            context->abi->saved_register_s0);
        if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
        status=write_stack(context,progress,UINT32_C(0x80099510),
            progress->frame_stack_pointer+UINT32_C(0x14),
            context->abi->return_address);
        if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    }
    if(state->c55c2_debug_level>=2u){
        args[0]=DEBUG_TEXT;args[1]=mode;
        status=invoke(context,state,progress,UINT32_C(0x80099528),
            state->c55bc_debug_callback,NBA97_GAME_GPU_SYNC_CALL_DEBUG,args,2);
        if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    }
    progress->stopped_pc=UINT32_C(0x8009953c);
    progress->stopped_address=state->c55b8_dispatch_table+UINT32_C(0x3c);
    if(!context->resolve_dispatch)return NBA97_GAME_GPU_SYNC_RESOLVE_REQUIRED;
    memset(&target,0,sizeof target);
    status=context->resolve_dispatch(context->user,UINT32_C(0x8009953c),
        state->c55b8_dispatch_table,UINT32_C(0x3c),&target);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    ++progress->dispatch_resolutions;
    if(target.known_mask!=UINT32_MAX)return NBA97_GAME_GPU_SYNC_UNKNOWN;
    if(target.word!=TARGET_9B9B4){
        progress->stopped_pc=UINT32_C(0x80099544);
        progress->stopped_address=target.word;
        return NBA97_GAME_GPU_SYNC_DYNAMIC_DISPATCH;
    }

    memset(&before,0,sizeof before);
    if(mode==0){
        status=observe_backend(context,progress,UINT32_C(0x80099544),&before);
        if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
        progress->queued_through=before.submitted;
    }
    status=default_dispatch(context,state,progress,mode,&source_word);
    if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
    source_v0->word=source_word;
    source_v0->known_mask=UINT32_MAX;
    progress->source_completed=1;

    if(context->abi){
        /* 8009954C..80099558 reloads live words after the indirect call.
         * Do not restore from sanitized native temporaries: callbacks and
         * mapped aliasing are allowed to change the saved values. This source
         * epilogue precedes the native-only backend integrity fence below. */
        status=read_stack(context,progress,UINT32_C(0x8009954c),
            progress->frame_stack_pointer+UINT32_C(0x14),
            &progress->restored_return_address);
        if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
        status=read_stack(context,progress,UINT32_C(0x80099550),
            progress->frame_stack_pointer+UINT32_C(0x10),
            &progress->restored_saved_register_s0);
        if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
        progress->stack_pointer=context->abi->stack_pointer;
        progress->abi_completed=1;
    }
    if(mode==0&&source_word==0){
        status=observe_backend(context,progress,UINT32_C(0x8009954c),&after);
        if(status!=NBA97_GAME_GPU_SYNC_OK)return status;
        if(!after.idle||after.completed!=after.submitted||
                after.completed<before.submitted)
            return NBA97_GAME_GPU_SYNC_DEVICE_INCOMPLETE;
        progress->synchronized=1;
    }
    progress->stopped_pc=0;progress->stopped_address=0;
    return NBA97_GAME_GPU_SYNC_OK;
}
