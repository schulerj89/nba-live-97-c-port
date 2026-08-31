#include "game_body_geometry.h"
#include <string.h>
typedef Nba97GameBodyReference Ref;
typedef struct Run {const Nba97GameBodyGeometryInput* input;Nba97GameBodyWrite* journal;size_t capacity;Nba97GameBodyGeometryProgress* out;} Run;
static int32_t s32(uint32_t value){return value<=0x7fffffffu?(int32_t)value:-1-(int32_t)~value;}
static int valid_ref(Ref value){return value.known<=1&&(value.known||(!value.allocation&&!value.offset));}
static Ref plus(Ref value,uint32_t delta){if(value.known)value.offset+=delta;return value;}
static void advance(Run* run,uint32_t delta){run->out->cursor=plus(run->out->cursor,delta);}
#define TRY(expr) do{int result_=(expr);if(result_!=NBA97_BODY_OK)return result_;}while(0)
static int scalar(Nba97GamePeriodValue value,uint32_t* out){
    if(value.known>1||(!value.known&&value.word))return NBA97_BODY_ARGUMENT;
    if(!value.known)return NBA97_BODY_UNKNOWN;*out=value.word;return NBA97_BODY_OK;
}
static int access(Run* run,Ref ref,uint32_t pc,Nba97GameBodyBuffer** buffer,Nba97GameBodyCell** cell,uint8_t** bytes,int* unknown){
    Nba97GameBodyBuffer* b;Nba97GameBodyCell* c;uint64_t index;unsigned i;
    run->out->stopped_reference=ref;run->out->stopped_pc=pc;
    if(!valid_ref(ref))return NBA97_BODY_ARGUMENT;
    if(!ref.known)return NBA97_BODY_UNKNOWN;
    if(ref.allocation>=run->input->buffer_count)return NBA97_BODY_BOUNDS;
    b=&run->input->buffers[ref.allocation];
    if(!b->bytes||ref.offset>b->size||4>b->size-ref.offset)return NBA97_BODY_BOUNDS;
    if(b->address_mod4_known>1||b->address_mod4>3)return NBA97_BODY_ARGUMENT;
    if(!b->address_mod4_known)return NBA97_BODY_ALIGNMENT_UNKNOWN;
    if(((uint64_t)ref.offset+b->address_mod4)&3u)return NBA97_BODY_ALIGNMENT_TRAP;
    index=((uint64_t)ref.offset+b->address_mod4)>>2;
    if(!b->cells||index>=b->cell_count)return NBA97_BODY_BOUNDS;
    c=&b->cells[(size_t)index];
    if(c->is_reference>1||!valid_ref(c->reference)||(!c->is_reference&&c->reference.known))return NBA97_BODY_ARGUMENT;
    *unknown=0;
    if(b->known)for(i=0;i<4;++i){
        if(b->known[ref.offset+i]>1)return NBA97_BODY_ARGUMENT;
        if(!b->known[ref.offset+i])*unknown=1;
    }
    *buffer=b;*cell=c;*bytes=b->bytes+ref.offset;return NBA97_BODY_OK;
}
static int word(Run* run,Ref ref,uint32_t pc,uint32_t* out){
    Nba97GameBodyBuffer* b;Nba97GameBodyCell* c;uint8_t* p;int unknown;
    TRY(access(run,ref,pc,&b,&c,&p,&unknown));
    if(c->is_reference)return c->reference.known?NBA97_BODY_ADDRESS_REQUIRED:NBA97_BODY_UNKNOWN;
    if(unknown)return NBA97_BODY_UNKNOWN;
    *out=(uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);return NBA97_BODY_OK;
}
static int pointer(Run* run,Ref ref,uint32_t pc,Ref* out){
    Nba97GameBodyBuffer* b;Nba97GameBodyCell* c;uint8_t* p;int unknown;
    TRY(access(run,ref,pc,&b,&c,&p,&unknown));
    if(!c->is_reference)return unknown?NBA97_BODY_UNKNOWN:NBA97_BODY_REFERENCE_REQUIRED;
    *out=c->reference;return NBA97_BODY_OK;
}
static int write_value(Run* run,Ref destination,uint32_t pc,int is_ref,Ref reference,uint32_t raw){
    Nba97GameBodyBuffer* b;Nba97GameBodyCell* c;uint8_t* p;int unknown;unsigned i;Nba97GameBodyWrite* event;
    run->out->stopped_reference=destination;run->out->stopped_pc=pc;
    if(run->out->writes>=run->capacity)return NBA97_BODY_JOURNAL_LIMIT;
    if(is_ref&&!valid_ref(reference))return NBA97_BODY_ARGUMENT;
    TRY(access(run,destination,pc,&b,&c,&p,&unknown));
    event=&run->journal[run->out->writes++];memset(event,0,sizeof *event);
    event->destination=destination;event->pc=pc;event->is_reference=(uint8_t)is_ref;
    memset(c,0,sizeof *c);
    if(is_ref){
        event->reference=reference;c->is_reference=1;c->reference=reference;
        /* These zero bytes are ONLY unused representation metadata. Numeric
         * source addresses require a separate proven allocation-base owner. */
        for(i=0;i<4;++i){p[i]=0;if(b->known)b->known[destination.offset+i]=0;}
    }else{
        event->word=raw;
        for(i=0;i<4;++i){p[i]=(uint8_t)(raw>>(8*i));if(b->known)b->known[destination.offset+i]=1;}
    }
    return NBA97_BODY_OK;
}
static int sw(Run* r,Ref dst,uint32_t pc,uint32_t raw){Ref empty={0,0,0};return write_value(r,dst,pc,0,empty,raw);}
static int sp(Run* r,Ref dst,uint32_t pc,Ref ref){return write_value(r,dst,pc,1,ref,0);}
typedef struct ParentWrite {uint32_t pc;uint16_t destination,target;uint8_t root;} ParentWrite;
static const ParentWrite parents[]={
    {0x80050a60,0xa4,0,1},{0x80050a64,0x2f4,0,1},{0x80050a68,0x544,0,1},
    {0x80050a70,0x138,0x64,0},{0x80050a78,0x388,0x2b4,0},{0x80050a80,0x66c,0x598,0},
    {0x80050a84,0x794,0x598,0},{0x80050a88,0x9e4,0x598,0},{0x80050a90,0x828,0x754,0},
    {0x80050a98,0xa78,0x9a4,0},{0x80050aa0,0x5d8,0x504,0},{0x80050aa8,0x1cc,0xf8,0},
    {0x80050ab0,0x41c,0x348,0},{0x80050ab8,0x260,0x18c,0},{0x80050ac0,0x4b0,0x3dc,0},
    {0x80050ac8,0x8bc,0x7e8,0},{0x80050ad0,0xb0c,0xa38,0},{0x80050ad8,0x950,0x87c,0},
    {0x80050ae0,0xba0,0xacc,0},{0x80050ae8,0x700,0x62c,0},
    {0x80050afc,0x13c,0x84,0},{0x80050b04,0x38c,0x2d4,0},{0x80050b0c,0x670,0x5b8,0},
    {0x80050b10,0x798,0x5b8,0},{0x80050b14,0x9e8,0x5b8,0},{0x80050b1c,0x82c,0x774,0},
    {0x80050b24,0xa7c,0x9c4,0},{0x80050b2c,0x5dc,0x524,0},{0x80050b34,0x1d0,0x118,0},
    {0x80050b3c,0x420,0x368,0},{0x80050b44,0xa8,0,2},{0x80050b48,0x2f8,0,2},
    {0x80050b4c,0x548,0,2},{0x80050b50,0x264,0x1ac,0},
    {0x80050b60,0x4b4,0x3fc,0},{0x80050b68,0x8c0,0x808,0},{0x80050b70,0xb10,0xa58,0},
    {0x80050b78,0x954,0x89c,0},{0x80050b80,0xba4,0xaec,0},{0x80050b88,0x704,0x64c,0}
};
static int relocate(Run* run,Ref context,Ref descriptor_slot,Ref groups_slot,unsigned secondary){
    Ref descriptor,a,b,index,group,packet,scratch;uint32_t count,encoded,raw,n=0,selected;unsigned count_offset=secondary?4:0;
    const uint32_t start=secondary?0x80050cd4:0x80050bfc;
    TRY(pointer(run,descriptor_slot,start,&descriptor));
    TRY(word(run,plus(descriptor,count_offset),start+8,&count));
    TRY(pointer(run,plus(descriptor,secondary?0x20:0x18),start+12,&a));
    TRY(pointer(run,plus(descriptor,secondary?0x24:0x1c),start+16,&b));
    TRY(pointer(run,plus(descriptor,secondary?0x14:0x10),start+20,&index));
    /* Source signed test is on wrapped3*count, not count itself. No unsigned
     * triangle loop, clamped count, or presumed positive resource schema. */
    if(s32(count*3u)<=0)return NBA97_BODY_OK;
    do{
        if(!secondary){
            TRY(word(run,a,0x80050c2c,&encoded));TRY(sw(run,a,0x80050c38,encoded&0xffffffu));
            TRY(word(run,b,0x80050c3c,&raw));TRY(sw(run,b,0x80050c48,raw&0xffffffu));selected=encoded>>24;
            TRY(pointer(run,plus(context,selected*0x94u+0xb0u),0x80050c64,&group));
            TRY(word(run,a,0x80050c68,&raw));TRY(pointer(run,plus(group,8),0x80050c6c,&packet));
            TRY(sp(run,a,0x80050c78,plus(packet,raw)));
            TRY(pointer(run,plus(context,selected*0x94u+0xb0u),0x80050c7c,&group));
            TRY(word(run,b,0x80050c80,&raw));TRY(pointer(run,plus(group,12),0x80050c84,&packet));
            /* Original B uses A's captured high-byte group, even if B encodes
             * a different group. Do not independently decode BankB's high byte. */
            TRY(sp(run,b,0x80050c90,plus(packet,raw)));
            TRY(pointer(run,descriptor_slot,0x80050c94,&descriptor));
            TRY(word(run,index,0x80050c98,&raw));TRY(pointer(run,plus(descriptor,8),0x80050c9c,&scratch));
            TRY(sp(run,index,0x80050ca8,plus(scratch,raw<<2)));
            TRY(pointer(run,descriptor_slot,0x80050cac,&descriptor));
            ++n;TRY(word(run,descriptor,0x80050cb4,&count));
        }else{
            TRY(word(run,a,0x80050d04,&encoded));TRY(sw(run,a,0x80050d10,encoded&0xffffffu));
            TRY(word(run,b,0x80050d14,&raw));TRY(sw(run,b,0x80050d20,raw&0xffffffu));
            TRY(pointer(run,groups_slot,0x80050d24,&group));selected=encoded>>24;
            TRY(word(run,a,0x80050d30,&raw));TRY(pointer(run,plus(group,(selected<<4)+8),0x80050d38,&packet));
            TRY(sp(run,a,0x80050d44,plus(packet,raw)));
            TRY(pointer(run,groups_slot,0x80050d48,&group));
            TRY(word(run,b,0x80050d54,&raw));TRY(pointer(run,plus(group,(selected<<4)+12),0x80050d58,&packet));
            TRY(sp(run,b,0x80050d64,plus(packet,raw)));
            TRY(pointer(run,descriptor_slot,0x80050d68,&descriptor));
            TRY(word(run,index,0x80050d6c,&raw));TRY(pointer(run,plus(descriptor,12),0x80050d70,&scratch));
            TRY(sp(run,index,0x80050d7c,plus(scratch,raw<<2)));
            TRY(pointer(run,descriptor_slot,0x80050d80,&descriptor));
            ++n;TRY(word(run,plus(descriptor,4),0x80050d88,&count));
        }
        a=plus(a,4);b=plus(b,4);index=plus(index,4);
    }while(s32(n)<s32(count*3u));
    return NBA97_BODY_OK;
}
int nba97_game_body_geometry(const Nba97GameBodyGeometryInput* input,Nba97GameBodyWrite* journal,
                             size_t capacity,Nba97GameBodyGeometryProgress* out){
    Run run;unsigned player,part,k;Ref context,first_context,descriptor_slot,groups_slot,header,other={0,0,0},xyz,descriptor,group={0,0,0},root_a,root_b;
    uint32_t count,a_count,b_count,physical;
    static const uint8_t sections[]={0x14,0x18,0x1c,0x20,0x24,0x28,0x2c,0x30,0x34};
    static const uint8_t count_fields[]={0,4,0,0,4,4,0,0,4};
    static const uint8_t scales[]={12,12,12,12,12,12,32,32,32};
    static const uint8_t skips[]={0,4,0,0,0,4,0,4,0};
    static const uint32_t read_context_pc[]={0x80050890,0x800508b4,0x800508dc,0x80050900,0x80050924,0x80050948,0x80050970,0x8005098c,0x800509ac};
    static const uint32_t read_count_pc[]={0x80050898,0x800508bc,0x800508e4,0x80050908,0x8005092c,0x80050950,0x80050978,0x80050994,0x800509b4};
    static const uint32_t write_pc[]={0x800508b0,0x800508d8,0x800508fc,0x80050920,0x80050944,0x8005096c,0x80050988,0x800509a8,0x800509c4};
    static const uint8_t xyz_parts[]={1,2,3,5,6,7};
    static const uint32_t xyz_context_pc[]={0x80050b54,0x80050b98,0x80050bac,0x80050bc0,0x80050bd4,0x80050be8};
    static const uint32_t xyz_write_pc[]={0x80050b94,0x80050ba8,0x80050bbc,0x80050bd0,0x80050be4,0x80050bf8};
    if(!input||!out||(!journal&&capacity)||(!input->buffers&&input->buffer_count))return NBA97_BODY_ARGUMENT;
    memset(out,0,sizeof *out);run.input=input;run.journal=journal;run.capacity=capacity;run.out=out;
    context=first_context=input->context;out->cursor=plus(input->cursor,4);
    for(player=0;player<5;++player){
        descriptor_slot=plus(context,0xbc4);groups_slot=plus(context,0xbc8);
        TRY(sw(&run,context,0x8005078c,6));
        for(part=0;part<20;++part){
            Ref header_slot=plus(context,part*0x94u+0xb0u);
            advance(&run,4);TRY(sp(&run,plus(header_slot,0xfffffffcu),0x800507a0,out->cursor));
            advance(&run,8);TRY(sp(&run,header_slot,0x800507a8,out->cursor));advance(&run,16);
            if(!player){
                TRY(pointer(&run,header_slot,0x800507b4,&header));TRY(sp(&run,plus(header,4),0x800507bc,out->cursor));
                TRY(pointer(&run,header_slot,0x800507c0,&header));TRY(word(&run,header,0x800507c8,&count));advance(&run,count*24u);
            }else{
                TRY(pointer(&run,plus(first_context,part*0x94u+0xb0u),0x800507e4,&other));
                TRY(pointer(&run,header_slot,0x800507e8,&header));TRY(pointer(&run,plus(other,4),0x800507ec,&xyz));
                TRY(sp(&run,plus(header,4),0x800507f4,xyz));
            }
            TRY(pointer(&run,header_slot,0x800507f8,&header));TRY(sp(&run,plus(header,8),0x80050800,out->cursor));
            TRY(pointer(&run,header_slot,0x80050804,&header));TRY(word(&run,header,0x8005080c,&count));advance(&run,count<<5);
            TRY(sp(&run,plus(header,12),0x8005081c,out->cursor));
            TRY(pointer(&run,header_slot,0x80050820,&header));TRY(word(&run,header,0x80050828,&count));advance(&run,count<<5);
        }
        advance(&run,4);TRY(sp(&run,descriptor_slot,0x8005084c,out->cursor));
        TRY(pointer(&run,descriptor_slot,0x80050850,&descriptor));advance(&run,0x3c);
        TRY(sp(&run,plus(descriptor,8),0x80050858,out->cursor));
        out->stopped_pc=0x80050860;TRY(scalar(input->count_a_10423c,&a_count));
        out->stopped_pc=0x80050868;TRY(scalar(input->count_b_fc618,&b_count));
        TRY(pointer(&run,descriptor_slot,0x8005086c,&descriptor));advance(&run,a_count<<2);
        TRY(sp(&run,plus(descriptor,12),0x80050878,out->cursor));
        TRY(pointer(&run,descriptor_slot,0x8005087c,&descriptor));advance(&run,b_count<<2);advance(&run,4);
        TRY(sp(&run,plus(descriptor,16),0x8005088c,out->cursor));
        for(k=0;k<9;++k){
            TRY(pointer(&run,descriptor_slot,read_context_pc[k],&descriptor));
            TRY(word(&run,plus(descriptor,count_fields[k]),read_count_pc[k],&count));
            advance(&run,count*scales[k]);advance(&run,skips[k]);
            TRY(sp(&run,plus(descriptor,sections[k]),write_pc[k],out->cursor));
        }
        TRY(pointer(&run,descriptor_slot,0x800509c8,&descriptor));TRY(word(&run,plus(descriptor,4),0x800509d0,&count));
        advance(&run,count<<5);advance(&run,4);TRY(sp(&run,groups_slot,0x800509e4,out->cursor));advance(&run,0x64);
        for(k=0;k<6;++k){
            TRY(pointer(&run,groups_slot,0x800509ec,&group));TRY(sp(&run,plus(group,k*16u+8),0x800509f8,out->cursor));
            TRY(pointer(&run,groups_slot,0x800509fc,&group));group=plus(group,k*16u);
            TRY(word(&run,group,0x80050a08,&count));advance(&run,count<<5);
            TRY(sp(&run,plus(group,12),0x80050a18,out->cursor));
            TRY(pointer(&run,groups_slot,0x80050a1c,&group));TRY(word(&run,plus(group,k*16u),0x80050a28,&count));
            advance(&run,count<<5);advance(&run,4);
        }
        out->stopped_pc=0x80050a48;TRY(scalar(input->physical_base_febe0,&physical));
        root_a=plus(input->roots_a,(physical+player)<<5);root_b=plus(input->roots_b,(physical+player)<<5);
        for(k=0;k<40;++k){
            /* This part1 header load is before the last six alternate-parent
             * stores in the source. Keep its captured reference through them. */
            if(k==34){TRY(pointer(&run,plus(context,0x144),0x80050b54,&other));TRY(pointer(&run,groups_slot,0x80050b58,&group));}
            TRY(sp(&run,plus(context,parents[k].destination),parents[k].pc,
                parents[k].root==1?root_a:parents[k].root==2?root_b:plus(context,parents[k].target)));
        }
        for(k=0;k<6;++k){
            if(k){TRY(pointer(&run,plus(context,xyz_parts[k]*0x94u+0xb0u),xyz_context_pc[k],&other));
                TRY(pointer(&run,groups_slot,xyz_context_pc[k]+4,&group));}
            TRY(pointer(&run,plus(other,4),k?xyz_context_pc[k]+8:0x80050b8c,&xyz));
            TRY(sp(&run,plus(group,k*16u+4),xyz_write_pc[k],xyz));
        }
        TRY(relocate(&run,context,descriptor_slot,groups_slot,0));TRY(relocate(&run,context,descriptor_slot,groups_slot,1));
        context=plus(context,0xbcc);out->players_completed=(uint8_t)(player+1);
    }
    out->completed=1;out->return_v0=0;out->stopped_pc=0;memset(&out->stopped_reference,0,sizeof out->stopped_reference);
    return NBA97_BODY_OK;
}
