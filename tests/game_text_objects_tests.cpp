#include "recovered/game_text_objects.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
unsigned checks=0;
void check(bool b){++checks;if(!b)throw std::runtime_error("text object check "+std::to_string(checks));}
constexpr std::uint32_t Base=0x800b0000,Style=0x80120000,Object=0x80130000,Pool=0x80140000,Bitmap=0x80150000,Text=0x80110000;
struct Fixture {
    std::vector<std::uint8_t> data=std::vector<std::uint8_t>(0xb0000,0xa5),known=std::vector<std::uint8_t>(data.size(),1);
    Nba97GameTextRegion region{Base,data.data(),known.data(),data.size()};
    Nba97GameTextContext context{{&region,1},100000,io,this};
    Nba97GameTextProgress progress{};std::uint32_t result=0xfeedbeef;
    std::vector<Nba97GameTextEvent> events;unsigned refuse=0;bool poison=false;
    std::uint8_t* at(std::uint32_t a){return data.data()+a-Base;}
    void put(std::uint32_t a,std::uint32_t v,unsigned n){for(unsigned i=0;i<n;++i)at(a)[i]=static_cast<std::uint8_t>(v>>(i*8));}
    std::uint32_t get(std::uint32_t a,unsigned n){std::uint32_t v=0;for(unsigned i=0;i<n;++i)v|=static_cast<std::uint32_t>(at(a)[i])<<(i*8);return v;}
    Fixture(){
        put(0x800b2048,Style,4);put(Style+8,0x80124000,4);put(Style+12,0x80126000,4);
        put(Style+16,Object,4);put(Style+20,0x80131000,4);put(Style+24,Pool,4);put(Style+28,Bitmap,4);
        put(Style+32,32,2);put(Style+34,8,2);put(Style+38,256,2);put(Style+40,1,2);put(Style+42,3,2);put(Style+64,0,2);
        for(unsigned i=48;i<64;i+=2)put(Style+i,65535,2);
        put(Style+0x43,3,1);put(Style+0x4b,1,1);put(Style+0x52,12,1);
        for(unsigned i=0;i<16;++i)put(Object+i*64+0x12,65535,2);
        std::memset(at(0x80131000),255,1024);std::memset(at(Bitmap),0,256);std::memset(at(0x80126000),255,4096);
        for(unsigned i=0;i<2;++i){
            put(0x80126000+512+('A'+i)*2,i,2);const auto g=0x80124000+i*20;
            put(g,0x09ffffff,4);put(g+8,255,1);put(g+9,5+i,1);put(g+10,10,1);put(g+11,0x2c,1);
        }
        put(0x800c55b8,0x800c5578,4);put(0x800c5578+44,0x8009a97c,4);put(0x800c55c2,0,1);
        std::memcpy(at(Text),"AB",3);
    }
    static int io(void* context,const Nba97GameTextEvent* event){
        auto& f=*static_cast<Fixture*>(context);f.events.push_back(*event);
        if(f.events.size()==f.refuse)return 0;
        // ExplicittestDMAfixture, notproductiondefault/hardwareproof.
        if(event->kind==NBA97_TEXT_PACKET_CLEAR_DISPATCH)f.put(event->object,0xffffff,4);
        if(f.poison)f.known[event->object-Base]=2;
        return 1;
    }
    int create(int id=246,unsigned mode=1){return nba97_game_text_create(&context,id,Text,100,20,mode,&result,&progress);}
};
void geometry(){
    Fixture f;check(f.create()==1&&f.result==Object&&f.progress.glyphs_written==2);
    check(f.events.size()==2&&f.events[0].target==0x8009a97c&&f.events[0].count==1);
    check(f.get(Object+12,2)==2&&f.get(Object+8,4)==Pool&&f.get(Bitmap,1)==1);
    check(f.get(Object,4)==(Pool&0xffffff)&&f.get(Object+4,4)==((Pool+40)&0xffffff));
    check(f.get(Pool,4)==(0x09000000|((Pool+80)&0xffffff))&&f.get(Pool+80,4)==0x090c567c);
    check(f.get(Pool+8,2)==96&&f.get(Pool+10,2)==19&&f.get(Pool+16,2)==101&&f.get(Pool+26,2)==29);
    check(f.get(Pool+80+8,2)==100&&f.get(Pool+4,3)==0x808080);
    check(f.get(Style+60,2)==0&&f.get(Style+62,2)==0&&f.get(Object+28,2)==65535);
    check(f.get(Object+30,2)==0&&f.get(Object+32,2)==0);
    check(nba97_game_text_reset_group(&f.context,3,&f.progress)==1&&f.progress.objects_reset==1);
    check(f.get(Object+18,2)==0&&f.get(Style+60,2)==0&&f.get(Bitmap,1)==1); // Notfree/unlink.
    check(f.create()==1&&f.result==Object+64&&f.get(Object+28,2)==1);
    check(f.get(Object+64+26,2)==0&&f.get(Object+64+28,2)==65535);
}
void allocator(){
    Fixture f;check(nba97_game_text_allocate_packets(&f.context,0,&f.result,&f.progress)==1&&f.result==Pool&&f.get(Bitmap,1)==0);
    check(nba97_game_text_allocate_packets(&f.context,2,&f.result,&f.progress)==1&&f.result==Pool&&f.get(Bitmap,1)==1);
    check(nba97_game_text_allocate_packets(&f.context,3,&f.result,&f.progress)==1&&f.result==Pool+160&&f.get(Bitmap+1,1)==1&&f.get(Bitmap+2,1)==1);
    Fixture g;g.put(Style+32,0,2);
    check(nba97_game_text_allocate_packets(&g.context,1,&g.result,&g.progress)==1&&g.result==Pool&&g.get(Bitmap,1)==1); // Strict<offbyone.
    check(nba97_game_text_allocate_packets(&g.context,1,&g.result,&g.progress)==1&&g.result==0);
    Fixture h;h.put(Style+34,1,2);h.put(Object+18,0,2);
    check(h.create()==1&&h.result==Object); // Exhaustedobjectscan stillusesreachedslot.
    check(h.create()==1&&h.result==Object&&h.get(Object+28,2)==0); // Reusecancreateselflink.
    h.context.step_budget=24;
    check(nba97_game_text_reset_group(&h.context,3,&h.progress)==NBA97_TEXT_LIMIT&&h.progress.objects_reset>1);
    Fixture empty;empty.put(Text,0,1);
    check(empty.create()==1&&empty.result==Object&&empty.get(Object+12,2)==0&&empty.get(Bitmap,1)==0);
    check(empty.get(Object,4)==0xc567c&&empty.progress.glyphs_written==0);
}
void boundaries(){
    Fixture f;f.refuse=2;
    check(f.create()==NBA97_TEXT_IO_REFUSED&&f.result==0xfeedbeef&&f.progress.callbacks_completed==1);
    check(f.get(Object,4)==0xc567c&&f.get(Object+4,4)==0xa5a5a5a5&&f.get(Bitmap,1)==1);
    check(f.get(Object+12,2)==2&&f.get(Style+60,2)==0&&f.progress.glyphs_written==0);
    Fixture g;g.context.io=nullptr;check(g.create()==NBA97_TEXT_IO_REFUSED&&g.get(Object+8,4)==Pool);
    Fixture h;h.known[Object+18-Base]=0;
    check(h.create()==NBA97_TEXT_UNKNOWN&&h.get(Style+64,2)==0&&h.get(Bitmap,1)==0);
    Fixture i;i.known[Object+14-Base]=0;i.known[Object+15-Base]=2;
    check(i.create()==NBA97_TEXT_ARGUMENT&&i.get(Object+8,4)==Pool&&i.known[Object+14-Base]==0&&i.known[Object+15-Base]==2);
    Fixture j;j.poison=true;
    check(j.create()==NBA97_TEXT_ARGUMENT&&j.known[Object-Base]==2&&j.get(Object,4)==0xffffff&&j.progress.callbacks_completed==1);
    Fixture k;k.known[Pool+38-Base]=0;
    check(k.create()==1&&k.known[Pool+38-Base]==0&&k.known[Pool+78-Base]==0); // Opaquepadding copiedunknown.
    Fixture l;l.put(Style+24,Pool+2,4);
    check(l.create()==NBA97_TEXT_ALIGNMENT_TRAP&&l.events.size()==2&&l.progress.glyphs_written==0);
    Fixture m;m.put(0x800c55c2,2,1);m.put(0x800c55bc,0x801e0000,4);
    check(m.create()==1&&m.events.size()==4&&m.events[0].kind==NBA97_TEXT_DIAGNOSTIC_99960&&m.events[1].kind==NBA97_TEXT_PACKET_CLEAR_DISPATCH);
    m.progress.steps=777;check(nba97_game_text_create(nullptr,0,0,0,0,0,&m.result,&m.progress)==0&&m.progress.steps==777);
    Fixture n;n.put(Style+8,0x80170000,4);
    check(n.create()==NBA97_TEXT_RESOURCE&&n.get(Object+12,2)==0&&n.events.empty()&&n.result==0xfeedbeef);
    Fixture o;o.context.step_budget=0;
    check(o.create()==NBA97_TEXT_LIMIT&&o.progress.steps==0&&o.get(Object+12,2)==0xa5a5);
    Fixture p;std::array<Nba97GameTextRegion,2> overlapping{p.region,p.region};
    p.context.memory={overlapping.data(),overlapping.size()};p.progress.steps=987;
    check(p.create()==NBA97_TEXT_ARGUMENT&&p.progress.steps==987&&p.result==0xfeedbeef);
    Fixture q;q.put(Style+32,0,2);q.put(Bitmap,1,1);
    check(q.create()==1&&q.result==0&&q.get(Object+8,4)==0&&q.get(Object+12,2)==2&&q.events.empty());
}
void span(){
    Fixture a,b;check(a.create()==1);
    const Nba97GameTextSpan text{b.at(Text),b.known.data()+Text-Base,3};
    check(nba97_game_text_create_span(&b.context,246,text,100,20,1,&b.result,&b.progress)==1);
    check(a.data==b.data&&a.known==b.known&&a.result==b.result); // Sharedowner, nofakeoriginalstackaddress.
    Fixture c;const std::array<std::uint8_t,2> truncated{'A','B'};
    check(nba97_game_text_create_span(&c.context,246,{truncated.data(),nullptr,2},100,20,1,&c.result,&c.progress)==NBA97_TEXT_RESOURCE);
    check(c.progress.stopped_in_text==1&&c.progress.stopped_address==2&&c.get(Object+12,2)==2&&c.events.empty());
    Fixture d;std::array<std::uint8_t,3> known{1,1,0};
    check(nba97_game_text_create_span(&d.context,246,{d.at(Text),known.data(),3},100,20,1,&d.result,&d.progress)==NBA97_TEXT_UNKNOWN);
    check(d.progress.stopped_in_text==1&&d.progress.stopped_address==2&&d.get(Object+12,2)==2);
}
}
int main(){try{geometry();allocator();boundaries();span();std::cout<<checks<<" text object checks passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
