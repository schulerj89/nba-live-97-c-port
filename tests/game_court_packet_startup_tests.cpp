#include "recovered/game_court_packet_startup.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
namespace {
unsigned checks;
void check(bool value){++checks;if(!value){std::fprintf(stderr,"court packet startup check %u failed\n",checks);std::abort();}}
constexpr std::uint32_t Ram=0x80000000,Pad=0x1f800000,Contexts=0x80120000;
constexpr std::uint32_t Empty=0x80140000,Body=0x80140100,Bc4=0x80140200;
constexpr std::uint32_t BodySource=0x80150000,BodyDestination=0x80151000;
constexpr std::uint32_t Bc4Source=0x80152000,Bc4Destination=0x80153000;
struct Fixture {
    std::vector<std::uint8_t> ram=std::vector<std::uint8_t>(0x200000,0xcd),ram_known=std::vector<std::uint8_t>(0x200000,1);
    std::array<std::uint8_t,1024> pad{},pad_known{};std::array<Nba97GameTextRegion,2> regions{};
    std::vector<Nba97CourtPacketStartupEvent> journal=std::vector<Nba97CourtPacketStartupEvent>(512),calls;
    Nba97CourtPacketStartupContext context{};Nba97CourtPacketStartupProgress progress{};
    unsigned refuse_call=0,page_calls=0;bool unknown_page=false,shrink_body=false,redirect_bc4=false,sync_select=false;
    Fixture(){pad_known.fill(1);regions={Nba97GameTextRegion{Ram,ram.data(),ram_known.data(),ram.size()},Nba97GameTextRegion{Pad,pad.data(),pad_known.data(),pad.size()}};
        context={{regions.data(),regions.size()},10000,io,this};put(0x800f0ed8,Contexts);put(Empty,0);put(Empty+8,BodySource);put(Empty+12,BodyDestination);
        for(unsigned player=0;player<10;++player){const auto c=Contexts+player*0xbccu;for(unsigned group=0;group<20;++group)put(c+0xb0u+group*0x94u,Empty);put(c+0xbc4u,Empty);}
        put(Body,2);put(Body+8,BodySource);put(Body+12,BodyDestination);put(Contexts+0xb0,Body);
        put(Bc4,1);put(Bc4+0x28,Bc4Source);put(Bc4+0x2c,Bc4Destination);put(Contexts+0xbc4,Bc4);put(Pad+12,1);
    }
    std::pair<std::uint8_t*,std::uint8_t*> at(std::uint32_t address){if(address>=Ram&&address-Ram<ram.size())return{ram.data()+address-Ram,ram_known.data()+address-Ram};if(address>=Pad&&address-Pad<pad.size())return{pad.data()+address-Pad,pad_known.data()+address-Pad};std::abort();}
    void put(std::uint32_t address,std::uint32_t value,unsigned width=4){for(unsigned i=0;i<width;++i){auto p=at(address+i);*p.first=static_cast<std::uint8_t>(value>>(i*8));*p.second=1;}}
    std::uint32_t get(std::uint32_t address,unsigned width=4)const{std::uint32_t value=0;for(unsigned i=0;i<width;++i){if(address>=Ram)value|=std::uint32_t(ram[address-Ram+i])<<(i*8);else value|=std::uint32_t(pad[address-Pad+i])<<(i*8);}return value;}
    static int io(void* user,const Nba97GameTextMemory*,const Nba97CourtPacketStartupEvent* event,Nba97CourtPacketStartupValue* returned){auto& f=*static_cast<Fixture*>(user);f.calls.push_back(*event);if(f.refuse_call==f.calls.size())return 0;
        if(event->entry==0x800994f4){if(f.sync_select)f.put(Pad+12,1);return 1;}if(event->entry!=0x8009bf98)return 0;
        ++f.page_calls;if(f.shrink_body&&f.page_calls==1)f.put(Body,1);
        if(f.redirect_bc4&&f.page_calls==3){const auto alternate=Contexts+0xbccu;f.put(0x800f0ed4,alternate);f.put(alternate+0xbc4u,Empty);}
        if(f.unknown_page){*returned={0,0};return 1;}*returned={0x1200u+f.page_calls,1};return 1;}
    int run(std::size_t capacity=512){return nba97_game_court_packet_startup(&context,journal.data(),capacity,&progress);}
};
void unselected_and_sync(){Fixture f;f.put(Pad+12,0);check(f.run()==NBA97_TEXT_COMPLETE&&f.progress.completed);check(f.calls.size()==1&&f.calls[0].entry==0x800994f4&&f.calls[0].argument_count==1&&f.calls[0].argument[0]==0);check(f.progress.players_selected==0&&f.progress.stores==0&&f.progress.events==1);check(f.progress.stopped_pc==0x80048744);
    Fixture live;live.put(Pad+12,0);live.sync_select=true;check(live.run()==1&&live.progress.players_selected==1);check(live.progress.services_completed==4&&live.page_calls==3);
}
void complete_patch(){Fixture f;check(f.run()==1&&f.progress.completed);check(f.progress.players_selected==1&&f.progress.body_groups_scanned==20);check(f.progress.body_packets_patched==2&&f.progress.bc4_packets_patched==1);check(f.progress.stores==46&&f.progress.events==50&&f.progress.services_completed==4);
    check(f.calls[1].pc==0x8004855c&&f.calls[1].entry==0x8009bf98&&f.calls[1].argument_count==4);check(f.calls[1].argument[0]==2&&f.calls[1].argument[1]==0&&f.calls[1].argument[2]==0x200&&f.calls[1].argument[3]==0x100);check(f.calls[3].pc==0x80048640);
    check(f.get(0x800f0ed4)==Contexts&&f.get(0x8010b270)==2&&f.get(0x800fda04)==0x20&&f.get(0x800fda08)==0x20);
    check(f.get(BodySource+0x16,2)==0x1201&&f.get(BodyDestination+0x16,2)==0x1201);check(f.get(BodySource+0x20+0x16,2)==0x1202&&f.get(BodyDestination+0x20+0x16,2)==0x1202);
    check(f.get(BodySource+0x0c,1)==0&&f.get(BodySource+0x0d,1)==0x5f&&f.get(BodySource+0x14,1)==0x10&&f.get(BodySource+0x15,1)==0x5f);check(f.get(BodySource+0x1c,1)==0&&f.get(BodySource+0x1d,1)==0x6f);check(f.get(BodyDestination+0x0d,1)==0x5f&&f.get(BodyDestination+0x1d,1)==0x6f);
    check(f.get(Bc4Source+0x16,2)==0x1203&&f.get(Bc4Destination+0x16,2)==0x1203);check(f.get(Bc4Source+0x0d,1)==0x5f&&f.get(Bc4Source+0x1d,1)==0x6e);check(f.journal[2].kind==NBA97_COURT_PACKET_STARTUP_PAGE&&f.journal[3].address==BodyDestination+0x16&&f.journal[3].pc==0x80048564);
}
void live_reloads(){Fixture f;f.shrink_body=true;check(f.run()==1);check(f.progress.body_packets_patched==1&&f.progress.bc4_packets_patched==1&&f.page_calls==2);check(f.get(BodySource+0x20+0x16,2)==0xcdcd);
    Fixture bc;bc.put(Bc4,2);bc.redirect_bc4=true;check(bc.run()==1);check(bc.progress.body_packets_patched==2&&bc.progress.bc4_packets_patched==1&&bc.page_calls==3);check(bc.get(Bc4Source+0x20+0x16,2)==0xcdcd);
    Fixture negative;negative.put(Body,0xffffffff);negative.put(Bc4,0x80000000);check(negative.run()==1&&negative.page_calls==0);check(negative.progress.body_packets_patched==0&&negative.progress.bc4_packets_patched==0&&negative.progress.stores==4);
}
void refusals_and_bounds(){Fixture sync;sync.refuse_call=1;check(sync.run()==NBA97_TEXT_IO_REFUSED&&sync.progress.events==1&&sync.progress.stores==0);check(!sync.journal[0].completed&&sync.progress.stopped_pc==0x800484b8);
    Fixture page;page.refuse_call=3;check(page.run()==NBA97_TEXT_IO_REFUSED);check(page.progress.body_packets_patched==1&&page.get(BodySource+0x16,2)==0x1201);check(page.progress.stopped_pc==0x8004855c&&!page.journal[page.progress.events-1].completed);
    Fixture missing;missing.context.io=nullptr;check(missing.run()==NBA97_TEXT_IO_REFUSED&&missing.progress.stopped_pc==0x800484b8);
    Fixture unknown;unknown.unknown_page=true;check(unknown.run()==NBA97_TEXT_UNKNOWN&&unknown.progress.stopped_pc==0x8004855c);check(unknown.get(0x800f0ed4)==Contexts&&unknown.progress.stores==1);
    Fixture access;access.context.access_budget=0;check(access.run()==NBA97_TEXT_LIMIT&&access.progress.stopped_pc==0x80055f0c);
    Fixture journal;check(journal.run(0)==NBA97_TEXT_LIMIT&&journal.progress.events==0&&journal.progress.stopped_pc==0x800484b8);
    Fixture unmapped;unmapped.regions[0].size=0xf0ed8;check(unmapped.run()==NBA97_TEXT_RESOURCE);
    Fixture unknown_mask;unknown_mask.pad_known[12]=0;check(unknown_mask.run()==NBA97_TEXT_UNKNOWN&&unknown_mask.progress.stopped_address==Pad+12);
}
void metadata(){Fixture f;f.regions[1].base=0x80100000;check(f.run()==NBA97_TEXT_ARGUMENT);Fixture no_journal;check(nba97_game_court_packet_startup(&no_journal.context,nullptr,1,&no_journal.progress)==NBA97_TEXT_ARGUMENT);check(nba97_game_court_packet_startup(nullptr,nullptr,0,nullptr)==NBA97_TEXT_ARGUMENT);}
}
int main(){unselected_and_sync();complete_patch();live_reloads();refusals_and_bounds();metadata();std::printf("game_court_packet_startup: %u checks passed\n",checks);return 0;}
