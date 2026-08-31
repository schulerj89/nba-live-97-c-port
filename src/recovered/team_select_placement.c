#include "team_select_placement.h"
#include <string.h>

static int16_t add16(int16_t a, int16_t b) {
    const uint16_t bits=(uint16_t)((uint16_t)a+(uint16_t)b);
    return (int16_t)(bits<32768u ? (int32_t)bits:(int32_t)bits-65536);
}
static void create(Nba97TeamPlacementNode* n, int x, int y) {
    memset(n,0,sizeof(*n));
    n->x=(int16_t)x;n->y=(int16_t)y;n->alive=1;
}
static void queue(Nba97TeamPlacementNode* n, int dx, int dy) {
    n->dx=(int16_t)dx;n->dy=(int16_t)dy;
    n->elapsed=0;n->duration=1;n->relative=1;
}
static void tick_node(Nba97TeamPlacementNode* n) {
    if(!n->alive || !n->relative) return;
    n->elapsed=(uint8_t)(n->elapsed+1u);
    if(n->elapsed<=n->duration) {
        n->x=add16(n->x,n->dx);n->y=add16(n->y,n->dy);
        n->offset_x=add16(n->offset_x,n->dx);
        n->offset_y=add16(n->offset_y,n->dy);
    } else {
        n->elapsed=n->duration;n->relative=0;
    }
}

int nba97_team_select_placement_open(Nba97TeamSelectPlacement* out, unsigned side) {
    Nba97TeamSelectPlacement s;unsigned i;
    if(!out || side>1) return 0;
    memset(&s,0,sizeof(s));
    s.arrow_group=(uint16_t)(120u+side);s.graphic_count=2;
    create(&s.value[0],388,86);create(&s.value[6],112,86);
    for(i=0;i<5;++i) {
        const int home_y=122+(int)i*16, away_y=218+(int)i*16;
        create(&s.label[i+1],248,home_y);
        create(&s.label[i+7],248,away_y);
        create(&s.value[i+1],388,home_y);
        create(&s.value[i+7],388,away_y);
        queue(&s.value[i+7],-276,-96);
        if(side) {
            queue(&s.label[i+1],0,200);
            queue(&s.label[i+7],0,-96);
        } else queue(&s.label[i+7],0,104);
    }
    create(&s.arrow[0],320,96);create(&s.arrow[1],460,96);
    create(&s.arrow[2],-458,96);create(&s.arrow[3],-318,96);
    if(side) for(i=0;i<4;++i) queue(&s.arrow[i],500,0);
    *out=s;return 1;
}

int nba97_team_select_placement_switch_side(Nba97TeamSelectPlacement* s, unsigned old_side) {
    unsigned i;const int dx=old_side ? -500:500,dy=old_side ? -200:200;
    if(!s || old_side>1) return 0;
    for(i=0;i<4;++i) queue(&s->arrow[i],dx,0);
    for(i=1;i<6;++i) {queue(&s->label[i],0,dy);queue(&s->label[i+6],0,-dy);}
    return 1;
}

int nba97_team_select_placement_refresh_values(Nba97TeamSelectPlacement* s, unsigned side) {
    unsigned i;const int x=side ? 112:388;
    if(!s || side>1) return 0;
    create(&s->value[side*6],x,86);
    for(i=1;i<6;++i) create(&s->value[side*6+i],x,106+(int)i*16);
    return 1;
}

void nba97_team_select_placement_tick(Nba97TeamSelectPlacement* s) {
    unsigned i;if(!s) return;
    for(i=0;i<12;++i) {tick_node(&s->label[i]);tick_node(&s->value[i]);}
    for(i=0;i<4;++i) tick_node(&s->arrow[i]);
}

int nba97_team_select_placement_selected_moving(const Nba97TeamSelectPlacement* s, unsigned descriptor) {
    const Nba97TeamPlacementNode* n;
    if(!s || descriptor>=12) return 0;
    n=&s->value[descriptor];
    return n->alive && n->relative && n->elapsed<n->duration ? 8:0;
}
