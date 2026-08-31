#include "team_select_text.h"
#include <string.h>

static int index_or_none(unsigned i) { return i<200 || i==NBA97_TEAM_TEXT_NONE; }

static int valid(const Nba97TeamTextState* s) {
    unsigned i,g,layer,count,previous,next;
    uint8_t grouped[200]={0},layered[200]={0};
    if(!s || s->initialized!=1 || s->anchored>1 || s->opened>1 ||
       s->help_active>1 || s->focus>=12 || s->hint>=200) return 0;
    for(i=0;i<200;++i) {
        const Nba97TeamTextNode* n=&s->slots[i];
        if((n->known.start|n->known.alternate|n->known.target|n->known.rgb)&~7u) return 0;
        if(n->lifetime!=-1 && n->lifetime!=0 && n->lifetime!=32767) return 0;
        if(n->group_linked>1 || (n->lifetime==32767 && !n->group_linked) ||
           (n->lifetime<0 && n->group_linked)) return 0;
        if(n->tint.flags&~0xc3u) return 0; /* No glyph-range/crop/motion flags. */
        if((n->tint.flags&2) && (!n->tint.duration || n->tint.duration==255 ||
                               n->tint.elapsed>n->tint.duration)) return 0;
        if(n->lifetime>=0 && (n->group>=255 || n->layer>=3)) return 0;
        if(!index_or_none(n->group_previous) || !index_or_none(n->group_next) ||
           !index_or_none(n->layer_previous) || !index_or_none(n->layer_next)) return 0;
    }
    for(g=0;g<255;++g) {
        previous=NBA97_TEAM_TEXT_NONE;next=s->group_head[g];count=0;
        while(next!=NBA97_TEAM_TEXT_NONE) {
            const Nba97TeamTextNode* n;
            if(next>=200 || ++count>200 || grouped[next]) return 0;
            n=&s->slots[next];
            if(n->lifetime<0 || n->group!=g || n->group_previous!=previous) return 0;
            grouped[next]=1;previous=next;next=n->group_next;
        }
    }
    for(layer=0;layer<3;++layer) {
        previous=NBA97_TEAM_TEXT_NONE;next=s->layer_head[layer];count=0;
        while(next!=NBA97_TEAM_TEXT_NONE) {
            const Nba97TeamTextNode* n;
            if(next>=200 || ++count>200 || layered[next]) return 0;
            n=&s->slots[next];
            if(n->lifetime<0 || n->layer!=layer || n->layer_previous!=previous) return 0;
            layered[next]=1;previous=next;next=n->layer_next;
        }
        if(s->layer_tail[layer]!=previous) return 0;
    }
    for(i=0;i<200;++i)
        if(grouped[i]!=s->slots[i].group_linked ||
           layered[i]!=(s->slots[i].lifetime>=0)) return 0;
    for(i=0;i<12;++i) if(!index_or_none(s->label[i]) || !index_or_none(s->value[i])) return 0;
    for(i=0;i<4;++i) if(!index_or_none(s->arrow[i])) return 0;
    for(i=0;i<6;++i) if(!index_or_none(s->help[i])) return 0;
    return index_or_none(s->header);
}

static void clear_references(Nba97TeamTextState* s) {
    unsigned i;
    for(i=0;i<12;++i) s->label[i]=s->value[i]=NBA97_TEAM_TEXT_NONE;
    for(i=0;i<4;++i) s->arrow[i]=NBA97_TEAM_TEXT_NONE;
    for(i=0;i<6;++i) s->help[i]=NBA97_TEAM_TEXT_NONE;
    s->header=NBA97_TEAM_TEXT_NONE;s->help_active=0;
}

static void initialize(Nba97TeamTextState* s) {
    unsigned i;
    memset(s,0,sizeof(*s));s->initialized=1;
    clear_references(s);
    for(i=0;i<255;++i) s->group_head[i]=NBA97_TEAM_TEXT_NONE;
    for(i=0;i<3;++i) s->layer_head[i]=s->layer_tail[i]=NBA97_TEAM_TEXT_NONE;
    for(i=0;i<200;++i) {
        Nba97TeamTextNode* n=&s->slots[i];
        n->lifetime=-1;n->group=NBA97_TEAM_TEXT_NONE;n->layer=255;
        n->group_previous=n->group_next=n->layer_previous=n->layer_next=NBA97_TEAM_TEXT_NONE;
    }
}

