#include "recovered/game_animation_advance.h"
#include "recovered/game_animation_queue.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
static uint16_t maps[7][22];
static Nba97GameAnimationResources resources;
static Nba97GameAnimationAdvanceInput fixture() {
    resources={};
    for(unsigned ch=0;ch<2;++ch)for(auto& c:resources.clip[ch])c={{0,0,4,1},16};
    for(unsigned m=0;m<7;++m) {
        for(unsigned i=0;i<22;++i)maps[m][i]=(uint16_t)i;
        resources.map[m]={maps[m],22,0};
    }
    Nba97GameAnimationAdvanceInput v{};
    auto& a=v.previous;
    a.animation.known=65535;a.animation.height_known=1;
    a.extra_known=(1u<<23)-1;a.queue_blend_known[0]=a.queue_blend_known[1]=15;
    a.word00={0,1};a.previous_height2c={0,1};a.actor1a={0,1};
    a.animation.field[NBA97_ANIM_48]=a.animation.field[NBA97_ANIM_4C]=65535;
    a.animation.field[NBA97_ANIM_70]=a.animation.field[NBA97_ANIM_78]=65535;
    v.tick_fdb6c={1,1};v.controlled_fdbcc={65535,1};v.resources=&resources;
    return v;
}
int main() {
    Nba97GameAnimationEffects e{};
    auto v=fixture();
    assert(nba97_game_animation_advance_frames(&e,&v)==1);
    assert(e.state.animation.field[NBA97_ANIM_50]==1 && e.state.animation.field[NBA97_ANIM_54]==1 && e.store_count==4);
    // Secondary queued launch owns the velocity write; actor1A is untouched.
    v=fixture();auto& a=v.previous;
    a.animation.field[NBA97_ANIM_46]=a.animation.field[NBA97_ANIM_4A]=77;
    a.animation.field[NBA97_ANIM_50]=a.animation.field[NBA97_ANIM_54]=3;
    a.extra[NBA97_ANIM_EXTRA_C4]=600;
    resources.clip[1][78].header.flags=0x100;
    Nba97GameAnimationQueueInput q{};q.previous=a;q.request=78;
    assert(nba97_game_animation_queue(&e,&q)==1);
    q.previous=e.state;q.request=79;assert(nba97_game_animation_queue(&e,&q)==1);
    v.previous=e.state;
    assert(nba97_game_animation_advance_frames(&e,&v)==1);
    assert(e.state.animation.field[NBA97_ANIM_46]==78 && e.state.animation.field[NBA97_ANIM_4A]==78);
    assert(e.state.extra[NBA97_ANIM_EXTRA_18]==600 && e.state.actor1a.word==0);
    assert(e.state.animation.field[NBA97_ANIM_70]==79 && e.state.animation.field[NBA97_ANIM_78]==79);
    // Nonzero queued blend starts a forced clip; old normal timing stays stale.
    v=fixture();v.previous.animation.field[NBA97_ANIM_54]=3;v.previous.animation.field[NBA97_ANIM_5C]=23;
    v.previous.animation.field[NBA97_ANIM_78]=78;v.previous.queue_blend[1][0]=40;
    v.previous.queue_blend[1][3]=0xa5;
    assert(nba97_game_animation_advance_frames(&e,&v)==1);
    assert(e.state.animation.field[NBA97_ANIM_4C]==78 && e.state.animation.field[NBA97_ANIM_4A]==0);
    assert(e.state.animation.field[NBA97_ANIM_5C]==23 && e.state.animation.field[NBA97_ANIM_96]==40);
    assert(e.state.queue_blend[1][3]==0xa5 && e.state.extra[NBA97_ANIM_EXTRA_80]==40);
    // Blend addition wraps to16 bits before comparison; promotion discards status bits.
    v=fixture();v.previous.animation.field[NBA97_ANIM_4C]=78;
    v.previous.animation.field[NBA97_ANIM_96]=65535;v.previous.extra[NBA97_ANIM_EXTRA_80]=1;
    assert(nba97_game_animation_advance_frames(&e,&v)==1);
    assert(e.state.animation.field[NBA97_ANIM_4C]==78 && e.state.animation.field[NBA97_ANIM_96]==0);
    v.previous.animation.field[NBA97_ANIM_96]=255;v.previous.animation.field[NBA97_ANIM_9A]=65535;
    assert(nba97_game_animation_advance_frames(&e,&v)==1);
    assert(e.state.animation.field[NBA97_ANIM_4C]==65535 && e.state.animation.field[NBA97_ANIM_9A]==7);
    // Primary mode2 copies secondary state even when frames exceed clip count.
    v=fixture();resources.clip[0][0].header.mode2=2;resources.clip[1][0].header.mode2=4;
    v.previous.animation.field[NBA97_ANIM_54]=1000;v.previous.animation.field[NBA97_ANIM_5C]=7;
    assert(nba97_game_animation_advance_frames(&e,&v)==1);
    assert(e.state.animation.field[NBA97_ANIM_50]==1000 && e.state.animation.field[NBA97_ANIM_58]==7);
    // Transition synchronization reads the OLD primary header, not the new one.
    for(unsigned old_mode: {0u,2u}) {
        v=fixture();resources.clip[0][0].header.mode2=(uint8_t)old_mode;
        resources.clip[0][1].header.mode2=(uint8_t)(2-old_mode);
        resources.clip[1][1].header.mode2=4;v.previous.animation.field[NBA97_ANIM_4A]=1;
        v.previous.animation.field[NBA97_ANIM_50]=3;v.previous.animation.field[NBA97_ANIM_54]=2;
        v.previous.animation.field[NBA97_ANIM_5C]=7;v.previous.animation.field[NBA97_ANIM_70]=1;
        v.previous.extra[NBA97_ANIM_EXTRA_9C]=256;
        assert(nba97_game_animation_advance_frames(&e,&v)==1);
        assert(e.state.animation.field[NBA97_ANIM_50]==(old_mode==2?2:0));
        assert(e.state.animation.field[NBA97_ANIM_58]==(old_mode==2?7:0));
    }
    // Angular endpoints are strict; -step is retained without stepping backwards.
    v=fixture();resources.clip[1][0].header.mode2=3;v.previous.extra[NBA97_ANIM_EXTRA_EA]=65520;
    assert(nba97_game_animation_advance_frames(&e,&v)==1);
    assert(e.state.animation.field[NBA97_ANIM_54]==0 && e.state.animation.field[NBA97_ANIM_5C]==65520);
    // Source default11 exception leaves11 even while both live clips become0.
    v=fixture();v.previous.animation.field[NBA97_ANIM_4E]=11;maps[2][11]=0;
    v.previous.animation.field[NBA97_ANIM_46]=v.previous.animation.field[NBA97_ANIM_4A]=11;
    assert(nba97_game_animation_remap(&e,&v)==1);
    assert(e.state.animation.field[NBA97_ANIM_4E]==11 && e.state.animation.field[NBA97_ANIM_46]==0);
    // Landing is a forced38/40 transition; it resets14/16/E6, not actor1A.
    v=fixture();v.previous.previous_height2c={100,1};v.previous.extra[NBA97_ANIM_EXTRA_14]=v.previous.extra[NBA97_ANIM_EXTRA_16]=99;
    assert(nba97_game_animation_advance(&e,&v)==1);
    assert(e.state.animation.field[NBA97_ANIM_48]==38 && e.state.animation.field[NBA97_ANIM_4C]==38);
    assert(e.state.extra[NBA97_ANIM_EXTRA_14]==0 && e.state.extra[NBA97_ANIM_EXTRA_16]==0 && e.state.extra[NBA97_ANIM_EXTRA_E6]==10);
    // Zero timing is an original infinite loop; never silently replace it with1.
    v=fixture();resources.clip[1][0].step3=0;
    std::memset(&e,0x5a,sizeof(e));auto old=e;
    assert(nba97_game_animation_advance_frames(&e,&v)==NBA97_ANIMATION_SOURCE_NONTERMINATING);
    assert(!std::memcmp(&e,&old,sizeof(e)));
    // Copied unknown forced frame stays unknown when mode2 synchronizes; no fake0.
    v=fixture();v.previous.animation.field[NBA97_ANIM_48]=v.previous.animation.field[NBA97_ANIM_4C]=0;
    resources.clip[0][0].header.mode2=2;resources.clip[1][0].header.mode2=4;
    v.previous.extra_known^=1u<<NBA97_ANIM_EXTRA_56;
    assert(nba97_game_animation_advance_frames(&e,&v)==1);
    assert(!(e.state.extra_known&(1u<<NBA97_ANIM_EXTRA_52)) && e.state.extra[NBA97_ANIM_EXTRA_52]==0);
    v=fixture();v.previous.animation.field[NBA97_ANIM_4A]=84;
    std::memset(&e,0x5a,sizeof(e));old=e;
    assert(nba97_game_animation_advance_frames(&e,&v)==NBA97_ANIMATION_REFERENCE && !std::memcmp(&e,&old,sizeof(e)));
    v=fixture();Nba97GameAnimationEffects expected{};
    assert(nba97_game_animation_advance(&expected,&v)==1);
    union Overlap {
        Nba97GameAnimationAdvanceInput input;
        Nba97GameAnimationEffects effects;
    } overlap{};
    overlap.input=v;
    assert(nba97_game_animation_advance(&overlap.effects,&overlap.input)==1);
    assert(overlap.effects.store_count==expected.store_count);
    assert(!std::memcmp(&overlap.effects.state,&expected.state,sizeof(expected.state)));
    std::puts("game_animation_advance: queue launch/blend/status/landing/angular/remap/copy provenance and failure atomicity passed");
}
