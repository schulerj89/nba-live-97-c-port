#include "game_period_dependencies.h"
#include <string.h>

static int valid_value(Nba97GamePeriodValue v,unsigned width)
{ return v.known<=1 && (v.known || !v.word) && (width==4 || v.word<65536); }
static int valid_reference(Nba97GamePeriodReference r)
{ return r.known<=1 && (r.known || !r.record); }
static int32_t signed32(uint32_t v)
{ return v<=0x7fffffffu?(int32_t)v:-1-(int32_t)(~v); }
static int read_entity(const Nba97GameRenderSortState* s,unsigned slot,unsigned* entity)
{
    if(!s->render_table[slot].known) return NBA97_PERIOD_DEPENDENCY_UNRESOLVED;
    *entity=s->render_table[slot].record;
    if(*entity>=11) return NBA97_PERIOD_DEPENDENCY_REFERENCE;
    if(!s->x[*entity].known) return NBA97_PERIOD_DEPENDENCY_UNRESOLVED;
    return NBA97_PERIOD_DEPENDENCY_OK;
}
static void sort_write(Nba97GameRenderSortEffects* e,unsigned field,unsigned record,unsigned value)
{
    Nba97GamePeriodDependencyWrite* w=&e->write[e->count++];
    w->field=(uint8_t)field;w->record=(uint8_t)record;w->value=(uint16_t)value;w->known=1;
    if(field==NBA97_RENDER_SORT_TABLE) {
        e->state.render_table[record].record=(uint8_t)value;e->state.render_table[record].known=1;
    } else {e->state.index06[record].word=value;e->state.index06[record].known=1;}
}
int nba97_game_period_sort_render(Nba97GameRenderSortEffects* out,const Nba97GameRenderSortState* input)
{
    Nba97GameRenderSortEffects next;
    unsigned i,left,moving; int result;
    if(!out || !input) return NBA97_PERIOD_DEPENDENCY_ARGUMENT;
    memset(&next,0,sizeof(next));memcpy(&next.state,input,sizeof(*input));
    for(i=0;i<11;++i)
        if(!valid_reference(next.state.render_table[i]) || !valid_value(next.state.x[i],4) ||
           !valid_value(next.state.index06[i],2)) return NBA97_PERIOD_DEPENDENCY_ARGUMENT;
    for(i=1;i<11;++i) {
        int slot=(int)i;
        result=read_entity(&next.state,i-1,&left);if(result!=1) return result;
        result=read_entity(&next.state,i,&moving);if(result!=1) return result;
        if(signed32(next.state.x[moving].word)>=signed32(next.state.x[left].word)) continue;
        for(;;) {
            /* Preserve source's four writes for each insertion step. A generic
             * sort followed by rebuilding all indices would fix stale indices. */
            sort_write(&next,NBA97_RENDER_SORT_TABLE,(unsigned)slot,left);
            sort_write(&next,NBA97_RENDER_SORT_INDEX06,left,(unsigned)slot);
            --slot;
            sort_write(&next,NBA97_RENDER_SORT_TABLE,(unsigned)slot,moving);
            sort_write(&next,NBA97_RENDER_SORT_INDEX06,moving,(unsigned)slot);
            if(slot==0) break;
            result=read_entity(&next.state,(unsigned)slot-1,&left);if(result!=1) return result;
            if(signed32(next.state.x[moving].word)>=signed32(next.state.x[left].word)) break;
        }
    }
    memcpy(out,&next,sizeof(next));return NBA97_PERIOD_DEPENDENCY_OK;
}

int nba97_game_period_reset_phase(Nba97GamePeriodResetEffects* out,const Nba97GamePeriodValue* phase)
{
    Nba97GamePeriodResetEffects next;
    unsigned i;
    if(!out || !phase || !valid_value(*phase,2)) return NBA97_PERIOD_DEPENDENCY_ARGUMENT;
    if(!phase->known) return NBA97_PERIOD_DEPENDENCY_UNRESOLVED;
    memset(&next,0,sizeof(next));next.phase=*phase;
    /* Signed LH: every negative raw phase clears too. No phase restoration. */
    if(phase->word<128 || phase->word>=32768) {
        next.phase.word=0;next.write[0].known=1;++next.count;
    }
    for(i=0;i<4;++i) {
        Nba97GamePeriodDependencyWrite* w=&next.write[next.count++];
        next.field[i]=(uint16_t)(i<2?0:65535);
        w->field=(uint8_t)(i+1);w->value=next.field[i];w->known=1;
    }
    memcpy(out,&next,sizeof(next));return NBA97_PERIOD_DEPENDENCY_OK;
}

