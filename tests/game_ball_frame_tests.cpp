#include "game_player_frame.hpp"
#include "recovered/game_ball_frame.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>
namespace {
unsigned checks=0;
void check(bool ok){++checks;if(!ok){std::fprintf(stderr,"ball check %u failed\n",checks);std::abort();}}
struct Fixture {
    std::vector<uint8_t> bytes=std::vector<uint8_t>(0x200000),known=std::vector<uint8_t>(0x200000,1);
    std::vector<Nba97GameBodyCell> cells=std::vector<Nba97GameBodyCell>(0x80000);
    Nba97GameBodyBuffer buffer{bytes.data(),known.data(),bytes.size(),cells.data(),cells.size(),0,1};
    Nba97PlayerProjectionAddress address{0x80000000,1};
    nba97::GamePlayerFrame owner;Nba97PlayerFrameProgress progress{};
    Fixture(){
        owner.buffers=&buffer;owner.buffer_count=1;owner.addresses=&address;owner.address_count=1;
        pointer(0x800fc660,0x80140000);pointer(0x800fc644,0x80140004);
        pointer(0x800fc658,0x80140008);pointer(0x80140008,0x80130000);
        pointer(0x80102924,0x80150000);put(0x800b729c,384);put(0x800dc7fc,500);
        for(unsigned i=0;i<4096;++i)put(0x80150000+i*4,0x55ffffff);
        for(uint32_t base:std::array<uint32_t,3>{0x80103ee4,0x8010b1f0,0x800d9234}){
            put(base,0x09ffffff);put(base+40,0x09ffffff);put(base+7,0x2e,1);put(base+47,0x2e,1);
        }
        for(unsigned i=0;i<9;++i)put(0x800f9fd8+i*2,i%4==0?4096:0,2);
        put(0x800f9ff4,1600);
        auto& g=owner.geometry.root;g.offset_x={256u<<16,1};g.offset_y={120u<<16,1};g.distance={384,1};g.depth_cue_a={0,1};g.depth_cue_b={0,1};
    }
    void put(uint32_t a,uint32_t v,unsigned n=4){const auto o=a-0x80000000;for(unsigned i=0;i<n;++i)bytes[o+i]=uint8_t(v>>(8*i));}
    uint32_t get(uint32_t a,unsigned n=4)const {const auto o=a-0x80000000;uint32_t v=0;for(unsigned i=0;i<n;++i)v|=uint32_t(bytes[o+i])<<(8*i);return v;}
    void pointer(uint32_t a,uint32_t p){cells[(a-0x80000000)/4]={{0,p-0x80000000,1},1};}
    int ball(std::size_t budget=10000){return owner.ball(budget,progress);}
    int shadow(){return owner.ballShadow(10000,progress);}
};
void projected_packets(){
    Fixture f;check(f.ball()==NBA97_BODY_OK&&f.progress.completed&&f.progress.links==2);
    check(f.get(0x80103ef0,1)==64&&f.get(0x80103ef8,1)==95);
    check(f.get(0x80103ef1,1)==96&&f.get(0x80103f01,1)==128);
    check(f.get(0x8010b1fd,1)==127&&f.get(0x8010b20d,1)==96);
    check(f.get(0x80103eec,2)==245&&f.get(0x80103ef4,2)==267);
    check(f.get(0x80103eee,2)==113&&f.get(0x80103efe,2)==127);
    check(f.get(0x8010b1f4,1)==128&&f.get(0x8010b1f5,1)==128&&f.get(0x8010b1f6,1)==128);
    check(f.get(0x80150640)==0x5510b1f0&&f.get(0x8010b1f0)==0x09103ee4&&f.get(0x80103ee4)==0x09ffffff);
    check(f.owner.geometry.root.depth[3].known&&f.owner.geometry.root.depth[3].word==1600);
    check(f.shadow()==NBA97_BODY_OK&&f.progress.shadows==1&&f.progress.links==1);
    check(f.get(0x800d8ef4,2)==65504&&f.get(0x800d8ef8,2)==32);
    check(f.get(0x80153ff8)==0x550d9234);check(f.get(0x800d9234)==0x09ffffff);
    Fixture suppressed;suppressed.put(0x800dcf10,1);check(suppressed.ball()==1&&suppressed.progress.links==1);
    check(suppressed.get(0x8010b1f8,2)==245); // Reflection bytes still updated.
}
void animation_and_banks(){
    for(unsigned frame=0;frame<15;++frame)for(unsigned bank=0;bank<2;++bank){
        Fixture f;f.put(0x80103f9c,frame);f.put(0x8001ede8,bank);const uint32_t offset=bank*40;
        check(f.ball()==1);check(f.get(0x80103ef0+offset,1)==64+(frame%6)*32);
        check(f.get(0x80103ef1+offset,1)==96+(frame/6)*32);
        check(f.get(0x8010b1fd+offset,1)==127+(frame/6)*32);
        check(f.get(0x80103ee4+(1-bank)*40)==0x09ffffff);
    }
    Fixture wrap;wrap.put(0x800dc7fc,UINT32_MAX);wrap.put(0x80103f9c,14);check(wrap.ball()==1);
    check(wrap.get(0x80103f9c)==0&&wrap.get(0x800dc7fc)==500);
    Fixture negative;negative.put(0x800dc7fc,UINT32_MAX);negative.put(0x80103f9c,uint32_t(-17));check(negative.ball()==1);
    check(negative.get(0x80103f9c)==UINT32_MAX&&negative.get(0x80103ef0,1)==32);
    Fixture paused;paused.put(0x80140000,1,2);paused.put(0x800dc7fc,UINT32_MAX);paused.put(0x80103f9c,14);check(paused.ball()==1);
    check(paused.get(0x80103f9c)==14&&paused.get(0x800dc7fc)==UINT32_MAX);
    paused.put(0x80103ed4,1,2);check(paused.ball()==1&&paused.get(0x80103f9c)==0&&paused.get(0x80103ed4,2)==0);
}
void refusal_and_knowledge(){
    Fixture unknown;unknown.known[0x103f9c]=0;check(unknown.ball()==NBA97_BODY_UNKNOWN);
    check(unknown.progress.stopped_pc==0x80049464&&unknown.progress.stores==1);
    Fixture padding;padding.known[0xf9fea]=0;padding.known[0xf9feb]=0;padding.known[0xfea6a]=0;padding.known[0xfea6b]=0;
    check(padding.ball()==1);check(!padding.known[0xf9fea]&&!padding.known[0xfea6b]);
    Fixture camera;camera.known[0xf9fd8]=0;check(camera.ball()==NBA97_BODY_UNKNOWN);
    check(camera.progress.stopped_pc==0x80055f18&&camera.progress.stores==17&&camera.get(0x80103ef8,1)==95);
    Fixture limits;check(limits.ball(7)==NBA97_BODY_JOURNAL_LIMIT);check(!limits.progress.completed&&limits.progress.stores==1);
    Fixture align;align.pointer(0x80102924,0x80150001);check(align.ball()==NBA97_PROJECTION_UNSUPPORTED_ALIGNMENT);
    check(align.progress.stopped_pc==0x80056914&&align.get(0x80103eec,2)==245);
    Fixture no_control;no_control.owner.geometry.root.distance={};check(no_control.ball()==NBA97_BODY_UNKNOWN);
    check(no_control.progress.stopped_pc==0x80056630&&no_control.get(0x80103ef0,1)==64);
}
}
int main(){projected_packets();animation_and_banks();refusal_and_knowledge();std::printf("%u ball checks passed\n",checks);}
