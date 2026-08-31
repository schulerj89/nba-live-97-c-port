#include "recovered/game_camera.h"
#include "game_player_root.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
namespace {
unsigned checks;
void check(bool value){++checks;if(!value){std::fprintf(stderr,"camera check %u failed\n",checks);std::abort();}}
struct Fixture {
    std::vector<uint8_t> bytes=std::vector<uint8_t>(0xd0000),known=std::vector<uint8_t>(0xd0000,1);
    std::array<uint8_t,104> table{},table_known{};
    std::array<uint8_t,64> scratch{},scratch_known{};
    std::array<Nba97GameTextRegion,3> regions{};
    std::vector<Nba97GameCameraEvent> events=std::vector<Nba97GameCameraEvent>(4096);
    Nba97GameCameraContext context{};Nba97GameCameraProgress progress{};
    nba97::GamePlayerRootGeometry geometry;
    uint32_t tick=6;unsigned ticks=0,polls=0,pads=0;bool tick_known=true;uint32_t refuse=0;
    Fixture(){
        table_known.fill(1);scratch_known.fill(1);
        regions={Nba97GameTextRegion{0x800b0000,bytes.data(),known.data(),bytes.size()},Nba97GameTextRegion{0x80026234,table.data(),table_known.data(),table.size()},Nba97GameTextRegion{0x1f800000,scratch.data(),scratch_known.data(),scratch.size()}};
        context.memory={regions.data(),regions.size()};context.access_budget=100000;context.io=callback;context.user=this;
        context.math=nba97::GamePlayerRootGeometry::callback;context.math_user=&geometry;
        put(0x800c4a74,5);put(0x800d7a48,3);put(0x800fc648,0x80170000);put(0x8010b60c,19);put(0x800fcc54,~0u);
        put(0x800b3254,0x10000000);put(0x800f9fe8,0xa5a51234);put(0x800fab98,100,2);put(0x800fab9a,uint16_t(-200),2);put(0x800fab9c,300,2);
        put(0x800fa630,uint16_t(-100),2);put(0x800fa632,200,2);put(0x800fa634,1000,2);
        for(unsigned i=0;i<26;++i)put(0x80026234+i*4,0x8004ed94);
    }
    uint8_t* at(uint32_t address){for(auto& r:regions)if(address>=r.base&&address-r.base<r.size)return r.data+(address-r.base);std::abort();}
    uint8_t* validity(uint32_t address){for(auto& r:regions)if(address>=r.base&&address-r.base<r.size)return r.known+(address-r.base);std::abort();}
    void put(uint32_t address,uint32_t value,unsigned width=4){auto p=at(address);for(unsigned i=0;i<width;++i)p[i]=uint8_t(value>>(i*8));}
    uint32_t get(uint32_t address,unsigned width=4){auto p=at(address);uint32_t v=0;for(unsigned i=0;i<width;++i)v|=uint32_t(p[i])<<(i*8);return v;}
    static int callback(void* u,Nba97GameTextMemory*,const Nba97GameCameraEvent* e,Nba97GamePeriodValue* out){
        auto& f=*static_cast<Fixture*>(u);if(e->target==f.refuse)return 0;*out={0,1};
        if(e->target==0x800a5810){++f.ticks;*out={f.tick_known?f.tick:0,uint8_t(f.tick_known)};}
        else if(e->target==0x80090f6c)++f.polls;
        else if(e->target==0x800913bc){++f.pads;*out={0,1};}
        else return 0;
        return 1;
    }
    int input(uint32_t index,size_t cap=4096){return nba97_game_camera_input_8f224(&context,index,events.data(),cap,&progress);}
    int controller(){return nba97_game_camera_controller(&context,events.data(),events.size(),&progress);}
    int camera(){return nba97_game_camera(&context,events.data(),events.size(),&progress);}
};
void cached_input(){
    Fixture f;check(f.input(0)==1);check(f.progress.completed&&f.ticks==1&&f.polls==1&&f.pads==8);check(f.get(0x800c4a74)==6);
    f.put(0x80103fb4+4*3,0x1234);f.put(0x80103fd4,0x87654321);
    check(f.input(3)==1&&f.progress.return_v0==0x1234);check(f.ticks==2&&f.polls==1&&f.pads==8);
    check(f.input(UINT32_MAX)==1&&f.progress.return_v0==0x87654321);check(f.progress.stores==0);
    f.tick=7;check(f.input(3)==1&&f.progress.return_v0==0);check(f.polls==2&&f.pads==16&&f.get(0x80103fd4)==0);
    Fixture refused;refused.refuse=0x80090f6c;check(refused.input(0)==NBA97_TEXT_IO_REFUSED);check(refused.get(0x800c4a74)==6&&refused.pads==0);
    check(refused.events[refused.progress.events-1].target==0x80090f6c&&!refused.events[refused.progress.events-1].completed);
    Fixture unknown;unknown.tick_known=false;check(unknown.input(0)==NBA97_TEXT_UNKNOWN);check(unknown.get(0x800c4a74)==5);
    Fixture limited;check(limited.input(0,1)==NBA97_TEXT_LIMIT);check(limited.ticks==1&&limited.polls==0&&limited.get(0x800c4a74)==5);
}
void matrix(){
    Fixture f;check(f.camera()==1&&f.progress.completed);check(f.ticks==9&&f.polls==1&&f.pads==8);
    const uint16_t expected[9]={6553,0,0,0,4096,0,0,0,4096};
    for(unsigned i=0;i<9;++i)check(f.get(0x800f9fd8+i*2,2)==expected[i]);
    check(f.get(0x800f9fea,2)==0xa5a5);check(f.get(0x800f9fec)==59);check(f.get(0x800f9ff0)==0);check(f.get(0x800f9ff4)==1300);
    check(f.get(0x800fc61c)==159&&f.get(0x800fc620)==uint32_t(-200)&&f.get(0x800fc624)==300);
    for(const auto& v:f.geometry.vector.translation)check(v.known&&v.word==0);
    Fixture padding;padding.put(0x800eb678,1);*padding.validity(0x800f9fea)=0;*padding.validity(0x800f9feb)=0;*padding.validity(0x800fab9e)=0;*padding.validity(0x800fab9f)=0;
    check(padding.camera()==1&&padding.ticks==0);check(!*padding.validity(0x800f9fea)&&!*padding.validity(0x800fab9e));check(padding.get(0x800f9fec)==59);
    Fixture missing;missing.put(0x800eb678,1);*missing.validity(0x800fb85a)=0;check(missing.camera()==NBA97_TEXT_UNKNOWN);check(missing.progress.stores==0&&missing.progress.stopped_pc==0x80051100);
    Fixture no_math;no_math.context.math=nullptr;no_math.put(0x800eb678,1);check(no_math.camera()==NBA97_TEXT_IO_REFUSED);check(no_math.get(0x800f9fd8,2)==6553&&no_math.get(0x800f9fec)==0);check(no_math.progress.stopped_pc==0x80055f2c);
}
void opaque_copies(){
    for(unsigned delta:std::array<unsigned,6>{0,1,2,3,4,128}){
        Fixture f;f.put(0x800c4a74,6);f.put(0x80103fb4,0x200);f.put(0x800faba4,1);
        constexpr uint32_t source=0x800b31fc;uint32_t destination=source+delta;
        f.put(0x80109a90,destination);std::array<uint8_t,26> before{};
        for(unsigned i=0;i<26;++i){before[i]=uint8_t(21+i*7);f.put(source+i,before[i],1);}
        *f.validity(source+7)=0;*f.validity(source+22)=0;
        check(f.controller()==1);
        for(unsigned i=0;i<26;++i){check(*f.at(destination+i)==before[i]);check(*f.validity(destination+i)==uint8_t(i!=7&&i!=22));}
    }
}
void metadata(){
    Fixture f;f.regions[0].size=SIZE_MAX;check(f.camera()==NBA97_TEXT_ARGUMENT);
    Fixture overlap;overlap.regions[1].base=overlap.regions[0].base;check(overlap.input(0)==NBA97_TEXT_ARGUMENT);
    Fixture alignment;alignment.regions[0].known[0xc4a70-0xb0000]=2;check(alignment.input(0)==NBA97_TEXT_ARGUMENT&&alignment.progress.events==0);
}
}
int main(){cached_input();matrix();opaque_copies();metadata();std::printf("%u camera checks passed\n",checks);}
