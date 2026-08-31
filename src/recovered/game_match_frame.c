#include "game_match_frame.h"
#include <string.h>
typedef struct Run {Nba97MatchFrameContext* in;Nba97MatchFrameProgress* out;} Run;
#define TRY(x) do{int status_=(x);if(status_!=NBA97_BODY_OK)return status_;}while(0)
static int32_t s32(uint32_t x){return x<0x80000000u?(int32_t)x:-1-(int32_t)~x;}
static uint32_t mask(unsigned n){return n==4?UINT32_MAX:(1u<<(8*n))-1;}
static int reserve(Run* r,uint32_t pc,uint32_t address,uint32_t entry){
    r->out->stopped_pc=pc;r->out->stopped_address=address;r->out->stopped_entry=entry;
    if(r->out->operations>=r->in->operation_budget)return NBA97_BODY_JOURNAL_LIMIT;
    ++r->out->operations;return NBA97_BODY_OK;
}
static int access(Run* r,uint32_t pc,uint32_t address,unsigned width,unsigned kind,Nba97PlayerFrameValue* v){
    unsigned i;int status;TRY(reserve(r,pc,address,0));
    if((width==4&&(address&3))||(width==2&&(address&1)))return NBA97_BODY_ALIGNMENT_TRAP;
    status=r->in->access(r->in->user,pc,address,width,kind,v);if(status!=NBA97_BODY_OK)return status;
    if(v->is_reference>1||v->reference.known>1||(!v->reference.known&&(v->reference.allocation||v->reference.offset)))return NBA97_BODY_ARGUMENT;
    if(!v->is_reference&&v->reference.known)return NBA97_BODY_ARGUMENT;
    if(v->is_reference&&(width!=4||(!v->reference.known&&(v->known_mask||v->word))))return NBA97_BODY_ARGUMENT;
    if(v->known_mask&~((1u<<width)-1)||v->word&~mask(width))return NBA97_BODY_ARGUMENT;
    for(i=0;i<width;++i)if(!(v->known_mask&(1u<<i))&&(v->word&(255u<<(i*8))))return NBA97_BODY_ARGUMENT;
    if(kind==NBA97_FRAME_READ)++r->out->reads;else ++r->out->stores;
    return NBA97_BODY_OK;
}
static int rd(Run* r,uint32_t pc,uint32_t a,unsigned n,uint32_t* value){
    Nba97PlayerFrameValue v={0};TRY(access(r,pc,a,n,NBA97_FRAME_READ,&v));
    if(v.known_mask!=((1u<<n)-1))return NBA97_BODY_UNKNOWN;
    *value=v.word;return NBA97_BODY_OK;
}
static int wr(Run* r,uint32_t pc,uint32_t a,unsigned n,uint32_t value,unsigned kind){
    Nba97PlayerFrameValue v={0};v.word=value&mask(n);v.known_mask=(uint8_t)((1u<<n)-1);
    return access(r,pc,a,n,kind,&v);
}
static int call(Run* r,uint32_t pc,uint32_t entry,uint32_t a,uint32_t b,uint32_t* result){
    Nba97MatchFrameCall q;Nba97GamePeriodValue v={0,0};int status;
    TRY(reserve(r,pc,0,entry));if(!r->in->io)return NBA97_MATCH_FRAME_IO_REQUIRED;
    q.pc=pc;q.entry=entry;q.args[0]=a;q.args[1]=b;status=r->in->io(r->in->user,&q,&v);
    if(status!=NBA97_BODY_OK)return status;
    ++r->out->calls;
    if(result){if(v.known>1||(!v.known&&v.word))return NBA97_BODY_ARGUMENT;
        if(!v.known)return NBA97_BODY_UNKNOWN;
        *result=v.word;}
    return NBA97_BODY_OK;
}
static int scratch_enabled(Run* r,uint32_t* enabled){
    uint32_t value;unsigned i;*enabled=0;
    /*535C8 short-circuits each sentinel; absent later words are not needed. */
    for(i=0;i<4;++i){TRY(rd(r,0x80055f0c,0x1f800030+i*4,4,&value));if(value!=0x1f800030+i*4)return NBA97_BODY_OK;}
    TRY(rd(r,0x80055f0c,0x1f800004,4,&value));*enabled=(value&0xff0000ffu)==0xff0000ffu;
    return NBA97_BODY_OK;
}
static int frame(Run* r){
    uint32_t bank,pointer,value,index,saved,enabled;
    for(;;){
        ++r->out->frames;TRY(call(r,0x80049024,0x800530fc,0,0,0));
        TRY(rd(r,0x80049030,0x8001ede8,4,&bank));bank=bank==0;
        TRY(wr(r,0x8004904c,0x8001ede8,4,bank,NBA97_FRAME_WRITE));
        TRY(wr(r,0x80049058,0x80102924,4,0x800f1c50+(bank<<14),NBA97_FRAME_WRITE_POINTER));
        TRY(wr(r,0x8004906c,0x801046d8,4,0x800fcc70+(bank<<7),NBA97_FRAME_WRITE_POINTER));
        TRY(call(r,0x80049070,0x80048ff4,0,0,&saved));
        TRY(rd(r,0x8004907c,0x801046d8,4,&pointer));TRY(call(r,0x80049084,0x80099960,pointer,32,0));
        TRY(rd(r,0x80049090,0x80102924,4,&pointer));TRY(call(r,0x80049094,0x80099960,pointer,4096,0));
        TRY(call(r,0x8004909c,0x8004900c,saved,0,0));
        TRY(rd(r,0x800490a8,0x800b729c,4,&value));TRY(call(r,0x800490ac,0x80056074,value,0,0));
        TRY(call(r,0x800490b4,0x80051098,0,0,0));TRY(call(r,0x800490c0,0x8005605c,256,120,0));
        TRY(call(r,0x800490c8,0x80075d40,0,0,0));
        /*545C4/545E0 only switch private scratch ABI stacks around net/court. */
        TRY(call(r,0x800490d8,0x8004b1a4,0,0,0));TRY(call(r,0x800490e8,0x80052914,0,0,0));
        TRY(call(r,0x800490f8,0x8004ac68,0,0,0));
        TRY(rd(r,0x8004910c,0x800fc660,4,&pointer));TRY(rd(r,0x80049114,pointer,2,&value));
        if(!value){
            TRY(rd(r,0x80049128,0x800fc630,4,&pointer));TRY(rd(r,0x80049130,pointer,2,&value));
            if(value==1)TRY(call(r,0x80049160,0x80057f5c,1,0,0));
            else if(value==2)TRY(call(r,0x80049170,0x80058120,1,0,0));
            else if(value==3)TRY(call(r,0x80049180,0x800581c0,1,0,0));
        }
        TRY(call(r,0x80049188,0x80049300,0,0,0));TRY(call(r,0x80049190,0x80049d34,0,0,0));
        TRY(call(r,0x80049198,0x80035bec,0,0,0));
        TRY(rd(r,0x800491a4,0x800b2048,4,&pointer));TRY(rd(r,0x800491ac,pointer+0x53,1,&value));
        /* Source XOR1 store is in the synchronization call's delay slot. */
        TRY(wr(r,0x800491bc,pointer+0x53,1,value^1u,NBA97_FRAME_WRITE));TRY(call(r,0x800491b8,0x800994f4,0,0,0));
        TRY(wr(r,0x800491c4,0x801029b0,4,0,NBA97_FRAME_WRITE));
        do{
            TRY(call(r,0x800491c8,0x80048ff4,0,0,&saved));TRY(call(r,0x800491d0,0x8004a044,0,0,0));++r->out->actor_updates;
            TRY(call(r,0x800491d8,0x8004900c,saved,0,0));TRY(call(r,0x800491e0,0x800994f4,0,0,0));
            /* Callees can change the index. Keep the reload, wrapped increment
             * and signed comparison; this is not a fixed ten-call loop. */
            TRY(rd(r,0x800491ec,0x801029b0,4,&index));++index;
            TRY(wr(r,0x800491fc,0x801029b0,4,index,NBA97_FRAME_WRITE));
        }while(s32(index)<10);
        TRY(call(r,0x8004920c,0x80048ff4,0,0,&saved));
        TRY(rd(r,0x80049218,0x8001ede8,4,&bank));TRY(call(r,0x80049234,0x80099ca4,0x8002205c+bank*20u,0,0));
        TRY(rd(r,0x80049240,0x8001ede8,4,&bank));TRY(call(r,0x80049264,0x80099acc,0x80021eec+bank*92u,0,0));
        TRY(call(r,0x8004926c,0x8004900c,saved,0,0));TRY(call(r,0x80049274,0x800994f4,0,0,0));
        TRY(call(r,0x8004927c,0x80048ff4,0,0,&saved));
        TRY(rd(r,0x80049288,0x801046d8,4,&pointer));TRY(call(r,0x80049290,0x80099a58,pointer+124,0,0));++r->out->submissions;
        TRY(rd(r,0x8004929c,0x80102924,4,&pointer));TRY(call(r,0x800492a0,0x80099a58,pointer+16380,0,0));++r->out->submissions;
        TRY(call(r,0x800492a8,0x800319b0,0,0,0));TRY(call(r,0x800492b0,0x800319b0,1,0,0));TRY(call(r,0x800492b8,0x800319b0,2,0,0));
        TRY(call(r,0x800492c0,0x8004900c,saved,0,0));
        TRY(scratch_enabled(r,&enabled));if(!enabled)return NBA97_BODY_OK;
        TRY(rd(r,0x80055f0c,0x1f800004,4,&value));if(!(value&0x200u))return NBA97_BODY_OK;
        /* The original redraw returns to530FC without returning to2DD84 or
         * advancing the outer game loop. Do not add a simulation tick here. */
    }
}
int nba97_game_match_frame(Nba97MatchFrameContext* c,Nba97MatchFrameProgress* p){
    Run r;int status;if(!p)return NBA97_BODY_ARGUMENT;memset(p,0,sizeof *p);
    if(!c||!c->access)return NBA97_BODY_ARGUMENT;
    r.in=c;r.out=p;status=frame(&r);
    if(status==NBA97_BODY_OK){p->completed=1;p->stopped_pc=p->stopped_address=p->stopped_entry=0;}
    return status;
}