static int valid_animation(const Nba97GameAnimationState* s)
{
    unsigned i;
    if(s->height_known>1 || (!s->height_known && s->height10)) return 0;
    for(i=0;i<16;++i) if(!(s->known&(1u<<i)) && s->field[i]) return 0;
    return 1;
}
static int read_animation(const Nba97GameAnimationState* s,unsigned field,uint16_t* value)
{
    if(!(s->known&(1u<<field))) return NBA97_PERIOD_DEPENDENCY_UNRESOLVED;
    *value=s->field[field];return NBA97_PERIOD_DEPENDENCY_OK;
}
static int equal_request(const Nba97GameAnimationState* s,unsigned field,uint32_t request,int* equal)
{
    uint16_t value;
    int result;
    /* LHU can never equal a request with nonzero high16 bits, even when the
     * original stored halfword has not yet acquired native provenance. */
    if(request>65535) {*equal=0;return NBA97_PERIOD_DEPENDENCY_OK;}
    result=read_animation(s,field,&value);if(result!=1) return result;
    *equal=value==request;return NBA97_PERIOD_DEPENDENCY_OK;
}
static void animation_write(Nba97GamePeriodMotionEffects* e,unsigned field,uint16_t value,int known)
{
    Nba97GamePeriodDependencyWrite* w=&e->write[e->count++];
    w->field=(uint8_t)field;w->value=value;w->known=(uint8_t)known;
    e->state.field[field]=value;e->written=(uint16_t)(e->written|(1u<<field));
    e->state.known=(uint16_t)((e->state.known&~(1u<<field))|(known?(1u<<field):0));
}
static void assign(Nba97GamePeriodMotionEffects* e,unsigned field,uint16_t value)
{ animation_write(e,field,value,1); }
static void copy_field(Nba97GamePeriodMotionEffects* e,unsigned to,unsigned from)
{ animation_write(e,to,e->state.field[from],(e->state.known&(1u<<from))!=0); }
static int resolve_motion(const Nba97GamePeriodMotionInput* s,unsigned channel)
{
    /* SLL request,2 discards the high two bits before the directory access.
     * Keep raw request separately for equality and signed SLTI below. */
    unsigned index=s->request&0x3fffffffu;
    if(index>=84 || s->header_index!=index) return NBA97_PERIOD_DEPENDENCY_REFERENCE;
    if(s->motion[channel].available==2) return NBA97_PERIOD_DEPENDENCY_UNRESOLVED;
    return s->motion[channel].available?NBA97_PERIOD_DEPENDENCY_OK:NBA97_PERIOD_DEPENDENCY_REFERENCE;
}
#define TRY(operation) do {int result_=(operation);if(result_!=NBA97_PERIOD_DEPENDENCY_OK)return result_;}while(0)
static int setter(Nba97GamePeriodMotionEffects* e,const Nba97GamePeriodMotionInput* source,unsigned channel)
{
    const unsigned lock=channel?NBA97_ANIM_4C:NBA97_ANIM_48;
    const unsigned clip=channel?NBA97_ANIM_4A:NBA97_ANIM_46;
    const unsigned flags=channel?NBA97_ANIM_64:NBA97_ANIM_60;
    const unsigned frame=channel?NBA97_ANIM_54:NBA97_ANIM_50;
    const unsigned time=channel?NBA97_ANIM_5C:NBA97_ANIM_58;
    uint16_t old_lock,old_flags,old_frame;
    const Nba97GameMotionHeaderView* motion=&source->motion[channel];
    int equal,sync=0,rewind;
    TRY(read_animation(&e->state,lock,&old_lock));
    if(old_lock<32768) return NBA97_PERIOD_DEPENDENCY_OK;
    TRY(equal_request(&e->state,clip,source->request,&equal));
    if(equal) return NBA97_PERIOD_DEPENDENCY_OK;
    TRY(read_animation(&e->state,flags,&old_flags));
    if(old_flags&2) return NBA97_PERIOD_DEPENDENCY_OK;
    TRY(resolve_motion(source,channel));
    assign(e,channel?NBA97_ANIM_78:NBA97_ANIM_70,65535);
    assign(e,channel?NBA97_ANIM_96:NBA97_ANIM_94,0);
    animation_write(e,NBA97_ANIM_9A,(uint16_t)(e->state.field[NBA97_ANIM_9A]&(channel?0xfff7:0xfffb)),
                    (e->state.known&(1u<<NBA97_ANIM_9A))!=0);
    if(!(motion->flags&1) && signed32(source->request)<21)
        assign(e,NBA97_ANIM_4E,(uint16_t)source->request);
    assign(e,clip,(uint16_t)source->request);
    if(!(motion->flags&1) && !channel && motion->mode2==2)
        TRY(equal_request(&e->state,NBA97_ANIM_4A,source->request,&sync));
    if(sync) {
        /* Intentional source bug: copied secondary frame is not checked
         * against primary count. Full raw request equality is required. */
        copy_field(e,NBA97_ANIM_50,NBA97_ANIM_54);copy_field(e,NBA97_ANIM_58,NBA97_ANIM_5C);
    } else {
        rewind=(motion->flags&1)!=0 || motion->count7==0;
        if(!rewind) {
            TRY(read_animation(&e->state,frame,&old_frame));
            /* Source retains a valid frame only when OLD low flag bits are0;
             * low bit1 in the old clip rewinds even an in-range frame. */
            rewind=old_frame>=motion->count7 || (old_flags&3)!=0;
        }
        if(rewind) {assign(e,time,0);assign(e,frame,0);}
    }
    assign(e,flags,motion->flags);
    return NBA97_PERIOD_DEPENDENCY_OK;
}
int nba97_game_period_switch_motion(Nba97GamePeriodMotionEffects* out,const Nba97GamePeriodMotionInput* input)
{
    Nba97GamePeriodMotionInput source;
    Nba97GamePeriodMotionEffects next;
    uint16_t v; int equal;
    if(!out || !input) return NBA97_PERIOD_DEPENDENCY_ARGUMENT;
    memcpy(&source,input,sizeof(source));
    if(!valid_animation(&source.previous) || source.operation>NBA97_PERIOD_MOTION_SECONDARY_56AA4 ||
       source.motion[0].available>2 || source.motion[1].available>2) return NBA97_PERIOD_DEPENDENCY_ARGUMENT;
    memset(&next,0,sizeof(next));next.state=source.previous;
    if(source.operation==NBA97_PERIOD_MOTION_BOTH_56B78) {
        TRY(read_animation(&next.state,NBA97_ANIM_48,&v));if(v<32768) goto done;
        TRY(read_animation(&next.state,NBA97_ANIM_4C,&v));if(v<32768) goto done;
        TRY(equal_request(&next.state,NBA97_ANIM_46,source.request,&equal));
        if(equal) {TRY(equal_request(&next.state,NBA97_ANIM_4A,source.request,&equal));if(equal) goto done;}
        TRY(read_animation(&next.state,NBA97_ANIM_60,&v));if(v&2) goto done;
        TRY(read_animation(&next.state,NBA97_ANIM_64,&v));if(v&2) goto done;
        next.secondary_called=1;TRY(setter(&next,&source,1));
        next.primary_called=1;TRY(setter(&next,&source,0));
    } else if(source.operation==NBA97_PERIOD_MOTION_PRIMARY_5699C) {
        next.primary_called=1;TRY(setter(&next,&source,0));
    } else {
        next.secondary_called=1;TRY(setter(&next,&source,1));
    }
done:
    memcpy(out,&next,sizeof(next));return NBA97_PERIOD_DEPENDENCY_OK;
}
