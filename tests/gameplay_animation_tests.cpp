#include "gameplay_animation.hpp"
#include <cstdio>
#include <cstdlib>
#include <type_traits>

using Bytes=std::vector<std::uint8_t>;
static unsigned checks;
static void check(bool ok) {++checks;if(!ok){std::fprintf(stderr,"animation resource check%u failed\n",checks);std::exit(1);}}
static void word(Bytes& b,unsigned at,std::uint32_t value) {
    for(unsigned i=0;i<4;++i)b.at(at+i)=std::uint8_t(value>>(i*8));
}
static void seal(Bytes& b) {
    std::uint32_t value=0xffffffffu;
    for(std::size_t i=20;i<b.size();++i) {
        value^=b[i];for(unsigned bit=0;bit<8;++bit)value=(value>>1)^((0u-(value&1u))&0xedb88320u);
    }
    word(b,16,~value);
}
static Bytes pack(const char* magic,unsigned size) {
    Bytes b(20+size);for(unsigned i=0;i<8;++i)b[i]=std::uint8_t(magic[i]);
    word(b,8,1);word(b,12,size);seal(b);return b;
}
static nba97::GameplaySetupResource setup() {
    Bytes raw(0x400);word(raw,0,8);word(raw,4,0x158);
    word(raw,8,0x300);word(raw,0x158,0x310);
    raw[0x300]=8;raw[0x302]=2;raw[0x303]=173;raw[0x307]=3;word(raw,0x308,12);
    raw[0x313]=251;raw[0x317]=7;word(raw,0x318,12);
    return nba97::decodeGameplaySetup(pack("NBA97PER",2112),nba97::decode_gameplay_mocap(std::move(raw)));
}
template<class F> static void refuses(F action) {
    bool failed=false;try{action();}catch(const std::exception&){failed=true;}check(failed);
}
int main(int argc,char** argv) {
    static_assert(!std::is_copy_constructible_v<nba97::GameplayAnimation>);
    static_assert(!std::is_move_constructible_v<nba97::GameplayAnimation>);
    auto b=pack("NBA97ANI",0x30084);
    for(unsigned i=20;i<b.size();++i)b[i]=std::uint8_t((i*29u+(i>>8))&255);
    seal(b);auto source=setup();auto owner=nba97::decodeGameplayAnimation(b,source);
    const auto& view=owner->view();
    const auto& physics=owner->physicsView();
    check(physics.direction.angle_count==257);
    for(unsigned i=0;i<257;++i)check(physics.direction.angle_d72b4[i]==b[20+0xd72b4-0xa850c+i]);
    for(unsigned side=0;side<2;++side) {
        check(physics.boundary_count[side]==8);
        for(unsigned i=0;i<8;++i) {
            const auto raw=b[20+0xb8a54-0xa850c+side*8+i];
            check(int(physics.boundary[side][i])==(raw<128?int(raw):int(raw)-256));
        }
    }
    check(view.clip[0][0].header.flags==0x38 && view.clip[0][0].header.mode2==2 &&
        view.clip[0][0].header.count7==6 && view.clip[0][0].step3==86);
    check(view.clip[1][0].header.count7==7 && view.clip[1][0].step3==251);
    check(view.clip[0][1].header.available==0 && view.clip[0][1].step3==0);
    constexpr unsigned address[]={0xb850c,0xb8538,0xb8564,0xb8590,0xb85bc,0xb85e8,0xb8614};
    for(unsigned table=0;table<7;++table) {
        const auto& map=view.map[table];const int first=(table==2 || table==3)?0:-32768;
        check(map.first_index==first && map.count==65536);
        for(unsigned position:{0u,1u,21u,22u,32767u,32768u,65534u,65535u}) {
            const auto at=std::size_t(std::int64_t(address[table])-0xa850c+(first+std::int64_t(position))*2+20);
            check(map.words[position]==unsigned(b.at(at))+(unsigned(b.at(at+1))<<8));
        }
    }
    auto retained=owner;const auto* borrowed=owner->view().map[3].words;
    const auto old=borrowed[65535];source.reset();b[40]^=1;
    refuses([&]{owner=nba97::decodeGameplayAnimation(b,retained->setup());});
    check(owner==retained && borrowed[65535]==old && retained->setup()->mocap()->header(0,0));
    b=pack("NBA97ANI",0x30084);auto replacement=nba97::decodeGameplayAnimation(b,retained->setup());
    owner=replacement;check(borrowed[65535]==old && owner!=retained);
    for(unsigned size:{0u,8u,20u,196759u}) {
        auto short_pack=b;short_pack.resize(size);refuses([&]{nba97::decodeGameplayAnimation(short_pack,retained->setup());});
    }
    b.push_back(0);refuses([&]{nba97::decodeGameplayAnimation(b,retained->setup());});
    b=pack("NBA97ANI",0x30084);word(b,8,2);refuses([&]{nba97::decodeGameplayAnimation(b,retained->setup());});
    refuses([&]{nba97::decodeGameplayAnimation(b,{});});
    if(argc>1) {
        const std::filesystem::path folder=argv[1];const auto actual=nba97::loadGameplayAnimation(folder/"animation_maps.bin",nba97::loadGameplaySetup(folder));
        check(actual->view().clip[0][77].header.available==1 && actual->view().clip[1][79].header.available==1);
        std::printf("actual private animation generation loaded\n");
    }
    std::printf("GAMEPLAY ANIMATION RESOURCE PASS: %u checks; complete raw index windows, header provenance and lifetime\n",checks);
    return 0;
}
