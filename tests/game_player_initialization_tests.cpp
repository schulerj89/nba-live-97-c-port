#include "recovered/game_player_initialization.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>

static void check(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}
static Nba97GameAnimationResetInput animation_input() {
    Nba97GameAnimationResetInput in{};
    in.previous.known=0xffff;in.previous.height_known=1;
    for(unsigned i=0;i<16;++i) in.previous.field[i]=uint16_t(0x100+i);
    in.previous.field[NBA97_ANIM_4E]=0;
    in.previous.field[NBA97_ANIM_50]=3;in.previous.field[NBA97_ANIM_54]=7;
    in.motion[0]={0x20,0,12,1};in.motion[1]={0x20,0,12,1};return in;
}
static Nba97GameAnimationResetEffects reset(const Nba97GameAnimationResetInput& in) {
    Nba97GameAnimationResetEffects out{};const auto before=in;
    check(nba97_game_player_animation_force_reset(&out,&in)==1,"animation reset failed");
    check(!std::memcmp(&in,&before,sizeof(in)),"animation input changed");return out;
}
static uint32_t get(const uint8_t* bytes,unsigned at,unsigned size) {
    uint32_t v=0;for(unsigned i=0;i<size;++i) v|=uint32_t(bytes[at+i])<<(8*i);return v;
}
static Nba97GamePlayerInitializationInput player_input() {
    Nba97GamePlayerInitializationInput in{};in.direction10=-1;in.duration=100;
    in.sum_known=1;in.cumulative_known=1;in.previous_cumulative48=0xfffffff0;
    in.previous_b4=0xffff;in.header32=2;
    const auto anim=animation_input();
    for(unsigned i=0;i<5;++i) {
        in.player_byte_d_known[i]=1;in.previous_animation[i]=anim.previous;
        in.formation[i][0]=int16_t(10+i);in.formation[i][1]=int16_t(-20-int(i));in.formation[i][2]=int16_t(i);
    }
    std::memcpy(in.motion0,anim.motion,sizeof(in.motion0));return in;
}
static Nba97GamePlayerInitializationEffects initialize(const Nba97GamePlayerInitializationInput& in) {
    Nba97GamePlayerInitializationEffects out{};const auto before=in;
    check(nba97_game_player_initialize(&out,&in)==1,"player init failed");
    check(!std::memcmp(&in,&before,sizeof(in)),"player input changed");return out;
}
static void tests() {
    auto in=animation_input();auto out=reset(in);
    check(out.state.field[NBA97_ANIM_50]==3 && out.state.field[NBA97_ANIM_54]==7,"forced reset unconditionally rewound frames");
    check(!(out.written&(1u<<NBA97_ANIM_50)) && !(out.written&(1u<<NBA97_ANIM_54)),"retained frame marked written");
    check(out.state.field[NBA97_ANIM_58]==in.previous.field[NBA97_ANIM_58] &&
          out.state.field[NBA97_ANIM_5C]==in.previous.field[NBA97_ANIM_5C],"retained timing changed");
    check(out.state.field[NBA97_ANIM_9A]==uint16_t(in.previous.field[NBA97_ANIM_9A]&0xfff3),"status masks");
    in.motion[0].mode2=2;in.motion[0].count7=1;out=reset(in);
    check(out.state.field[NBA97_ANIM_50]==7,"original cross-channel out-of-range frame bug repaired");
    check(out.state.field[NBA97_ANIM_58]==in.previous.field[NBA97_ANIM_5C],"secondary timing sync order");
    in.previous.field[NBA97_ANIM_5C]=0;in.previous.known&=uint16_t(0xffffu^(1u<<NBA97_ANIM_5C));out=reset(in);
    check(!(out.state.known&(1u<<NBA97_ANIM_58)) && (out.written&(1u<<NBA97_ANIM_58)),"unknown copied timing fabricated");
    in.motion[1].count7=0;out=reset(in);
    check(out.state.field[NBA97_ANIM_50]==0 && (out.state.known&(1u<<NBA97_ANIM_58)),"secondary zero-count reset must precede sync");
    in=animation_input();in.previous={};in.previous.height_known=1;in.previous.known=1u<<NBA97_ANIM_4E;
    in.motion[0].flags=1;in.motion[1].flags=1;out=reset(in);
    check(out.state.field[NBA97_ANIM_50]==0 && out.state.field[NBA97_ANIM_54]==0,"bit1 clip did not resolve frames");
    check(!(out.state.known&(1u<<NBA97_ANIM_9A)),"unknown status became known zero");
    Nba97GameAnimationResetEffects guard{};const auto before=guard;
    in.motion[1].flags=0;
    check(nba97_game_player_animation_force_reset(&guard,&in)==NBA97_PLAYER_INIT_UNRESOLVED &&
          !std::memcmp(&guard,&before,sizeof(guard)),"unknown decision frame was guessed");
    in.motion[0].flags=0;in.motion[1].flags=0;in.motion[0].count7=0;in.motion[1].count7=0;
    out=reset(in);
    check((out.state.known&(1u<<NBA97_ANIM_50)) && (out.state.known&(1u<<NBA97_ANIM_54)) &&
          out.state.field[NBA97_ANIM_50]==0 && out.state.field[NBA97_ANIM_54]==0,
          "source zero-count comparison must resolve any unsigned unknown frame");
    in=animation_input();in.previous.height10=-1;in.motion_index=37;out=reset(in);
    check(out.state.field[NBA97_ANIM_46]==37 && out.state.field[NBA97_ANIM_4A]==37 &&
          out.state.field[NBA97_ANIM_4E]==0,"airborne motion37/default retention");
    in.motion_index=0;check(nba97_game_player_animation_force_reset(&guard,&in)==NBA97_PLAYER_INIT_MOTION_REFERENCE,"wrong header index accepted");
    in=animation_input();in.motion[0].available=0;
    check(nba97_game_player_animation_force_reset(&guard,&in)==NBA97_PLAYER_INIT_MOTION_REFERENCE,"missing motion header invented");

    unsigned cases=0;
    for(unsigned mode=0;mode<3;++mode) for(unsigned primary_count=0;primary_count<256;++primary_count)
        for(unsigned secondary_count=0;secondary_count<256;++secondary_count) {
            in=animation_input();in.motion[0].mode2=uint8_t(mode);
            in.motion[0].count7=uint8_t(primary_count);in.motion[1].count7=uint8_t(secondary_count);
            out=reset(in);const auto secondary=secondary_count>7 ? 7:0;
            check(out.state.field[NBA97_ANIM_54]==secondary,"secondary count bound");
            check(out.state.field[NBA97_ANIM_50]==(mode==2 ? secondary:primary_count>3 ? 3:0),"primary count/sync bound");
            ++cases;
        }
    auto p=player_input();auto e=initialize(p);
    check(get(e.header_value,0x48,4)==100 && get(e.header_value,0xb4,2)==1,"initial duration or wrapping halfword sum");
    check(!e.header_written[0x32] && !e.header_written[0x16],"team score/lineup overwritten");
    for(unsigned i=0;i<5;++i) {
        const auto& v=e.entity[i];
        check(v.entity_index==i && v.table_slot==i && get(v.value,0,4)==i,"entity index registration");
        check(get(v.value,4,2)==0xffff && get(v.value,6,2)==i,"entity claim/index halfword");
        check(get(v.value,8,4)==uint32_t(10+i)*256 && get(v.value,0xc,4)==(uint32_t(-20-int(i))<<8),"home coordinates");
        check(!v.written[0x20] && !v.written[0xd6] && !v.written[0xcc],"binding/opponent fields clobbered");
        check(get(v.value,0x1a,1)==(i ? 2u:4u) && get(v.value,0xbe,2)==(i ? 0x50u:0u),"center/local actor fields");
    }
    p.period=2;p.side_word=5;p.direction10=0;p.special_center=-1;p.player_byte_d[0]=1;
    e=initialize(p);check(get(e.header_value,0x48,4)==84,"cumulative duration did not wrap");
    check(get(e.entity[0].value,8,4)==(uint32_t(-10)<<8) && get(e.entity[0].value,0xc,4)==(uint32_t(-20)<<8),"direction plus handedness transforms");
    check(get(e.entity[0].value,0x9a,2)==3,"player+D status");
    p.player_byte_d[0]=0;e=initialize(p);
    check(get(e.entity[0].value,0xc,4)==20*256,"special center uses player+D instead of team side");
    p=player_input();p.motion0[0].mode2=2;
    p.previous_animation[0].known&=uint16_t(0xffffu^(1u<<NBA97_ANIM_5C));p.previous_animation[0].field[NBA97_ANIM_5C]=0;
    e=initialize(p);
    check(e.unresolved_written_bytes==2 && e.entity[0].written[0x58] && !e.entity[0].known[0x58],"unknown copied timing receipt");
    p=player_input();p.previous_animation[0].known&=uint16_t(0xffffu^(1u<<NBA97_ANIM_9A));p.previous_animation[0].field[NBA97_ANIM_9A]=0;
    e=initialize(p);check(e.entity[0].known[0x9a] && !e.unresolved_written_bytes,"post-reset real status write did not resolve provenance");
    Nba97GamePlayerInitializationEffects unchanged;std::memset(&unchanged,0xa5,sizeof(unchanged));const auto frozen=unchanged;
    p.player_byte_d_known[4]=0;
    check(nba97_game_player_initialize(&unchanged,&p)==NBA97_PLAYER_INIT_UNRESOLVED &&
          !std::memcmp(&unchanged,&frozen,sizeof(frozen)),"late unknown player partially published");
    p=player_input();p.side_word=6;
    check(nba97_game_player_initialize(&unchanged,&p)==NBA97_PLAYER_INIT_STORAGE,"outside-owned side accepted");
    p=player_input();p.previous_animation[3].height_known=2;
    check(nba97_game_player_initialize(&unchanged,&p)==NBA97_PLAYER_INIT_ARGUMENT,"invalid provenance accepted");
    check(nba97_game_player_initialize(nullptr,&p)==0 && nba97_game_player_initialize(&unchanged,nullptr)==0,"null arguments");
    p=player_input();const auto expected=initialize(p);
    alignas(Nba97GamePlayerInitializationEffects) unsigned char storage[8192]{};
    for(unsigned a=0;a<=1024;a+=512) for(unsigned b=0;b<=1024;b+=512) {
        std::memcpy(storage+a,&p,sizeof(p));
        auto* src=reinterpret_cast<Nba97GamePlayerInitializationInput*>(storage+a);
        auto* dst=reinterpret_cast<Nba97GamePlayerInitializationEffects*>(storage+b);
        check(nba97_game_player_initialize(dst,src)==1 && !std::memcmp(dst,&expected,sizeof(expected)),"overlap changed init effects");
    }
    std::printf("GAME PLAYER INITIALIZATION PASS: %u count/mode pairs; exact fields, source quirks, provenance and guards\n",cases);
}
int main() {try {tests();return 0;}catch(const std::exception& e) {std::fprintf(stderr,"%s\n",e.what());return 1;}}
