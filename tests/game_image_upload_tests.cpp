#include "recovered/game_image_upload.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
unsigned checks=0;
void check(bool b){++checks;if(!b)throw std::runtime_error("image upload check "+std::to_string(checks));}
void word(std::uint8_t* p,std::uint32_t v){for(unsigned i=0;i<4;++i)p[i]=static_cast<std::uint8_t>(v>>(i*8));}
void half(std::uint8_t* p,std::uint16_t v){p[0]=static_cast<std::uint8_t>(v);p[1]=static_cast<std::uint8_t>(v>>8);}
std::uint16_t half(const std::uint8_t* p){return static_cast<std::uint16_t>(p[0]|p[1]<<8);}
struct Fixture {
    std::array<std::uint8_t,2048> bytes{},known{};
    Nba97GameImageMemory memory{bytes.data(),known.data(),bytes.size(),0,1};
    Nba97GameImageReference image{&memory,64};
    Nba97GameImagePlacement placement{100,200,768,250};
    Nba97GameImageUploadState state{7,1};
    Nba97GameImageUploadProgress progress{};
    std::vector<Nba97GameImageTransfer> events;
    std::vector<std::uint32_t> pending;
    unsigned refuse=0;
    bool change_height=false,change_link=false,poison_height=false;
    Fixture(){known.fill(1);header(64,0x40,0,8,4);}
    void header(std::size_t o,std::uint8_t type,std::int32_t relative,std::uint16_t w,std::uint16_t h) {
        word(bytes.data()+o,(static_cast<std::uint32_t>(relative)<<8)|type);
        half(bytes.data()+o+4,w);half(bytes.data()+o+6,h);
    }
    static int io(void* ctx,const Nba97GameImageTransfer* event) {
        auto& s=*static_cast<Fixture*>(ctx);s.events.push_back(*event);s.pending.push_back(s.state.pending_d7b14);
        if(s.refuse==s.events.size())return 0;
        if(s.change_height&&s.events.size()==2)half(s.bytes.data()+70,8);
        if(s.change_link&&s.events.size()==1)word(s.bytes.data()+64,0x48);
        if(s.poison_height&&s.events.size()==2){s.known[70]=0;s.known[71]=2;}
        s.state.pending_d7b14=9;s.state.pending_known=1;
        return 1;
    }
    int run(std::size_t budget=8){return nba97_game_image_upload(&state,image,placement,budget,io,this,&progress);}
    int chain(std::size_t budget=8){return nba97_game_image_upload_chain(&state,image,placement,budget,io,this,&progress);}
};
void ordinary() {
    Fixture f;check(f.run()==1);check(f.events.size()==1&&f.events[0].rect.w==2&&f.events[0].rect.h==4);
    check(f.events[0].source.offset==80&&f.events[0].pixel_words==8&&f.events[0].cpu_words==4);
    check(f.pending[0]==7&&f.state.pending_d7b14==1&&f.state.pending_known==1);
    check(f.bytes[64]==0x48&&half(f.bytes.data()+76)==100&&half(f.bytes.data()+78)==200);
    check(f.progress.headers_visited==1&&f.progress.uploads_completed==1&&!f.progress.temporary_height_active);
    Fixture g;g.header(64,0x41,128,8,3);g.header(192,0x23,0,16,1);g.placement.clut_x=g.placement.clut_y=0;
    check(g.chain()==1&&g.events.size()==1&&g.progress.headers_visited==2);
    check(g.bytes[192]==0x2b&&half(g.bytes.data()+204)==0&&half(g.bytes.data()+206)==0);
    Fixture h;h.header(64,0x23,0,16,1);h.placement.clut_x=65536;h.placement.clut_y=0;
    check(h.chain()==1&&h.events.size()==1&&h.events[0].rect.x==0); // Fullword,notlow16,suppression.
    Fixture j;j.header(64,0x80,0,8,4);check(j.chain()==1&&j.events.empty());
    std::uint32_t bits=99;
    check(nba97_game_image_bits(0x80,&bits)==NBA97_IMAGE_FORMAT_UNRESOLVED&&bits==99);
    check(nba97_game_image_bits(0xc0,&bits)==1&&bits==4);
    check(nba97_game_image_bits(0x7a,&bits)==NBA97_IMAGE_FORMAT_UNRESOLVED&&bits==4);
    check(nba97_game_image_bits(0x44,&bits)==1&&bits==1);
    Fixture u;u.bytes[64]=0x72;
    check(u.run()==NBA97_IMAGE_FORMAT_UNRESOLVED&&u.events.empty()&&u.bytes[64]==0x72);
}
void split() {
    Fixture f;f.header(64,0x40,0,12,4);check(f.run()==1);
    check(f.events.size()==3&&f.events[0].rect.w==1&&f.events[0].rect.h==2);
    check(f.events[0].source.offset==96&&f.events[0].rect.x==100&&f.events[0].rect.y==202);
    check(f.events[1].source.offset==100&&f.events[1].rect.w==2&&f.events[1].rect.y==203);
    check(f.events[2].rect.w==3&&f.events[2].rect.h==3&&f.events[2].cpu_words==5&&f.events[2].pixel_words==9);
    check(f.events[0].through_944f4==0&&f.events[1].through_944f4==0&&f.events[2].through_944f4==1);
    check(f.pending==std::vector<std::uint32_t>({7,9,9})&&half(f.bytes.data()+70)==4);
    Fixture g;g.header(64,0x40,0,12,4);g.change_height=true;check(g.run()==1);
    check(g.events[2].rect.h==7&&half(g.bytes.data()+70)==4); // Reread8,decrement7,restoresaved4.
    Fixture h;h.header(64,0x40,0,12,4);h.refuse=3;check(h.run()==NBA97_IMAGE_IO_REFUSED);
    check(half(h.bytes.data()+70)==3&&h.progress.temporary_height_active==1&&h.progress.uploads_completed==2);
    check(h.state.pending_d7b14==9); // Refused944F4doesnotwrite1.
    Fixture k;k.header(64,0x40,0,12,4);check(k.run(0)==NBA97_IMAGE_HEADER_LIMIT);
    check(k.events.size()==2&&half(k.bytes.data()+70)==3&&k.progress.temporary_height_active==1);
}
void references() {
    Fixture f;f.header(64,0x40,128,8,4);f.header(192,0x23,-160,16,1);f.header(32,0x23,0,16,1);
    check(f.chain()==1&&f.events.size()==3&&f.events[2].source.offset==48);
    Fixture g;g.header(64,0x40,128,8,4);g.header(192,0x23,-128,16,1);
    check(g.chain(3)==NBA97_IMAGE_HEADER_LIMIT&&g.events.size()==3&&g.progress.headers_visited==3);
    Fixture h;h.header(64,0x40,128,8,4);h.header(192,0x23,0,16,1);h.change_link=true;
    check(h.chain()==1&&h.events.size()==1); // Callbackterminateslivechain.
    Fixture i;i.header(64,0x23,-128,16,1);check(i.chain()==NBA97_IMAGE_RESOURCE&&i.events.size()==1);
    Fixture n;n.image={nullptr,0};check(n.chain()==1&&n.progress.headers_visited==0);
    check(n.run()==NBA97_IMAGE_RESOURCE&&n.events.empty());
}
void knownness() {
    Fixture f;f.known[68]=0;check(f.run()==NBA97_IMAGE_UNKNOWN&&f.events.empty()&&f.bytes[64]==0x40);
    Fixture g;g.known[70]=0;check(g.chain()==NBA97_IMAGE_UNKNOWN&&g.bytes[64]==0x48&&half(g.bytes.data()+76)==100);
    Fixture h;h.known[78]=h.known[79]=0;check(h.chain()==1&&h.known[78]==1&&h.known[79]==1);
    Fixture j;j.memory.address_mod4_known=0;check(j.chain()==NBA97_IMAGE_UNKNOWN&&j.bytes[64]==0x40);
    Fixture k;k.memory.address_mod4=1;check(k.run()==NBA97_IMAGE_ALIGNMENT_TRAP&&k.events.empty());
    Fixture l;l.header(64,0x23,0,16,1);l.known[64]=0;check(l.chain()==NBA97_IMAGE_UNKNOWN&&l.bytes[64]==0x23);
    Fixture m;m.memory.known=nullptr;check(m.run()==1);
    Fixture p;p.memory.size=80;p.header(64,0x23,0,2,1);p.placement.clut_x=p.placement.clut_y=0;
    half(p.bytes.data()+76,1234);p.known[78]=2;
    check(p.run()==NBA97_IMAGE_ARGUMENT&&p.events.empty());
    check(half(p.bytes.data()+76)==0&&p.bytes[64]==0x23&&p.known[78]==2&&p.state.pending_d7b14==7);
    check(p.progress.headers_visited==1&&p.progress.stopped_offset==78); // Earlier palette x-store retained.
    Fixture q;q.known[68]=0;q.known[69]=2;
    check(q.run()==NBA97_IMAGE_ARGUMENT&&q.bytes[64]==0x40); // Full read span, not first unknown only.
    Fixture r;r.known[78]=0;r.known[79]=2;
    check(r.chain()==NBA97_IMAGE_ARGUMENT&&r.known[78]==0&&r.known[79]==2&&r.bytes[64]==0x40);
    Fixture s;s.header(64,0x40,0,12,4);s.poison_height=true;
    check(s.run()==NBA97_IMAGE_ARGUMENT&&s.events.size()==2&&s.progress.uploads_completed==2);
    check(half(s.bytes.data()+70)==4&&!s.progress.temporary_height_active&&s.known[70]==0&&s.known[71]==2);
}
void raw_rect() {
    Fixture f;Nba97GameImageRect rect{1,2,3,4};
    check(nba97_game_image_upload_rect(&f.state,f.image,&rect,Fixture::io,&f,&f.progress)==1);
    check(rect.h==5&&f.events[0].rect.h==5&&f.events[0].cpu_words==8);
    Fixture g;rect={-1,-2,-3,-4};check(nba97_game_image_upload_rect(&g.state,g.image,&rect,Fixture::io,&g,&g.progress)==1);
    check(rect.h==-3&&g.events[0].footprint_known==0&&g.events[0].cpu_words==0);
    Fixture h;rect={0,0,0,0};check(nba97_game_image_upload_rect(&h.state,h.image,&rect,Fixture::io,&h,&h.progress)==1);
    check(h.events.size()==1&&!h.events[0].footprint_known); // BadSDKdomain isnotinventedemptyoperation.
    h.progress.headers_visited=123;
    check(nba97_game_image_upload(nullptr,h.image,h.placement,1,Fixture::io,&h,&h.progress)==0&&h.progress.headers_visited==123);
}
}
int main(){try{ordinary();split();references();knownness();raw_rect();std::cout<<checks<<" image upload checks passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
