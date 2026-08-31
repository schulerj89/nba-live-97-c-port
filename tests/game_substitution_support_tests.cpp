#include "recovered/game_substitution_support.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
static unsigned checks;
#define CHECK(x) do { ++checks;if(!(x)){std::fprintf(stderr,"line%d: %s\n",__LINE__,#x);std::exit(1);} } while(0)
static void clocks() {
    for(uint32_t old: {0u,1u,0x7fffffffu,0x80000000u,0xffffffffu})
        for(uint32_t sample: {0u,1u,0x7fffffffu,0x80000000u,0xffffffffu}) {
            Nba97GameClockValue a{old,1},b{sample,1};Nba97GameClockEffect out{};
            CHECK(nba97_game_clock_sample(&out,&a,&b)==1);
            CHECK(out.previous164.word==sample && out.previous164.known==1 && out.delta.word==sample-old && out.delta.known==1);
        }
    Nba97GameClockValue unknown{0,0},sample{123,1};Nba97GameClockEffect out{};
    CHECK(nba97_game_clock_sample(&out,&unknown,&sample)==1);
    CHECK(out.previous164.word==123 && out.previous164.known && !out.delta.known && !out.delta.word);
    CHECK(nba97_game_clock_sample(&out,&sample,&unknown)==1);
    CHECK(!out.previous164.known && !out.delta.known);
    std::memset(&out,0xa5,sizeof(out));const auto before=out;unknown.word=9;
    CHECK(nba97_game_clock_sample(&out,&unknown,&sample)==0);
    CHECK(std::memcmp(&out,&before,sizeof(out))==0);
}
struct Trace { std::vector<unsigned> calls;unsigned polls=0,queries=0;bool fail=false,unknown=false; };
static int call(void* context,Nba97GameWaitLiveState*,unsigned boundary,Nba97GameClockValue* reply) {
    auto& t=*static_cast<Trace*>(context);t.calls.push_back(boundary);
    reply->known=t.unknown?0:1;
    if(!t.unknown)reply->word=boundary==NBA97_WAIT_POLL_70068?(t.polls++<2?0x100:0):boundary==NBA97_WAIT_QUERY_31CB8?(t.queries++<2?1:0x100):0;
    return t.fail?0:1;
}
static void waiters() {
    for(unsigned owner=0;owner<2;++owner) {
        Nba97GameWaitCursor cursor{};Nba97GameWaitLiveState state{{0xfffffff0,1},{0xffffffe0,1},{123,1}};Trace t;
        CHECK(nba97_game_wait_begin(&cursor,owner)==NBA97_WAIT_PROGRESS);
        int status=1;unsigned steps=0;
        while(status==1 && steps++<100) {
            state.sample_d7a70.word+=17;
            status=nba97_game_wait_step(&cursor,&state,call,&t);
        }
        CHECK(status==2 && cursor.terminal==1 && steps<100 && state.frame6c.known);
        CHECK(nba97_game_wait_step(&cursor,&state,call,&t)==2);
        if(owner==0)CHECK(t.calls==std::vector<unsigned>({0,2,0,2,0}));
        else CHECK(t.calls==std::vector<unsigned>({1,2,0,3,1,2,0,3,1,0}));
    }
    Nba97GameWaitCursor c{};Nba97GameWaitLiveState s{{100,1},{0,0},{0,0}};Trace t;
    CHECK(nba97_game_wait_begin(&c,0)==1);
    CHECK(nba97_game_wait_step(&c,&s,nullptr,nullptr)==1 && s.previous164.known && !s.frame6c.known);
    CHECK(nba97_game_wait_step(&c,&s,nullptr,nullptr)==NBA97_WAIT_CALLBACK_REQUIRED && c.stage==1 && c.terminal==0);
    t.unknown=true;CHECK(nba97_game_wait_step(&c,&s,call,&t)==NBA97_WAIT_RETURN_UNKNOWN);
    CHECK(c.terminal==2 && nba97_game_wait_step(&c,&s,call,&t)==NBA97_WAIT_ALREADY_FAILED);
    CHECK(nba97_game_wait_begin(&c,0)==1);t={};t.fail=true;
    CHECK(nba97_game_wait_step(&c,&s,call,&t)==1);
    CHECK(nba97_game_wait_step(&c,&s,call,&t)==NBA97_WAIT_CALLBACK_FAILED && c.terminal==2);
}
int main(){clocks();waiters();std::printf("game_substitution_support: %u checks passed\n",checks);}
