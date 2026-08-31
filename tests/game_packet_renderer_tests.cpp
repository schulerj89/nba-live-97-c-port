#include "game_packet_renderer.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

namespace {
using R=nba97::GamePacketResult;
unsigned checks=0;
void require(bool ok,const char* reason){++checks;if(!ok)throw std::runtime_error(reason);}
std::uint32_t xy(int x,int y){return (std::uint32_t(x)&65535)|(std::uint32_t(y)<<16);}
struct Fixture {
    nba97::GameVramWords vram;
    nba97::GamePacketRenderer renderer{vram};
    nba97::GameDrawProgress progress;
    Fixture(){renderer.state={0,0,0,127|(127u<<10),0,0,63,{true,false,false,0,0,0,0,0}};}
    R draw(std::initializer_list<std::uint32_t> words){return renderer.drawWords(words.begin(),words.size(),progress);}
    std::uint16_t at(unsigned x,unsigned y){std::uint16_t v=0;require(vram.word(x,y,v),"pixel must be known");return v;}
    bool known(unsigned x,unsigned y){std::uint16_t v=0;return vram.word(x,y,v);}
};
void flat(){
    Fixture f;require(f.draw({0x280000f8,xy(0,0),xy(4,0),xy(0,4),xy(4,4)})==R::Complete,"flat quad");
    require(f.progress.pixels==16&&f.progress.triangles==2,"quad splits with one shared edge");
    for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)require(f.at(x,y)==31,"flat red");
    require(!f.known(4,0)&&!f.known(0,4),"lower/right edges excluded");
    Fixture unknown;unknown.renderer.state.known=0;
    require(unknown.draw({0x280000f8,xy(0,0),xy(4,0),xy(0,4),xy(4,4)})==R::UnknownState&&unknown.progress.pixels==0,"no invented environment");
    Fixture clipped;clipped.renderer.state.top_left=2|(2u<<10);clipped.renderer.state.bottom_right=3|(3u<<10);
    require(clipped.draw({0x280000f8,xy(0,0),xy(4,0),xy(0,4),xy(4,4)})==R::Complete&&clipped.progress.pixels==4,"inclusive drawing area");
    Fixture large;require(large.draw({0x20ffffff,xy(0,0),xy(1023,0),xy(0,512)})==R::Complete&&large.progress.pixels==0,"overlarge triangle rejected");
    Fixture zero;require(zero.draw({0x20ffffff,xy(1,1),xy(1,1),xy(1,1)})==R::Complete&&zero.progress.pixels==0,"degenerate no pixels");
    Fixture offset;offset.renderer.state.offset=5|(7u<<11);
    require(offset.draw({0x680000f8,xy(0,0)})==R::Complete&&offset.at(5,7)==31,"signed draw offset");
    Fixture display;display.renderer.state.display={true,true,false,0,0,0,2,2};
    require(display.draw({0x280000f8,xy(0,0),xy(4,0),xy(0,4),xy(4,4)})==R::Complete&&display.progress.pixels==12,"display area exclusion");
}
void textures(){
    constexpr std::uint16_t page=0x11a; // direct15 at640,256
    Fixture f;
    for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)f.vram.drawWord(640+x,256+y,std::uint16_t(1+x+y*4));
    require(f.draw({0x2dffffff,xy(0,0),0,xy(4,0),(std::uint32_t(page)<<16)|4,xy(0,4),4u<<8,xy(4,4),0x404})==R::Complete,"direct textured quad");
    for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)require(f.at(x,y)==1+x+y*4,"affine corner texture samples");
    require((f.renderer.state.mode&511)==page,"textured polygon retains page for later sprites");
    Fixture indexed;constexpr std::uint16_t clut=std::uint16_t(300*64+48),page4=0x1a;
    indexed.vram.drawWord(640,256,0x3210);
    for(unsigned i=0;i<4;++i)indexed.vram.drawWord(i,0,0x7c00);
    for(unsigned i=0;i<4;++i)indexed.vram.drawWord(768+i,300,std::array<std::uint16_t,4>{0,0x8000,31,0x83e0}[i]);
    indexed.renderer.state.mode=page4;
    require(indexed.draw({0x67000000,xy(0,0),std::uint32_t(clut)<<16,xy(4,1)})==R::Complete,"indexed semi-transparent sprite");
    require(indexed.at(0,0)==0x7c00&&indexed.at(1,0)==0xbc00,"zero is transparent,8000 is blended black");
    require(indexed.at(2,0)==31&&indexed.at(3,0)==0xbde0,"CLUT bit15 selects blending per pixel");
    require(indexed.progress.transparent==1&&indexed.progress.pixels==3,"transparent texture skipped");
    Fixture missing;missing.renderer.state.mode=page;
    require(missing.draw({0x6d808080,xy(0,0),0})==R::UnknownVram&&!missing.progress.completed&&missing.progress.pixels==0,"unknown texels never become skin or black");
    Fixture mask;mask.renderer.state.mode=page;mask.renderer.state.mask=2;mask.vram.drawWord(0,0,0x8001);
    require(mask.draw({0x6d808080,xy(0,0),0})==R::Complete&&mask.progress.masked==1&&mask.at(0,0)==0x8001,"masked destination needs no invented texture");
}
void blendAndDither(){
    for(unsigned mode=0;mode<4;++mode){
        Fixture f;f.renderer.state.mode=mode<<5;
        f.vram.drawWord(0,0,17);require(f.draw({0x6a000088,xy(0,0)})==R::Complete,"blend single pixel");
        require(f.at(0,0)==std::array<unsigned,4>{17,31,0,21}[mode],"four blend channel rules, including odd-pair average");
    }
    Fixture f;f.renderer.state.mode=32;
    for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)f.vram.drawWord(x,y,0);
    require(f.draw({0x2a000038,xy(0,0),xy(4,0),xy(0,4),xy(4,4)})==R::Complete,"blended quad");
    for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)require(f.at(x,y)==7,"shared quad edge must not blend twice");
    Fixture d;d.renderer.state.mode=512;
    require(d.draw({0x30070707,xy(0,0),0x070707,xy(4,0),0x070707,xy(0,4)})==R::Complete,"dithered Gouraud triangle");
    require(d.at(0,0)==0&&d.at(2,1)==0x421,"dither acts before five-bit conversion");
    Fixture gradient;
    require(gradient.draw({0x30000000,xy(0,0),63,xy(4,0),0,xy(0,4)})==R::Complete,"fractional color gradient");
    require(gradient.at(1,0)==2,"twelve-bit gradient retains the initial half-unit bias");
    Fixture uv;uv.renderer.state.mode=0x11a;
    for(unsigned i=0;i<8;++i)uv.vram.drawWord(640+i,256,std::uint16_t(i+1));
    require(uv.draw({0x25808080,xy(0,0),0,xy(4,0),(0x11au<<16)|7,xy(0,4),0})==R::Complete,"fractional UV gradient");
    require(uv.at(1,0)==3,"UV gradient rounds with the same half-unit bias");
}
void packetStream(){
    Fixture f;f.renderer.state.known=0;
    require(f.draw({0xe1000000,0xe2000000,0xe3000000,0xe401fc7f,0xe5000000,0xe6000001,0x680000f8,xy(1,1)})==R::Complete,"environment command sequence");
    require(f.at(1,1)==0x801f&&f.renderer.state.known==63,"mask and environment retained");
    require(f.draw({0x200000ff,xy(1,1)})==R::IncompleteCommand,"truncated packet refuses");
    require(f.draw({0xa0000000})==R::UnsupportedCommand,"unsupported transfer not invented");
    Fixture fill;fill.renderer.state.known=0;
    require(fill.draw({0x020000f8,xy(3,2),xy(1,1)})==R::Complete&&fill.progress.pixels==16,"fill rounds horizontal span and ignores environment");
    require(fill.at(0,2)==31&&fill.at(15,2)==31&&!fill.known(16,2),"rounded fill extent");
    Fixture bounded;bounded.renderer.pixel_budget=3;
    require(bounded.draw({0x600000f8,xy(0,0),xy(4,4)})==R::PixelLimit&&bounded.progress.pixels==3&&!bounded.progress.completed,"pixel budget retains prefix");
    std::map<std::uint32_t,std::uint32_t> memory={{0x1000,0x02001020},{0x1004,0x600000f8},{0x1008,xy(0,0)},{0x1020,0x01800000},{0x1024,xy(2,2)}};
    auto read=[](void* user,std::uint32_t a,std::uint32_t& v){auto& m=*static_cast<decltype(memory)*>(user);const auto i=m.find(a);if(i==m.end())return R::PacketUnavailable;v=i->second;return R::Complete;};
    Fixture linked;require(linked.renderer.drawOrderingTable(read,&memory,0x1000,10,linked.progress)==R::Complete&&linked.progress.pixels==4&&linked.progress.links==2,"command crosses real packet links");
    memory[0x1000]=0x1000;require(linked.renderer.drawOrderingTable(read,&memory,0x1000,3,linked.progress)==R::LinkLimit&&linked.progress.links==3,"source cycle bounded without repaired terminator");
    require(linked.renderer.drawOrderingTable(read,&memory,0x1001,3,linked.progress)==R::LinkAlignment,"unaligned link refused");
    Fixture line;require(line.draw({0x400000f8,xy(0,0),xy(3,0)})==R::Complete&&line.progress.pixels==4,"line includes final endpoint");
    Fixture descending;require(descending.draw({0x400000f8,xy(0,1),xy(2,0)})==R::Complete,"descending diagonal line");
    require(descending.known(0,1)&&descending.known(1,0)&&descending.known(2,0)&&!descending.known(1,1),"descending half-pixel tie bias");
    Fixture reverse;require(reverse.draw({0x400000f8,xy(2,0),xy(0,1)})==R::Complete,"reversed diagonal line");
    require(reverse.known(0,1)&&reverse.known(1,0)&&reverse.known(2,0)&&!reverse.known(1,1),"line endpoint order does not change coverage");
    Fixture shaded;require(shaded.draw({0x50000000,xy(0,0),63,xy(4,0)})==R::Complete,"Gouraud line gradient");
    require(shaded.at(1,0)==2,"line color usesQ12gradient and half-unit bias");
}
}
int main(){try{flat();textures();blendAndDither();packetStream();std::cout<<checks<<" packet renderer checks passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
