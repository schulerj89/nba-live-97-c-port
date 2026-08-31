#include "game_player_root.hpp"
#include <array>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>
namespace {
unsigned checks=0;
void require(bool v,const char* what){++checks;if(!v){std::cerr<<what<<'\n';std::exit(1);}}
Nba97GameBodyReference ref(unsigned offset){return {0,offset,1};}
struct Fixture {
    std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x9000,0x5a),known=std::vector<std::uint8_t>(0x9000,1);
    std::vector<Nba97GameBodyCell> cells=std::vector<Nba97GameBodyCell>(0x2400);
    Nba97GameBodyBuffer buffer{};nba97::GamePlayerRootGeometry geometry;
    Nba97GamePlayerRootInput input{};Nba97GamePlayerRootProgress progress{};std::array<Nba97GamePlayerGeometryWrite,64> journal{};
    void word(unsigned p,std::uint32_t v){for(unsigned i=0;i<4;++i)bytes[p+i]=std::uint8_t(v>>(i*8));}
    std::uint32_t word(unsigned p){return bytes[p]|(std::uint32_t(bytes[p+1])<<8)|(std::uint32_t(bytes[p+2])<<16)|(std::uint32_t(bytes[p+3])<<24);}
    void pointer(unsigned p,unsigned target){cells[p/4]={ref(target),1};}
    Fixture(){
        buffer={bytes.data(),known.data(),bytes.size(),cells.data(),cells.size(),0,1};
        input={&buffer,1,ref(0),ref(4),ref(0x100),ref(0x200),ref(0x300),ref(0x4000),ref(0x1000),ref(0x1200),ref(0x1400),ref(0x1600),ref(0x1700),ref(0x1740),nba97::GamePlayerRootGeometry::callback,&geometry};
        pointer(0,0x2000);pointer(0x2bc0,0x3100);word(4,0);word(0x100,65536);word(0x300,0);word(0x304,0);
        word(0x2004,100);word(0x2008,200);word(0x200c,30);bytes[0x2016]=bytes[0x2017]=0;word(0x3100,320u<<16);
        for(unsigned i=0;i<8;++i)word(0x200+i*4,0);
        word(0x200,4096);word(0x208,4096);word(0x210,4096);word(0x21c,1000);word(0x4000,0x10000000);
        geometry.offset_x={256u<<16,1};geometry.offset_y={120u<<16,1};geometry.distance={256,1};geometry.depth_cue_a={0,1};geometry.depth_cue_b={0,1};
    }
    int run(std::size_t cap=64){return nba97_game_player_root(&input,journal.data(),cap,&progress);}
};
struct RotationDelayProbe {
    nba97::GamePlayerRootGeometry* geometry;
    static int callback(void* user,const Nba97PlayerMathRequest* q,Nba97GamePeriodValue* out){
        auto& probe=*static_cast<RotationDelayProbe*>(user);
        if(q->kind==NBA97_PLAYER_ROTATION&&q->index==4&&q->pc>=0x80055f18&&q->pc<0x80055f44){
            require(q->pc==0x80055f40,"lastrotationCTC2 isJRdelay slot");
            return NBA97_BODY_UNKNOWN;
        }
        return probe.geometry->apply(*q,*out);
    }
};
void owner(){
    Fixture full;require(full.run()==1&&full.progress.completed,"complete5200C");require(full.progress.writes==37,"37visible stores");
    require(full.word(0x1000)==4096&&full.word(0x1008)==4096&&(full.word(0x1010)&65535)==4096,"worldEuler scaledidentity");
    require(full.word(0x1014)==100&&full.word(0x1018)==86&&full.word(0x101c)==200,"actualrootheight plus36");
    require(full.word(0x1214)==100&&full.word(0x1218)==86&&full.word(0x121c)==1200,"camera roottranslation");
    require(full.word(0x1414)==100&&full.word(0x1418)==std::uint32_t(-86)&&full.word(0x141c)==1200,"mirroronlyY");
    require(full.word(0x1740)==300,"actualSZ3div4");require(full.word(0x1700)==(120u<<16)+277,"actualcenter projection");
    require(full.word(0x1400)==0x5a5a5a5a,"alternate rotation untouched");
    for(std::size_t cap=0;cap<37;++cap){Fixture f;require(f.run(cap)==NBA97_BODY_JOURNAL_LIMIT,"journal refused");require(f.progress.writes==cap,"exactwriteprefix");
        for(std::size_t i=0;i<cap;++i)require(f.journal[i].pc==full.journal[i].pc&&f.journal[i].word==full.journal[i].word,"sourceprefixvalues");}
    Fixture delay;RotationDelayProbe probe{&delay.geometry};delay.input.math=RotationDelayProbe::callback;delay.input.math_user=&probe;
    require(delay.run()==NBA97_BODY_UNKNOWN,"delay-slot callback refusal");
    require(delay.progress.stopped_pc==0x80055f40&&delay.progress.writes==29,"delay-slot refusal retains priorCPUstores");
    Fixture noProjection;noProjection.geometry.distance={0,0};require(noProjection.run()==NBA97_BODY_UNKNOWN,"projectioncontrols required");
    require(noProjection.progress.writes==35&&noProjection.progress.stopped_pc==0x80056630,"earlierCPU/math prefix survives unknownprojection");
    Fixture opaque;opaque.known[0x302]=opaque.known[0x303]=opaque.known[0x306]=opaque.known[0x307]=0;
    opaque.known[0x1012]=opaque.known[0x1013]=opaque.known[0x212]=opaque.known[0x213]=opaque.known[0x1606]=opaque.known[0x1607]=0;
    require(opaque.run()==1,"unusedtemplate and matrix/vertexpadding remains unknown");require(!opaque.known[0x1012]&&!opaque.known[0x1606],"padding not fabricated");
    Fixture bad;bad.known[0x302]=0;bad.known[0x303]=2;require(bad.run()==NBA97_BODY_ARGUMENT&&bad.progress.writes==0,"opaquecopyvalidateswholeword");
    Fixture unknownAngle;unknownAngle.known[0x304]=0;require(unknownAngle.run()==NBA97_BODY_UNKNOWN&&unknownAngle.progress.writes==0&&unknownAngle.progress.stopped_pc==0x80056150,"unknownZ beforefirstEulerstore");
    Fixture scaleAlias;scaleAlias.input.scales_105f48=ref(0x1600);scaleAlias.word(0x1600,65536);require(scaleAlias.run()==1,"ground/scale alias");require((scaleAlias.word(0x1000)&65535)==6,"scale reread seesactualgroundwrites");
    Fixture wrap;wrap.word(4,0x80000000);require(wrap.run()==1,"index shifts wrap to0");
    Fixture unknownContext;unknownContext.cells[0].reference={0,0,0};require(unknownContext.run()==NBA97_BODY_UNKNOWN&&unknownContext.progress.stopped_pc==0x80052060,"deferredcontextpointer");
    Fixture outputAlias;outputAlias.input.screen_fea94=ref(4);require(outputAlias.run()==NBA97_BODY_BOUNDS&&outputAlias.progress.writes==36,"screen/indexaliaschangeslastdepthaddress");
    require(outputAlias.progress.stopped_pc==0x80052204,"noindexsnapshotshortcut");
    Fixture rounding;rounding.word(0x300,1);rounding.bytes[0x2016]=1;
    rounding.word(0x4004,0x10000001);rounding.word(0x4010,0x0fff0000);
    require(rounding.run()==1,"Eulerroundingfixture");require((rounding.word(0x1008)>>16)==65535,"negate product before signedrounding");
    Fixture badwrite;badwrite.known[0x1004]=0;badwrite.known[0x1005]=2;
    require(badwrite.run()==NBA97_BODY_ARGUMENT&&badwrite.progress.writes==0&&badwrite.progress.stopped_pc==0x80056154,"write-only span canonicalmetadata");
    Fixture noCamera;noCamera.known[0x200]=0;
    require(noCamera.run()==NBA97_BODY_UNKNOWN&&noCamera.progress.writes==24&&noCamera.progress.stopped_pc==0x800562cc,"camera missing only when actualcompose reads it");
}
void math(){
    nba97::GamePlayerRootGeometry g;Nba97GamePeriodValue out;Nba97PlayerMathRequest r{0,0,NBA97_ROOT_PROJECT,0};
    require(g.apply(r,out)==NBA97_BODY_UNKNOWN,"unknownnativeprojection");g.vector.rotation[0]={1,2};require(g.apply(r,out)==NBA97_BODY_ARGUMENT,"badmetadata priority");
    Fixture f;require(f.run()==1,"mathfixture");auto screen=f.geometry.screen;auto depth=f.geometry.depth;auto mac0=f.geometry.mac0;
    r.kind=NBA97_PLAYER_ROTATE;require(f.geometry.apply(r,out)==1,"shared MVMVA afterRTPS");
    for(unsigned i=0;i<3;++i)require(f.geometry.screen[i].word==screen[i].word&&f.geometry.screen[i].known==screen[i].known,"MVMVAkeepsscreenFIFO");
    for(unsigned i=0;i<4;++i)require(f.geometry.depth[i].word==depth[i].word&&f.geometry.depth[i].known==depth[i].known,"MVMVAkeepsdepthFIFO");
    require(f.geometry.mac0.word==mac0.word,"MVMVAkeepsMAC0");
}
}
int main(){owner();math();std::cout<<checks<<" player root checks passed\n";}
