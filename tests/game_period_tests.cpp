#include "recovered/game_period.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

static Nba97GamePeriodValue known(uint32_t v) { return {v,1}; }
static Nba97GamePeriodState fixture(uint16_t quarter)
{
    Nba97GamePeriodState s{};
    for(auto& v:s.scalar) v=known(0);
    for(auto& team:s.team) for(auto& v:team) v=known(0);
    for(unsigned i=0;i<11;++i) {
        for(auto& v:s.entity[i]) v=known(0);
        s.entity[i][NBA97_PERIOD_ENTITY_08]=known(100+i);
        s.entity[i][NBA97_PERIOD_ENTITY_0C]=known(200+i);
        s.entity_table[i]={(uint8_t)i,1}; s.render_table[i]={(uint8_t)i,1};
    }
    for(unsigned i=0;i<8;++i) { s.controller22[i]=known(0); s.controller_table[i]={(uint8_t)i,1}; }
    s.scalar[NBA97_PERIOD_FDB68]=known(quarter);
    s.scalar[NBA97_PERIOD_1EDF2]=known(77);
    s.team[0][NBA97_PERIOD_TEAM_34]=known(255); s.team[1][NBA97_PERIOD_TEAM_34]=known(127);
    s.team[0][NBA97_PERIOD_TEAM_10]=known(0x80000000u); s.team[1][NBA97_PERIOD_TEAM_10]=known(1);
    s.incoming_s6=known(0xc0000005u);
    return s;
}
/* These callbacks are deliberately synthetic dependency boundaries. They test
 * coordinator ordering; they do not claim the original callees are implemented. */
