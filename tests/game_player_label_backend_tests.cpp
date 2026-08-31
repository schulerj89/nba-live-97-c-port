#include "game_player_label_backend.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace nba97;
unsigned checks=0;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(std::to_string(checks)+": "+why);}
constexpr std::uint32_t Base=0x800b0000,Style=0x80120000,Object=0x80130000,Pool=0x80140000;
struct Fixture {
    GameRenderMemory memory;
    GameRenderMemory::Allocation allocation=0;
    Nba97GameRenderBuffer all{};
    std::vector<GameTextBinding> bindings;
    std::array<Nba97GamePlayerLabelEntity,10> entities{};
    Nba97GamePlayerLabels labels{};
    unsigned calls=0,refuse=0;
    bool poisonNextPlayer=false;
    bool misalignStyle=false;
    bool poisonObjectStore=false;
    std::uint8_t* at(std::uint32_t address){return all.data+address-Base;}
    void put(std::uint32_t address,std::uint32_t value,unsigned size){for(unsigned i=0;i<size;++i)at(address)[i]=static_cast<std::uint8_t>(value>>(i*8));}
    std::uint32_t get(std::uint32_t address,unsigned size){std::uint32_t value=0;for(unsigned i=0;i<size;++i)value|=std::uint32_t(at(address)[i])<<(i*8);return value;}
    Nba97GameRenderBuffer source(std::uint32_t address,unsigned size){
        for(const auto& b:bindings)if(address>=b.sourceAddress&&address-b.sourceAddress<=b.view.size&&size<=b.view.size-(address-b.sourceAddress))
            return {b.view.data+address-b.sourceAddress,size};
        return {};
    }
    Fixture() {
        allocation=memory.add(std::vector<std::uint8_t>(0xb0000,0xa5),std::vector<std::uint8_t>(0xb0000,1),0);
        all=memory.buffer(allocation,0,0xb0000);bindings.push_back({Base,all});
        put(0x800b2048,Style,4);put(Style+8,0x80124000,4);put(Style+12,0x80126000,4);
        put(Style+16,Object,4);put(Style+20,0x80131000,4);put(Style+24,Pool,4);put(Style+28,0x80150000,4);
        put(Style+32,64,2);put(Style+34,16,2);put(Style+38,256,2);put(Style+40,1,2);put(Style+42,1,2);put(Style+64,0,2);
        for(unsigned i=48;i<64;i+=2)put(Style+i,65535,2);
        put(Style+0x43,3,1);put(Style+0x4b,1,1);put(Style+0x52,12,1);
        for(unsigned i=0;i<16;++i)put(Object+i*64+0x12,65535,2);
        std::memset(at(0x80131000),255,1024);std::memset(at(0x80150000),0,256);std::memset(at(0x80126000),255,4096);
        for(unsigned i=0;i<2;++i){
            put(0x80126000+512+('A'+i)*2,i,2);const auto glyph=0x80124000+i*20;
            put(glyph,0x09ffffff,4);put(glyph+8,255,1);put(glyph+9,5+i,1);put(glyph+10,10,1);put(glyph+11,0x2c,1);
        }
        put(0x800c55b8,0x800c5578,4);put(0x800c5578+44,0x8009a97c,4);put(0x800c55c2,0,1);
        for(unsigned i=0;i<10;++i){
            auto player=memory.buffer(allocation,0xa8000+i*128,128);
            std::memcpy(player.data+0x29,"AB",3);entities[i]={i,0,player};labels.entity_table[i]=&entities[i];
        }
        labels.option21d83=3;
    }
    static int io(void* context,const Nba97GameTextEvent* event){
        auto& f=*static_cast<Fixture*>(context);++f.calls;
        if(f.calls==f.refuse)return 0;
        // Explicit source boundary fixture, not production DMA/hardware proof.
        if(event->kind!=NBA97_TEXT_PACKET_CLEAR_DISPATCH || event->target!=0x8009a97c || event->count!=1)return 0;
        if(f.poisonNextPlayer && f.calls==1){Nba97GameImageMemory view{};
            f.memory.describe(f.entities[1].player,view);view.known[0x29]=0;}
        if(f.misalignStyle && f.calls==1)f.put(0x800b2048,Style+1,4);
        if(f.poisonObjectStore && f.calls==2){Nba97GameImageMemory view{};
            f.memory.describe(f.all,view);view.known[Object+0x20-Base]=0;view.known[Object+0x21-Base]=0;
            view.known[Object+0x1e - Base]=0;view.known[Object+0x1f-Base]=2;}
        const auto word=f.source(event->object,4);if(!word.data)return 0;
        word.data[0]=word.data[1]=word.data[2]=255;word.data[3]=0;return 1;
    }
    GamePlayerLabelResult run(){return runGamePlayerLabels(memory,labels,bindings,io,this);}
    void split(bool omitFont=false){
        bindings.clear();auto same=[&](std::uint32_t a,std::size_t n){bindings.push_back({a,memory.buffer(allocation,a-Base,n)});};
        same(Base,Style-Base);same(Style+0x54,Object-(Style+0x54));same(Object+16*64,Base+all.size-(Object+16*64));
        same(Style+8,0x1e);if(!omitFont)same(Style+0x26,2);same(Style+0x28,0x1a);
        same(Style+0x43,1);same(Style+0x4b,1);same(Style+0x52,1);
        for(unsigned i=0;i<16;++i)for(unsigned offset=0;offset<64;offset+=4){
            if(offset>0x20&&offset!=0x28&&offset!=0x38)continue;
            const auto a=Object+i*64+offset;
            const auto id=memory.add(std::vector<std::uint8_t>(at(a),at(a)+4),std::vector<std::uint8_t>(4,1),0);
            bindings.push_back({a,memory.buffer(id,0,4)});
        }
    }
};
void composed(){
    Fixture f;const auto result=f.run();
    check(result.result==NBA97_RENDER_COMPLETE&&result.completed==10,"all ten actual labels composed");
    check(result.textResult==NBA97_TEXT_COMPLETE&&f.calls==40,"create plus outer35A24 resets both banks");
    check(f.labels.dirty_fdb4e==1&&f.get(Style+42,2)==1,"final style group and dirty flag");
    check(f.get(Style+60,2)==0&&f.get(Style+62,2)==9,"ten objects linked in source group3");
    for(unsigned i=0;i<10;++i){
        const auto object=Object+i*64,packet=Pool+i*160;
        check(f.get(object+8,4)==packet&&f.get(object+12,2)==2,"real packet allocation per label");
        check(f.get(object,4)==0xc567c&&f.get(object+4,4)==0xc567c,"outer reset hides created packet lists exactly");
        check(f.get(object+14,2)==65516&&f.get(object+16,2)==65516,"source offscreen label position");
        check(f.get(packet+8,2)==65512&&f.get(packet+10,2)==65515,"actual glyph positions retained");
    }
    check(f.labels.style.data==f.at(Style),"current style remains actual retained view");
}
void refusal(){
    Fixture f;f.refuse=3;auto result=f.run();
    check(result.result==NBA97_RENDER_IO_REFUSED&&result.completed==0&&result.textResult==NBA97_TEXT_IO_REFUSED,"outer reset failure is explicit");
    check(f.get(Object,4)==(Pool&0xffffff)&&f.labels.dirty_fdb4e==0,"created list source prefix survives refused outer reset");
    Fixture missing;result=runGamePlayerLabels(missing.memory,missing.labels,missing.bindings,nullptr,nullptr);
    check(result.result==NBA97_RENDER_IO_REFUSED&&result.textResult==NBA97_TEXT_IO_REFUSED,"no default SDK completion");
    check(missing.get(Object+8,4)==Pool&&missing.get(Style+60,2)==0,"allocation prefix remains before missing DMA");
    Fixture bad;bad.bindings[0].sourceAddress=Base+1;const auto before=bad.get(Style+42,2);result=bad.run();
    check(result.result==NBA97_RENDER_ARGUMENT&&bad.calls==0&&bad.get(Style+42,2)==before,"alignment provenance mismatch before source work");
    Fixture unknown;Nba97GameImageMemory view{};check(unknown.memory.describe(unknown.all,view),"owned memory described");
    view.known[0xa8000+0x29]=0;result=unknown.run();
    check(result.result==NBA97_LABEL_UNKNOWN&&unknown.calls==0&&unknown.get(Style+42,2)==3,"unknown text refused at actual read after group store");
    Fixture overlap;overlap.bindings.push_back(overlap.bindings.front());result=overlap.run();
    check(result.result==NBA97_RENDER_ARGUMENT&&overlap.calls==0,"source binding overlap rejected");
    Fixture late;late.poisonNextPlayer=true;result=late.run();
    check(result.result==NBA97_LABEL_UNKNOWN&&result.completed==1,"callback-created unknown next name refused only when reached");
    check(late.calls==4&&late.get(Object+64+0x12,2)==65535,"label0 resets complete before unknown label1 read");
    Fixture alignment;const auto oddFontBefore=alignment.get(Style+0x27,2);alignment.misalignStyle=true;result=alignment.run();
    check(result.result==NBA97_LABEL_ALIGNMENT&&result.completed==1,"callback-created odd style refused at next actual style store");
    check(alignment.calls==4&&alignment.get(Style+0x27,2)==oddFontBefore,"both outer resets precede odd style refusal");
}
void partial(){
    Fixture f;Nba97GameImageMemory view{};check(f.memory.describe(f.all,view),"partial retained memory");
    auto unknown=[&](std::uint32_t address,std::size_t count){std::memset(view.known+address-Base,0,count);};
    unknown(Style,8);unknown(Style+0x42,16);view.known[Style+0x43-Base]=view.known[Style+0x4b-Base]=1;
    unknown(Style+0x26,2);unknown(Style+0x2a,2);
    unknown(Object,16*64);for(unsigned i=0;i<16;++i)view.known[Object+i*64+0x12-Base]=view.known[Object+i*64+0x13-Base]=1;
    unknown(Pool,0x1000);
    for(unsigned i=0;i<2;++i)unknown(0x80124000+i*20,3);
    for(unsigned i=0;i<10;++i){unknown(Base+0xa8000+i*128,128);std::memset(view.known+0xa8000+i*128+0x29,1,3);}
    // Unvisited metadata is not repaired or eagerly validated.
    view.known[Style+0x49-Base]=2;
    const auto result=f.run();
    check(result.result==1&&result.completed==10&&f.calls==40,"partial producer-style resources create all labels");
    check(view.known[Style-Base]==0&&view.known[Style+0x49-Base]==2,"unread style bytes retained");
    check(view.known[Object+0x22-Base]==0&&view.known[Object+0x3f-Base]==0,"object padding remains unknown");
    check(view.known[Object+0x1e - Base]==1&&view.known[Object+0x20-Base]==1,"reached direct stores known");
    check(view.known[Pool+38-Base]==0&&view.known[Pool+78-Base]==0,"packet padding copied opaque");
    check(view.known[Pool-Base]==1&&view.known[Pool+40-Base]==1&&view.known[0x80124000-Base]==0,"splice replaces copied unknown links without changing font");
    Fixture bad;bad.poisonObjectStore=true;const auto stopped=bad.run();bad.memory.describe(bad.all,view);
    check(stopped.result==NBA97_RENDER_ARGUMENT&&stopped.completed==0&&bad.calls==2,"reached write rejects invalid later knownness byte");
    check(view.known[Object+0x20-Base]==1&&view.known[Object+0x21-Base]==1&&view.known[Object+0x1e - Base]==0&&view.known[Object+0x1f-Base]==2,"35B90 prefix remains before35B98 metadata refusal");
}
void split_ranges(){
    Fixture f;f.split();const auto result=f.run();
    check(result.result==1&&result.completed==10&&f.calls==40,"split style and noncontiguous native object fields compose");
    check(f.labels.style.data==nullptr&&f.get(Style+42,2)==1,"unmapped unused style byte0 does not refuse real stores");
    const auto word=f.source(Object,4),other=f.source(Object+4,4),field=f.source(Object+0x20,2);
    check(word.data&&other.data&&word.data+4!=other.data&&word.data[0]==0x7c&&other.data[0]==0x7c,"two reset offsets preserve source identity across separate allocations");
    check(field.data[0]==0&&field.data[1]==0&&f.get(Object,4)==0xa5a5a5a5,"actual split object stores never mutate discarded flat mirror");
    Fixture missing;missing.split(true);const auto stopped=missing.run();
    check(stopped.result==NBA97_RENDER_RESOURCE&&stopped.completed==0&&missing.calls==0,"missing reached style half refuses before create");
    check(missing.get(Style+42,2)==3&&missing.labels.dirty_fdb4e==0,"initial35A74 store survives missing35B70 span");
}
void cloned(){
    Fixture live;auto candidate=live.memory;auto labels=live.labels;auto entities=live.entities;
    auto bindings=live.bindings;
    for(auto& binding:bindings)check(candidate.rebind(live.memory,binding.view,binding.view),"binding clone");
    for(unsigned i=0;i<10;++i){check(candidate.rebind(live.memory,entities[i].player,entities[i].player),"player clone");labels.entity_table[i]=&entities[i];}
    struct Clear {Nba97GameRenderBuffer all;unsigned calls=0;
        static int io(void* p,const Nba97GameTextEvent* event){auto& s=*static_cast<Clear*>(p);++s.calls;
            if(event->kind!=NBA97_TEXT_PACKET_CLEAR_DISPATCH||event->count!=1)return 0;
            auto* bytes=s.all.data+event->object-Base;bytes[0]=bytes[1]=bytes[2]=255;bytes[3]=0;return 1;}} clear{bindings[0].view};
    auto result=runGamePlayerLabels(candidate,labels,bindings,Clear::io,&clear);
    check(result.result==NBA97_RENDER_COMPLETE&&clear.calls==40,"composed candidate complete");
    check(live.get(Style+60,2)==65535&&live.labels.dirty_fdb4e==0&&live.get(Object+18,2)==65535,"original allocation generation untouched");
    check(labels.style.data!=live.at(Style),"no stale style pointer after clone");
}
}
int main(){try{composed();refusal();partial();split_ranges();cloned();std::cout<<checks<<" composed native player label checks passed\n";return 0;}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