int nba97_team_text_seed(Nba97TeamTextState* s,const uint8_t* rgb,size_t bytes,unsigned hint) {
    Nba97TeamTextState next;
    unsigned i;
    if(!s || !rgb || bytes!=600 || hint>=200) return 0;
    initialize(&next);next.anchored=1;next.hint=(uint16_t)hint;
    for(i=0;i<200;++i) {
        memcpy(next.slots[i].tint.start,rgb+i*3,3);next.slots[i].known.start=7;
    }
    *s=next;return 1;
}
void nba97_team_text_unknown(Nba97TeamTextState* s) { if(s) initialize(s); }
void nba97_team_text_invalidate(Nba97TeamTextState* s) { if(s) initialize(s); }

static void pulse(Nba97TeamTextNode* n) {
    n->known.start=n->known.alternate=n->known.target=7;
    /* The setter does not draw; preserve current visible-channel knownness. */
    nba97_reorder_tint_pulse(&n->tint);
}
static void unpulse(Nba97TeamTextNode* n) {
    if(n->tint.flags&2) n->known.start=(uint8_t)((n->known.start&1)|(n->known.target&6));
    n->known.target=7;nba97_reorder_tint_unpulse(&n->tint);
}
static void flash(Nba97TeamTextNode* n) {
    const unsigned phase=n->tint.flags&0xc0;
    if(phase!=0x40 && phase!=0xc0) {
        if(n->tint.flags&2) n->known.start=(uint8_t)((n->known.start&1)|(n->known.target&6));
        n->known.target=7;
    }
    nba97_reorder_tint_flash(&n->tint);
}
static void tick(Nba97TeamTextNode* n) {
    const unsigned flags=n->tint.flags;
    const unsigned elapsed=(uint8_t)(n->tint.elapsed+1);
    Nba97TeamTextKnown* k=&n->known;
    if(!(flags&2)) return;
    if(elapsed>n->tint.duration) {
        if(flags&1) {
            uint8_t old_target=k->target;
            k->start=old_target;k->target=k->alternate;k->alternate=old_target;k->rgb=k->start;
        } else if((flags&0xc0)==0xc0) {
            k->start=k->target;k->target=7;
        } else {
            /* Both fade->hold and final cleanup retain the visible color;
             * source writes G/B but leaves stored red unchanged. */
            k->start=(uint8_t)((k->start&1)|(k->target&6));
        }
    } else if((flags&0xc0)!=0xc0) {
        k->rgb=elapsed==0 ? k->start:elapsed==n->tint.duration ? k->target:
            (uint8_t)(k->start&k->target);
    }
    nba97_reorder_tint_tick(&n->tint);
}

static uint16_t create(Nba97TeamTextState* s,unsigned group,unsigned layer) {
    unsigned offset,index;
    Nba97TeamTextNode* n;
    uint8_t start[3],known_start;
    for(offset=0;offset<200;++offset) {
        index=(s->hint+offset)%200;
        if(s->slots[index].lifetime<0) break;
    }
    if(offset==200) return NBA97_TEAM_TEXT_NONE;
    n=&s->slots[index];memcpy(start,n->tint.start,3);known_start=n->known.start;
    memset(&n->tint,0,sizeof(n->tint));memset(&n->known,0,sizeof(n->known));
    memcpy(n->tint.start,start,3);memset(n->tint.rgb,128,3);
    n->known.start=known_start;n->known.rgb=7;
    n->lifetime=32767;n->group=(uint16_t)group;n->layer=(uint8_t)layer;n->group_linked=1;
    n->group_previous=NBA97_TEAM_TEXT_NONE;n->group_next=s->group_head[group];
    if(n->group_next!=NBA97_TEAM_TEXT_NONE) s->slots[n->group_next].group_previous=(uint16_t)index;
    s->group_head[group]=(uint16_t)index;
    n->layer_previous=s->layer_tail[layer];n->layer_next=NBA97_TEAM_TEXT_NONE;
    if(n->layer_previous!=NBA97_TEAM_TEXT_NONE) s->slots[n->layer_previous].layer_next=(uint16_t)index;
    else s->layer_head[layer]=(uint16_t)index;
    s->layer_tail[layer]=(uint16_t)index;s->hint=(uint16_t)index;
    return (uint16_t)index;
}

