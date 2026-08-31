#include "recovered/game_player_label_frame.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
namespace {
unsigned checks;void check(bool ok){++checks;if(!ok){std::fprintf(stderr,"player labels check %u failed\n",checks);std::abort();}}
constexpr uint32_t STYLE=0x80120000,POOL=0x80121000,HEADS=0x80122000,MAP=0x80123000,PACK=0x80124000,ACTOR=0x80125000,OT=0x80130000;
struct Fixture {
    std::vector<uint8_t> bytes=std::vector<uint8_t>(0x200040),known=std::vector<uint8_t>(0x200040,1);
    Nba97PlayerFrameContext context{access,nullptr,nullptr,this,10000};Nba97PlayerFrameProgress progress{};
    static std::size_t off(uint32_t a){return a>=0x80000000?a-0x80000000:0x200000+a-0x1f800000;}
    void put(uint32_t a,uint32_t v,unsigned n=4){for(unsigned i=0;i<n;++i){bytes[off(a)+i]=static_cast<uint8_t>(v>>(8*i));known[off(a)+i]=1;}}
    uint32_t get(uint32_t a,unsigned n=4)const{uint32_t v=0;for(unsigned i=0;i<n;++i)v|=uint32_t(bytes[off(a)+i])<<(8*i);return v;}
    Fixture(){put(0x800b2048,STYLE);put(STYLE+0x10,POOL);put(STYLE+0x14,HEADS);put(STYLE+0x18,PACK);put(STYLE+0x1c,MAP);put(0x80102924,OT);
        for(unsigned i=0;i<10;++i){put(HEADS+0x1ec+i*2,0xffff,2);put(0x80020bec+i*4,ACTOR+i*244);put(0x80106038+i*4,5);put(MAP+i,1,1);}
        put(HEADS+0x1ec,0,2);put(POOL+8,PACK);put(POOL+0xc,1,2);put(POOL+0xe,10,2);put(POOL+0x10,20,2);put(POOL+0x12,1,2);
        for(unsigned offset:{0x16u,0x18u,0x1au,0x1cu})put(POOL+offset,0xffff,2);
        put(POOL+0x1e,2,2);put(POOL+0x20,3,2);put(0x800fea94,100,2);put(0x800fea96,200,2);put(0x80021d84,1,1);
        put(PACK,0x09000000);put(PACK+40+8,5,2);put(PACK+40+16,15,2);put(PACK+40+10,7,2);put(PACK+40+26,17,2);put(OT+20,0xabcdef);
    }
    static int access(void* p,uint32_t,uint32_t a,unsigned n,unsigned kind,Nba97PlayerFrameValue* v){
        auto& f=*static_cast<Fixture*>(p);if(!((a>=0x80000000&&uint64_t(a)+n<=0x80200000)||(a>=0x1f800000&&uint64_t(a)+n<=0x1f800040)))return NBA97_BODY_BOUNDS;
        for(unsigned i=0;i<n;++i)if(f.known[off(a)+i]>1)return NBA97_BODY_ARGUMENT;
        if(kind)f.put(a,v->word,n);else for(unsigned i=0;i<n;++i)if(f.known[off(a)+i]){v->word|=uint32_t(f.bytes[off(a)+i])<<(8*i);v->known_mask|=static_cast<uint8_t>(1u<<i);}
        return 1;
    }
    int run(){return nba97_game_player_label_frame(&context,&progress);}
    int release(uint32_t packet,uint32_t count){return nba97_game_player_label_frame_release_packets(&context,packet,count,&progress);}
};
}
int main(){
    {
        Fixture f;check(f.run()==1&&f.progress.completed&&f.progress.actors==10&&f.progress.indicators==1&&f.progress.links==1);
        check(f.get(POOL+0x1e,2)==90&&f.get(POOL+0x20,2)==183&&f.get(POOL+0x12,2)==1);
        check(f.get(PACK+8,2)==93&&f.get(PACK+24,2)==93&&f.get(PACK+16,2)==103&&f.get(PACK+32,2)==103);
        check(f.get(PACK+10,2)==187&&f.get(PACK+18,2)==187&&f.get(PACK+26,2)==197&&f.get(PACK+34,2)==197);
        check(f.get(PACK)==0x09abcdef&&f.get(OT+20,3)==(PACK&0xffffff));
    }
    for(unsigned route=0;route<3;++route){
        Fixture f;if(route==0)f.put(0x80109afc,1);else if(route==1)f.put(0x1f80000c,1);else {f.put(0x80021d84,0,1);f.put(0x800fdbcc,0xffff,2);f.put(ACTOR+4,0xffff,2);}
        check(f.run()==1);check(f.get(POOL+0x1e,2)==static_cast<uint16_t>(-30)&&f.get(POOL+0x20,2)==static_cast<uint16_t>(-40));
        check(f.get(PACK+8,2)==static_cast<uint16_t>(-27)&&f.get(PACK+10,2)==static_cast<uint16_t>(-36));
    }
    for(uint32_t count:{0u,1u,2u,3u,4u,5u,0x7fffu,0x8000u,0xffffu,0x10001u}){
        Fixture f;f.context.operation_budget=20000;check(f.release(PACK+160,count)==1);
        unsigned expected=(count&0xffff)>=0x8000?0:((count&0xffff)+1)/2;check(f.progress.stores==expected);
        if(expected)check(f.get(MAP+1,1)==0);
        check(f.get(MAP,1)==1);
    }
    {
        Fixture f;f.put(POOL+0x12,0,2);check(f.run()==1&&f.progress.links==0&&f.progress.child_calls==1);
        check(f.get(POOL+0x12,2)==0xffff&&f.get(MAP,1)==0);check(f.get(STYLE+0x3c,2)==0xffff&&f.get(STYLE+0x3e,2)==0xffff&&f.get(HEADS+0x1ec,2)==0xffff);
    }
    {
        Fixture f;f.put(POOL+0xc,0,2);check(f.run()==1&&f.progress.links==0&&f.get(POOL+0x1e,2)==90);
        Fixture cycle;cycle.put(POOL+0x18,0,2);cycle.context.operation_budget=100;check(cycle.run()==NBA97_BODY_JOURNAL_LIMIT&&!cycle.progress.completed&&cycle.progress.stores>0);
        Fixture negative;negative.put(POOL+0xc,0xffff,2);negative.context.operation_budget=100;check(negative.run()==NBA97_BODY_JOURNAL_LIMIT&&!negative.progress.completed&&negative.progress.links>1);
    }
    {
        Fixture f;f.known[Fixture::off(PACK+40+16)]=0;check(f.run()==NBA97_BODY_UNKNOWN&&f.progress.stopped_pc==0x80035e60&&f.progress.stores==4);
        check(f.get(PACK+8,2)==93&&f.get(PACK+16,2)==0);
        Fixture released;released.put(POOL+0x12,0,2);released.known[Fixture::off(STYLE+0x18)]=0;
        check(released.run()==NBA97_BODY_UNKNOWN&&released.progress.stopped_pc==0x8002f118&&released.get(POOL+0x12,2)==0xffff&&released.get(HEADS+0x1ec,2)==0);
        Fixture unaligned;unaligned.put(0x80102924,OT+1);check(unaligned.run()==NBA97_PROJECTION_UNSUPPORTED_ALIGNMENT&&unaligned.progress.stores==10);
    }
    std::printf("game_player_label_frame: %u checks passed\n",checks);
}
