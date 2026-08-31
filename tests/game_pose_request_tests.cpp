#include "recovered/game_pose_request.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

static unsigned checks=0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"line%d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
static void set(Nba97GamePoseEntity& e,unsigned f,unsigned v) { e.half[f]=static_cast<std::uint16_t>(v);e.half_known|=1u<<f; }
static Nba97GameMocapIndex index_fixture() {
    Nba97GameMocapIndex index{};index.header_count=2;
    index.header[0].flags=0x38;index.header[0].count=4;
    index.header[1].flags=0x20;index.header[1].count=7;
    for(unsigned i=0;i<84;++i) index.reference[1][i]=1;
    return index;
}
static void entities_fixture(Nba97GamePoseEntity (&entities)[10]) {
    std::memset(entities,0,sizeof entities);
    for (auto& e:entities) {
        set(e,NBA97_POSE_46,0);set(e,NBA97_POSE_48,0xffff);set(e,NBA97_POSE_4A,83);
        set(e,NBA97_POSE_4C,0x8000);set(e,NBA97_POSE_50,0);set(e,NBA97_POSE_54,6);
        set(e,NBA97_POSE_9A,0xfff3);e.half[NBA97_POSE_8E]=0xbeef;
    }
}
struct Calls { unsigned count=0;unsigned stop=99; };
static int foot(void* context,unsigned row,const Nba97GamePoseEntity* e,unsigned leg,Nba97GameFootOffset* out) {
    auto& calls=*static_cast<Calls*>(context);++calls.count;
    CHECK(e->foot_e0==leg && e->foot_e0_known);
    CHECK(e->half[NBA97_POSE_88]==83 && e->half[NBA97_POSE_90]==6);
    if (row==calls.stop) return 0;
    *out={10,-20,777};return 1;
}
int main() {
    auto index=index_fixture();Nba97GamePoseEntity e[10];unsigned complete=99;
    entities_fixture(e);
    CHECK(nba97_game_pose_requests(&index,e,nullptr,nullptr,&complete)==0 && complete==10);
    for (const auto& entity:e) {
        CHECK(entity.half[NBA97_POSE_84]==0 && entity.half[NBA97_POSE_86]==0xffff);
        CHECK(entity.half[NBA97_POSE_8C]==0 && entity.half[NBA97_POSE_90]==6);
        CHECK(entity.half[NBA97_POSE_8E]==0xbeef && !(entity.half_known&(1u<<NBA97_POSE_8E)));
        CHECK(entity.half[NBA97_POSE_EC]==0 && (entity.half_known&(1u<<NBA97_POSE_EC)));
        Nba97GamePosePacket packet{};CHECK(nba97_game_pose_packet(&entity,&packet)==0);
    }
    entities_fixture(e);set(e[0],NBA97_POSE_50,3);
    CHECK(nba97_game_pose_requests(&index,e,nullptr,nullptr,&complete)==0);
    CHECK(e[0].half[NBA97_POSE_8C]==1 && e[0].half[NBA97_POSE_8E]==0 && e[0].half[NBA97_POSE_94]==128);
    CHECK(e[0].half[NBA97_POSE_9A]==0xfff7);
    index.header[1].flags=0x38;index.header[1].count=7;
    entities_fixture(e);set(e[0],NBA97_POSE_50,3);set(e[0],NBA97_POSE_54,5);
    CHECK(nba97_game_pose_requests(&index,e,nullptr,nullptr,&complete)==0);
    CHECK(e[0].half[NBA97_POSE_8E]==0 && e[0].half[NBA97_POSE_92]==3);
    CHECK(e[0].half[NBA97_POSE_96]==128 && e[0].half[NBA97_POSE_9A]==0xffff);
    index.header[1].count=6;entities_fixture(e);set(e[0],NBA97_POSE_54,5);
    CHECK(nba97_game_pose_requests(&index,e,nullptr,nullptr,&complete)==0);
    CHECK(e[0].half[NBA97_POSE_90]==2 && e[0].half[NBA97_POSE_92]==0);
    index.header[1].flags=0x20;index.header[1].count=7;
    entities_fixture(e);set(e[0],NBA97_POSE_50,65535);
    CHECK(nba97_game_pose_requests(&index,e,nullptr,nullptr,&complete)==0);
    CHECK(e[0].half[NBA97_POSE_8C]==32767 && e[0].half[NBA97_POSE_8E]==32768);
    entities_fixture(e);set(e[1],NBA97_POSE_48,0);set(e[1],NBA97_POSE_50,3);set(e[1],NBA97_POSE_52,65535);
    CHECK(nba97_game_pose_requests(&index,e,nullptr,nullptr,&complete)==0);
    CHECK(e[1].half[NBA97_POSE_8C]==1 && e[1].half[NBA97_POSE_8E]==32767);
    Nba97GamePosePacket untouched;std::memset(&untouched,0xa5,sizeof untouched);const auto before=untouched;
    CHECK(nba97_game_pose_packet(&e[1],&untouched)==NBA97_GAME_POSE_UNKNOWN); // active blend weight never initialized
    CHECK(std::memcmp(&before,&untouched,sizeof before)==0);
    entities_fixture(e);e[3].half_known&=~(1u<<NBA97_POSE_48);
    CHECK(nba97_game_pose_requests(&index,e,nullptr,nullptr,&complete)==NBA97_GAME_POSE_UNKNOWN && complete==3);
    CHECK(e[3].half[NBA97_POSE_60]==0x38 && e[3].half[NBA97_POSE_64]==0x20);
    CHECK(!(e[4].half_known&(1u<<NBA97_POSE_60)));
    entities_fixture(e);set(e[2],NBA97_POSE_46,84);
    CHECK(nba97_game_pose_requests(&index,e,nullptr,nullptr,&complete)==NBA97_GAME_POSE_REFERENCE && complete==2);
    index.header[1].flags=0x40;
    for (unsigned counter: {0u,2u,3u,32767u,32768u,65535u}) {
        entities_fixture(e);
        for(auto& entity:e) {
            entity.word_known=31;entity.word[NBA97_POSE_08]=100;entity.word[NBA97_POSE_0C]=200;
            entity.word[NBA97_POSE_30]=500;entity.word[NBA97_POSE_34]=600;
            entity.foot_e0_known=1;entity.foot_e0=0;set(entity,NBA97_POSE_EC,counter);
        }
        Calls calls;
        CHECK(nba97_game_pose_requests(&index,e,foot,&calls,&complete)==0 && complete==10 && calls.count==10);
        auto next=static_cast<std::uint16_t>(counter+1);
        CHECK(e[0].half[NBA97_POSE_EC]==next);
        if (next<4 || next>=32768) CHECK(e[0].word[NBA97_POSE_30]==110 && e[0].word[NBA97_POSE_34]==180);
        else CHECK(e[0].word[NBA97_POSE_08]==490 && e[0].word[NBA97_POSE_0C]==620);
    }
    entities_fixture(e);e[0].word_known=1u<<NBA97_POSE_10;e[0].foot_e0_known=1;e[0].foot_e0=1;
    Calls calls;calls.stop=0;
    CHECK(nba97_game_pose_requests(&index,e,foot,&calls,&complete)==NBA97_GAME_POSE_FOOT_REQUIRED && complete==0);
    CHECK(e[0].foot_e0==0 && e[0].half[NBA97_POSE_EC]==0 && (e[0].half_known&(1u<<NBA97_POSE_EC)));
    CHECK(!(e[1].half_known&(1u<<NBA97_POSE_60)));
    CHECK(nba97_game_pose_requests(nullptr,e,nullptr,nullptr,&complete)==NBA97_GAME_POSE_ARGUMENT);
    std::printf("game_pose_request: %u checks passed\n",checks);
}
