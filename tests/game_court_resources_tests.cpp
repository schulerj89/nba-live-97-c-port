#include "recovered/game_court_resources.h"
#include "recovered/game_heap_allocate.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
unsigned checks=0;
void require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
constexpr std::uint32_t Resource=0x80130000,Records=0x80140000,Arena=0x80150000,Heap=0x80103d50,Free=0x800eb688,Serial=0x800c4a8c;
struct Fixture {
    std::array<std::vector<std::uint8_t>,11> bytes,known;
    std::array<Nba97GameTextRegion,11> regions{};
    std::vector<Nba97GameTextPoolEvent> events=std::vector<Nba97GameTextPoolEvent>(2000);
    std::vector<Nba97GameHeapEvent> heap_events=std::vector<Nba97GameHeapEvent>(1000);
    Nba97GameCourtResourceProgress progress{};Nba97GameHeapProgress heap_progress{};
    Nba97GameTextPoolContext context{};unsigned allocations=0;bool refuse=false,change_count=false;
    Fixture(){
        const std::uint32_t base[]={0x800dcf10,0x800fc900,0x801029c0,0x8010b60c,Resource,0x800260c0,Heap,Free,Serial,Records,Arena};
        const std::size_t size[]={16,0x3000,0x400,16,0x1000,64,384,16,16,4096,65536};
        for(unsigned i=0;i<11;++i){bytes[i].resize(size[i],0xa5);known[i].resize(size[i],i==4?1:0);regions[i]={base[i],bytes[i].data(),known[i].data(),size[i]};}
        context={{regions.data(),regions.size()},allocate,this};
        put(0x800dcf10,0);put(0x801029c0,0);put(Resource,1);put(Resource+4,1);
        put(Resource+20+4,2);put(Resource+36+4,3);
        for(unsigned bank=0;bank<2;++bank)for(unsigned i=0;i<3;++i){const auto packet=Resource+212+bank*72+i*24;put(packet+4,0x28030201u+bank*0x101010u+i);}
        put(Heap,Records);put(Heap+4,Records+64);put(Heap+8,3);put(Heap+12,15);put(Heap+20,0);
        put(Free,Records+128);put(Serial,7);put(Records,Arena);put(Records+16,0);put(Records+32,Records+64);put(Records+36,0);
        put(Records+64,Arena+65536);put(Records+80,0);put(Records+96,0);put(Records+100,Records);
        for(unsigned i=0;i<12;++i)put(Records+128+i*40+32,i==11?0:Records+128+(i+1)*40);
        for(auto name:{0x800260c0u,0x800260d0u}){const char text[]="fixture";for(unsigned i=0;i<sizeof text;++i)byte(name+i,std::uint8_t(text[i]));}
    }
    std::pair<std::uint8_t*,std::uint8_t*> at(std::uint32_t p){for(auto& r:regions)if(p>=r.base&&p-r.base<r.size)return {r.data+p-r.base,r.known+p-r.base};throw std::runtime_error("fixture address");}
    void byte(std::uint32_t p,std::uint8_t v){auto a=at(p);*a.first=v;*a.second=1;}
    void put(std::uint32_t p,std::uint32_t v){for(unsigned i=0;i<4;++i)byte(p+i,std::uint8_t(v>>(i*8)));}
    std::uint32_t get(std::uint32_t p){std::uint32_t v=0;for(unsigned i=0;i<4;++i){auto a=at(p+i);require(*a.second==1,"fixture value known");v|=std::uint32_t(*a.first)<<(i*8);}return v;}
    static int bios(void* user,const Nba97GameTextMemory*,const Nba97GameHeapEvent* e,Nba97GameHeapValue* result){
        auto& f=*static_cast<Fixture*>(user);
        if(e->kind!=NBA97_HEAP_BIOS_A0_1A){*result={0,1};return 1;}
        // Explicit bounded strncpy fixture; BIOS/timing provenance is not claimed.
        bool ended=false;for(unsigned i=0;i<12;++i){auto value=ended?std::uint8_t(0):*f.at(e->argument[1]+i).first;if(!value)ended=true;f.byte(e->argument[0]+i,value);}return 1;
    }
    static int allocate(void* user,const Nba97GameTextMemory* memory,const Nba97GameTextPoolEvent* event,Nba97GameTextPoolValue* result){
        auto& f=*static_cast<Fixture*>(user);++f.allocations;
        require(event->argument[2]==0&&event->argument[3]==1,"court allocation flags0 mode1 preserved");
        if(f.refuse)return 0;
        Nba97GameHeapContext heap{*memory,10000,bios,&f};
        Nba97GameHeapArguments args{event->argument[0],event->argument[1],event->argument[2],event->argument[3]};
        const int rc=nba97_game_heap_allocate(&heap,&args,f.heap_events.data(),f.heap_events.size(),&f.heap_progress);
        if(rc!=1)return 0;
        *result={f.heap_progress.descriptor.word,f.heap_progress.descriptor.known};
        if(f.change_count&&f.allocations==1)f.put(Resource+4,0);
        return 1;
    }
    int run(std::size_t capacity=2000,std::size_t budget=10000){return nba97_game_court_resources(&context,Resource,budget,events.data(),capacity,&progress);}
};
void composed(){
    Fixture f;require(f.run()==1&&f.progress.completed,"real heap and relocation complete");
    require(f.progress.allocations_completed==4&&f.progress.callbacks_completed==4,"both lists and both line banks allocated");
    require(f.get(Resource+12)==Resource+20&&f.get(Resource+16)==Resource+36&&f.get(Resource+8)==Resource+356,"resource cursor layout");
    require(f.get(Resource+28)==Resource+52&&f.get(Resource+32)==Resource+132,"textured banks40-byte stride");
    require(f.get(Resource+44)==Resource+212&&f.get(Resource+48)==Resource+284,"flat banks24-byte stride");
    require(f.get(0x8010b60c)==3&&f.get(0x800fc964)==Resource+52,"final global values");
    const auto first=f.get(0x800feda0),second=f.get(0x800feda4);
    require(first==Arena&&second==Arena+4,"actual forward heap allocation, no fabricated addresses");
    for(unsigned bank=0;bank<2;++bank){const auto line=f.get(bank?second:first);require(line==Arena+8+bank*48,"independent line bank allocation");
        for(unsigned i=0;i<3;++i){require(*f.at(line+i*16+3).first==3&&*f.at(line+i*16+3).second==1,"line tag length established");
            require(f.get(line+i*16+4)==0x40030201u+bank*0x101010u+i,"bank-specific original RGB and command");
            for(unsigned j:{0u,1u,2u,8u,9u,10u,11u,12u,13u,14u,15u})require(*f.at(line+i*16+j).second==0,"line links and coordinates remain unknown");
        }
    }
    require(*f.at(Arena+104).second==0,"unallocated tail not cleared");
}
void prefixes(){
    Fixture reference;require(reference.run()==1,"prefix baseline");
    for(std::size_t cap=0;cap<reference.progress.events;++cap){Fixture f;require(f.run(cap)==NBA97_TEXT_LIMIT,"journal cutoff");require(f.progress.events==cap&&!f.progress.completed,"exact bounded prefix");
        for(std::size_t i=0;i<cap;++i){const auto& a=f.events[i];const auto& b=reference.events[i];require(a.pc==b.pc&&a.address==b.address&&a.value==b.value&&a.kind==b.kind,"ordered store/callback prefix");}}
    Fixture refusal;refusal.refuse=true;require(refusal.run()==NBA97_TEXT_IO_REFUSED&&refusal.progress.stopped_pc==0x80090234,"real allocation refuses, no fallback");
    require(refusal.get(Resource+8)==Resource+356&&refusal.progress.allocations_completed==0,"prior relocations remain");
    Fixture live;live.change_count=true;require(live.run()==1&&live.progress.allocations_completed==2,"post-allocation count reread controls later allocations");
    Fixture bad;*bad.at(Resource).second=0;*bad.at(Resource+3).second=2;require(bad.run()==NBA97_TEXT_ARGUMENT&&bad.progress.stores==1,"whole reached span validates metadata before unknown refusal");
    Fixture unknown;*unknown.at(Resource).second=0;require(unknown.run()==NBA97_TEXT_UNKNOWN&&unknown.progress.stopped_pc==0x80048a64,"unknown count, prior global retained");
    Fixture address;require(nba97_game_court_resources(&address.context,Resource+1,1000,address.events.data(),address.events.size(),&address.progress)==NBA97_TEXT_ALIGNMENT_TRAP&&address.progress.stores==1,"original source alignment");
}
}
int main(){try{composed();prefixes();std::cout<<checks<<" court resource checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
