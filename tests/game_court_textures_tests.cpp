#include "recovered/game_court_textures.h"
#include "game_render_backend.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
unsigned checks=0;
void require(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
struct Fixture {
    std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(32768),known=std::vector<std::uint8_t>(32768,1);
    Nba97GameImageMemory memory{bytes.data(),known.data(),bytes.size(),0,1};
    Nba97GameCourtTextureState state{};Nba97GameCourtTextureProgress progress{};
    nba97::GameVramWords vram;std::vector<Nba97GameImageTransfer> events;
    unsigned reject=0;bool stop_after_first=false;
    Fixture(unsigned count=2){put(8,4,count);for(unsigned i=0;i<count;++i){const unsigned at=512+i*256;put(20+i*8,4,at);header(at,0x40);}}
    void put(unsigned at,unsigned width,std::uint32_t value){for(unsigned i=0;i<width;++i)bytes.at(at+i)=std::uint8_t(value>>(i*8));}
    unsigned get(unsigned at,unsigned width=2){unsigned value=0;for(unsigned i=0;i<width;++i)value|=unsigned(bytes.at(at+i))<<(i*8);return value;}
    void header(unsigned at,unsigned type){
        put(at,4,(64u<<8)|type);put(at+4,2,8);put(at+6,2,2);put(at+12,2,640);put(at+14,2,32);
        put(at+64,4,0x23);put(at+68,2,32);put(at+70,2,1);
        for(unsigned i=0;i<16;++i)put(at+16+i*2,2,0x3210);
        for(unsigned i=0;i<32;++i)put(at+80+i*2,2,i);
    }
    static int io(void* user,const Nba97GameImageTransfer* event){
        auto& f=*static_cast<Fixture*>(user);f.events.push_back(*event);
        if(f.reject==f.events.size())return 0;
        if(f.stop_after_first)f.put(8,4,1);
        if(event->rect.w<=0||event->rect.h<=0){nba97::GameRenderBackend sdk;
            sdk.sdkTransferLimitsKnown=true;sdk.sdkTransferWidth=1024;sdk.sdkTransferHeight=512;
            return nba97::GameRenderBackend::transferIo(&sdk,event);}
        return f.vram.upload(event->rect,event->source)==nba97::GameRenderBackendResult::Complete?1:0;
    }
    int run(std::size_t budget=100){return nba97_game_court_textures({&memory,0},&state,budget,100,io,this,&progress);}
};
void placement(){
    Fixture f(18);f.header(512+256,0x41);require(f.run()==NBA97_IMAGE_COMPLETE,"real image owner and VRAM compose");
    require(f.progress.completed&&f.progress.images_completed==18,"entire container");
    require(f.progress.palette_stores==17&&f.progress.palette_width_stores==17,"only four-bit palettes capped");
    require(f.events.size()==36,"texture and palette transfers retained");
    require(f.events[1].rect.x==512&&f.events[1].rect.y==252&&f.events[1].rect.w==16,"initial four-bit palette");
    require(f.events[3].rect.x==512&&f.events[3].rect.y==240&&f.events[3].rect.w==32,"other palette row independent");
    require(f.events[33].rect.x==752&&f.events[33].rect.y==252,"last palette before row wrap");
    require(f.events[35].rect.x==512&&f.events[35].rect.y==251,"four-bit row wraps after16 entries");
    require(f.state.palette_known&&f.state.palette_fed1c.memory==&f.memory&&f.state.palette_fed1c.offset==512+17*256+64,"last real palette reference");
    require(f.get(512+256+68)==32&&f.get(512+68)==16,"signed width clamp is format-specific");
    std::uint16_t word=0;require(f.vram.word(512+15,251,word)&&word==15,"palette pixel upload");
    require(!f.vram.word(528,251,word),"clamp does not upload discarded entries");
}
void partial(){
    Fixture f;f.reject=2;require(f.run()==NBA97_IMAGE_IO_REFUSED,"palette transfer refusal");
    require(!f.progress.completed&&f.progress.images_completed==0&&f.progress.palette_stores==1&&f.progress.palette_width_stores==1,"prior palette/width effects survive");
    require(f.progress.stopped_pc==0x80048868&&f.progress.image.uploads_completed==1,"nested upload prefix retained");
    Fixture budget;require(budget.run(1)==NBA97_COURT_TEXTURE_IMAGE_LIMIT&&budget.progress.images_completed==1&&!budget.progress.completed,"bounded image loop");
    Fixture changed;changed.stop_after_first=true;require(changed.run()==NBA97_IMAGE_COMPLETE&&changed.progress.images_completed==1,"count reread after callback");
    Fixture empty(0);require(empty.run(0)==NBA97_IMAGE_COMPLETE&&empty.events.empty(),"zero count has no image reads");
    Fixture negative(0);negative.put(8,4,0xffffffff);require(negative.run()==NBA97_IMAGE_COMPLETE&&negative.events.empty(),"signed negative count returns");
    Fixture dead;dead.known.back()=2;require(dead.run()==NBA97_IMAGE_COMPLETE,"unreached tail metadata not preflighted");
    Fixture unknown;unknown.known[8]=0;require(unknown.run()==NBA97_IMAGE_UNKNOWN&&unknown.progress.stopped_pc==0x800a3fe0,"unknown count refused");
    Fixture poison;poison.known[8]=0;poison.known[11]=2;require(poison.run()==NBA97_IMAGE_ARGUMENT,"whole reached width checked for malformed knownness");
    Fixture align;align.memory.address_mod4=1;require(align.run()==NBA97_IMAGE_ALIGNMENT_TRAP,"original alignment, not host pointer");
    Fixture short_palette;short_palette.known[512+68]=0;require(short_palette.run()==NBA97_IMAGE_UNKNOWN&&!short_palette.state.palette_known,"palette width read precedes pointer publication");
    Fixture xy;xy.known[512+12]=0;require(xy.run()==NBA97_IMAGE_UNKNOWN&&xy.state.palette_known&&xy.get(512+68)==16,"width clamp precedesXY read");
    Fixture slot;slot.state.palette_known=2;require(slot.run()==NBA97_IMAGE_ARGUMENT&&slot.progress.stopped_pc==0x8004883c&&slot.get(512+68)==32,"malformed palette metadata not overwritten");
    Fixture malformed(1);malformed.put(512+68,2,0xffff);require(malformed.run()==NBA97_IMAGE_COMPLETE&&malformed.get(512+68)==0xffff&&malformed.progress.palette_width_stores==0,"negative palette width retained; SDK returns without transfer");
}
}
int main(){try{placement();partial();std::cout<<checks<<" court texture checks passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
