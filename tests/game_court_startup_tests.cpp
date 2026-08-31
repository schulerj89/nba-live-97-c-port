#include "recovered/game_court_startup.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
unsigned checks=0;
void require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
struct Fixture {
    std::array<std::vector<std::uint8_t>,7> bytes,known;
    std::array<Nba97GameTextRegion,7> regions{};
    std::array<Nba97GameCourtStartupEvent,100> events{};
    Nba97GameCourtStartupContext context{};
    Nba97GameCourtStartupProgress progress{};
    bool refuse=false,null_load=false,change_on_free=false;
    Fixture(){
        const std::uint32_t bases[]={0x800dcf10,0x80103508,0x800fcc54,0x8001ec94,0x80021d74,0x800b763c,0x801041a4};
        const std::size_t sizes[]={4,4,4,4,4,0x10c,144};
        for(unsigned i=0;i<7;++i){bytes[i].resize(sizes[i],0xa5);known[i].resize(sizes[i],0);regions[i]={bases[i],bytes[i].data(),known[i].data(),sizes[i]};}
        context={{regions.data(),regions.size()},io,this};
        put(0x800dcf10,0);put(0x8001ec94,0);put(0x80021d74,0);
        for(unsigned i=0;i<32;++i){put(0x800b763c+i*4,0x80120000+i*16);put(0x800b76c8+i*4,0x80130000+i*16);}
    }
    std::pair<std::uint8_t*,std::uint8_t*> at(std::uint32_t address){
        for(auto& r:regions)if(address>=r.base&&address-r.base<r.size)return {r.data+address-r.base,r.known+address-r.base};
        throw std::runtime_error("fixture address");
    }
    void put(std::uint32_t address,std::uint32_t value){for(unsigned i=0;i<4;++i){auto p=at(address+i);*p.first=std::uint8_t(value>>(i*8));*p.second=1;}}
    // Explicit service fixtures exercise orchestration ONLY, not source loading,
    // heap/free, GPU synchronization, original resources or natural match entry.
    static int io(void* user,const Nba97GameTextMemory*,const Nba97GameCourtStartupEvent* event,std::uint32_t* returned){
        auto& f=*static_cast<Fixture*>(user);
        if(f.refuse)return 0;
        if(event->kind==NBA97_COURT_STARTUP_FREE_90698&&f.change_on_free)f.put(0x800dcf10,1);
        *returned=event->kind==NBA97_COURT_STARTUP_LOAD_29BFC&&!f.null_load?0x80150000u:0u;
        return 1;
    }
    int texture(std::size_t cap=100,std::size_t budget=1000){return nba97_game_court_startup_select_texture(&context,budget,events.data(),cap,&progress);}
    int geometry(std::size_t cap=100,std::size_t budget=1000){return nba97_game_court_startup_select_geometry(&context,0x80140000,budget,events.data(),cap,&progress);}
};
void selection(){
    for(unsigned team=0;team<32;++team){
        Fixture f;f.put(0x80021d74,team);
        require(f.texture()==1&&f.progress.filename==0x80130000+team*16,"home texture selection");
        require(f.progress.events==3&&f.progress.stopped_pc==0x800487b8&&f.progress.loaded_resource==0x80150000,"texture interval boundary");
        require(f.events[0].pc==0x80048754&&f.events[1].pc==0x8004875c,"sentinel store order");
        require(f.geometry()==1&&f.progress.filename==0x80120000+team*16,"home geometry selection");
        require(f.events[0].kind==NBA97_COURT_STARTUP_SYNC_994F4&&f.events[1].kind==NBA97_COURT_STARTUP_FREE_90698,"sync precedes free");
        require(f.events[1].argument[0]==0x80140000&&f.events[2].argument[1]==0,"original free pointer and flags0");
    }
    Fixture neutral;neutral.put(0x8001ec94,0xffffffffu);*neutral.at(0x80021d74).second=0;
    require(neutral.texture()==1&&neutral.progress.filename==0x801301f0,"nonzero neutral ignores unknown home");
    require(neutral.geometry()==1&&neutral.progress.filename==0x801201f0,"neutral geometry slot31");
    Fixture wrapped;wrapped.put(0x80021d74,0x40000002u);
    require(wrapped.texture()==1&&wrapped.progress.filename==0x80130020,"raw source index wrapping");
    require(wrapped.geometry()==1&&wrapped.progress.filename==0x80120020,"geometry raw wrapping");
}
void special_packets(){
    Fixture f;f.put(0x800dcf10,0x80000000u);*f.at(0x8001ec94).second=0;
    require(f.texture()==1&&f.progress.filename==0x800260a0,"special literal texture name");
    require(f.geometry()==1&&f.progress.filename==0x800260b0&&f.progress.stores==92,"all four special gradient packets");
    for(unsigned packet=0;packet<4;++packet){const auto base=0x801041a4+packet*36;
        for(unsigned offset=0;offset<36;++offset){const auto p=f.at(base+offset);
            const bool untouched=offset<3||offset==15||offset==23||offset==31;
            require(*p.second==(untouched?0:1),"only source-initialized bytes known");
            if(untouched)require(*p.first==0xa5,"untouched packet bytes retained");
        }
        require(*f.at(base+3).first==8&&*f.at(base+7).first==0x38,"tag and command");
        require(*f.at(base+26).first==(packet%2+1)*48,"bottom coordinate");
    }
    Fixture live;live.change_on_free=true;
    require(live.geometry()==1&&live.progress.stores==92&&live.progress.filename==0x800260b0,"post-free selector reread");
}
void refusal_prefixes(){
    for(bool special:{false,true})for(bool geometry:{false,true}){
        Fixture baseline;baseline.put(0x800dcf10,special?1:0);require((geometry?baseline.geometry():baseline.texture())==1,"prefix baseline");
        for(std::size_t cap=0;cap<baseline.progress.events;++cap){Fixture f;f.put(0x800dcf10,special?1:0);
            require((geometry?f.geometry(cap):f.texture(cap))==NBA97_TEXT_LIMIT&&!f.progress.completed,"event budget refusal");
            require(f.progress.events==cap,"exact event cutoff");
            for(std::size_t i=0;i<cap;++i){const auto& a=f.events[i];const auto& b=baseline.events[i];
                require(a.pc==b.pc&&a.kind==b.kind&&a.address==b.address&&a.value==b.value&&a.completed==b.completed,"preserved complete prefix");}
        }
    }
    Fixture missing;missing.context.io=nullptr;
    require(missing.texture()==NBA97_TEXT_IO_REFUSED&&missing.progress.stores==2,"missing real service retains sentinels");
    require(missing.geometry()==NBA97_TEXT_IO_REFUSED&&missing.progress.events==1,"missing sync never frees");
    Fixture inconsistent;inconsistent.null_load=true;
    require(inconsistent.texture()==NBA97_TEXT_IO_REFUSED&&!inconsistent.events[2].completed,"29BFC cannot successfully returnNULL");
    Fixture unknown;*unknown.at(0x800dcf10).second=0;
    require(unknown.texture()==NBA97_TEXT_UNKNOWN&&unknown.progress.stores==0,"initial special read precedes sentinels");
    require(unknown.geometry()==NBA97_TEXT_UNKNOWN&&unknown.progress.callbacks_completed==2,"sync/free precede unknown special read");
    Fixture invalid;*invalid.at(0x800dcf10).second=0;*invalid.at(0x800dcf10+3).second=2;
    require(invalid.texture()==NBA97_TEXT_ARGUMENT,"validate entire reached knownness before unknown");
    Fixture memory;memory.put(0x80021d74,32);
    require(memory.texture()==NBA97_TEXT_RESOURCE&&memory.progress.stores==2,"no repaired out-of-table index");
    Fixture budget;require(budget.texture(100,1)==NBA97_TEXT_LIMIT&&budget.progress.stores==0,"access cutoff before first store");
}
}
int main(){try{selection();special_packets();refusal_prefixes();std::cout<<checks<<" court startup checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