static void release(Nba97TeamTextState* s,unsigned index) {
    Nba97TeamTextNode* n=&s->slots[index];
    if(n->group_linked) {
        if(n->group_previous==NBA97_TEAM_TEXT_NONE) s->group_head[n->group]=n->group_next;
        else s->slots[n->group_previous].group_next=n->group_next;
        if(n->group_next!=NBA97_TEAM_TEXT_NONE) s->slots[n->group_next].group_previous=n->group_previous;
    }
    if(n->layer_previous==NBA97_TEAM_TEXT_NONE) s->layer_head[n->layer]=n->layer_next;
    else s->slots[n->layer_previous].layer_next=n->layer_next;
    if(n->layer_next==NBA97_TEAM_TEXT_NONE) s->layer_tail[n->layer]=n->layer_previous;
    else s->slots[n->layer_next].layer_previous=n->layer_previous;
    n->lifetime=-1;n->group=NBA97_TEAM_TEXT_NONE;n->layer=255;n->group_linked=0;
    n->group_previous=n->group_next=n->layer_previous=n->layer_next=NBA97_TEAM_TEXT_NONE;
}

static void present(Nba97TeamTextState* s) {
    unsigned layer,index,next;
    for(layer=0;layer<3;++layer) {
        index=s->layer_head[layer];
        while(index!=NBA97_TEAM_TEXT_NONE) {
            next=s->slots[index].layer_next;
            if(s->slots[index].lifetime==0) release(s,index);
            else tick(&s->slots[index]);
            index=next;
        }
    }
}
static void retire_all(Nba97TeamTextState* s) {
    unsigned i;
    for(i=0;i<200;++i) if(s->slots[i].lifetime>=0) s->slots[i].lifetime=0;
    s->opened=0;s->help_active=0;
}
static void tint_group(Nba97TeamTextState* s,unsigned group,int begin) {
    unsigned i=s->group_head[group];
    while(i!=NBA97_TEAM_TEXT_NONE) {
        if(begin) pulse(&s->slots[i]);else unpulse(&s->slots[i]);
        i=s->slots[i].group_next;
    }
}

static int replace_value(Nba97TeamTextState* s,unsigned descriptor) {
    const unsigned old=s->value[descriptor];
    unsigned next;
    if(old>=200 || s->slots[old].lifetime!=32767) return 0;
    s->slots[old].lifetime=0;
    next=create(s,descriptor,0);if(next==NBA97_TEAM_TEXT_NONE) return 0;
    /* 2C244 mode2: transfer tint and both pages' uniform visible color,
     * exclude movement flags. Opaque inactive +2C/+2E and geometry are not
     * represented by this metadata boundary. */
    s->slots[next].tint=s->slots[old].tint;
    s->slots[next].tint.flags&=0xc7;
    s->slots[next].known=s->slots[old].known;s->value[descriptor]=(uint16_t)next;
    return 1;
}
static int refresh(Nba97TeamTextState* s,unsigned side) {
    unsigned i;
    for(i=side*6;i<side*6+6;++i) if(!replace_value(s,i)) return 0;
    return 1;
}

