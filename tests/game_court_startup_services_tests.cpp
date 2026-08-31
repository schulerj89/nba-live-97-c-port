#include "recovered/game_court_startup_services.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
unsigned checks=0;
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
constexpr std::uint32_t Base=0x80110000,Low=Base,Descriptor=Base+40,High=Base+80,
    Free=Base+120,Payload=Base+256,Lock=Base+384,Heap=0x80103d50;
struct Fixture {
    std::array<std::vector<std::uint8_t>,8> bytes,known;
    std::array<Nba97GameTextRegion,8> regions{};
    std::array<Nba97GameCourtStartupEvent,4> journal{};
    std::array<Nba97GameHeapReleaseStore,8> releases{};
    Nba97GameCourtStartupProgress progress{};
    Nba97GameCourtStartupServices services{};
    Nba97GameCourtStartupContext context{};
    unsigned syncs=0,loads=0;bool refuse_sync=false;
    Fixture(){
        const std::uint32_t bases[]={Base,Heap,0x800eb688,0x801029c0,0x800dcf10,0x8001ec94,0x80021d74,0x800b763c};
        const std::size_t sizes[]={512,384,4,4,4,4,4,128};
        for(unsigned i=0;i<8;++i){bytes[i].assign(sizes[i],0xcd);known[i].assign(sizes[i],0);regions[i]={bases[i],bytes[i].data(),known[i].data(),sizes[i]};}
        for(unsigned bank=0;bank<16;++bank)put(Heap+24*bank+4,0);
        put(Heap,Low);put(Heap+4,High);put(Low+32,Descriptor);put(Descriptor,Payload);put(Descriptor+24,5);
        put(Descriptor+32,High);put(Descriptor+36,Low);put(High,Base+512);put(High+24,0x8005);put(High+36,Descriptor);
        put(0x800eb688,Free);put(Free+32,0);put(0x801029c0,Lock);put(Lock,0);
        put(0x800dcf10,0);put(0x8001ec94,0);put(0x80021d74,0);put(0x800b763c,0x80123400);
        services.load_or_sync=external;services.user=this;services.release_access_budget=1000;
        services.release_journal=releases.data();services.release_capacity=releases.size();
        context={{regions.data(),regions.size()},nba97_game_court_startup_service_io,&services};
    }
    std::pair<std::uint8_t*,std::uint8_t*> at(std::uint32_t a){for(auto& r:regions)if(a>=r.base&&a-r.base<r.size)return {r.data+a-r.base,r.known+a-r.base};throw std::runtime_error("fixture address");}
    void put(std::uint32_t a,std::uint32_t v){for(unsigned i=0;i<4;++i){auto p=at(a+i);*p.first=std::uint8_t(v>>(i*8));*p.second=1;}}
    std::uint32_t get(std::uint32_t a){std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t(*at(a+i).first)<<(i*8);return v;}
    // Only loading and synchronization are fixture services. Release executes
    // the complete existing native90698 owner against this actual fixture heap.
    static int external(void* user,const Nba97GameTextMemory*,const Nba97GameCourtStartupEvent* e,std::uint32_t* returned){
        auto& f=*static_cast<Fixture*>(user);
        if(e->kind==NBA97_COURT_STARTUP_SYNC_994F4){++f.syncs;require(f.get(Descriptor)==Payload,"sync precedes heap release");return !f.refuse_sync;}
        require(e->kind==NBA97_COURT_STARTUP_LOAD_29BFC,"free cannot reach external fixture");++f.loads;
        require(f.get(Descriptor)==0&&f.get(0x800eb688)==Descriptor,"real descriptor/free-list effects precede geometry load");
        *returned=0x80150000;return 1;
    }
    int run(){return nba97_game_court_startup_select_geometry(&context,Payload,1000,journal.data(),journal.size(),&progress);}
};
void composed(){
    Fixture f;require(f.run()==1&&f.progress.completed,"complete bridge with native heap release");
    require(f.services.release_status==1&&f.services.release.completed&&f.services.release.stores==7,"all original release stores");
    require(f.syncs==1&&f.loads==1&&f.progress.callbacks_completed==3,"source service order");
    require(f.get(Low+32)==High&&f.get(High+36)==Low&&f.get(Descriptor+32)==Free&&f.get(Lock)==0,"live unlink/lock effects");
    for(unsigned i=0;i<128;++i){auto p=f.at(Payload+i);require(*p.first==0xcd&&*p.second==0,"freed payload untouched and unknown");}
    Fixture bad;bad.put(0x801029c0,0);require(bad.run()==NBA97_TEXT_IO_REFUSED&&!bad.progress.completed,"native release refusal propagates");
    require(bad.services.release_status==NBA97_TEXT_RESOURCE&&bad.services.release.stopped_pc==0x800a4064,"actual missing-lock source refusal");
    require(bad.syncs==1&&bad.loads==0&&bad.get(Descriptor)==Payload,"no loader success after failed release");
    Fixture partial;partial.services.release_capacity=4;
    require(partial.run()==NBA97_TEXT_IO_REFUSED&&partial.services.release.stores==4,"partial heap prefix retained");
    require(partial.get(Descriptor)==0&&partial.get(Lock)==1&&partial.loads==0,"no rollback/unlock after release cutoff");
    Fixture sync;sync.refuse_sync=true;require(sync.run()==NBA97_TEXT_IO_REFUSED&&sync.get(Descriptor)==Payload&&sync.services.release.stores==0,"sync refusal prevents free");
    Fixture missing;missing.services.load_or_sync=nullptr;require(missing.run()==NBA97_TEXT_IO_REFUSED&&missing.services.release.stores==0,"no successful default sync");
}
}
int main(){try{composed();std::cout<<checks<<" court startup service checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
