#include "recovered/game_render_textures.h"
#include "recovered/game_head_cache.h"
#include "recovered/game_player_labels.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
unsigned checks=0;
void check(bool value) {++checks;if(!value)throw std::runtime_error("render entry check "+std::to_string(checks));}
template<std::size_t N>Nba97GameRenderBuffer buffer(std::array<std::uint8_t,N>& a) {return {a.data(),a.size()};}
void word(std::uint8_t* p,std::uint32_t v) {for(unsigned i=0;i<4;++i)p[i]=static_cast<std::uint8_t>(v>>(i*8));}
struct Io {
    std::vector<Nba97GameRenderIoEvent> events;
    std::vector<std::vector<std::uint8_t>> uploads;
    unsigned refuse=0;
    Nba97GameHeadCache* live=nullptr;
    bool remove_after_service=false;
    static int run(void* ctx,const Nba97GameRenderIoEvent* e) {
        auto& s=*static_cast<Io*>(ctx);s.events.push_back(*e);
        if(s.refuse==s.events.size())return 0;
        if(e->kind==NBA97_RENDER_UPLOAD_946B8)
            s.uploads.emplace_back(e->image.storage.data+e->image.offset,e->image.storage.data+e->image.storage.size);
        if(e->kind==NBA97_RENDER_STORE_99780)std::memset(e->destination.data,0x4e,e->destination.size);
        if(e->kind==NBA97_RENDER_SERVICE_8892C&&s.remove_after_service)s.live->count[0]=0;
        return 1;
    }
};
struct TextureFixture {
    Nba97GameRenderTextures s{};
    std::array<std::uint8_t,100> player{};
    Nba97GameRenderPlayer p{buffer(player)};
    std::array<std::uint8_t,1516> name{};
    std::array<std::uint8_t,1040> number{};
    std::array<std::uint8_t,1040> digit{};
    std::array<std::uint8_t,1040> palette{};
    std::array<std::uint8_t,0x210> team{};
    std::array<std::uint8_t,0x2000> skin{};
    std::array<std::array<std::uint8_t,32>,2> polygon{};
    TextureFixture() {
        for(auto& pp:s.player)pp=&p;
        s.name_scratch={buffer(name),0};s.number_scratch=buffer(number);
        for(auto& side:s.digit)for(auto& image:side)image={buffer(digit),0};
        for(auto& im:s.number_base)im={buffer(palette),0};
        for(auto& im:s.team_palette)im={buffer(team),0};
        s.skin_bank=buffer(skin);s.bypass_name_uv=1;
        word(name.data(),0x12345678);word(digit.data(),0x12345678);
    }
};
void textures() {
    TextureFixture f;Io io;f.player[7]=255;
    check(nba97_game_render_name(&f.s,0,Io::run,&io)==1);
    check(io.events.size()==2&&io.events[0].kind==NBA97_RENDER_UPLOAD_946B8);
    check(f.name[0]==0x78&&f.name[1]==0&&f.name[2]==0&&f.name[3]==0);
    check(f.s.name_cursor==50&&f.s.name_spacing==2);
    check(nba97_game_render_number(&f.s,0,Io::run,&io)==1);
    check(f.s.number_value==-1&&io.events.size()==8);
    f.player[7]=0;std::fill(f.palette.begin()+16,f.palette.begin()+48,std::uint8_t{1});io={};
    check(nba97_game_render_number(&f.s,5,Io::run,&io)==1);
    check(io.events.size()==8&&f.number[16]==0x10&&f.number[1039]==0x10);
    check(f.number[0]==0x78&&f.number[1]==0);
    f.player[7]=254;io={};
    check(nba97_game_render_number(&f.s,0,Io::run,&io)==NBA97_RENDER_RESOURCE);
    check(f.s.number_value==-2&&io.events.empty());
    f.player[7]=246; // Original signed-10 has remainder0 and is drawable.
    check(nba97_game_render_number(&f.s,0,Io::run,&io)==1);
    f.player[7]=10;word(f.palette.data(),0x500000);io={};
    check(nba97_game_render_number(&f.s,0,Io::run,&io)==NBA97_RENDER_RESOURCE);
    check(io.events.size()==4); // Two digits upload before the palette is consumed.
    word(f.palette.data(),0);
    io={};f.skin[0x1b0]=7;
    check(nba97_game_render_palette(&f.s,0,Io::run,&io)==1);
    check(f.team[16]==0x84&&f.team[17]==0x90&&f.team[0x1b0]==7&&f.team[0x1b1]==0x80);
    check(io.events[0].clut_x==512&&io.events[0].clut_y==192);
    io={};io.refuse=1;f.team[16]=0;f.team[17]=0;
    check(nba97_game_render_palette(&f.s,0,Io::run,&io)==NBA97_RENDER_IO_REFUSED);
    check(f.team[16]==0x84&&io.events.size()==1); // Exact mutation before refused upload.
    f.s.bypass_name_uv=0;
    for(auto& p:f.polygon){p[12]=20;p[28]=40;}
    for(unsigned j=0;j<4;++j)f.s.name_polygon[0][j]=buffer(f.polygon[j%2]);
    io={};check(nba97_game_render_name(&f.s,0,Io::run,&io)==1);
    check(f.s.name_center[0][0]==31&&f.s.name_center[0][2]==31);
    f.player[0x29]='a';f.player[0x2a]=0;io={};
    check(nba97_game_render_name(&f.s,0,Io::run,&io)==NBA97_RENDER_RESOURCE);
    check(io.events.empty()&&f.name[16]==255); // Clear precedes missingglyph refusal.
    unsigned done=99;check(nba97_game_render_textures(nullptr,Io::run,&io,&done)==0&&done==99);
    std::uint32_t a[12]{},b[12]{};for(unsigned j=0;j<12;++j){a[j]=j;b[j]=100+j;}
    Nba97GameRenderBindings bindings{};check(nba97_game_render_bindings(&bindings,a,b)==1);
    check(bindings.render_first==0&&bindings.entity_offset[9]==0&&bindings.entity_offset[10]==-0xa4);
    check(bindings.copied20b8c[11]==11&&bindings.copied20bbc[11]==111);
}
struct HeadFixture {
    Nba97GameHeadCache s{};
    std::array<std::uint8_t,0x107c> scratch{};
    std::array<std::array<std::uint8_t,7*0x107c>,2> bench{};
    HeadFixture() {
        s.scratch=buffer(scratch);
        for(unsigned side=0;side<2;++side) {
            s.bench[side]=buffer(bench[side]);s.count[side]=12;
            for(unsigned i=0;i<12;++i)s.lineup[side][i]=static_cast<std::int16_t>(s.current[side][i]=i);
            for(unsigned i=0;i<7;++i)std::fill_n(bench[side].begin()+i*0x107c,0x107c,static_cast<std::uint8_t>(i+side*16));
        }
    }
};
void heads() {
    HeadFixture f;Io io;check(nba97_game_head_cache(&f.s,-1,Io::run,&io)==1&&io.events.size()==1);
    std::swap(f.s.lineup[0][5],f.s.lineup[0][6]);std::swap(f.s.lineup[1][5],f.s.lineup[1][6]);io={};
    check(nba97_game_head_cache(&f.s,-1,Io::run,&io)==1);
    check(io.events.size()==6); // Entry sync;home service+3sync;awayservice.
    check(io.events[1].kind==NBA97_RENDER_SERVICE_8892C&&io.events[5].kind==NBA97_RENDER_SERVICE_8892C);
    check(f.s.current[0][5]==6&&f.s.current[1][6]==5&&f.bench[0][0]==1&&f.bench[1][0]==17);
    f.s.lineup[0][0]=-1;io={};
    check(nba97_game_head_cache(&f.s,-1,Io::run,&io)==NBA97_RENDER_SEARCH_OUTSIDE_OWNER&&io.events.size()==1);
    HeadFixture g;g.s.lineup[0][0]=1;io={};io.live=&g.s;io.remove_after_service=true;
    check(nba97_game_head_cache(&g.s,-1,Io::run,&io)==1);
    check(io.events.size()==2&&g.s.current[0][0]==0); // Count sampled after service.
    HeadFixture h;std::swap(h.s.lineup[0][5],h.s.lineup[0][6]);io={};io.refuse=5;
    check(nba97_game_head_cache(&h.s,-1,Io::run,&io)==NBA97_RENDER_IO_REFUSED);
    check(h.s.current[0][6]==5&&h.s.current[0][5]==5&&h.bench[0][0]==1); // Lastsync before finalcachewrite.
}
struct Labels {
    Nba97GamePlayerLabels s{};
    std::array<std::uint8_t,100> player{},style{},object{};
    Nba97GamePlayerLabelEntity entity{0,0,buffer(player)};
    std::vector<std::string> text;
    unsigned resets=0,creates=0;
    bool fail=false;
    Labels(){s.style=buffer(style);for(auto& e:s.entity_table)e=&entity;player[7]=255;}
    static int run(void* p,const Nba97GamePlayerLabelEvent* e,Nba97GameRenderBuffer* created) {
        auto& s=*static_cast<Labels*>(p);
        if(e->kind==NBA97_LABEL_CREATE_30D18) {
            ++s.creates;s.text.emplace_back(e->text);check(e->x==-20&&e->y==-20&&e->id==245+static_cast<int>(s.creates));
            *created=s.fail?Nba97GameRenderBuffer{nullptr,0}:buffer(s.object);
        }else if(e->kind==NBA97_LABEL_RESET_PACKET_99960)++s.resets;
        return 1;
    }
};
void labels() {
    Labels f;unsigned done=0;f.s.option21d83=1;
    check(nba97_game_player_labels(&f.s,Labels::run,&f,&done)==1);
    check(done==10&&f.resets==20&&f.text.front()=="-1"&&f.s.dirty_fdb4e==1);
    check(f.style[0x2a]==1&&f.style[0x26]==0&&f.style[0x27]==1);
    Labels g;g.fail=true;g.s.option21d83=4;
    check(nba97_game_player_labels(&g.s,Labels::run,&g,&done)==1&&g.resets==0&&g.text[0].empty());
    Labels h;h.s.option21d83=3;std::memset(h.player.data()+0x29,'x',32);done=99;
    check(nba97_game_player_labels(&h.s,Labels::run,&h,&done)==NBA97_RENDER_TEXT_OVERFLOW);
    check(done==0&&h.style[0x2a]==3&&h.creates==0&&h.s.dirty_fdb4e==0);
    Labels k;k.s.option21d83=2;k.entity.word00=0x80000005u;
    check(nba97_game_player_labels(&k.s,Labels::run,&k,&done)==1&&k.text[0]=="-2147483648");
}
}
int main(){try{textures();heads();labels();std::cout<<checks<<" render entry checks passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
