#include "game_court_geometry.hpp"
#include "game_packet_renderer.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
unsigned checks=0;
void require(bool b,const char* what){++checks;if(!b)throw std::runtime_error(what);}
constexpr std::uint32_t Base=0x80140000u,Packet=Base+512,Table=Base+1024;
std::uint32_t xy(int x,int y){return (std::uint32_t(x)&65535)|(std::uint32_t(y)<<16);}
struct Fixture {
    std::array<std::uint8_t,4096> data{},known{};
    nba97::GameCourtGeometry geometry;
    std::vector<std::uint32_t> write_pc;
    std::vector<std::array<Nba97CourtValue,6>> projected_inputs;
    Fixture(){
        known.fill(1);data.fill(0xa5);
        geometry.camera.rotation={4096,0,0,0,4096,0,0,0,4096};
        geometry.camera.translation={0,0,1024};geometry.camera.offset_x=256*65536;
        geometry.camera.offset_y=120*65536;geometry.camera.distance=128;
        geometry.camera.average_scale4=1024;geometry.camera.known=true;
        geometry.leading_bits={32,1};
        for(unsigned q=0;q<3;++q){
            const unsigned at=q*32;
            put(at,4,xy(-128,-128));put(at+4,4,0);
            put(at+8,4,xy(128,-128));put(at+12,4,0);
            put(at+16,4,xy(128,128));put(at+20,4,0);
            put(at+24,4,xy(-128,128));put(at+28,4,0);
        }
        put(1024,4,0xab123456);put(512,4,0x09eeeeee);
    }
    void put(unsigned at,unsigned width,std::uint32_t word){for(unsigned i=0;i<width;++i)data[at+i]=std::uint8_t(word>>(i*8));}
    std::uint32_t get(unsigned at,unsigned width=4)const{
        std::uint32_t word=0;for(unsigned i=0;i<width;++i)word|=std::uint32_t(data[at+i])<<(i*8);return word;
    }
    static int access(void* user,std::uint32_t pc,std::uint32_t address,unsigned width,int write,Nba97CourtValue* v){
        auto& f=*static_cast<Fixture*>(user);
        if(address<Base||address-Base>f.data.size()||width>f.data.size()-(address-Base))return NBA97_COURT_RESOURCE;
        const unsigned at=address-Base;
        for(unsigned i=0;i<width;++i)if(f.known[at+i]>1)return NBA97_COURT_ARGUMENT;
        if(write){f.put(at,width,v->word);for(unsigned i=0;i<width;++i)f.known[at+i]=1;f.write_pc.push_back(pc);}
        else{for(unsigned i=0;i<width;++i)if(!f.known[at+i]){*v={0,0};return NBA97_COURT_COMPLETE;}*v={f.get(at,width),1};}
        return NBA97_COURT_COMPLETE;
    }
    static int math(void* user,const Nba97CourtMathRequest* request,Nba97CourtValue* out){
        auto& f=*static_cast<Fixture*>(user);
        if(request->kind==NBA97_COURT_PROJECT_THREE)f.projected_inputs.push_back(f.geometry.vertex);
        return f.geometry.apply(*request,*out);
    }
    Nba97CourtContext context(std::size_t budget=10000){return {access,math,this,budget};}
    int run(Nba97CourtPacketPass pass,Nba97CourtProgress& p,unsigned count=1,std::uint32_t packet=Packet,std::size_t budget=10000){
        auto c=context(budget);return nba97_game_court_packets(&c,pass,Base,packet,Table,count,0,&p);
    }
};
void packetCases(){
    for(auto pass:{NBA97_COURT_FIXED_TEXTURED_54D4C,NBA97_COURT_DEPTH_TEXTURED_54ED8,NBA97_COURT_FIXED_FLAT_54E50}){
        Fixture f;Nba97CourtProgress p{};require(f.run(pass,p)==1,"native composed pass");
        require(p.completed&&p.quads==1&&p.linked==1,"pass completion");
        require(f.get(520)==xy(240,104),"first projected corner");
        const bool flat=pass==NBA97_COURT_FIXED_FLAT_54E50;
        require(f.get(flat?524:528)==xy(272,104),"second projected corner");
        require(f.get(flat?528:536)==xy(240,136),"fourth perimeter vertex uses third packet slot");
        require(f.get(flat?532:544)==xy(272,136),"third perimeter vertex uses last packet slot");
        require((f.get(512)&0xffffff)==0x123456,"old table link retained");
        require(f.get(1024)==(pass==NBA97_COURT_FIXED_TEXTURED_54D4C?0xab140200u:0x00140200u),"original tag high-byte policy");
        require((f.get(512)>>24)==(flat?5u:9u),"packet word count");
        require(f.get(516)==0xa5a5a5a5,"RGB/code retained");
        for(unsigned count:{0u,0xffffffffu,0x80000002u}){
            Fixture zero;Nba97CourtProgress zp{};require(zero.run(pass,zp,count)==1&&zp.quads==1,"source do-while count quirk");
        }
        const auto operations=p.operations;
        for(std::size_t budget=0;budget<operations;++budget){
            Fixture stopped;Nba97CourtProgress sp{};
            require(stopped.run(pass,sp,1,Packet,budget)==NBA97_COURT_LIMIT,"every budget stops");
            require(!sp.completed&&sp.operations==budget,"budget retains prefix");
        }
    }
    Fixture prefetch;prefetch.known[32]=0;Nba97CourtProgress p{};
    require(prefetch.run(NBA97_COURT_FIXED_TEXTURED_54D4C,p)==NBA97_COURT_UNKNOWN,"last-quad prefetch requires real bytes");
    require(p.stopped_pc==0x80054dbc&&p.stores==0&&prefetch.geometry.screen[2].known,"prefetch refusal after projection");
    Fixture noPrefetch;noPrefetch.known[32]=0;Nba97CourtProgress np{};
    require(noPrefetch.run(NBA97_COURT_DEPTH_TEXTURED_54ED8,np)==1,"depth pass has no extra prefetch");
    Fixture reject;reject.put(0,4,xy(-128,128));reject.put(8,4,xy(128,128));reject.put(16,4,xy(128,-128));
    Nba97CourtProgress rp{};require(reject.run(NBA97_COURT_FIXED_TEXTURED_54D4C,rp)==1&&rp.linked==0,"negative normal culling");
    require(reject.get(512)==0x09eeeeee&&reject.geometry.vertex[0].word==xy(-128,128),"rejected packet stale; last input retained");
    Fixture alias;Nba97CourtProgress ap{};
    std::array<std::uint32_t,6> cached{};for(unsigned i=0;i<6;++i)cached[i]=alias.get(32+i*4);
    require(alias.run(NBA97_COURT_FIXED_TEXTURED_54D4C,ap,2,Base+32)==1,"packet/next-vertex alias");
    require(alias.projected_inputs.size()==2,"two aliased projections");
    for(unsigned i=0;i<6;++i)require(alias.projected_inputs[1][i].word==cached[i],"prefetched values survive aliased packet writes");
    Fixture unknown;unknown.geometry.camera.known=false;Nba97CourtProgress up{};
    require(unknown.run(NBA97_COURT_FIXED_FLAT_54E50,up)==NBA97_COURT_UNKNOWN&&up.stores==0,"no invented camera");
    Fixture flags;flags.geometry.flags={0xffffffff,1};Nba97CourtProgress fp{};
    require(flags.run(NBA97_COURT_FIXED_FLAT_54E50,fp)==1&&fp.linked==1,"LZCR is not projection flags");
}
void linkCases(){
    Fixture f;auto c=f.context();Nba97CourtProgress p{};
    require(nba97_game_court_link(&c,Table,Packet,&p)==1,"56914 link");
    require(f.get(512)==0x09123456&&f.get(1024)==0xab140200,"56914 preserves tag upper bytes");
    require(f.write_pc==std::vector<std::uint32_t>{0x8005691c,0x80056924},"56914 store order");
    Fixture self;auto sc=self.context();Nba97CourtProgress sp{};
    require(nba97_game_court_link(&sc,Table,Table,&sp)==1&&self.get(1024)==0xab140400,"self alias remains a source cycle");
    Fixture upper;upper.known[1027]=0;upper.known[515]=0;auto uc=upper.context();Nba97CourtProgress u{};
    require(nba97_game_court_link(&uc,Table,Packet,&u)==1&&!upper.known[1027]&&!upper.known[515],"low24 link does not consume unknown high bytes");
}
void mathCases(){
    nba97::GameCourtGeometry g;Nba97CourtValue out{};Nba97CourtMathRequest request{};
    request.kind=NBA97_COURT_SCREEN;require(g.apply(request,out)==NBA97_COURT_UNKNOWN,"unknown FIFO");
    request.kind=NBA97_COURT_LEADING_BITS;require(g.apply(request,out)==NBA97_COURT_UNKNOWN,"unknown leading count");
    g.leading_bits={33,1};require(g.apply(request,out)==NBA97_COURT_ARGUMENT,"leading count domain");
    g.screen={Nba97CourtValue{xy(-32768,-32768),1},{xy(32767,-32768),1},{xy(32767,32767),1}};
    request.kind=NBA97_COURT_NORMAL_CLIP;require(g.apply(request,out)==1,"wide normal determinant");
    require(out.word==0xfffe0001&&g.flags.word==0x80010000,"normal determinant wrap and overflow");
    g.camera.known=true;g.camera.average_scale4=32767;for(auto& d:g.depth)d={65535,1};
    request.kind=NBA97_COURT_AVERAGE_FOUR;require(g.apply(request,out)==1,"wide depth product");
    require(g.order_depth.word==65535&&(g.flags.word&0x80050000)==0x80050000,"depth clamp and overflow");
    g.camera.average_scale4=-1;require(g.apply(request,out)==1&&g.order_depth.word==0,"negative source average scale");
}
struct FrameFixture {
    static constexpr std::uint32_t Origin=0x80000000,Root=0x80110000,Tex=Root+32,Flat=Root+512;
    std::vector<std::uint8_t> data=std::vector<std::uint8_t>(0x200000,0xa5);
    nba97::GameCourtGeometry geometry=Fixture{}.geometry;
    std::vector<std::uint32_t> writes;
    std::uint32_t unknown=0;
    bool hide_after_first=false;
    void put(std::uint32_t address,unsigned width,std::uint32_t word){
        for(unsigned i=0;i<width;++i)data.at(address-Origin+i)=std::uint8_t(word>>(i*8));
    }
    std::uint32_t get(std::uint32_t address,unsigned width=4)const{
        std::uint32_t word=0;for(unsigned i=0;i<width;++i)word|=std::uint32_t(data.at(address-Origin+i))<<(i*8);return word;
    }
    FrameFixture(){
        put(0x800febe4,4,Root);put(Root,4,11);put(Root+4,4,1);put(Root+8,4,Base);
        put(Root+12,4,Tex);put(Root+16,4,Flat);put(0x800dcf10,4,0);put(0x8001ede8,4,0);
        put(0x801046d8,4,0x80150000);put(0x80102924,4,0x80160000);
        put(0x800febec,4,0);put(0x800fcc54,4,0xffffffff);
        for(unsigned i=0;i<5;++i){
            const auto lo=std::uint16_t(geometry.camera.rotation[i*2]);
            const auto hi=i==4?0u:std::uint32_t(std::uint16_t(geometry.camera.rotation[i*2+1]))<<16;
            put(0x800f9fd8+i*4,4,lo|hi);
        }
        for(unsigned i=0;i<3;++i)put(0x800f9fec+i*4,4,std::uint32_t(geometry.camera.translation[i]));
        for(unsigned group=0;group<12;++group){
            const auto record=group<11?Tex+group*16:Flat;
            put(record,4,group);put(record+4,4,1);
            put(record+8,4,0x80130000+group*128);put(record+12,4,0x80130040+group*128);
            const std::array<std::uint32_t,4> points={xy(-128,-128),xy(128,-128),xy(128,128),xy(-128,128)};
            for(unsigned i=0;i<4;++i){put(Base+group*32+i*8,4,points[i]);put(Base+group*32+i*8+4,4,0);}
        }
        put(0x800feda0,4,0x80112000);put(0x80112000,4,0x80112100);
    }
    static int access(void* p,std::uint32_t pc,std::uint32_t address,unsigned width,int write,Nba97CourtValue* v){
        auto& f=*static_cast<FrameFixture*>(p);
        if(address<Origin||address-Origin>f.data.size()||width>f.data.size()-(address-Origin))return NBA97_COURT_RESOURCE;
        if(write){f.put(address,width,v->word);f.writes.push_back(pc);
            if(f.hide_after_first&&pc==0x80054e1c)f.put(0x800fcc54,4,0);
        }else if(f.unknown>=address&&f.unknown-address<width){*v={0,0};}
        else *v={f.get(address,width),1};
        return NBA97_COURT_COMPLETE;
    }
    static int math(void* p,const Nba97CourtMathRequest* r,Nba97CourtValue* v){return static_cast<FrameFixture*>(p)->geometry.apply(*r,*v);}
    int run(Nba97CourtProgress& p){Nba97CourtContext c{access,math,this,10000};return nba97_game_court_frame(&c,&p);}
};
void frameCases(){
    FrameFixture f;Nba97CourtProgress p{};
    require(f.run(p)==1&&p.completed,"complete court caller with actual native projection");
    require(p.quads==12&&p.linked==13,"one fixed, ten depth, one flat and one edge");
    require(f.get(0x80102c84)==FrameFixture::Tex+11*16&&f.get(0x800fc964)==FrameFixture::Flat+16,"caller publishes final live cursors");
    require(f.get(0x80112108)==xy(240,104)&&f.get(0x8011210c)==xy(272,136),"flat edge uses first and third perimeter corners");
    FrameFixture live;live.hide_after_first=true;Nba97CourtProgress lp{};
    require(live.run(lp)==1&&lp.quads==1&&lp.linked==1,"visibility reread after prior packet side effect");
    FrameFixture matrix;matrix.unknown=0x800f9fe8;Nba97CourtProgress mp{};
    require(matrix.run(mp)==NBA97_COURT_UNKNOWN&&mp.stopped_pc==0x80055f28&&mp.math_operations==0&&mp.stores==0,"camera loads precede all matrix writes");
    FrameFixture edge;edge.unknown=0x80112000;Nba97CourtProgress ep{};
    require(edge.run(ep)==NBA97_COURT_UNKNOWN&&ep.stopped_pc==0x8004b00c&&ep.quads==11,"unused flat fifth argument still consumes pointer before flat projection");
    FrameFixture bank;bank.put(0x800febec,4,1);bank.put(0x800fe9cc,4,0);bank.put(0x800fcc54,4,0);Nba97CourtProgress bp{};
    require(bank.run(bp)==1&&bp.quads==0,"special group hidden on zero bank");
    bank.put(0x8001ede8,4,1);require(bank.run(bp)==1&&bp.quads==1,"special group visible on nonzero bank");
}
void pixelsFromCourtPackets(){
    // Explicit camera/resources are fixtures, not a natural match capture.
    // Projection and packet links below are produced by the recovered owner.
    for(bool textured:{false,true}){
        Fixture f;f.put(1024,4,0x00ffffff);f.put(516,4,textured?0x2d808080u:0x280000f8u);
        if(textured){f.put(524,4,0);f.put(532,4,(0x11au<<16)|32);f.put(540,4,32u<<8);f.put(548,4,0x2020);}
        Nba97CourtProgress p{};
        require(f.run(textured?NBA97_COURT_FIXED_TEXTURED_54D4C:NBA97_COURT_FIXED_FLAT_54E50,p)==NBA97_COURT_COMPLETE,"court produces actual linked packets");
        nba97::GameVramWords vram;nba97::GamePacketRenderer renderer(vram);nba97::GameDrawProgress draw;
        renderer.state={0,0,0,511|(239u<<10),0,0,63,{true,false,false,0,0,0,0,0}};
        if(textured)for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x)vram.drawWord(640+x,256+y,std::uint16_t(1+((x+y)&30)));
        auto read=[](void* user,std::uint32_t low,std::uint32_t& word){
            Nba97CourtValue v{};const auto status=Fixture::access(user,0,0x80000000u|low,4,0,&v);
            if(status!=NBA97_COURT_COMPLETE)return nba97::GamePacketResult::PacketUnavailable;
            if(!v.known)return nba97::GamePacketResult::UnknownState;
            word=v.word;return nba97::GamePacketResult::Complete;
        };
        require(renderer.drawOrderingTable(read,&f,Table&0xffffff,4,draw)==nba97::GamePacketResult::Complete,"native court packets reach pixel backend");
        require(draw.completed&&draw.links==2&&draw.pixels==1024,"full projected quad rendered once");
        for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x){std::uint16_t value=0;
            require(vram.word(240+x,104+y,value)&&value==(textured?1+((x+y)&30):31),"court projection texture and strip ordering compose");}
        std::uint16_t outside=0;require(!vram.word(272,104,outside)&&!vram.word(240,136,outside),"court quad boundary remains untouched");
    }
}
}
int main(){try{packetCases();linkCases();mathCases();frameCases();pixelsFromCourtPackets();std::cout<<checks<<" court checks passed\n";return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
