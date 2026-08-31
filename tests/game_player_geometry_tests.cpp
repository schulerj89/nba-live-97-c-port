#include "game_player_geometry.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>
namespace {
unsigned checks=0;
void require(bool v,const char* why){++checks;if(!v){std::cerr<<why<<'\n';std::exit(1);}}
Nba97GameBodyReference ref(std::uint32_t offset,unsigned allocation=0){return {allocation,offset,1};}
struct Fixture {
    std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x9000,0xa5),known=std::vector<std::uint8_t>(0x9000,1);
    std::vector<Nba97GameBodyCell> cells=std::vector<Nba97GameBodyCell>(0x2400);
    Nba97GameBodyBuffer buffer{};nba97::GamePlayerGeometry geometry{};
    Nba97GamePlayerGeometryInput input{};Nba97GamePlayerGeometryProgress progress{};
    std::array<Nba97GamePlayerGeometryWrite,512> journal{};
    void word(unsigned at,std::uint32_t v){for(unsigned i=0;i<4;++i)bytes[at+i]=std::uint8_t(v>>(i*8));}
    std::uint32_t word(unsigned at)const{return bytes[at]|(std::uint32_t(bytes[at+1])<<8)|(std::uint32_t(bytes[at+2])<<16)|(std::uint32_t(bytes[at+3])<<24);}
    void pointer(unsigned at,unsigned to){cells[at/4].is_reference=1;cells[at/4].reference=ref(to);}
    void matrix(unsigned at){for(unsigned i=0;i<5;++i)word(at+i*4,0);word(at,4096);word(at+8,4096);word(at+16,4096);word(at+20,10);word(at+24,20);word(at+28,30);}
    Fixture(){
        buffer={bytes.data(),known.data(),bytes.size(),cells.data(),cells.size(),0,1};
        input.buffers=&buffer;input.buffer_count=1;input.math=nba97::GamePlayerGeometry::callback;input.math_user=&geometry;
        input.context_f0ed4=ref(0);input.root_10292c=ref(4);input.work_f1c4c=ref(8);input.work_f9cf8=ref(12);
        input.work_f9c54=ref(16);input.work_f9d00=ref(20);input.foot_f9d04=ref(24);input.foot_fea38=ref(28);
        input.hand_f0fb4=ref(32);input.hand_fc62c=ref(36);input.angle_103edc=ref(40);input.trig_b3254=ref(0x4000);
        pointer(0,0x1000);pointer(4,0x2000);matrix(0x2000);
        for(unsigned i=0;i<4;++i){pointer(8+i*4,0x2100+i*32);matrix(0x2100+i*32);pointer(24+i*4,0x2200+i*8);}
        word(40,0);word(0x1004,10);word(0x100c,20);word(0x1008,30);
        pointer(0x1bbc,0x3000);pointer(0x1bc0,0x3100);
        for(unsigned i=0;i<0x200;++i)bytes[0x3000+i]=0;
        word(0x4000,0x10000000);
        for(unsigned i=0;i<20;++i){const unsigned p=0x1024+i*0x94;pointer(p+0x80,0x2000);pointer(p+0x84,0x2000);pointer(p+0x88,0x3200+i*8);word(0x3200+i*8,0x00020001);word(0x3204+i*8,3);}
        //21st prefetch intentionally contains raw unknown, not a fake pointer.
        for(unsigned i=0;i<4;++i)known[0x1024+20*0x94+0x80+i]=0;
    }
    int run(std::size_t capacity=512){return nba97_game_player_geometry(&input,journal.data(),capacity,&progress);}
};
void owner(){
    Fixture complete;require(complete.run()==1,"complete55368");require(complete.progress.parts_completed==20&&complete.progress.completed,"20parts");
    require(complete.progress.writes==293,"source store count");
    for(unsigned i=0;i<20;++i){unsigned p=0x1024+i*0x94;require(complete.word(p)==4096&&complete.word(p+8)==4096&&complete.word(p+16)==4096,"primaryrotation");
        require(complete.word(p+0x54)==11&&complete.word(p+0x58)==22&&complete.word(p+0x5c)==33,"fullMACendpoint");
        if(i<8){require(complete.word(p+0x20+8)==0xf000,"mirrorY");require(complete.word(p+0x20+16)==0xfffff000,"signextendedIRpad");require(complete.word(p+0x74)==11&&complete.word(p+0x78)==18&&complete.word(p+0x7c)==27,"mirroredendpoint");}}
    for(unsigned h=0;h<4;++h){unsigned p=0x2200+h*8;unsigned n=h<2?4:6;require(complete.word(p)==n+(n*2<<16),"hotpointXheight");require((complete.word(p+4)&65535)==n*3,"hotpointZ");require((complete.word(p+4)>>16)==0xa5a5,"hotpointpaddinguntouched");}
    for(std::size_t cap=0;cap<complete.progress.writes;++cap){Fixture f;require(f.run(cap)==NBA97_BODY_JOURNAL_LIMIT,"boundedjournal");require(f.progress.writes==cap,"retainedprefixlength");
        for(std::size_t i=0;i<cap;++i)require(f.journal[i].pc==complete.journal[i].pc&&f.journal[i].word==complete.journal[i].word,"retainedprefixvalues");}
    Fixture malformed;malformed.known[0x1024]=0;malformed.known[0x1026]=2;require(malformed.run()==NBA97_BODY_ARGUMENT&&malformed.progress.writes==0,"write canonical checks allbytes");
    Fixture late;late.cells[(0x1024+20*0x94+0x80)/4].is_reference=2;require(late.run()==NBA97_BODY_ARGUMENT,"final prefetch validates reached metadata");require(late.progress.writes==complete.progress.writes&&late.progress.stopped_pc==0x8005594c,"final prefetch retains all stores");
    Fixture dead;dead.cells[16/4].reference={0,0,0};require(dead.run()==NBA97_BODY_UNKNOWN,"deferred workpointer unknown");require(dead.progress.parts_completed==9&&dead.progress.writes>100&&dead.progress.stopped_pc==0x80055dd8,"unknown workS6 first reached write");
    Fixture aligned;aligned.cells[(0x1024+0x88)/4].reference.offset+=2;require(aligned.run()==NBA97_BODY_ALIGNMENT_TRAP&&aligned.progress.writes==5,"pivot source LW alignment prefix");
    Fixture alias;alias.cells[36/4].reference=ref(0x2200);require(alias.run()==1,"hotpoint destination alias");require(alias.word(0x2200)==0x000c0006,"laterhand overwrites priorfoot");
    Fixture unknown;unknown.known[0x3104]=0;require(unknown.run()==NBA97_BODY_UNKNOWN&&unknown.progress.writes==0,"unknown pose is not zero-filled");
    Fixture untouched;untouched.known[0x1024+0x18]=2;require(untouched.run()==1&&untouched.known[0x1024+0x18]==2,"unread padding is not preflighted");
    Fixture pad;pad.known[0x2012]=pad.known[0x2013]=0;
    for(unsigned i=0;i<20;++i)pad.known[0x3206+i*8]=pad.known[0x3207+i*8]=0;
    require(pad.run()==1,"discarded highhalves may remain unknown");require(pad.known[0x2012]==0&&pad.known[0x3206]==0,"no fabricated padding knowledge");
    Fixture badpad;badpad.known[0x2012]=2;require(badpad.run()==NBA97_BODY_ARGUMENT&&badpad.progress.stopped_pc==0x80055640,"discarded highhalf still canonical checked");
    require(badpad.geometry.rotation[3].known&&!badpad.geometry.rotation[4].known,"earlier math load prefix retained");
    Fixture shortContext;std::vector<std::uint8_t> cb(shortContext.bytes.begin()+0x1000,shortContext.bytes.begin()+0x1bcc),ck(cb.size(),1);
    std::vector<Nba97GameBodyCell> cc(shortContext.cells.begin()+0x400,shortContext.cells.begin()+0x400+(cb.size()+3)/4);
    std::array<Nba97GameBodyBuffer,2> split={shortContext.buffer,Nba97GameBodyBuffer{cb.data(),ck.data(),cb.size(),cc.data(),cc.size(),0,1}};
    shortContext.cells[0].reference=ref(0,1);shortContext.input.buffers=split.data();shortContext.input.buffer_count=split.size();
    require(shortContext.run()==NBA97_BODY_BOUNDS,"context-only allocation exposes original final prefetch");
    require(shortContext.progress.writes==complete.progress.writes&&shortContext.progress.parts_completed==20&&shortContext.progress.stopped_pc==0x8005594c,"full store prefix before missing21stslot");
    Fixture partialPointer;partialPointer.cells[32/4].reference=ref(0x1024+0x80);
    require(partialPointer.run()==NBA97_BODY_ADDRESS_REQUIRED&&partialPointer.progress.stopped_pc==0x80055eb8,"half overwrite requires actual numeric pointer bits");
}
void math(){
    nba97::GamePlayerGeometry g;Nba97GamePeriodValue out{};Nba97PlayerMathRequest r{};
    r.kind=NBA97_PLAYER_ROTATE;require(g.apply(r,out)==NBA97_BODY_UNKNOWN,"unknowngeometry");
    for(unsigned i=0;i<5;++i){r={0,i==0||i==2||i==4?4096u:0u,NBA97_PLAYER_ROTATION,i};require(g.apply(r,out)==1,"loadrotation");}
    r={0,0x80007fff,NBA97_PLAYER_VERTEX,0};require(g.apply(r,out)==1,"loadxy");r={0,0xdeadffff,NBA97_PLAYER_VERTEX,1};require(g.apply(r,out)==1&&g.vertex[1].word==0xffffffff,"signedvertexZ");
    r={0,0,NBA97_PLAYER_ROTATE,0};require(g.apply(r,out)==1,"no translation dependency");require(g.mac[0].word==32767&&g.mac[1].word==0xffff8000&&g.mac[2].word==0xffffffff,"identityMAC");
    require(g.flags.word==0,"identityflags");r.kind=NBA97_PLAYER_TRANSFORM;require(g.apply(r,out)==NBA97_BODY_UNKNOWN,"translation required");
    for(auto& t:g.translation)t={0x7fffffffu,1};
    require(g.apply(r,out)==1,"wide transform");require(g.mac[0].word!=g.ir[0].word&&g.flags.word!=0,"fullMAC vs saturatedIR");
    auto before=g.mac;g.vertex[0]={0,0};g.vertex[1]={1,2};require(g.apply(r,out)==NBA97_BODY_ARGUMENT,"canonical error beats unknown");require(g.mac[0].word==before[0].word,"math refusal is not fake success");
}
}
int main(){owner();math();std::cout<<checks<<" player geometry checks passed\n";}