int nba97_team_text_open(Nba97TeamTextState* s,unsigned focus) {
    Nba97TeamTextState next;
    unsigned i;
    if(!valid(s) || focus>=12) return 0;
    next=*s;retire_all(&next);present(&next);clear_references(&next);
    for(i=0;i<12;++i) {
        if(i%6) {
            next.label[i]=create(&next,i,0);if(next.label[i]==NBA97_TEAM_TEXT_NONE) return 0;
        }
        next.value[i]=create(&next,i,0);if(next.value[i]==NBA97_TEAM_TEXT_NONE) return 0;
    }
    tint_group(&next,focus,1);
    for(i=0;i<4;++i) {
        next.arrow[i]=create(&next,120+focus/6,0);if(next.arrow[i]==NBA97_TEAM_TEXT_NONE) return 0;
    }
    if(!replace_value(&next,0) || !replace_value(&next,6)) return 0;
    next.header=create(&next,190,0);if(next.header==NBA97_TEAM_TEXT_NONE) return 0;
    next.focus=(uint8_t)focus;next.opened=1;*s=next;return 1;
}
int nba97_team_text_refresh(Nba97TeamTextState* s,unsigned side) {
    Nba97TeamTextState next;
    if(!valid(s) || !s->opened || s->help_active || side>=2) return 0;
    next=*s;if(!refresh(&next,side)) return 0;*s=next;return 1;
}
int nba97_team_text_direction(Nba97TeamTextState* s,unsigned side,unsigned focus) {
    Nba97TeamTextState next;
    if(!valid(s) || !s->opened || s->help_active || focus>=12 ||
       side!=focus/6 || focus!=s->focus) return 0;
    next=*s;if(!refresh(&next,side) || !replace_value(&next,focus)) return 0;
    *s=next;return 1;
}
int nba97_team_text_focus(Nba97TeamTextState* s,unsigned focus) {
    if(!valid(s) || !s->opened || s->help_active || focus>=12) return 0;
    if(focus!=s->focus) {tint_group(s,s->focus,0);tint_group(s,focus,1);s->focus=(uint8_t)focus;}
    return 1;
}
int nba97_team_text_flash(Nba97TeamTextState* s,unsigned arrow) {
    if(!valid(s) || !s->opened || s->help_active || arrow>=4 || s->arrow[arrow]>=200 ||
       s->slots[s->arrow[arrow]].lifetime!=32767) return 0;
    flash(&s->slots[s->arrow[arrow]]);return 1;
}
int nba97_team_text_help_create(Nba97TeamTextState* s) {
    Nba97TeamTextState next;
    unsigned i;
    if(!valid(s) || !s->opened || s->help_active) return 0;
    next=*s;
    for(i=0;i<6;++i) {next.help[i]=create(&next,200+i,2);if(next.help[i]==NBA97_TEAM_TEXT_NONE) return 0;}
    next.help_active=1;*s=next;return 1;
}
int nba97_team_text_help_retire(Nba97TeamTextState* s) {
    unsigned i;
    if(!valid(s) || !s->opened || !s->help_active) return 0;
    for(i=0;i<6;++i)
        if(s->help[i]>=200 || s->slots[s->help[i]].lifetime!=32767) return 0;
    for(i=0;i<6;++i) {
        /* 2C004 clears each group head immediately, but leaves its retiring
         * node in layer2 until the next 2D348 pass. */
        s->slots[s->help[i]].lifetime=0;
        s->slots[s->help[i]].group_linked=0;
        s->group_head[200+i]=NBA97_TEAM_TEXT_NONE;
    }
    s->help_active=0;return 1;
}
int nba97_team_text_retire_all(Nba97TeamTextState* s) {
    if(!valid(s)) return 0;retire_all(s);return 1;
}
int nba97_team_text_present(Nba97TeamTextState* s) {
    if(!valid(s)) return 0;present(s);return 1;
}

static Nba97TeamTextPaint paint(const Nba97TeamTextState* s,unsigned index) {
    Nba97TeamTextPaint result;
    memset(&result,0,sizeof(result));
    if(index<200 && s->slots[index].lifetime==32767) {
        result.tint=s->slots[index].tint;result.rgb_known=s->slots[index].known.rgb;result.active=1;
    }
    return result;
}
int nba97_team_text_view(const Nba97TeamTextState* s,Nba97TeamTextView* out) {
    Nba97TeamTextView next;
    unsigned i;
    if(!out || !valid(s)) return 0;
    memset(&next,0,sizeof(next));next.anchored=s->anchored;
    for(i=0;i<12;++i) {next.label[i]=paint(s,s->label[i]);next.value[i]=paint(s,s->value[i]);}
    for(i=0;i<4;++i) next.arrow[i]=paint(s,s->arrow[i]);
    next.header=paint(s,s->header);*out=next;return 1;
}
