#include "recovered/game_body_names.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>
namespace {
unsigned checks=0;
void check(bool ok){++checks;if(!ok){std::fprintf(stderr,"name UV check %u failed\n",checks);std::exit(1);}}
using Ref=Nba97GameBodyReference;
struct Fixture {
    std::array<std::vector<uint8_t>,2> bytes,known;
    std::array<std::vector<Nba97GameBodyCell>,2> cells;
    std::array<Nba97GameBodyBuffer,2> buffers{};Nba97GameBodyNamesState state{};
    std::array<Nba97GameBodyNameWrite,200> journal{};Nba97GameBodyNamesProgress progress{};
    Fixture(){bytes[0].resize(10*0xbcc);bytes[1].resize(10*0x300,0xa7);
        for(unsigned id=0;id<2;++id){known[id].assign(bytes[id].size(),1);cells[id].resize(bytes[id].size()/4);
            buffers[id]={bytes[id].data(),known[id].data(),bytes[id].size(),cells[id].data(),cells[id].size(),0,1};}
        state.buffers=buffers.data();state.buffer_count=2;state.contexts_f0ed8={0,0,1};
        for(unsigned p=0;p<10;++p){pointer(0,p*0xbcc+0x5e4,{1,p*0x300,1});
            for(unsigned b=0;b<2;++b){const uint32_t base=p*0x300+0x20+b*0x160;pointer(1,p*0x300+8+b*4,{1,base,1});
                for(unsigned q=0;q<2;++q){const auto packet=base+0x40+q*0x40;
                    bytes[1][packet+0xc]=static_cast<uint8_t>(q?155:56);bytes[1][packet+0x1c]=static_cast<uint8_t>(q?56:155);
                    known[1][packet+0x14]=0;state.name_center[p][b*2+q]={q?0xffffffffu:7u,static_cast<uint8_t>(q?0:1)};}}}
    }
    void pointer(unsigned id,uint32_t off,Ref ref){cells[id][off/4]={ref,1};for(unsigned i=0;i<4;++i){bytes[id][off+i]=0;known[id][off+i]=0;}}
    int run(size_t cap=200){return nba97_game_body_names(&state,journal.data(),cap,&progress);}
};
void arithmetic(){for(unsigned low=0;low<256;++low){Fixture f;
    for(unsigned p=0;p<10;++p)for(unsigned b=0;b<2;++b)f.state.name_center[p][b*2]={0x80000000u|low,1};
    check(f.run()==NBA97_BODY_OK);check(f.progress.completed&&f.progress.players_completed==10&&f.progress.banks_completed==20&&f.progress.writes==200);
    for(unsigned p=0;p<10;++p)for(unsigned b=0;b<2;++b){
        for(unsigned q=0;q<2;++q){const unsigned i=b*2+q;auto ref=f.state.name_polygon[p][i];
            check(ref.known&&ref.allocation==1&&ref.offset==p*0x300+0x20+b*0x160+0x40+q*0x40);
            check(f.state.name_center[p][i].known&&f.state.name_center[p][i].word==105);
            const auto start=static_cast<uint8_t>(q?104+low:105-low),end=static_cast<uint8_t>(q?105-low:104+low);
            check(f.bytes[1][ref.offset+0xc]==start&&f.bytes[1][ref.offset+0x14]==start&&f.bytes[1][ref.offset+0x1c]==end);
            check(f.known[1][ref.offset+0x14]==1&&f.bytes[1][ref.offset+0xd]==0xa7);}}
    constexpr uint32_t pcs[]={0x800505e4,0x80050610,0x80050648,0x80050684,0x80050688,0x800506a8,0x800506cc,0x800506f0,0x80050714,0x80050738};
    for(unsigned e=0;e<200;++e)check(f.journal[e].pc==pcs[e%10]&&f.journal[e].player==e/20);
}}
void boundaries(){
    {Fixture f;f.state.name_center[0][0]={0xdeadbeef,0};check(f.run()==NBA97_BODY_UNKNOWN);check(f.progress.writes==2&&f.progress.stopped_pc==0x80050624);check(f.state.name_center[0][0].word==0xdeadbeef);}
    {Fixture f;f.known[1][0x20+0x40+0x1c]=0;check(f.run()==NBA97_BODY_UNKNOWN);check(f.progress.writes==2&&f.progress.stopped_pc==0x80050614&&f.progress.index==0);}
    {Fixture f;f.known[1][0x20+0x80+0xc]=0;check(f.run()==NBA97_BODY_UNKNOWN);check(f.progress.writes==3&&f.progress.stopped_pc==0x80050650&&f.progress.index==1);check(f.state.name_center[0][0].word==105);}
    {Fixture f;f.state.name_center[0][1].known=2;check(f.run()==NBA97_BODY_ARGUMENT);check(f.progress.writes==3&&f.progress.stopped_pc==0x80050684);}
    {Fixture f;f.known[0][0x5e4]=0;f.known[0][0x5e7]=2;check(f.run()==NBA97_BODY_ARGUMENT);check(!f.progress.writes);}
    {Fixture f;f.buffers[0].address_mod4_known=0;check(f.run()==NBA97_BODY_ALIGNMENT_UNKNOWN);}
    {Fixture f;f.state.contexts_f0ed8.offset=1;check(f.run()==NBA97_BODY_ALIGNMENT_TRAP);}
    {Fixture f;f.cells[0][0x5e4/4]={};f.known[0][0x5e4]=1;f.known[0][0x5e5]=1;f.known[0][0x5e6]=1;f.known[0][0x5e7]=1;
        check(f.run()==NBA97_BODY_REFERENCE_REQUIRED);}
    {Fixture f;f.pointer(1,0x20+0x40+0x1c,{1,0,1});check(f.run()==NBA97_BODY_ADDRESS_REQUIRED);check(f.progress.writes==2&&f.progress.stopped_pc==0x80050614);}
    {Fixture f;f.pointer(1,0x20+0x40+0x14,{1,0,1});check(f.run()==NBA97_BODY_ADDRESS_REQUIRED);check(f.progress.writes==5&&f.progress.stopped_pc==0x800506a8);}
    {Fixture f;f.pointer(1,8,{});check(f.run()==NBA97_BODY_UNKNOWN);check(f.progress.writes==2&&f.progress.stopped_pc==0x80050614);
        check(!f.state.name_polygon[0][0].known&&!f.state.name_polygon[0][1].known);}
    {Fixture f;f.pointer(1,8,{20,0,1});check(f.run()==NBA97_BODY_BOUNDS);check(f.progress.writes==2);}
    {Fixture f;f.pointer(1,8,{1,0x21,1});check(f.run()==NBA97_BODY_OK);check(f.state.name_polygon[0][0].offset==0x61&&f.state.name_center[0][0].word==167);}
    {Fixture f;f.known[1][0x20+0x40+0x14]=2;check(f.run()==NBA97_BODY_ARGUMENT);check(f.progress.writes==5&&f.progress.stopped_pc==0x800506a8);}
    {Fixture f;f.known[1][0x20+0x40+0x15]=2;check(f.run()==NBA97_BODY_OK);check(f.known[1][0x20+0x40+0x15]==2);}
    {Fixture f;f.pointer(1,8,{1,0xffffffe0u,1});check(f.run()==NBA97_BODY_OK);
        check(f.state.name_polygon[0][0].offset==0x20);}
    for(unsigned cap=0;cap<=200;++cap){Fixture f;auto result=f.run(cap);check(f.progress.writes==cap);check(result==(cap==200?NBA97_BODY_OK:NBA97_BODY_JOURNAL_LIMIT));
        check(f.progress.banks_completed==cap/10&&f.progress.players_completed==cap/20);}
}
void aliases(){
    {Fixture f;f.pointer(1,12,f.cells[1][8/4].reference);f.state.name_center[0][2]={10,1};check(f.run()==NBA97_BODY_OK);
        check(f.state.name_center[0][2].word==104&&f.state.name_center[0][3].word==104);
        check(f.bytes[1][0x20+0x40+0xc]==94&&f.bytes[1][0x20+0x80+0xc]==113);}
    {Fixture f;f.pointer(0,5*0xbcc+0x5e4,{1,0,1});f.state.name_center[5][0]={10,1};check(f.run()==NBA97_BODY_OK);
        check(f.state.name_polygon[5][0].allocation==f.state.name_polygon[0][0].allocation&&f.state.name_polygon[5][0].offset==f.state.name_polygon[0][0].offset);
        check(f.state.name_center[5][0].word==104);}
    {Fixture f;f.pointer(1,12,{1,0x60,1});f.state.name_center[0][2]={10,1};check(f.run()==NBA97_BODY_OK);
        check(f.state.name_center[0][2].word==104&&f.state.name_center[0][3].word==167);}
}
}
int main(){arithmetic();boundaries();aliases();std::printf("game_body_names: %u checks passed\n",checks);}
