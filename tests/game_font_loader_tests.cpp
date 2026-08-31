#include "recovered/game_font_loader.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
static unsigned checks;
static void check(bool v){++checks;if(!v){std::fprintf(stderr,"check%u failed\n",checks);std::abort();}}
namespace {
constexpr uint32_t Base=0x80000000,Style=0x80001000,Font=0x80002000,Map=0x80003000,Desc=0x80004000,Filename=0x80005000;
struct Fixture {
    std::vector<uint8_t> bytes=std::vector<uint8_t>(0x100000),known=std::vector<uint8_t>(bytes.size(),1);
    Nba97GameTextRegion region{Base,bytes.data(),known.data(),bytes.size()};
    Nba97GameFontContext context{{&region,1},10000,io,this};Nba97GameFontScratch scratch{};Nba97GameFontProgress progress{};
    std::vector<Nba97GameFontEvent> events;unsigned nulls=0,refuse=0;bool mutate=false;
    void put(uint32_t at,uint32_t v,unsigned n){for(unsigned i=0;i<n;++i)bytes[at-Base+i]=uint8_t(v>>(i*8));}
    uint32_t get(uint32_t at,unsigned n){uint32_t v=0;for(unsigned i=0;i<n;++i)v|=uint32_t(bytes[at-Base+i])<<(i*8);return v;}
    Fixture(){
        put(0x800b2048,Style,4);put(Style+8,Desc,4);put(Style+12,Map,4);
        std::memcpy(bytes.data()+0x24920,"zovlfont.psh",12);std::memcpy(bytes.data()+Filename-Base,"font.psh",9);
        put(Font+8,1,4);put(Font+16,0x31343030,4);put(Font+20,0x100,4);
        put(Font+0x100,0x20041,4);put(Font+0x104,8,2);put(Font+0x106,12,2);
        put(Font+0x10a,2,1);put(Font+0x10c,64,2);put(Font+0x10e,256,2);
        put(Font+0x300,0x23,4);put(Font+0x304,256,2);put(Font+0x306,1,2);
        std::memset(scratch.packet_known,1,sizeof scratch.packet_known);std::memset(scratch.name_known,1,sizeof scratch.name_known);
        scratch.packet[0]=0x12;scratch.packet[1]=0x34;scratch.packet[2]=0x56;
    }
    static int io(void* user,const Nba97GameFontEvent* e,uint32_t* result){
        auto& f=*static_cast<Fixture*>(user);f.events.push_back(*e);if(f.refuse==f.events.size())return 0;
        if(e->kind==NBA97_FONT_LOAD_ATTEMPT_941C8){if(f.nulls){--f.nulls;*result=0;}else *result=Font;}
        // Explicit test upload boundary models these fixtures' sourceheader
        // coordinatewrites; it does not establish GPU/imagebackend ownership.
        if(e->kind==NBA97_FONT_UPLOAD_CHAIN_94540||e->kind==NBA97_FONT_UPLOAD_946B8){f.put(e->resource+12,uint32_t(e->x),2);f.put(e->resource+14,uint32_t(e->y),2);}
        if(f.mutate&&e->kind==NBA97_FONT_RELEASE_90698)f.put(Style+0x24,32767,2);
        return 1;
    }
    int run(){return nba97_game_font_load(&context,Filename,8,0,768,240,1,&scratch,&progress);}
};
}
int main(){
    check(nba97_game_font_decode('f','F')==255);check(nba97_game_font_decode('g','z')==0);check(nba97_game_font_decode(0x131,0x139)==25);
    {Fixture f;check(f.run()==1);check(f.events.size()==4);check(f.progress.glyphs_written==1);check(f.get(Desc,4)==0x09563412);
        check(f.get(Desc+8,1)==254);check(f.get(Desc+9,1)==8);check(f.get(Desc+10,1)==12);check(f.get(Desc+11,1)==0x2c);
        check(f.get(Style+0x24,2)==1&&f.get(Style+0x26,2)==256);check(f.get(Map+65*2,2)==0);check(f.get(Font+0x100,4)==0x41);}
    {Fixture f;f.nulls=2;check(f.run()==1);check(f.events.size()==6);check(f.progress.callbacks_completed==6);}
    {Fixture f;f.nulls=10000;f.context.step_budget=40;check(f.run()==NBA97_FONT_LIMIT);check(f.get(Style+0x24,2)==0);}
    {Fixture f;f.refuse=3;check(f.run()==NBA97_FONT_IO_REFUSED);check(f.get(Font+0x100,4)==0x41);check(f.progress.glyphs_written==0);check(f.events.size()==3);}
    {Fixture f;f.mutate=true;check(f.run()==1);check(f.get(Style+0x24,2)==32768);}
    {Fixture f;f.scratch.packet_known[0]=0;check(f.run()==1);check(f.known[Desc-Base]==0);check(f.known[Desc-Base+3]==1);}
    {Fixture f;f.scratch.packet_known[0]=0;f.scratch.packet_known[2]=2;check(f.run()==NBA97_FONT_ARGUMENT);check(f.get(Desc,4)==0);check(f.get(Desc+9,1)==8);}
    {Fixture f;f.known[Desc-Base+14]=2;check(f.run()==NBA97_FONT_ARGUMENT);check(f.get(Desc+9,1)==0);}
    {Fixture f;f.known[Font-Base+0x104]=0;check(f.run()==NBA97_FONT_UNKNOWN);check(f.events.size()==2);}
    {Fixture f;f.put(Font+20,0x101,4);check(f.run()==NBA97_FONT_ALIGNMENT_TRAP);check(f.events.size()==1);}
    {Fixture f;f.put(Font+8,0xffffffff,4);check(f.run()==1);check(f.events.size()==2);check(f.get(Style+0x24,2)==65535);}
    {Fixture f;uint32_t out=123;check(nba97_game_font_shpp_entry(&f.context,Font,1,&out,&f.progress)==1&&out==0);
        f.put(Font+20,0xffffff00,4);check(nba97_game_font_shpp_entry(&f.context,Font,0,&out,&f.progress)==1&&out==Font-256);
        f.known[Font-Base+8]=0;out=123;check(nba97_game_font_shpp_count(&f.context,Font,&out,&f.progress)==NBA97_FONT_UNKNOWN&&out==123);}
    {Fixture f;f.put(Desc,0xaabbccdd,4);check(nba97_game_font_shpp_name(&f.context,Font,1,Desc,&f.progress)==1);check(f.get(Desc,4)==0);
        check(nba97_game_font_shpp_name(&f.context,Font,0,Desc,&f.progress)==1);check(f.get(Desc,4)==0x31343030);
        check(nba97_game_font_shpp_name(&f.context,Font,0xffffffff,Desc+1,&f.progress)==NBA97_FONT_ALIGNMENT_TRAP);}
    std::printf("game_font_loader: %u checks passed\n",checks);return 0;
}
