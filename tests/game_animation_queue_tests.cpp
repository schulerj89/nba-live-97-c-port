#include "recovered/game_animation_queue.h"
#include <cassert>
#include <cstdio>
#include <cstring>
static Nba97GameAnimationQueueInput fixture() {
    Nba97GameAnimationQueueInput v{};
    v.previous.animation.known=65535;v.previous.animation.height_known=1;
    v.previous.animation.field[NBA97_ANIM_48]=v.previous.animation.field[NBA97_ANIM_4C]=65535;
    v.previous.animation.field[NBA97_ANIM_70]=v.previous.animation.field[NBA97_ANIM_78]=65535;
    v.previous.extra_known=(1u<<23)-1;
    v.previous.queue_blend_known[0]=v.previous.queue_blend_known[1]=15;
    v.request=78;return v;
}
int main() {
    Nba97GameAnimationEffects e{};
    auto v=fixture();assert(nba97_game_animation_queue(&e,&v)==1);
    assert(e.state.animation.field[NBA97_ANIM_70]==78 && e.state.animation.field[NBA97_ANIM_78]==78 && e.store_count==6);
    v.previous=e.state;v.request=79;
    assert(nba97_game_animation_queue(&e,&v)==1);
    assert(e.state.extra[NBA97_ANIM_EXTRA_72]==79 && e.state.extra[NBA97_ANIM_EXTRA_7A]==79);
    assert(e.state.extra[NBA97_ANIM_EXTRA_74]==65535 && e.state.extra[NBA97_ANIM_EXTRA_7C]==65535);
    v=fixture();v.request=0x1234ffffu;v.blend=0xffffffabu;
    assert(nba97_game_animation_queue(&e,&v)==1 && e.state.animation.field[NBA97_ANIM_70]==65535);
    assert(e.state.queue_blend[0][0]==0xab && e.state.queue_blend[1][0]==0xab);
    for(unsigned lock=0;lock<65536;++lock) {
        v=fixture();v.previous.animation.field[NBA97_ANIM_48]=(uint16_t)lock;
        assert(nba97_game_animation_queue(&e,&v)==1);
        assert(e.store_count==(lock<32768?3u:6u));
        assert(e.state.animation.field[NBA97_ANIM_70]==(lock<32768?65535:78));
    }
    v=fixture();v.previous.animation.field[NBA97_ANIM_70]=65534; // Other negative value is occupied.
    v.previous.extra[NBA97_ANIM_EXTRA_72]=7;v.previous.extra[NBA97_ANIM_EXTRA_74]=8;v.previous.extra[NBA97_ANIM_EXTRA_76]=9;
    assert(nba97_game_animation_queue(&e,&v)==1 && e.store_count==3);
    assert(e.state.animation.field[NBA97_ANIM_70]==65534 && e.state.animation.field[NBA97_ANIM_78]==78);
    v=fixture();v.previous.extra[NBA97_ANIM_EXTRA_72]=900;v.previous.extra[NBA97_ANIM_EXTRA_74]=901;
    v.previous.queue_blend[0][1]=0xa5;
    assert(nba97_game_animation_queue(&e,&v)==1);
    assert(e.state.extra[NBA97_ANIM_EXTRA_72]==65535 && e.state.extra[NBA97_ANIM_EXTRA_74]==901 && e.state.queue_blend[0][1]==0xa5);
    v=fixture();v.previous.animation.field[NBA97_ANIM_70]=0;v.previous.animation.known&=uint16_t(65535u^(1u<<NBA97_ANIM_70));
    std::memset(&e,0x5a,sizeof(e));auto old=e;
    assert(nba97_game_animation_queue(&e,&v)==NBA97_ANIMATION_UNRESOLVED && !std::memcmp(&e,&old,sizeof(e)));
    v.previous.animation.field[NBA97_ANIM_48]=0;
    assert(nba97_game_animation_queue(&e,&v)==1 && e.store_count==3); // Unknown locked queue is not consumed.
    std::puts("game_animation_queue:65536 signed locks, tip77->78->79 queue, independent channels/full/stale/truncation/provenance passed");
}
