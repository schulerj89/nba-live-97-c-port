#include "recovered/game_substitution.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1); } } while(0)
static Nba97GameSubstitutionState fixture() {
    Nba97GameSubstitutionState s{};s.duration58=s.remaining60=10800;
    s.flag92=0xfffe;s.saved6c=0x8123;s.phase90=128;s.lock54=1;
    for(unsigned side=0;side<2;++side) {
        auto& t=s.team[side];t.side14=static_cast<uint16_t>(side*5);t.fielda2=17;t.fieldc2=0;t.field77=1;
        for(unsigned i=0;i<12;++i)t.lineup[i]=static_cast<uint16_t>(i);
    }
    for(unsigned i=0;i<10;++i) { s.entity_table[i]=static_cast<uint8_t>(i);s.entity[i]={19,23}; }
    return s;
}
struct Trace {
    std::vector<Nba97GameSubstitutionCall> calls;
    uint32_t query=0;
    int fail=-1,mode=0;
    bool known=true;
};
static int boundary(void* context,Nba97GameSubstitutionState* state,const Nba97GameSubstitutionCall* call,Nba97GameSubstitutionReply* reply) {
    auto& t=*static_cast<Trace*>(context);t.calls.push_back(*call);
    if(call->owner==NBA97_SUB_31CB8) {reply->value=t.query;reply->value_known=t.known?1:0;}
    if(t.mode==1) {
        if(call->owner==NBA97_SUB_353A0) {state->team[0].side14=0xffff;state->team[0].lineup[2]=0xfffe;}
        if(call->owner==NBA97_SUB_646A8)state->lock54=0;
        if(call->owner==NBA97_SUB_A584C) {state->flag92=123;state->saved6c=456;}
    }
    if(t.mode==2 && call->owner==NBA97_SUB_35378) {
        state->remaining60=state->duration58;state->team[0].field34=0;state->marker8e=0x8001;
    }
    if(t.mode==3 && call->owner==NBA97_SUB_64914)state->team[0].side14=0xffff;
    return t.fail==int(t.calls.size()-1)?0:1;
}
static void owners(const Trace& t,std::initializer_list<Nba97GameSubstitutionOwner> expected) {
    CHECK(t.calls.size()==expected.size());unsigned i=0;
    for(auto owner:expected)CHECK(t.calls[i++].owner==owner);
}
static void period_start() {
    auto s=fixture();Trace t;
    CHECK(nba97_game_substitute(&s,1,2,8,0,1,boundary,&t)==NBA97_SUBSTITUTION_OK);
    owners(t,{NBA97_SUB_646A8,NBA97_SUB_A584C});
    CHECK(s.team[1].lineup[2]==8 && s.team[1].lineup[8]==2);
    CHECK(s.team[1].fieldc0==1800 && s.team[1].fieldc2==0 && s.team[1].fielda2==0);
    CHECK(s.entity[7].fieldde==0 && s.entity[7].fielddf==0 && s.entity[2].fieldde==19);
    CHECK(s.flag92==0xfffe && s.saved6c==0x8123);
    s=fixture();s.lock54=0;t={};
    CHECK(nba97_game_substitute(&s,0,6,6,0,0,boundary,&t)==1);
    owners(t,{NBA97_SUB_646A8,NBA97_SUB_63EDC,NBA97_SUB_A584C});
    CHECK(s.entity[6].fieldde==19 && s.team[0].lineup[6]==6);
}
static void live_callbacks() {
    auto s=fixture();s.remaining60=9000;Trace t;t.mode=1;
    CHECK(nba97_game_substitute(&s,0,2,8,0,0,boundary,&t)==1);
    owners(t,{NBA97_SUB_31CB8,NBA97_SUB_64914,NBA97_SUB_35378,NBA97_SUB_29258,NBA97_SUB_64964,
        NBA97_SUB_29258,NBA97_SUB_353A0,NBA97_SUB_7F914,NBA97_SUB_64964,NBA97_SUB_646A8,NBA97_SUB_63EDC,NBA97_SUB_A584C});
    CHECK(s.team[0].fieldc2==0xffff && s.team[0].field34==255 && s.phase90==128 && s.marker8e==16 && s.delaya8==300 && s.flag86==1);
    CHECK(t.calls[6].argument_count==3 && t.calls[6].argument[0]==0 && t.calls[6].argument[1]==8 && t.calls[6].argument[2]==2);
    CHECK(t.calls[7].argument_count==2 && t.calls[7].argument[0]==65535 && t.calls[7].argument[1]==0xfffffffe);
    CHECK(s.flag92==0xfffe && s.saved6c==0x8123); // Actual entry values override final callback mutations.
    s=fixture();s.remaining60=1;t={};t.mode=2;
    CHECK(nba97_game_substitute(&s,0,2,8,0,0,boundary,&t)==1);
    owners(t,{NBA97_SUB_31CB8,NBA97_SUB_64914,NBA97_SUB_35378,NBA97_SUB_29258,NBA97_SUB_64964,NBA97_SUB_646A8,NBA97_SUB_A584C});
    CHECK(s.marker8e==0x8011 && s.team[0].field34==255 && s.delaya8==300);
}
static void signed_branches_and_query() {
    for(uint32_t query=0;query<512;++query) {
        auto s=fixture();s.remaining60=1;s.marker8e=0;s.team[0].field77=0;Trace t;t.query=query;
        CHECK(nba97_game_substitute(&s,0,0,7,0,0,boundary,&t)==1);
        CHECK(t.calls[0].owner==NBA97_SUB_31CB8);
        CHECK(t.calls[1].owner==((query&255)?NBA97_SUB_29258:NBA97_SUB_64914));
        CHECK(t.calls.size()==((query&255)?10:7));
    }
    for(uint16_t phase: {uint16_t(0),uint16_t(127),uint16_t(128),uint16_t(32767),uint16_t(32768),uint16_t(65535)}) {
        auto s=fixture();s.remaining60=1;s.phase90=phase;Trace t;
        CHECK(nba97_game_substitute(&s,0,0,5,0,0,boundary,&t)==1);
        CHECK(t.calls[0].owner==((phase>=128 && phase<32768)?NBA97_SUB_31CB8:NBA97_SUB_29258));
    }
    for(uint16_t marker: {uint16_t(0x10),uint16_t(0x8000),uint16_t(0xffff)}) {
        auto s=fixture();s.remaining60=1;s.marker8e=marker;Trace t;
        CHECK(nba97_game_substitute(&s,0,0,5,0,1,boundary,&t)==1);
        CHECK(t.calls[0].owner==NBA97_SUB_64914 && t.calls[1].owner==NBA97_SUB_64964 && t.calls[2].owner==NBA97_SUB_64914);
    }
}
static void negative_reason_cleanup() {
    auto s=fixture();s.remaining60=1;s.team[0].lineup[11]=0xffff;s.status20[10]=0x8000;Trace t;
    CHECK(nba97_game_substitute(&s,0,2,8,-1,0xffffffff,boundary,&t)==1);
    owners(t,{NBA97_SUB_64914,NBA97_SUB_64964,NBA97_SUB_64914,NBA97_SUB_62BFC,NBA97_SUB_64964,
        NBA97_SUB_29258,NBA97_SUB_353A0,NBA97_SUB_7F84C,NBA97_SUB_64964,NBA97_SUB_646A8,NBA97_SUB_A584C});
    CHECK(s.message8bc==19 && s.player8c8==2);
    CHECK(s.team[0].lineup[2]==8 && s.team[0].lineup[8]==9 && s.team[0].lineup[9]==2);
    // Home's positive player12 crosses into away's status bank in the source.
    s=fixture();s.team[0].lineup[11]=12;s.status20[12]=0;t={};
    CHECK(nba97_game_substitute(&s,0,2,8,-1,0,boundary,&t)==1);
    CHECK(s.team[0].lineup[8]==12 && s.team[0].lineup[11]==2);
    // Stop at bench without inspecting it; source skips all negative status rows.
    s=fixture();for(auto& x:s.status20)x=0xffff;t={};
    CHECK(nba97_game_substitute(&s,0,2,10,-1,0,boundary,&t)==1);
    CHECK(s.team[0].lineup[10]==2 && s.team[0].lineup[11]==11);
}
static void refusal_prefixes() {
    auto s=fixture();const auto before=s;
    CHECK(nba97_game_substitute(&s,2,0,8,0,1,nullptr,nullptr)==NBA97_SUBSTITUTION_ARGUMENT);
    CHECK(std::memcmp(&s,&before,sizeof(s))==0);
    CHECK(nba97_game_substitute(nullptr,0,0,8,0,1,nullptr,nullptr)==NBA97_SUBSTITUTION_ARGUMENT);
    CHECK(nba97_game_substitute(&s,0,2,8,0,1,nullptr,nullptr)==NBA97_SUBSTITUTION_CALLBACK_REQUIRED);
    CHECK(s.team[0].lineup[2]==8 && s.entity[2].fieldde==0 && s.flag92==1);
    s=fixture();Trace t;
    CHECK(nba97_game_substitute(&s,0,2,12,0,1,boundary,&t)==NBA97_SUBSTITUTION_OUTSIDE_STORAGE);
    CHECK(t.calls.empty() && s.team[0].fielda2==0 && s.team[0].fieldc0==1800 && s.flag92==1 && s.team[0].lineup[2]==2);
    s=fixture();s.entity_table[2]=10;t={};
    CHECK(nba97_game_substitute(&s,0,2,8,0,1,boundary,&t)==NBA97_SUBSTITUTION_OUTSIDE_STORAGE);
    CHECK(s.team[0].lineup[2]==8 && s.entity[2].fieldde==19);
    s=fixture();s.remaining60=1;t={};t.known=false;
    CHECK(nba97_game_substitute(&s,0,2,8,0,1,boundary,&t)==NBA97_SUBSTITUTION_RETURN_UNKNOWN);
    CHECK(t.calls.size()==1 && s.team[0].lineup[2]==2 && s.team[0].fieldc2==0xffff && s.flag92==1);
    for(int fail=0;fail<3;++fail) {
        s=fixture();s.lock54=0;t={};t.fail=fail;
        CHECK(nba97_game_substitute(&s,0,2,8,0,1,boundary,&t)==NBA97_SUBSTITUTION_CALLBACK_FAILED);
        CHECK(t.calls.size()==static_cast<unsigned>(fail+1) && s.flag92==1 && s.team[0].lineup[2]==8);
    }
}
int main() {
    period_start();live_callbacks();signed_branches_and_query();negative_reason_cleanup();refusal_prefixes();
    std::printf("game_substitution: %u checks passed\n",checks);
}
