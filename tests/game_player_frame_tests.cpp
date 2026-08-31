#include "game_player_frame.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>
namespace {
unsigned checks=0;
void check(bool ok){++checks;if(!ok){std::fprintf(stderr,"frame check %u failed\n",checks);std::abort();}}
struct Fixture {
    std::vector<uint8_t> bytes=std::vector<uint8_t>(0x200000,0),known=std::vector<uint8_t>(0x200000,1);
    std::vector<Nba97GameBodyCell> cells=std::vector<Nba97GameBodyCell>(0x80000);
    Nba97GameBodyBuffer buffer{bytes.data(),known.data(),bytes.size(),cells.data(),cells.size(),0,1};
    Nba97PlayerProjectionAddress address{0x80000000,1};
    nba97::GamePlayerFrame owner;Nba97PlayerFrameProgress progress{};
    Fixture(){owner.buffers=&buffer;owner.buffer_count=1;owner.addresses=&address;owner.address_count=1;}
    void put(uint32_t a,uint32_t v,unsigned n=4){const auto o=a-address.word;for(unsigned i=0;i<n;++i)bytes[o+i]=uint8_t(v>>(8*i));}
    uint32_t get(uint32_t a,unsigned n=4)const {const auto o=a-address.word;uint32_t v=0;for(unsigned i=0;i<n;++i)v|=uint32_t(bytes[o+i])<<(8*i);return v;}
    void pointer(uint32_t a,uint32_t v){put(a,v);cells[(a-address.word)/4]={{0,v-address.word,1},1};}
    void indicator(int x,int y,int claim=0,int selection=-1){
        pointer(0x800fc644,0x80140000);put(0x80140000,1);
        pointer(0x800fc640,0x80140004);put(0x80140004,uint32_t(selection),2);
        pointer(0x800fc64c,0x80140008);put(0x80140008,2);
        pointer(0x800fc660,0x8014000c);put(0x8014000c,0,2);
        pointer(0x800fc638,0x80140010);put(0x80140010,0,2);
        pointer(0x800fc654,0x80130000);put(0x80130004,uint32_t(claim),2);
        put(0x801029b0,0);put(0x8001ede8,0);put(0x800fdbcc,0xffff,2);
        put(0x800fea94,(uint32_t(x)&65535)|(uint32_t(y)<<16));pointer(0x80102924,0x80150000);put(0x80150028,0x7fffffff);
        for(unsigned d=0;d<8;++d){const uint32_t p=0x800faa54+d*40;for(unsigned i=0;i<40;++i)put(p+i,(d*47+i)&255,1);
            put(p+12,250,1);put(p+36,5,1);put(p+13,10,1);put(p+37,20,1);}
        put(0x800b72a0,91,1);put(0x800b72a8,123,1);put(0x800b72b0,199,1);
    }
};
void copy_tests(){Fixture f;const uint32_t base=0x80120000;
    for(int delta=-44;delta<=44;delta+=4){
        for(unsigned i=0;i<160;++i){f.put(base+i,i*13+7,1);f.known[base-f.address.word+i]=1;}
        const uint32_t source=base+48,dest=source+uint32_t(delta);std::array<uint8_t,40> saved{};
        for(unsigned i=0;i<40;++i)saved[i]=uint8_t(f.get(source+i,1));
        check(f.owner.copy40(source,dest,1000,f.progress)==NBA97_BODY_OK);
        for(unsigned i=0;i<40;++i)check(f.get(dest+i,1)==saved[i]);
        check(f.progress.stores==((delta>0&&delta<40)?12u:10u));
    }
    for(unsigned i=0;i<40;++i){f.put(base+i,80+i,1);f.known[base-f.address.word+i]=uint8_t(i%3!=0);}
    check(f.owner.copy40(base,base+64,1000,f.progress)==NBA97_BODY_OK);
    for(unsigned i=0;i<40;++i)check(f.known[base+64-f.address.word+i]==uint8_t(i%3!=0));
    f.known[base-f.address.word]=0;f.known[base+3-f.address.word]=2;
    check(f.owner.copy40(base,base+64,1000,f.progress)==NBA97_BODY_ARGUMENT);check(f.progress.stores==0);
    f.known[base+3-f.address.word]=1;
    check(f.owner.copy40(base+1,base+64,1000,f.progress)==NBA97_PROJECTION_UNSUPPORTED_ALIGNMENT);
    f.known[base-f.address.word]=1;
    for(unsigned i=0;i<40;++i)f.known[base+i-f.address.word]=1;
    check(f.owner.copy40(base,base+64,6,f.progress)==NBA97_BODY_JOURNAL_LIMIT);check(f.progress.stores==1);
    // Opaque packet copies preserve a normalized pointer when explicit source
    // aliases reach it; they do not silently downgrade it to an unusable scalar.
    f.pointer(base,base+128);check(f.owner.copy40(base,base+64,1000,f.progress)==NBA97_BODY_OK);
    check(f.cells[(base+64-f.address.word)/4].is_reference==1);
    check(f.cells[(base+64-f.address.word)/4].reference.offset==base+128-f.address.word);
    f.cells[(base-f.address.word)/4]={{0,0,0},1};
    check(f.owner.copy40(base,base+64,1000,f.progress)==NBA97_BODY_OK);
    check(f.cells[(base+64-f.address.word)/4].is_reference==1&&!f.cells[(base+64-f.address.word)/4].reference.known);
    check(f.known[base+64-f.address.word]==0);
}
void indicators(){Fixture f;const std::array<std::array<int,2>,9> xy{{{{100,-10}},{{600,-10}},{{600,100}},{{600,300}},{{100,300}},{{-10,300}},{{-10,100}},{{-10,-10}},{{100,100}}}};
    for(unsigned d=0;d<xy.size();++d){f.indicator(xy[d][0],xy[d][1]);check(f.owner.indicator(1000,f.progress)==NBA97_BODY_OK);
        if(d==8){check(f.progress.stores==0);continue;}
        check(f.progress.indicators==1);check(f.progress.links==1);check(f.get(0x80106097,1)==((d*47+7)&255));
        check(f.get(0x80106094,1)==91&&f.get(0x80106095,1)==123&&f.get(0x80106096,1)==199);
        check(f.get(0x801060a0,2)-f.get(0x80106098,2)==245);check(f.get(0x801060aa,2)-f.get(0x8010609a,2)==10);
        check(f.get(0x80150028)==0x7f106090);check((f.get(0x80106090)&0xffffff)==0xffffff);
    }
    f.indicator(-10,100,0,-2);check(f.owner.indicator(1000,f.progress)==NBA97_BODY_OK);check(f.progress.indicators==1&&f.progress.links==0);
    f.indicator(-10,100,-1);check(f.owner.indicator(1000,f.progress)==NBA97_BODY_OK);check(f.progress.stores==0);
    f.indicator(-10,100);f.known[0x140004]=0;check(f.owner.indicator(1000,f.progress)==NBA97_BODY_UNKNOWN);check(f.progress.stores==0);
    f.known[0x140005]=2;check(f.owner.indicator(1000,f.progress)==NBA97_BODY_ARGUMENT);f.known[0x140004]=f.known[0x140005]=1;
    f.indicator(-10,100);f.pointer(0x80102924,0x80150001);
    check(f.owner.indicator(1000,f.progress)==NBA97_PROJECTION_UNSUPPORTED_ALIGNMENT);
    check(f.progress.stopped_pc==0x80056914&&f.progress.stopped_address==0x80150029);
    check(f.progress.indicators==1&&f.progress.links==0);
}
void shadow(){Fixture f;f.pointer(0x800f0ed4,0x80120000);f.put(0x80120004,25);f.put(0x80120008,12);f.put(0x801029b0,2);f.put(0x8001ede8,1);
    f.pointer(0x80102924,0x80140000);f.put(0x80143ff8,0x54ffffff);const uint32_t packet=0x800d8f14+200;f.put(packet,0x09000000);
    const std::array<uint32_t,5> identity{{4096,0,4096,0,4096}};for(unsigned i=0;i<5;++i)f.put(0x800f9fd8+i*4,identity[i]);f.put(0x800f9ff4,1000);
    auto& g=f.owner.geometry.root;g.offset_x={256u<<16,1};g.offset_y={120u<<16,1};g.distance={384,1};g.depth_cue_a={0,1};g.depth_cue_b={0,1};
    check(f.owner.shadow(1000,f.progress)==NBA97_BODY_OK);check(f.progress.shadows==1&&f.progress.links==1);
    check(f.get(0x800d8ef4,2)==uint16_t(25-128));check(f.get(0x800d8ef8,2)==140);check(f.get(0x800d8f10,2)==uint16_t(12-128));
    check(f.get(packet)==0x09ffffff);check(f.get(0x80143ff8)==(0x54000000|(packet&0xffffff)));
    check(g.depth[3].known&&g.depth[3].word==884);check(g.screen[2].known);
    f.known[0x0f9fd8]=0;check(f.owner.shadow(1000,f.progress)==NBA97_BODY_UNKNOWN);check(f.progress.stores==12);
}
void initial_prefix(){Fixture f;f.put(0x800b72d4,15);f.put(0x800b72d8,2);f.pointer(0x800f0ed8,0x80120000);f.pointer(0x800fc650,0x80130000);f.pointer(0x800fc654,0x80131000);f.pointer(0x80130000,0x80132000);
    f.put(0x80131098,0xffff,2);f.put(0x80132008,0xfffffff0);f.put(0x8013200c,96);f.put(0x80132010,160);f.put(0x801320a8,123,2);
    const int status=f.owner.run(1000,f.progress);check(status!=NBA97_BODY_OK);check(f.owner.last_child==0x8005200c);
    check(f.get(0x800b72d4)==15&&f.get(0x800b72d8)==0xfffffffe);check(f.get(0x80120004)==0xffffffff&&f.get(0x80120008)==3&&f.get(0x8012000c)==5);
    check(f.get(0x80103edc)==0xfffffffc);check(f.get(0x80120016,2)==123);check(f.cells[0x10292c/4].is_reference==1);
    check(f.progress.actors==0&&f.progress.child_calls==0);check(f.progress.stopped_pc==0x80052b1c);
    const auto retained_flags=f.owner.geometry.root.vector.flags;
    f.known[0x0b72d4]=0;check(f.owner.run(1000,f.progress)==NBA97_BODY_UNKNOWN);
    check(f.owner.last_child==0&&f.owner.last_child_writes.empty());check(f.owner.root_progress.writes==0);
    check(f.owner.geometry.root.vector.flags.word==retained_flags.word&&f.owner.geometry.root.vector.flags.known==retained_flags.known);
    Fixture one_past;one_past.pointer(0x800f0ed8,0x80200000);one_past.pointer(0x800fc650,0x80130000);
    one_past.pointer(0x800fc654,0x80131000);one_past.pointer(0x80130000,0x80132000);
    check(one_past.owner.run(1000,one_past.progress)==NBA97_BODY_BOUNDS);
    check(one_past.progress.stopped_pc==0x800529e0);
    const auto context=one_past.cells[0x0f0ed4/4];check(context.is_reference&&context.reference.known&&context.reference.offset==one_past.bytes.size());
}
}
int main(){copy_tests();indicators();shadow();initial_prefix();std::printf("%u player-frame checks passed\n",checks);}
