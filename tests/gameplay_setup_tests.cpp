#include "gameplay_setup.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

using Bytes=std::vector<std::uint8_t>;
static unsigned checks;
static void check(bool b) {++checks;if(!b){std::fprintf(stderr,"setup check%u failed\n",checks);std::exit(1);}}
static void word(Bytes& b,std::size_t at,std::uint32_t value) {
    for(unsigned i=0;i<4;++i)b.at(at+i)=std::uint8_t(value>>(8*i));
}
static void seal(Bytes& b) {
    std::uint32_t crc=0xffffffffu;
    for(std::size_t i=20;i<b.size();++i) {
        crc^=b[i];for(unsigned n=0;n<8;++n)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);
    }
    word(b,16,~crc);
}
static Bytes pack() {
    Bytes b(2132);const char magic[]="NBA97PER";
    for(unsigned i=0;i<8;++i)b[i]=std::uint8_t(magic[i]);
    word(b,8,1);word(b,12,2112);
    b[20]=0xff;b[21]=0xff;b[52]=0;b[53]=0x80;
    for(unsigned i=0;i<512;++i)word(b,84+4*i,0xa5000000u+i);
    seal(b);return b;
}
static nba97::GameplayMocapResource mocap() {
    Bytes b(0x400);word(b,0,8);word(b,4,0x158);
    word(b,8,0x300);word(b,0x158,0x310);
    b[0x300]=8;b[0x302]=2;b[0x303]=6;b[0x307]=3;word(b,0x308,12);
    b[0x317]=7;word(b,0x318,12);
    return nba97::decode_gameplay_mocap(std::move(b));
}
template<class F> static void refuses(F f) {
    bool threw=false;try{f();}catch(const std::exception&){threw=true;}check(threw);
}
int main() {
    auto bytes=pack();auto motions=mocap();auto owner=nba97::decodeGameplaySetup(bytes,motions);
    check(owner->formation(0)[0][0]==-1 && owner->formation(1)[0][0]==-32768);
    for(unsigned i=0;i<256;++i) {
        check(owner->duration(false,std::uint8_t(i))==0xa5000000u+i);
        check(owner->duration(true,std::uint8_t(i))==0xa5000100u+i);
    }
    const auto primary=owner->motionView(0,0),secondary=owner->motionView(1,0);
    check(primary.available==1 && primary.flags==0x38 && primary.mode2==2 && primary.count7==6);
    check(secondary.available==1 && secondary.flags==0x20 && secondary.count7==7);
    check(owner->motionView(0,1).available==0);
    refuses([&]{owner->motionView(2,0);});refuses([&]{owner->motionView(0,84);});
    refuses([&]{owner->formation(2);});refuses([&]{nba97::decodeGameplaySetup(bytes,{});});
    auto retained=owner;motions.reset();bytes[20]^=1;
    refuses([&]{owner=nba97::decodeGameplaySetup(bytes,owner->mocap());});
    check(owner==retained && retained->formation(0)[0][0]==-1);
    bytes=pack();bytes.push_back(0);refuses([&]{nba97::decodeGameplaySetup(bytes,owner->mocap());});
    for(std::size_t n: {std::size_t(0),std::size_t(8),std::size_t(20),std::size_t(2131)}) {
        bytes=pack();bytes.resize(n);refuses([&]{nba97::decodeGameplaySetup(bytes,owner->mocap());});
    }
    bytes=pack();word(bytes,8,2);refuses([&]{nba97::decodeGameplaySetup(bytes,owner->mocap());});
    // Connect actual resolver-derived views to the recovered initializer.
    Nba97GamePlayerInitializationInput in{};Nba97GamePlayerInitializationEffects out{};
    in.side_word=0;in.direction10=1;in.special_center=-1;in.sum_known=1;
    in.motion0[0]=primary;in.motion0[1]=secondary;
    for(unsigned i=0;i<5;++i) {
        in.player_byte_d_known[i]=1;
        in.previous_animation[i].known=0xffff;
        in.previous_animation[i].height_known=1;
        in.previous_animation[i].field[NBA97_ANIM_54]=6;
        in.previous_animation[i].field[NBA97_ANIM_5C]=111;
    }
    check(nba97_game_player_initialize(&out,&in)==NBA97_PLAYER_INIT_OK);
    for(const auto& entity:out.entity) {
        check(entity.written[0x50] && entity.known[0x50] && entity.value[0x50]==6);
        check(entity.value[0x58]==111); // Preserve out-of-range primary copiedframe.
    }
    check(out.unresolved_written_bytes==0);
    std::printf("GAMEPLAY SETUP PASS: %u checks; signed data, fullraw lookup windows, resource lifetime and source reset views\n",checks);
}
