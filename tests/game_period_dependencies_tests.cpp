#include "recovered/game_period_dependencies.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

static Nba97GamePeriodMotionInput motion()
{
    Nba97GamePeriodMotionInput v{};
    v.previous.known=65535;v.previous.height_known=1;
    v.previous.field[NBA97_ANIM_48]=65535;v.previous.field[NBA97_ANIM_4C]=65535;
    v.previous.field[NBA97_ANIM_46]=7;v.previous.field[NBA97_ANIM_4A]=8;
    v.previous.field[NBA97_ANIM_50]=3;v.previous.field[NBA97_ANIM_54]=7;
    v.previous.field[NBA97_ANIM_58]=123;v.previous.field[NBA97_ANIM_5C]=456;
    v.previous.field[NBA97_ANIM_9A]=65535;
    v.request=v.header_index=39;
    v.motion[0]={0,0,20,1};v.motion[1]={0,0,20,1};return v;
}
int main()
{
    Nba97GamePeriodResetEffects reset{};
    for(unsigned raw=0;raw<65536;++raw) {
        Nba97GamePeriodValue phase{raw,1};
        assert(nba97_game_period_reset_phase(&reset,&phase)==1);
        bool cleared=raw<128 || raw>=32768;
        assert(reset.phase.word==(cleared?0:raw) && reset.phase.known);
        assert(reset.count==(cleared?5:4));
        for(unsigned i=0;i<4;++i) {
            assert(reset.field[i]==(i<2?0:65535));
            assert(reset.write[i+(cleared?1:0)].field==i+1);
        }
    }
    Nba97GameRenderSortState sort{};Nba97GameRenderSortEffects ordered{};
    for(unsigned i=0;i<11;++i) {sort.render_table[i]={(uint8_t)i,1};sort.x[i]={10-i,1};sort.index06[i]={65535,1};}
    assert(nba97_game_period_sort_render(&ordered,&sort)==1 && ordered.count==220);
    for(unsigned i=0;i<11;++i) assert(ordered.state.render_table[i].record==10-i && ordered.state.index06[i].word==10-i);
    sort=ordered.state;sort.index06[5]={};
    assert(nba97_game_period_sort_render(&ordered,&sort)==1 && !ordered.count);
    assert(!ordered.state.index06[5].known); // Original does not repair an already sorted stale index.
    for(unsigned i=0;i<11;++i) {sort.render_table[i]={3,1};sort.x[i]={};sort.index06[i]={};}
    sort.x[3]={0x80000000u,1};
    assert(nba97_game_period_sort_render(&ordered,&sort)==1 && !ordered.count);
    assert(!ordered.state.index06[3].known);
    sort.render_table[5]={99,1};std::memset(&ordered,0x5a,sizeof(ordered));auto oldsort=ordered;
    assert(nba97_game_period_sort_render(&ordered,&sort)==NBA97_PERIOD_DEPENDENCY_REFERENCE);
    assert(!std::memcmp(&ordered,&oldsort,sizeof(ordered)));

    Nba97GamePeriodMotionEffects e{};
    for(unsigned count=0;count<256;++count) for(unsigned frame=0;frame<256;++frame) for(unsigned flags=0;flags<2;++flags) {
        auto v=motion();v.operation=NBA97_PERIOD_MOTION_SECONDARY_56AA4;
        v.motion[1].count7=(uint8_t)count;v.previous.field[NBA97_ANIM_54]=(uint16_t)frame;
        v.previous.field[NBA97_ANIM_64]=(uint16_t)flags;
        assert(nba97_game_period_switch_motion(&e,&v)==1);
        bool rewind=frame>=count || flags;
        assert(e.state.field[NBA97_ANIM_54]==(rewind?0:frame));
        assert(e.state.field[NBA97_ANIM_5C]==(rewind?0:456));
        assert(e.state.field[NBA97_ANIM_50]==3 && e.secondary_called && !e.primary_called);
    }
    {
        auto v=motion();v.motion[0].mode2=2;v.motion[0].count7=1;
        assert(nba97_game_period_switch_motion(&e,&v)==1);
        assert(e.state.field[NBA97_ANIM_50]==7 && e.state.field[NBA97_ANIM_58]==456);
        assert(e.state.field[NBA97_ANIM_50]>=v.motion[0].count7); // Source synchronization bypasses count.
        assert(e.state.field[NBA97_ANIM_9A]==0xfff3 && e.secondary_called && e.primary_called);
        v.previous.field[NBA97_ANIM_64]=1;
        assert(nba97_game_period_switch_motion(&e,&v)==1);
        assert(!e.state.field[NBA97_ANIM_50] && !e.state.field[NBA97_ANIM_54]);
    }
    {
        auto v=motion();v.request=0xc0000005u;v.header_index=5;v.motion[0].mode2=2;
        assert(nba97_game_period_switch_motion(&e,&v)==1);
        assert(e.state.field[NBA97_ANIM_4E]==5 && e.state.field[NBA97_ANIM_46]==5 && e.state.field[NBA97_ANIM_4A]==5);
        assert(e.state.field[NBA97_ANIM_50]==3); // Stored halfword5 != full request, so no sync.
        v.request=0x40000005u;
        assert(nba97_game_period_switch_motion(&e,&v)==1 && e.state.field[NBA97_ANIM_4E]==v.previous.field[NBA97_ANIM_4E]);
    }
    {
        auto v=motion();v.previous.field[NBA97_ANIM_48]=0;v.previous.field[NBA97_ANIM_4C]=0;
        v.previous.known=(uint16_t)(v.previous.known&~(1u<<NBA97_ANIM_4C));v.motion[0].available=v.motion[1].available=2;
        assert(nba97_game_period_switch_motion(&e,&v)==1 && !e.count && !e.primary_called && !e.secondary_called);
        v=motion();v.previous.field[NBA97_ANIM_46]=v.previous.field[NBA97_ANIM_4A]=39;
        v.motion[0].available=v.motion[1].available=0;
        assert(nba97_game_period_switch_motion(&e,&v)==1 && !e.count);
        v=motion();v.previous.field[NBA97_ANIM_4A]=39;v.motion[1].available=0;
        assert(nba97_game_period_switch_motion(&e,&v)==1 && e.secondary_called && e.primary_called);
    }
    {
        auto v=motion();v.previous.field[NBA97_ANIM_9A]=0;v.previous.known=(uint16_t)(v.previous.known&~(1u<<NBA97_ANIM_9A));
        assert(nba97_game_period_switch_motion(&e,&v)==1);
        assert((e.written&(1u<<NBA97_ANIM_9A)) && !(e.state.known&(1u<<NBA97_ANIM_9A)));
        v.previous.field[NBA97_ANIM_54]=0;v.previous.known=(uint16_t)(v.previous.known&~(1u<<NBA97_ANIM_54));
        auto before=e;
        assert(nba97_game_period_switch_motion(&e,&v)==NBA97_PERIOD_DEPENDENCY_UNRESOLVED);
        assert(!std::memcmp(&e,&before,sizeof(e)));
        v.motion[1].count7=0;
        assert(nba97_game_period_switch_motion(&e,&v)==1 && (e.state.known&(1u<<NBA97_ANIM_54)));
    }
    {
        auto v=motion();v.operation=NBA97_PERIOD_MOTION_PRIMARY_5699C;v.motion[0].mode2=2;
        v.previous.field[NBA97_ANIM_4A]=39;
        v.previous.field[NBA97_ANIM_54]=v.previous.field[NBA97_ANIM_5C]=0;
        v.previous.known=(uint16_t)(v.previous.known&~((1u<<NBA97_ANIM_54)|(1u<<NBA97_ANIM_5C)));
        assert(nba97_game_period_switch_motion(&e,&v)==1);
        assert(!(e.state.known&(1u<<NBA97_ANIM_50)) && !(e.state.known&(1u<<NBA97_ANIM_58)));
    }
    {
        auto v=motion();assert(nba97_game_period_switch_motion(&e,&v)==1);auto expected=e;
        alignas(Nba97GamePeriodMotionEffects) unsigned char storage[sizeof(Nba97GamePeriodMotionEffects)+sizeof(v)]{};
        std::memcpy(storage,&v,sizeof(v));
        assert(nba97_game_period_switch_motion(reinterpret_cast<Nba97GamePeriodMotionEffects*>(storage),reinterpret_cast<Nba97GamePeriodMotionInput*>(storage))==1);
        assert(!std::memcmp(storage,&expected,sizeof(expected)));
    }
    std::puts("game_period_dependencies: 65536 signed phases, 131072 frame/old-flag combinations, sort aliases/stale indices, motion provenance/quirks passed");
}
