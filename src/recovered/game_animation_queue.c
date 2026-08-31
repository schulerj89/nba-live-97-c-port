#include "game_animation_queue.h"
#include "game_animation_internal.h"
static int queue_channel(Nba97GameAnimationEffects* e,uint32_t request,uint32_t blend,unsigned channel) {
    const unsigned lock=channel?0x4c:0x48,head=channel?0x78:0x70,aux=channel?0x6c:0x68;
    uint32_t value;unsigned i;
    AI_READ(e,lock,value);if(value<32768) return NBA97_ANIMATION_OK;
    for(i=0;i<4;++i) {
        AI_READ(e,head+i*2,value);
        if(value==65535) {
            ai_assign(e,head+i*2,request&65535);ai_assign(e,aux+i,blend&255);
            /* Original deliberately truncates at the new next sentinel and
             * leaves its old aux byte and every later stale entry untouched. */
            if(i!=3) ai_assign(e,head+(i+1)*2,65535);
            break;
        }
    }
    /* Full queues silently drop the request. Never shift or grow them. */
    return NBA97_ANIMATION_OK;
}
int nba97_game_animation_queue(Nba97GameAnimationEffects* out,const Nba97GameAnimationQueueInput* input) {
    Nba97GameAnimationQueueInput source;Nba97GameAnimationEffects next;
    if(!out || !input) return NBA97_ANIMATION_ARGUMENT;
    memcpy(&source,input,sizeof(source));
    if(!ai_valid(&source.previous) || source.operation>NBA97_ANIMATION_QUEUE_SECONDARY_56C84) return NBA97_ANIMATION_ARGUMENT;
    memset(&next,0,sizeof(next));next.state=source.previous;
    if(source.operation!=NBA97_ANIMATION_QUEUE_PRIMARY_56C28)
        AI_TRY(queue_channel(&next,source.request,source.blend,1));
    if(source.operation!=NBA97_ANIMATION_QUEUE_SECONDARY_56C84)
        AI_TRY(queue_channel(&next,source.request,source.blend,0));
    memcpy(out,&next,sizeof(next));return NBA97_ANIMATION_OK;
}
