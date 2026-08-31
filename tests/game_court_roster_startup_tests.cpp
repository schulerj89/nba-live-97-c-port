#include "recovered/game_court_roster_startup.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
unsigned checks=0;
void require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
constexpr std::uint32_t Roster=0x80110000,Table=0x800fc664,Tags=0x80102c8c,Pad=0x1f800000;
struct Fixture {
    std::array<std::vector<std::uint8_t>,7> bytes,known;
    std::array<Nba97GameTextRegion,7> regions{};
    std::vector<Nba97GameCourtRosterEvent> journal=std::vector<Nba97GameCourtRosterEvent>(2000);
    Nba97GameCourtRosterProgress progress{};Nba97GameCourtRosterContext context{};
    Fixture(){
        const std::uint32_t bases[]={Roster,Table,Tags,Pad,0x800dcf10,0x800b7254,0x800b840a};
        const std::size_t sizes[]={24*128,96,24*32,1024,4,72,0xc2};
        for(unsigned i=0;i<7;++i){bytes[i].assign(sizes[i],0xcd);known[i].assign(sizes[i],0);regions[i]={bases[i],bytes[i].data(),known[i].data(),sizes[i]};}
        context={{regions.data(),regions.size()},200000};put(Pad+20,0);
        pair(0x800b7254,"First","One");pair(0x800b726c,"Second","Two");pair(0x800b7284,"Third","Three");
        for(unsigned i=0;i<24;++i){put(Table+i*4,Roster+i*128);put(Roster+i*128,0,2);pair(Roster+i*128+41,"Other","Name");
            for(unsigned field:{18u,19u,21u,22u,23u,26u,28u,32u})put(Roster+i*128+field,200,1);}
    }
    std::pair<std::uint8_t*,std::uint8_t*> at(std::uint32_t a){for(auto& r:regions)if(a>=r.base&&a-r.base<r.size)return {r.data+a-r.base,r.known+a-r.base};throw std::runtime_error("fixture address");}
    void put(std::uint32_t a,std::uint32_t v,unsigned width=4){for(unsigned i=0;i<width;++i){auto p=at(a+i);*p.first=std::uint8_t(v>>(i*8));*p.second=1;}}
    std::uint32_t get(std::uint32_t a,unsigned width=4){std::uint32_t v=0;for(unsigned i=0;i<width;++i){auto p=at(a+i);require(*p.second==1,"fixture read is known");v|=std::uint32_t(*p.first)<<(i*8);}return v;}
    void pair(std::uint32_t a,const char* first,const char* second){for(const char* s:{first,second}){do{put(a++,std::uint8_t(*s),1);}while(*s++);}}
    int run(std::size_t capacity=2000){return nba97_game_court_roster_startup(&context,journal.data(),capacity,&progress);}
    int match(std::uint32_t i){return nba97_game_court_roster_match(&context,i,journal.data(),journal.size(),&progress);}
};
void matching(){
    Fixture f;
    require(f.match(0)==1&&f.progress.match_result==0,"unmatched ordinary ID");
    f.pair(Roster+41,"First","One");require(f.match(0)==1&&f.progress.match_result==1,"first paired name");
    f.pair(Roster+41,"Second","Two");require(f.match(0)==1&&f.progress.match_result==2,"second paired name");
    f.pair(Roster+41,"Third","Three");f.put(Roster,0x1c0,2);
    require(f.match(0)==1&&f.progress.match_result==0,"third paired name bypasses special ID fallback");
    f.pair(Roster+41,"Other","Name");require(f.match(0)==1&&f.progress.match_result==3,"exact fallback ID");
    f.put(Roster,0xffc0,2);require(f.match(0)==1&&f.progress.match_result==0,"signed ID does not match low byte");
    f.pair(Roster+41,"First","Wrong");require(f.match(0)==1&&f.progress.match_result==0,"both names must match");
    f.pair(Roster+41,"first","One");require(f.match(0)==1&&f.progress.match_result==0,"case-sensitive BIOS equality");
    f.pair(Roster+41,"First","One");require(f.match(0x40000000)==1&&f.progress.match_result==1,"source index shift wraps");
}
void prefix(){
    Fixture plain;require(plain.run()==1&&plain.progress.completed&&!plain.progress.special_roster_seen,"all ordinary prefix");
    require(plain.progress.stores==30&&plain.progress.matches_completed==24,"exact plain store/match footprint");
    require(plain.get(0x800dcf10)==0&&plain.get(Pad+12)==0,"only source clear values established");
    for(unsigned i=0;i<4;++i)require(plain.get(Pad+48+i*4)==Pad+48+i*4,"actual scratch self pointers");
    require(*plain.at(Pad+4).second==0&&*plain.at(Pad+40).second==0,"unused scratch words remain unknown");
    require(*plain.at(0x800b840a).second==0&&plain.get(Roster+28,1)==200,"no clamps or global changes without selection");
    Fixture selected;selected.pair(Roster+41,"First","One");selected.put(Pad+20,0x12345);
    require(selected.run()==1&&selected.progress.special_roster_seen&&selected.progress.matches_completed==25,"selected slot invokes matcher twice");
    require(selected.get(Pad+4)==0x12344&&selected.get(Pad+20)==0x12345,"source +14-derived write targets+4");
    require(selected.get(Pad+12)==1&&selected.get(Pad+40)==Roster&&selected.get(Tags,1)==1,"selected mask cached pointer and second result");
    require(selected.get(Roster+28,1)==99&&selected.get(Roster+18,1)==255&&selected.get(Roster+32,1)==23,"selected final overrides");
    require(selected.get(Roster+128+28,1)==96&&selected.get(Roster+128+18,1)==99&&selected.get(Roster+128+32,1)==23,"all other rosters clamped");
    require(selected.get(0x800b840a,2)==0x40e&&selected.get(0x800b846a,2)==0x2dd&&selected.get(0x800b84ca,2)==0x42c,"three exact final halves");
    for(unsigned i=0;i<24;++i)for(unsigned j=1;j<32;++j)require(*selected.at(Tags+i*32+j).second==0,"tag padding never initialized");
}
void prefixes(){
    Fixture baseline;baseline.pair(Roster+41,"First","One");require(baseline.run()==1,"prefix baseline");
    for(std::size_t cap=0;cap<baseline.progress.events;++cap){Fixture f;f.pair(Roster+41,"First","One");
        require(f.run(cap)==NBA97_TEXT_LIMIT&&!f.progress.completed&&f.progress.events==cap,"every journal cutoff retains prefix");
        for(std::size_t i=0;i<cap;++i){const auto& a=f.journal[i];const auto& b=baseline.journal[i];
            require(a.pc==b.pc&&a.address==b.address&&a.value==b.value&&a.kind==b.kind&&a.completed==b.completed,"exact earlier journal events");}
    }
    Fixture missing;*missing.at(Pad+20).second=0;require(missing.run()==NBA97_TEXT_UNKNOWN&&missing.progress.stores==6,"unknown incoming scratch retains initial six stores");
    Fixture align;align.put(Table,Roster+1);align.pair(Roster+42,"Other","Name");
    require(align.match(0)==NBA97_TEXT_ALIGNMENT_TRAP&&align.progress.stopped_pc==0x8004796c,"source fallback LH trap");
    Fixture invalid;*invalid.at(Table).second=0;*invalid.at(Table+3).second=2;
    require(invalid.match(0)==NBA97_TEXT_ARGUMENT,"validate full knownness span before unknown");
    Fixture budget;budget.context.access_budget=1;require(budget.run()==NBA97_TEXT_LIMIT&&budget.progress.stores==1,"access budget preserves DCF10 clear");
    Fixture bios;bios.pair(Roster+41,"First","One");*bios.at(Roster+47).second=0;
    require(bios.match(0)==NBA97_TEXT_UNKNOWN&&bios.progress.stopped_pc==0xa0&&bios.progress.events==2&&!bios.journal[1].completed,"BIOS semantic comparison refuses missing second name");
}
}
int main(){try{matching();prefix();prefixes();std::cout<<checks<<" court roster startup checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
