#include "game_player_labels.h"
#include <string.h>
static int fits(Nba97GameRenderBuffer b,size_t o,size_t n) {return b.data&&o<=b.size&&n<=b.size-o;}
static int32_t s32(uint32_t u) {return u<=0x7fffffffu?(int32_t)u:-1-(int32_t)~u;}
static void half(uint8_t* p,uint16_t v) {p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static int access(Nba97GamePlayerLabelResolve resolve,void* ctx,Nba97GameRenderBuffer b,
    size_t offset,size_t size,uint32_t pc,int style,int write,Nba97GamePlayerLabelStorage* out) {
    size_t i;int r;Nba97GamePlayerLabelAccess a;
    memset(out,0,sizeof *out);
    if(resolve){a.buffer=b;a.offset=offset;a.size=size;a.source_pc=pc;a.role=(uint8_t)style;a.write=(uint8_t)write;
        r=resolve(ctx,&a,out);if(r!=1)return r;
    }else{if(!fits(b,offset,size))return NBA97_RENDER_RESOURCE;out->data=b.data+offset;out->address_mod2_known=1;}
    if(!out->data||out->address_mod2>1||out->address_mod2_known>1)return NBA97_RENDER_ARGUMENT;
    if(size==2&&(!out->address_mod2_known||out->address_mod2))return NBA97_LABEL_ALIGNMENT;
    /* Validate every reached metadata byte before checking unknown payloads.
     * Writes may establish unknown bytes, but never repair invalid metadata. */
    if(out->known){for(i=0;i<size;++i)if(out->known[i]>1)return NBA97_RENDER_ARGUMENT;
        if(!write)for(i=0;i<size;++i)if(!out->known[i])return NBA97_LABEL_UNKNOWN;}
    return 1;
}
static int store(Nba97GamePlayerLabelResolve resolve,void* ctx,Nba97GameRenderBuffer b,
    size_t offset,uint16_t value,uint32_t pc,int style) {
    Nba97GamePlayerLabelStorage m;int r=access(resolve,ctx,b,offset,2,pc,style,1,&m);
    if(r!=1)return r;half(m.data,value);if(m.known)m.known[0]=m.known[1]=1;return 1;
}
static int text(char out[32],Nba97GameRenderBuffer b,size_t offset,
    Nba97GamePlayerLabelResolve resolve,void* ctx,uint32_t pc) {
    size_t n=0;
    for(;;) {
        Nba97GamePlayerLabelStorage m;int r;
        if(!resolve&&!fits(b,offset+n,1))return NBA97_RENDER_RESOURCE;
        /* Source9CB6C/9CB7C can overflow the32-byte stack string. This guard
         * is native safety, never source truncation or an invented empty name. */
        if(n==32)return NBA97_RENDER_TEXT_OVERFLOW;
        r=access(resolve,ctx,b,offset+n,1,pc,0,0,&m);if(r!=1)return r;
        out[n]=(char)m.data[0];if(!out[n])return 1;++n;
    }
}
static void decimal(char out[32],int32_t value) {
    char reversed[10];unsigned n=0,i=0;uint32_t magnitude=value<0?0u-(uint32_t)value:(uint32_t)value;
    do{reversed[n++]=(char)('0'+magnitude%10);magnitude/=10;}while(magnitude);
    if(value<0)out[i++]='-';while(n)out[i++]=reversed[--n];out[i]=0;
}
static int labels(Nba97GamePlayerLabels* s,Nba97GamePlayerLabelIo io,Nba97GamePlayerLabelResolve resolve,void* ctx,unsigned* completed) {
    Nba97GamePlayerLabelEvent e;Nba97GameRenderBuffer object;unsigned i;int r;
    if(!s||!io||!completed)return NBA97_RENDER_ARGUMENT;*completed=0;
    memset(&e,0,sizeof e);memset(&object,0,sizeof object);e.kind=NBA97_LABEL_RESET_GROUP_30758;e.group=3;
    if(io(ctx,&e,&object)!=1)return NBA97_RENDER_IO_REFUSED;
    r=store(resolve,ctx,s->style,0x2a,3,0x80035a74u,1);if(r!=1)return r;
    for(i=0;i<10;++i) {
        char label[32]={0};Nba97GamePlayerLabelEntity* entity=s->entity_table[i];
        if(!entity)return NBA97_RENDER_RESOURCE;
        switch(s->option21d83) {
        case 0:case 5: {
            /*35B04 left shift drops the high two index bits before lookup. */
            uint32_t index=(entity->word00-entity->side_d9)&0x3fffffffu;
            if(!s->position_name||index>=s->position_count)return NBA97_RENDER_RESOURCE;
            r=text(label,s->position_name[index],0,resolve,ctx,0x80035b10u);if(r!=1)return r;break;
        }
        case 1:case 6: {
            Nba97GamePlayerLabelStorage m;
            r=access(resolve,ctx,entity->player,7,1,s->option21d83==1?0x80035abcu:0x80035b20u,0,0,&m);if(r!=1)return r;
            /*35ABC/35B20 use signedLB: rawFF renders "-1", unlike the
             * jersey texture owner's special00 branch. Keep this source bug. */
            decimal(label,m.data[0]<128?m.data[0]:(int32_t)m.data[0]-256);break;
        }
        case 2:decimal(label,s32(5u-(entity->word00-entity->side_d9)));break;
        case 3:case 7:r=text(label,entity->player,0x29,resolve,ctx,0x80035b44u);if(r!=1)return r;break;
        default:break;
        }
        r=store(resolve,ctx,s->style,0x26,256,0x80035b70u,1);if(r!=1)return r;
        memset(&e,0,sizeof e);memset(&object,0,sizeof object);e.kind=NBA97_LABEL_CREATE_30D18;
        e.id=(int32_t)i+246;e.x=-20;e.y=-20;e.argument=1;e.text=label;
        if(io(ctx,&e,&object)!=1)return NBA97_RENDER_IO_REFUSED;
        if(object.data) {
            r=store(resolve,ctx,object,0x20,0,0x80035b90u,2);if(r!=1)return r;
            r=store(resolve,ctx,object,0x1e,0,0x80035b98u,2);if(r!=1)return r;
            memset(&e,0,sizeof e);e.kind=NBA97_LABEL_RESET_PACKET_99960;e.argument=1;e.object=object;
            if(io(ctx,&e,NULL)!=1)return NBA97_RENDER_IO_REFUSED;
            if(resolve)e.object_offset=4;
            else{e.object.data+=4;e.object.size-=4;}
            if(io(ctx,&e,NULL)!=1)return NBA97_RENDER_IO_REFUSED;
        }else if(object.size)return NBA97_RENDER_RESOURCE;
        *completed=i+1;
    }
    r=store(resolve,ctx,s->style,0x2a,1,0x80035bc4u,1);if(r!=1)return r;
    s->dirty_fdb4e=1;return NBA97_RENDER_COMPLETE;
}
int nba97_game_player_labels(Nba97GamePlayerLabels* s,Nba97GamePlayerLabelIo io,void* ctx,unsigned* completed) {
    return labels(s,io,NULL,ctx,completed);
}
int nba97_game_player_labels_checked(Nba97GamePlayerLabels* s,Nba97GamePlayerLabelIo io,
    Nba97GamePlayerLabelResolve resolve,void* ctx,unsigned* completed) {
    if(!resolve)return NBA97_RENDER_ARGUMENT;return labels(s,io,resolve,ctx,completed);
}
