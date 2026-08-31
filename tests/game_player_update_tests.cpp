#include "recovered/game_player_update.h"
#include <cassert>
#include <cstdio>
static Nba97GamePlayerUpdateState fixture(){
    Nba97GamePlayerUpdateState s{};
    for(unsigned i=0;i<11;++i){s.entity_table[i]={(uint8_t)i,1};for(auto& v:s.entity[i])v.known=1;}
    s.flags_fe8c4={65535,1};return s;
}
struct Observation {unsigned calls;uint32_t first_ea;uint8_t first_ea_known;bool mutate;};
static int observe(void* context,Nba97GamePlayerUpdateState* s,const Nba97GamePlayerUpdateCall* c){
    auto& o=*static_cast<Observation*>(context);
    assert(c->owner==o.calls%2);++o.calls;
    if(o.calls==1){o.first_ea=s->entity[c->entity][NBA97_UPDATE_EA].word;o.first_ea_known=s->entity[c->entity][NBA97_UPDATE_EA].known;}
    if(o.mutate){
        if(c->slot==0&&c->owner==0){
            assert(c->entity==0);s->current_fdc3c={7,1};s->entity_table[0]={8,1};s->entity_table[1]={0,1};
        }
        if(c->slot==0&&c->owner==1){assert(c->entity==0&&s->current_fdc3c.record==7);}
        if(c->slot==1)assert(c->entity==0); // Table change observed; alias runs again.
        if(c->owner==1){
            s->entity[c->entity][NBA97_UPDATE_A8].word=0x3456;
            s->entity[c->entity][NBA97_UPDATE_A6].word=0xabcd;
            s->entity[c->entity][NBA97_UPDATE_A2].word=0xffff;
        }
        s->team_fdc40={0,1};s->flags_fe8c4.word=0x1237;
    }
    return 1; // Explicit synthetic boundary test; no real animation/physics credit.
}
int main(){
    Nba97GamePlayerUpdateReceipt receipt{};auto s=fixture();Observation o{};
    s.entity[0][NBA97_UPDATE_A8].word=1;s.entity[0][NBA97_UPDATE_A6].word=0;
    assert(nba97_game_player_update(&s,observe,&o,&receipt)==1&&o.calls==20);
    assert(o.first_ea==4092&&s.entity[0][NBA97_UPDATE_A8].word==0); // Preserved negative-wrap snap bug.
    s=fixture();o={};s.entity[0][NBA97_UPDATE_A8].word=1;s.entity[0][NBA97_UPDATE_9A].word=2;
    assert(nba97_game_player_update(&s,observe,&o,&receipt)==1&&o.first_ea==61444);
    s=fixture();o={};s.entity[0][NBA97_UPDATE_A6].word=512;
    assert(nba97_game_player_update(&s,observe,&o,&receipt)==1&&o.first_ea==65396);
    assert(s.entity[0][NBA97_UPDATE_A8].word==989); // Exact opposite direction uses negative35.
    s=fixture();o={};s.entity[0][NBA97_UPDATE_1A].word=20;s.entity[0][NBA97_UPDATE_EA]={0,0};s.entity[0][NBA97_UPDATE_9A]={0,0};
    assert(nba97_game_player_update(&s,observe,&o,&receipt)==1&&!o.first_ea_known);
    s=fixture();o={};o.mutate=true;
    assert(nba97_game_player_update(&s,observe,&o,&receipt)==1&&o.calls==20);
    assert(s.entity[0][NBA97_UPDATE_A8].word==(0x3456&1023));
    assert(s.entity[0][NBA97_UPDATE_A6].word==(0xabcd&1023)&&s.entity[0][NBA97_UPDATE_A2].word==1023);
    assert(s.team_fdc40.record==0&&s.flags_fe8c4.word==0x1235); // No end-of-loop team restore.
    assert(s.entity[10][NBA97_UPDATE_A8].word==0); // Normal ball entry untouched.
    s=fixture();assert(nba97_game_player_update(&s,nullptr,nullptr,&receipt)==NBA97_PLAYER_UPDATE_PENDING);
    assert(receipt.count==5&&receipt.event[4].kind==NBA97_UPDATE_CALLBACK&&!receipt.event[4].completed);
    assert(!receipt.completed&&s.team_fdc40.known&&s.current_fdc3c.known&&s.flags_fe8c4.word==65535);
    s=fixture();s.entity_table[0]={0,0};
    assert(nba97_game_player_update(&s,observe,&o,&receipt)==NBA97_PLAYER_UPDATE_UNRESOLVED&&receipt.count==2);
    assert(!s.current_fdc3c.known); // Pointer copy is retained before unresolved dereference.
    s=fixture();s.entity_table[0]={255,1};
    assert(nba97_game_player_update(&s,observe,&o,&receipt)==NBA97_PLAYER_UPDATE_REFERENCE&&receipt.count==2);
    s=fixture();o={};s.entity[0][NBA97_UPDATE_A2]={0,0};s.flags_fe8c4={0,0};
    assert(nba97_game_player_update(&s,observe,&o,&receipt)==1&&!s.entity[0][NBA97_UPDATE_A2].known&&!s.flags_fe8c4.known);
    std::puts("game_player_update: wrap snap/tie/status quirks, live aliases, captured entity, postcall rereads and pending/provenance passed");
}
