#include "recovered/game_player_jump.h"
#include <cassert>
#include <cstdio>
#include <cstring>
static Nba97GamePlayerJumpState fixture(){
    Nba97GamePlayerJumpState s{};s.player_count=2;s.status_count=2;s.ball_fdc48={10,1};
    for(unsigned i=0;i<11;++i){
        for(auto& v:s.entity[i])v.known=1;
        s.player_reference[i]={i%2,1};s.status_reference[i]={i%2,1};s.entity[i][NBA97_JUMP_00].word=i;
    }
    for(auto& v:s.global)v.known=1;
    for(unsigned i=0;i<2;++i){s.player[i]={{78,1},{60,1}};s.status20[i]={32767,1};}
    s.rng1edee={0,1};s.global[NBA97_JUMP_FDB90].word=129;return s;
}
struct Calls {unsigned count=0;bool special=false,zero_divisor=false;};
static int observe(void* context,Nba97GamePlayerJumpState* s,const Nba97GameJumpCall* c){
    auto& o=*static_cast<Calls*>(context);++o.count;
    if(o.special){
        if(o.count==1){
            assert(c->owner==NBA97_JUMP_CALL_5A570&&c->argument_count==2);
            assert(c->argument[0]==0xfffffffeu&&c->argument[1]==0xffffffffu);
            s->entity[c->entity][NBA97_JUMP_A4]={0xaaaa,1};s->global[NBA97_JUMP_FDC04]={1,1};
            s->global[NBA97_JUMP_FDC08]={8,1};s->global[NBA97_JUMP_FDC0C]={3,1};
            s->global[NBA97_JUMP_FDC0A]={2,1};s->global[NBA97_JUMP_FDC14]={600,1};
        }else if(o.count==2){
            assert(c->owner==NBA97_JUMP_CALL_5699C&&c->argument[0]==12);
            s->global[NBA97_JUMP_FDC04]={0,1};s->global[NBA97_JUMP_FDC0C]={4,1};
        }else if(o.count==3){
            assert(c->owner==NBA97_JUMP_CALL_56AA4&&c->argument[0]==13);
            s->global[NBA97_JUMP_FDC0C]={5,1};
        }else{
            assert(o.count==4&&c->owner==NBA97_JUMP_CALL_56CE0&&c->argument[0]==14&&c->argument[1]==15);
            assert(s->entity[c->entity][NBA97_JUMP_50].word==5);
            s->global[NBA97_JUMP_FDC0C]={6,1};s->global[NBA97_JUMP_FDC14]={720,1};
            s->global[NBA97_JUMP_FDC0A]={o.zero_divisor?0u:4u,1};
        }
    }else{
        assert(o.count<=3&&c->owner==(o.count==1?NBA97_JUMP_CALL_56B78:NBA97_JUMP_CALL_56CE0));
    }
    return 1; // Synthetic mutable boundary, never a claim that5A570 or motion ran.
}
int main(){
    Nba97GameJumpRngEffects rng{};Nba97GamePeriodValue seed{0,1};
    assert(nba97_game_jump_rng(&rng,&seed)==1&&rng.count==2&&rng.write[0]==0xa5a5&&rng.value==0x4b4a);
    seed={0x4000,1};assert(nba97_game_jump_rng(&rng,&seed)==1&&rng.value==0x9d87);
    seed={0x8000,1};assert(nba97_game_jump_rng(&rng,&seed)==1&&rng.value==0); // Feedback uses bit14.
    auto prior=rng;seed={0,0};assert(nba97_game_jump_rng(&rng,&seed)==NBA97_JUMP_UNRESOLVED&&std::memcmp(&prior,&rng,sizeof(rng))==0);
    auto s=fixture();Calls calls;Nba97GameJumpReceipt receipt{};
    s.entity[10][NBA97_JUMP_08].word=2001;s.entity[10][NBA97_JUMP_0C].word=uint32_t(-61);
    assert(nba97_game_player_jump(&s,0,0,nullptr,observe,&calls,&receipt)==1&&receipt.accepted&&calls.count==3);
    assert(s.entity[0][NBA97_JUMP_14].word==12&&s.entity[0][NBA97_JUMP_16].word==65535);
    assert(s.entity[0][NBA97_JUMP_C4].word==600&&s.rng1edee.word==0x4b4a);
    s=fixture();s.entity[10][NBA97_JUMP_08].word=100000;
    assert(nba97_game_player_jump(&s,0,0,nullptr,observe,&calls,&receipt)==1&&!receipt.accepted&&s.rng1edee.word==0x4b4a);
    assert(receipt.count==3); // Rejected range still consumed RNG; no entity write.
    s=fixture();s.player[0].byte17.word=80;s.entity[10][NBA97_JUMP_10].word=0x4800;
    s.global[NBA97_JUMP_21D93].word=1;s.status20[0].word=0x8000;
    assert(nba97_game_player_jump(&s,0,1,nullptr,nullptr,nullptr,&receipt)==1&&!receipt.accepted&&receipt.count==3);
    assert(s.rng1edee.word==0x4b4a); // Signed status20>>10 gives -32, not +32.
    s=fixture();assert(nba97_game_player_jump(&s,0,0,nullptr,nullptr,nullptr,&receipt)==NBA97_JUMP_PENDING);
    assert(s.entity[0][NBA97_JUMP_C4].word==600&&!receipt.completed&&!receipt.accepted);
    s=fixture();s.ball_fdc48={0,0};s.entity[0][NBA97_JUMP_BE].word=41;
    assert(nba97_game_player_jump(&s,0,0,nullptr,nullptr,nullptr,&receipt)==1&&!receipt.accepted&&receipt.count==0);
    s=fixture();s.entity[10][NBA97_JUMP_10].word=0x10000;
    assert(nba97_game_player_jump(&s,0,0,nullptr,nullptr,nullptr,&receipt)==NBA97_JUMP_REFERENCE&&receipt.count==0);
    s.entity[10][NBA97_JUMP_18]={0,0};
    assert(nba97_game_player_jump(&s,0,0,nullptr,nullptr,nullptr,&receipt)==NBA97_JUMP_UNRESOLVED&&receipt.count==0);
    s=fixture();s.ball_fdc48={0,1};calls={};
    assert(nba97_game_player_jump(&s,0,0,nullptr,observe,&calls,&receipt)==1&&receipt.accepted&&calls.count==3);
    s=fixture();s.global[NBA97_JUMP_FDB90].word=65535;s.entity[0][NBA97_JUMP_C0]={0,0};calls={};
    s.player[0].byte09={0,0};s.status_reference[0]={0,0};
    assert(nba97_game_player_jump(&s,0,0,nullptr,observe,&calls,&receipt)==1&&receipt.accepted&&!s.entity[0][NBA97_JUMP_A6].known);
    const uint8_t motions[]={1,2,3,4,12,13,14,15};Nba97GamePlayerJumpResources resources{};
    resources.motion_b86f4=motions;resources.motion_row_count=2;
    s=fixture();s.player[0].byte17.word=80;s.entity[0][NBA97_JUMP_BC].word=65534;
    calls={0,true,false};
    assert(nba97_game_player_jump(&s,0,0,&resources,observe,&calls,&receipt)==1&&receipt.accepted&&calls.count==4);
    assert(s.entity[0][NBA97_JUMP_A6].word==0xaaaa&&s.entity[0][NBA97_JUMP_50].word==5);
    assert(s.entity[0][NBA97_JUMP_18].word==720&&s.entity[0][NBA97_JUMP_9E].word==316);
    assert(s.entity[0][NBA97_JUMP_1A].word==19&&s.global[NBA97_JUMP_FDB90].word==0);
    s=fixture();s.player[0].byte17.word=80;s.entity[0][NBA97_JUMP_BC].word=65534;calls={0,true,true};
    assert(nba97_game_player_jump(&s,0,0,&resources,observe,&calls,&receipt)==NBA97_JUMP_SOURCE_DIVZERO);
    assert(s.entity[0][NBA97_JUMP_18].word==720&&s.entity[0][NBA97_JUMP_1A].word==0&&!receipt.completed);
    s=fixture();s.player[0].byte17.word=80;
    assert(nba97_game_player_jump(&s,0,0,&resources,nullptr,nullptr,&receipt)==NBA97_JUMP_PENDING);
    assert(receipt.event[3].call.owner==NBA97_JUMP_CALL_5A570&&s.rng1edee.word==0x4b4a);
    std::puts("game_player_jump: shared RNG, rejection prefix, source velocities, precise mutable boundaries and traps passed");
}