struct Context {
    unsigned calls=0,stop=999,mode=0,init_calls=0,motion_calls=0;
    int stop_result=0;
    uint8_t expected_formation=0;
    int expected_special=-1;
};
static int boundary(void* data,Nba97GamePeriodState* s,const Nba97GamePeriodCall* c)
{
    auto& x=*static_cast<Context*>(data);
    if(x.calls++==x.stop) return x.stop_result;
    if(c->owner==NBA97_PERIOD_CALL_65B18) {
        assert(c->formation==x.expected_formation && c->argument==x.expected_special);
        assert(c->side==(x.init_calls++?5:0));
    }
    if(c->owner==NBA97_PERIOD_CALL_653E8) {
        assert(c->incoming_s6.known && c->incoming_s6.word==0xc0000005u);
        assert(s->entity[10][NBA97_PERIOD_ENTITY_08].word==0);
        assert(s->entity[10][NBA97_PERIOD_ENTITY_10].word==0x5c00);
    }
    if(x.mode==1) {
        if(c->callsite==0x80065dc4u) s->scalar[NBA97_PERIOD_FDB68]=known(2);
        if(c->callsite==0x800660a8u) s->scalar[NBA97_PERIOD_FDB68]=known(4);
        if(c->callsite==0x80066184u) s->team[1][NBA97_PERIOD_TEAM_10]=known(7);
        if(c->owner==NBA97_PERIOD_CALL_5828C) {
            s->entity_table[5]={7,1}; s->ball_fdc48={8,1};
            s->entity[7][NBA97_PERIOD_ENTITY_08]=known(4321);
            s->entity[7][NBA97_PERIOD_ENTITY_0C]=known(9876);
            s->scalar[NBA97_PERIOD_FDB72]=known(0xffff);
        }
    }
    if(c->owner==NBA97_PERIOD_CALL_56B78) {
        assert(c->argument==39);
        if(x.mode==2) {
            if(!x.motion_calls) { assert(c->entity==0); s->entity_table[5]={3,1}; }
            else assert(c->entity==3);
        }
        ++x.motion_calls;
    }
    return 1;
}
static Nba97GamePeriodDurations durations()
{
    Nba97GamePeriodDurations d{};
    for(unsigned row=0;row<2;++row) for(unsigned i=0;i<256;++i) d.value[row][i]=18000+3600*(i+row);
    return d;
}
int main()
{
    const auto d=durations(); Nba97GamePeriodReceipt r{};
    for(unsigned q=0;q<65536;++q) {
        auto s=fixture((uint16_t)q); Context c;
        c.expected_formation=(q==0 || q==4)?0:1; c.expected_special=c.expected_formation?0:-1;
        assert(nba97_game_period_initialize(&s,&d,boundary,&c,&r)==NBA97_PERIOD_COMPLETE);
        assert(r.complete && r.captured_quarter_known && (uint16_t)r.captured_quarter==q);
        assert(r.completed_calls==(q==0 || q==4?10:q==2?9:8));
        assert(s.scalar[NBA97_PERIOD_1EDF2].word==(q==4?1u:77u));
        assert(s.scalar[NBA97_PERIOD_FDB90].word==(q==0 || q==4?0x81u:0x82u));
        assert(c.init_calls==2);
        assert(r.count<NBA97_PERIOD_EVENT_CAPACITY);
        if(q==3) assert(s.team[0][NBA97_PERIOD_TEAM_34].word==255 && s.team[1][NBA97_PERIOD_TEAM_34].word==4);
        if(q==2) assert(s.team[0][NBA97_PERIOD_TEAM_10].word==0x80000000u && s.team[1][NBA97_PERIOD_TEAM_10].word==0xffffffffu);
    }
    for(unsigned row=0;row<2;++row) for(unsigned option=0;option<256;++option) {
        auto s=fixture((uint16_t)(row?4:0)); s.scalar[NBA97_PERIOD_21D73]=known(option); Context c;
        assert(nba97_game_period_initialize(&s,&d,boundary,&c,&r)==1);
        assert(s.scalar[NBA97_PERIOD_FDB60].word==d.value[row][option]);
    }
    {
        auto s=fixture(0); Context c; c.mode=1; c.expected_formation=1; c.expected_special=0;
        assert(nba97_game_period_initialize(&s,&d,boundary,&c,&r)==1);
        assert(r.captured_quarter==2 && s.scalar[NBA97_PERIOD_FDB68].word==4);
        assert(s.team[1][NBA97_PERIOD_TEAM_10].word==0xfffffff9u);
        assert(s.scalar[NBA97_PERIOD_FDB96].word==0xfffa);
        assert(s.ball_fdc48.record==8 && s.entity[10][NBA97_PERIOD_ENTITY_08].word==4321);
        assert(s.entity[10][NBA97_PERIOD_ENTITY_0C].word==9876 && s.entity[8][NBA97_PERIOD_ENTITY_08].word==108);
    }
    { auto s=fixture(0); Context c; c.mode=2; assert(nba97_game_period_initialize(&s,&d,boundary,&c,&r)==1); assert(c.motion_calls==2); }
    for(unsigned stop=0;stop<10;++stop) for(int result:{0,-1}) {
        auto s=fixture(0); Context c; c.stop=stop; c.stop_result=result;
        assert(nba97_game_period_initialize(&s,&d,boundary,&c,&r)==(result?NBA97_PERIOD_CALLBACK_FAILED:NBA97_PERIOD_CALLBACK_PENDING));
        assert(!r.complete && r.completed_calls==stop && r.count);
        assert(r.event[r.count-1].kind==NBA97_PERIOD_EVENT_CALL && !r.event[r.count-1].call_completed);
    }
    {
        auto s=fixture(0),before=s;
        assert(nba97_game_period_initialize(&s,&d,nullptr,nullptr,&r)==NBA97_PERIOD_CALLBACK_PENDING);
        assert(!std::memcmp(&s,&before,sizeof(s)) && r.count==1 && !r.completed_calls);
        s.scalar[NBA97_PERIOD_21D73]=known(256); before=s; std::memset(&r,0x5a,sizeof(r)); auto old=r;
        assert(nba97_game_period_initialize(&s,&d,nullptr,nullptr,&r)==NBA97_PERIOD_ARGUMENT);
        assert(!std::memcmp(&s,&before,sizeof(s)) && !std::memcmp(&r,&old,sizeof(r)));
    }
    {
        auto s=fixture(0); s.scalar[NBA97_PERIOD_21D73]={}; Context c;
        assert(nba97_game_period_initialize(&s,&d,boundary,&c,&r)==NBA97_PERIOD_UNRESOLVED);
        assert(s.scalar[NBA97_PERIOD_FDBB4].known && !s.scalar[NBA97_PERIOD_FDBB4].word);
        assert(r.completed_calls==1 && r.count==35);
    }
    {
        auto s=fixture(0); s.controller_table[3]={200,1}; Context c;
        assert(nba97_game_period_initialize(&s,&d,boundary,&c,&r)==NBA97_PERIOD_REFERENCE);
        assert(s.controller22[2].word==5 && !s.controller22[3].word && r.completed_calls==1);
    }
    {
        auto s=fixture(0); s.scalar[NBA97_PERIOD_1EDEC]=known(65535); Context c;
        assert(nba97_game_period_initialize(&s,&d,boundary,&c,&r)==1);
        assert(s.scalar[NBA97_PERIOD_FDB60].word==5400 && s.scalar[NBA97_PERIOD_FDB64].word==10);
    }
    std::puts("game_period: 65536 raw quarters, 512 raw options, callback ordering/prefix/provenance tests passed");
}
