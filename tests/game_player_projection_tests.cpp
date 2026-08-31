#include "game_player_projection.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>
namespace {
unsigned checks=0;
void require(bool b,const char* s){++checks;if(!b){std::cerr<<s<<'\n';std::exit(1);}}
Nba97GameBodyReference ref(unsigned p){return {0,p,1};}
struct Fixture {
    std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x9000,0xa5),known=std::vector<std::uint8_t>(0x9000,1);
    std::vector<Nba97GameBodyCell> cells=std::vector<Nba97GameBodyCell>(0x2400);
    Nba97GameBodyBuffer buffer{};Nba97PlayerProjectionAddress base{0x80120000,1};
    nba97::GamePlayerProjectionGeometry g;Nba97GamePlayerProjectionInput in{};Nba97GamePlayerProjectionProgress progress{};
    std::array<Nba97GamePlayerGeometryWrite,300> journal{};
    void word(unsigned p,std::uint32_t v){for(unsigned i=0;i<4;++i)bytes[p+i]=std::uint8_t(v>>(i*8));}
    std::uint32_t word(unsigned p)const{return bytes[p]|(std::uint32_t(bytes[p+1])<<8)|(std::uint32_t(bytes[p+2])<<16)|(std::uint32_t(bytes[p+3])<<24);}
    void pointer(unsigned p,unsigned target){cells[p/4]={ref(target),1};}
    Fixture(){
        buffer={bytes.data(),known.data(),bytes.size(),cells.data(),cells.size(),0,1};in={&buffer,1,&base,1,ref(0),ref(4),ref(8),ref(12),ref(16),ref(20),nba97::GamePlayerProjectionGeometry::callback,&g};
        g.root.vector.rotation={{{4096,1},{0,1},{4096,1},{0,1},{4096,1}}};g.root.vector.translation={{{0,1},{0,1},{1000,1}}};
        g.root.offset_x={256u<<16,1};g.root.offset_y={120u<<16,1};g.root.distance={256,1};g.root.depth_cue_a={0,1};g.root.depth_cue_b={0,1};g.average_scale3={1365,1};
        for(unsigned n=0;n<10;++n){const unsigned p=0x100+n*24;word(p,0xfff6fff6);word(p+4,0);word(p+8,0x000afff6);word(p+12,0);word(p+16,0xfff6000a);word(p+20,0);}
        for(unsigned p=0x3000;p<0x7000;p+=4)word(p,0x44ffffff);
    }
    int group(std::uint32_t pc=0x80054660,unsigned n=3,std::size_t cap=300){return nba97_game_player_project_group(&in,pc,ref(0x100),ref(0x1000),ref(0x2000),n,ref(0x3000),4095,12,journal.data(),cap,&progress);}
    int assemble(unsigned n=1,std::size_t cap=300){return nba97_game_player_assemble(&in,ref(0x1800),ref(0x800),n,ref(0x900),ref(0x3000),journal.data(),cap,&progress);}
    void assembly(){for(unsigned i=0;i<3;++i)pointer(0x800+i*4,0x1000+i*8+8);pointer(0x900,0x2000);}
};
void groups(){
    Fixture full;require(full.group()==1,"completeprimary");require(full.progress.writes==18,"3triangles6stores");
    require((full.word(0x1000)>>24)==0xa5,"primarykeepspackethighbyte");require((full.word(0x3000+full.word(0x2000))>>24)==0x44,"primarykeepstablehighbyte");
    require(full.word(0x1000+8)==(117u<<16)+253,"actualprojectedXY");
    for(std::size_t cap=0;cap<full.progress.writes;++cap){Fixture f;require(f.group(0x80054660,3,cap)==NBA97_BODY_JOURNAL_LIMIT,"boundedjournal");require(f.progress.writes==cap,"preserveprefixsize");for(std::size_t j=0;j<cap;++j)require(f.journal[j].pc==full.journal[j].pc&&f.journal[j].word==full.journal[j].word,"preserveexactprefix");}
    for(unsigned n=0;n<5;++n){Fixture f;require(f.group(0x80054660,n)==1,"originalshortcountdomain");require(f.progress.writes==(n<2?3:n)*6,"minimumcountquirkpreserved");}
    for(unsigned n=1;n<=2;++n){Fixture f;
        Nba97GameBodyBuffer split[2]={f.buffer,{f.bytes.data()+0x100,f.known.data()+0x100,48,f.cells.data()+0x40,12,0,1}};f.in.buffers=split;f.in.buffer_count=2;
        const int rc=nba97_game_player_project_group(&f.in,0x80054660,{1,0,1},ref(0x1000),ref(0x2000),n,ref(0x3000),4095,0,f.journal.data(),300,&f.progress);
        require(rc==(n==2?NBA97_BODY_OK:NBA97_BODY_BOUNDS),"exactXYZresourceextent");
        require(f.progress.writes==(n==2?12u:6u),"onecountreachesunownedthirdXYZafterfirstlink");
        if(n==1)require(f.progress.stopped_pc==0x80054750,"thirdXYZprefetchPC");
    }
    Fixture biased;require(biased.group(0x8005483c)==1,"biasedgroup");require(biased.word(0x2000)+48==full.word(0x2000),"bias12beforemaskandshift");require((biased.word(0x1000)>>24)==7,"biasedforcespacketwordcount7");
    Fixture opaqueTable;for(unsigned p=0x3003;p<0x7000;p+=4)opaqueTable.known[p]=0;
    require(opaqueTable.group(0x8005483c)==1,"fulltableLWdiscardedhighbytecanbeunknown");
    require(opaqueTable.known[0x3003+opaqueTable.word(0x2000)]==1,"actualfulltableSWestablisheshighzero");
    Fixture alternate;require(alternate.group(0x80054adc)==1,"alternategroup");require(alternate.progress.writes==12,"oppositewindingcullsalllinks");require(alternate.word(0x1000)==0xa5a5a5a5,"culledtaguntouched");
    Fixture noaddress;noaddress.base={0,0};require(noaddress.group()==NBA97_BODY_ADDRESS_REQUIRED&&noaddress.progress.writes==4,"addressneededonlyafterfirstdepth");
    Fixture inconsistent;inconsistent.base.word|=1;require(inconsistent.group()==NBA97_BODY_ARGUMENT&&inconsistent.progress.writes==4,"numericbaseagreeswithsourcealignmentprovenance");
    Fixture noaddress32;noaddress32.base={0,0};require(noaddress32.group(0x8005483c)==NBA97_BODY_ADDRESS_REQUIRED&&noaddress32.progress.writes==5,"fulltagwrittenbeforeownaddressneeded");
    Fixture culled;culled.base={0,0};require(culled.group(0x80054adc)==1,"culledroutehasnoaddressdependency");
    Fixture padding;for(unsigned n=0;n<3;++n)for(unsigned v=0;v<3;++v)padding.known[0x100+n*24+v*8+6]=padding.known[0x100+n*24+v*8+7]=0;
    padding.known[0x1003]=0;require(padding.group()==1&&!padding.known[0x1003],"unusedZpaddingandhightagunknownretained");
    Fixture unvisited;unvisited.known[0x1003]=2;require(unvisited.group()==1&&unvisited.known[0x1003]==2,"low24storedoesnotreachhighmetadata");
    Fixture reached;reached.known[0x1003]=2;require(reached.group(0x8005483c)==NBA97_BODY_ARGUMENT&&reached.progress.writes==4,"fulltagstorevalidateshighmetadataatreach");
    Fixture bad;bad.known[0x100]=0;bad.known[0x103]=2;require(bad.group()==NBA97_BODY_ARGUMENT&&bad.progress.writes==0,"canonicalprioritywithinreachedspan");
    Fixture scale;scale.g.average_scale3={0,0};require(scale.group()==NBA97_BODY_UNKNOWN&&scale.progress.writes==3,"AVSZ3missingafterthreeXYstores");
    Fixture biaswrap;require(biaswrap.group(0x8005483c)==1,"depthbiasfixture");
    Fixture noCamera;noCamera.g.root.distance={0,0};require(noCamera.group()==NBA97_BODY_UNKNOWN&&noCamera.progress.writes==0,"realcameraHrequired");
}
void assembled(){
    Fixture f;require(f.group()==1,"assemblysource");f.assembly();require(f.assemble()==1&&f.progress.writes==5,"assembledtriangle");
    Fixture dead;require(dead.group()==1,"deadprefetchsource");dead.assembly();for(unsigned p=0x80c;p<0x818;++p)dead.known[p]=0;
    require(dead.assemble()==1,"finalprefetchunknownreferencecontentunused");
    Fixture bad;require(bad.group()==1,"prefetchbadsource");bad.assembly();bad.known[0x817]=2;
    require(bad.assemble()==NBA97_BODY_ARGUMENT&&bad.progress.writes==0&&bad.progress.stopped_pc==0x80054a6c,"finalprefetchstillvalidatesspan");
    Fixture zero;require(zero.group()==1,"zerocountassemblysource");zero.assembly();require(zero.assemble(0)==1&&zero.progress.writes==5,"zeroassemblyexecutesone");
    Fixture clipped;require(clipped.group()==1,"clippedassemblysource");clipped.assembly();clipped.pointer(0x804,0x1018);clipped.pointer(0x808,0x1010);
    require(clipped.assemble()==1&&clipped.progress.writes==1,"culledassemblywritesonlyfirstXY");
    Fixture shortspan;require(shortspan.group()==1,"shortprefetchsource");shortspan.assembly();
    Nba97GameBodyBuffer split[2]={shortspan.buffer,{shortspan.bytes.data()+0x800,shortspan.known.data()+0x800,12,shortspan.cells.data()+0x200,3,0,1}};shortspan.in.buffers=split;shortspan.in.buffer_count=2;
    require(nba97_game_player_assemble(&shortspan.in,ref(0x1800),{1,0,1},1,ref(0x900),ref(0x3000),shortspan.journal.data(),300,&shortspan.progress)==NBA97_BODY_BOUNDS,"finalprefetchexactseparateextent");
    require(shortspan.progress.writes==0&&shortspan.progress.stopped_pc==0x80054a64,"finalprefetchbeforefirstpacketstore");
}
void math(){
    nba97::GamePlayerProjectionGeometry g;Nba97GamePeriodValue out{};Nba97PlayerMathRequest q{0,0,NBA97_PROJECTION_AVERAGE_THREE,0};
    g.average_scale3={32767,1};for(unsigned i=1;i<4;++i)g.root.depth[i]={65535,1};
    require(g.apply(q,out)==1,"AVSZ3wideproduct");require(g.order_depth.word==65535&&g.root.mac0.word==std::uint32_t((196605LL*32767)&0xffffffffLL),"OTZclampsfullproductMACwraps");
    require(g.root.vector.flags.word==0x80050000,"AVSZ3overflowflags");g.average_scale3={65535,1};require(g.apply(q,out)==1&&g.order_depth.word==0,"signedZSF3");
    g.root.depth[0]={0,0};require(g.apply(q,out)==1,"AVSZ3ignoresSZ0");g.root.depth[1]={0,0};g.root.depth[3]={1,2};require(g.apply(q,out)==NBA97_BODY_ARGUMENT,"laterbadmetadataoverunknown");
}
}
int main(){groups();assembled();math();std::cout<<checks<<" player projection checks passed\n";}
