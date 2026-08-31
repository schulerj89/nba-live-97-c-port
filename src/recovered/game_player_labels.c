#include "game_player_labels.h"
#include <string.h>
static int fits(Nba97GameRenderBuffer b,size_t o,size_t n) {return b.data&&o<=b.size&&n<=b.size-o;}
static int32_t s32(uint32_t u) {return u<=0x7fffffffu?(int32_t)u:-1-(int32_t)~u;}
static void half(uint8_t* p,uint16_t v) {p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static int text(char out[32],Nba97GameRenderBuffer b,size_t offset) {
    size_t n=0;
    for(;;) {
        if(!fits(b,offset+n,1))return NBA97_RENDER_RESOURCE;
        /* Source9CB6C/9CB7C can overflow the32-byte stack string. This guard
         * is native safety, never source truncation or an invented empty name. */
        if(n==32)return NBA97_RENDER_TEXT_OVERFLOW;
        out[n]=(char)b.data[offset+n];if(!out[n])return 1;++n;
    }
}
static void decimal(char out[32],int32_t value) {
    char reversed[10];unsigned n=0,i=0;uint32_t magnitude=value<0?0u-(uint32_t)value:(uint32_t)value;
    do{reversed[n++]=(char)('0'+magnitude%10);magnitude/=10;}while(magnitude);
    if(value<0)out[i++]='-';while(n)out[i++]=reversed[--n];out[i]=0;
}
int nba97_game_player_labels(Nba97GamePlayerLabels* s,Nba97GamePlayerLabelIo io,void* ctx,unsigned* completed) {
    Nba97GamePlayerLabelEvent e;Nba97GameRenderBuffer object;unsigned i;int r;
    if(!s||!io||!completed)return NBA97_RENDER_ARGUMENT;*completed=0;
    memset(&e,0,sizeof e);memset(&object,0,sizeof object);e.kind=NBA97_LABEL_RESET_GROUP_30758;e.group=3;
    if(io(ctx,&e,&object)!=1)return NBA97_RENDER_IO_REFUSED;
    if(!fits(s->style,0x2a,2))return NBA97_RENDER_RESOURCE;half(s->style.data+0x2a,3);
    for(i=0;i<10;++i) {
        char label[32]={0};Nba97GamePlayerLabelEntity* entity=s->entity_table[i];
        if(!entity)return NBA97_RENDER_RESOURCE;
        switch(s->option21d83) {
        case 0:case 5: {
            /*35B04 left shift drops the high two index bits before lookup. */
            uint32_t index=(entity->word00-entity->side_d9)&0x3fffffffu;
            if(!s->position_name||index>=s->position_count)return NBA97_RENDER_RESOURCE;
            r=text(label,s->position_name[index],0);if(r!=1)return r;break;
        }
        case 1:case 6:
            if(!fits(entity->player,7,1))return NBA97_RENDER_RESOURCE;
            /*35ABC/35B20 use signedLB: rawFF renders "-1", unlike the
             * jersey texture owner's special00 branch. Keep this source bug. */
            decimal(label,entity->player.data[7]<128?entity->player.data[7]:(int32_t)entity->player.data[7]-256);break;
        case 2:decimal(label,s32(5u-(entity->word00-entity->side_d9)));break;
        case 3:case 7:r=text(label,entity->player,0x29);if(r!=1)return r;break;
        default:break;
        }
        if(!fits(s->style,0x26,2))return NBA97_RENDER_RESOURCE;half(s->style.data+0x26,256);
        memset(&e,0,sizeof e);memset(&object,0,sizeof object);e.kind=NBA97_LABEL_CREATE_30D18;
        e.id=(int32_t)i+246;e.x=-20;e.y=-20;e.argument=1;e.text=label;
        if(io(ctx,&e,&object)!=1)return NBA97_RENDER_IO_REFUSED;
        if(object.data) {
            if(!fits(object,0x20,2))return NBA97_RENDER_RESOURCE;
            half(object.data+0x20,0);half(object.data+0x1e,0);
            memset(&e,0,sizeof e);e.kind=NBA97_LABEL_RESET_PACKET_99960;e.argument=1;e.object=object;
            if(io(ctx,&e,NULL)!=1)return NBA97_RENDER_IO_REFUSED;
            e.object.data+=4;e.object.size-=4;
            if(io(ctx,&e,NULL)!=1)return NBA97_RENDER_IO_REFUSED;
        }else if(object.size)return NBA97_RENDER_RESOURCE;
        *completed=i+1;
    }
    if(!fits(s->style,0x2a,2))return NBA97_RENDER_RESOURCE;
    half(s->style.data+0x2a,1);s->dirty_fdb4e=1;return NBA97_RENDER_COMPLETE;
}
