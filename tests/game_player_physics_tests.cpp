#include "recovered/game_player_physics.h"
#include <cassert>
#include <cstdio>
#include <cstring>
static uint8_t angle[257];
static int8_t edges[2][8];
static Nba97GamePlayerPhysicsResources resources;
static Nba97GamePlayerPhysicsState fixture(){
    Nba97GamePlayerPhysicsState s{};
    for(auto& v:s.entity)v.known=1;for(auto& v:s.global)v.known=1;s.team_direction10={1,1};
    s.global[NBA97_PHYSICS_FDBCC].word=65535;s.global[NBA97_PHYSICS_FDB6C].word=1;
    s.entity[NBA97_PHYSICS_C8].word=256;
    for(unsigned i=0;i<257;++i)angle[i]=(uint8_t)(i/2);
    for(auto& row:edges)for(auto& b:row)b=0;
    resources={{angle,257},{edges[0],edges[1]},{8,8}};return s;
}
struct Calls{unsigned count;};
static int callback(void* context,Nba97GamePlayerPhysicsState* s,const Nba97GamePhysicsCall* c){
    auto& calls=*static_cast<Calls*>(context);
    assert(c->owner==calls.count++);
    assert(c->has_argument==(c->owner!=3));
    if(c->owner==0){assert(c->argument==11);s->global[NBA97_PHYSICS_FDB94].word=1;}
    if(c->owner==1)assert(c->argument==5000); // Branch argument was captured before callback0.
    if(c->owner==2)assert(c->argument==9);
    if(c->owner==3){
        s->entity[NBA97_PHYSICS_14].word=10;s->entity[NBA97_PHYSICS_1A].word=12;
        s->global[NBA97_PHYSICS_FE910].word=2;s->entity[NBA97_PHYSICS_EE].word=999;
    }
    return 1;
}
int main(){
    Nba97GamePhysicsReceipt receipt{};auto s=fixture();
    s.entity[NBA97_PHYSICS_18].word=600;
    assert(nba97_game_player_physics(&s,&resources,nullptr,nullptr,&receipt)==1);
    assert(s.entity[NBA97_PHYSICS_18].word==576 && s.entity[NBA97_PHYSICS_10].word==576);
    assert(s.entity[NBA97_PHYSICS_2C].word==0);
    s=fixture();s.entity[NBA97_PHYSICS_10].word=1;s.entity[NBA97_PHYSICS_18].word=65535;
    assert(nba97_game_player_physics(&s,&resources,nullptr,nullptr,&receipt)==1);
    assert(s.entity[NBA97_PHYSICS_10].word==0 && s.entity[NBA97_PHYSICS_18].word==0);
    assert(s.entity[NBA97_PHYSICS_2C].word==1); // Original comparison never creates intendedFF marker.
    s=fixture();s.entity[NBA97_PHYSICS_1A].word=20;s.entity[NBA97_PHYSICS_10].word=1;s.entity[NBA97_PHYSICS_18].word=65280;
    assert(nba97_game_player_physics(&s,&resources,nullptr,nullptr,&receipt)==1);
    assert(s.entity[NBA97_PHYSICS_18].word==70 && s.entity[NBA97_PHYSICS_10].word==0);
    s=fixture();s.entity[NBA97_PHYSICS_08].word=0x20000;s.entity[NBA97_PHYSICS_14].word=1;
    assert(nba97_game_player_physics(&s,&resources,nullptr,nullptr,&receipt)==1);
    assert(s.entity[NBA97_PHYSICS_08].word==0x1a000 && s.entity[NBA97_PHYSICS_14].word==0 && s.entity[NBA97_PHYSICS_C2].word==769);
    s=fixture();s.entity[NBA97_PHYSICS_0C].word=0xc800;edges[0][0]=-1;
    assert(nba97_game_player_physics(&s,&resources,nullptr,nullptr,&receipt)==1);
    assert(s.entity[NBA97_PHYSICS_C2].word==65281);
    s=fixture();s.entity[NBA97_PHYSICS_08].word=0xe000;s.team_direction10.word=0xe000;
    assert(nba97_game_player_physics(&s,&resources,nullptr,nullptr,&receipt)==1);
    assert(s.entity[NBA97_PHYSICS_EE].word==1); // Exact zero XOR passes source<=0 for nonactor1.
    s=fixture();s.entity[NBA97_PHYSICS_08].word=0xe000;s.entity[NBA97_PHYSICS_1A].word=1;
    s.global[NBA97_PHYSICS_FE8E0].word=s.global[NBA97_PHYSICS_FDB58].word=s.global[NBA97_PHYSICS_21D8F].word=1;
    s.entity[NBA97_PHYSICS_EE].word=299;
    auto before=s;
    assert(nba97_game_player_physics(&s,&resources,nullptr,nullptr,&receipt)==NBA97_PHYSICS_CALLBACK_PENDING);
    assert(!receipt.completed && receipt.event[receipt.count-1].kind==2 && !receipt.event[receipt.count-1].completed);
    assert(s.entity[NBA97_PHYSICS_24].word==0xe000 && s.entity[NBA97_PHYSICS_EE].word==299);
    s=before;Calls calls{};
    assert(nba97_game_player_physics(&s,&resources,callback,&calls,&receipt)==1 && calls.count==4);
    assert(s.entity[NBA97_PHYSICS_EE].word==300 && s.global[NBA97_PHYSICS_FE882].word==2);
    assert(s.entity[NBA97_PHYSICS_A0].word==0 && s.entity[NBA97_PHYSICS_9C].word==10); // LaterA0zero does not recompute9C.
    s=fixture();s.entity[NBA97_PHYSICS_14].word=0;s.entity[NBA97_PHYSICS_16].word=0;s.entity[NBA97_PHYSICS_A2]={0,0};
    assert(nba97_game_player_physics(&s,nullptr,nullptr,nullptr,&receipt)==1);
    assert(!s.entity[NBA97_PHYSICS_A2].known); // Zero vector retains unknown direction.
    Nba97GameDirectionEffects e{};
    assert(nba97_game_direction_speed(&e,1,1,{0,0},nullptr)==1 && e.angle.word==128 && e.magnitude==1);
    assert(nba97_game_direction_speed(&e,0,0,{0x345,1},nullptr)==1 && e.count==0 && e.angle.word==0x345);
    assert(nba97_game_direction_speed(&e,1,0,{0,0},&resources.direction)==1 && e.angle.word==256);
    std::memset(&e,0x5a,sizeof(e));auto old=e;
    assert(nba97_game_direction_speed(&e,0x80000000u,0,{0,0},nullptr)==NBA97_PHYSICS_SOURCE_DIVZERO && !std::memcmp(&e,&old,sizeof(e)));
    assert(nba97_game_direction_speed(&e,1,0,{0,0},nullptr)==NBA97_PHYSICS_REFERENCE && !std::memcmp(&e,&old,sizeof(e)));
    std::puts("game_player_physics: gravity/bounce/boundary/sign bugs, callback ordering/prefix, stale movement, direction provenance/traps passed");
}
