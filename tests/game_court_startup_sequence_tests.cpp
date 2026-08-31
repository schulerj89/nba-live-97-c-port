#include "recovered/game_court_startup_sequence.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check(bool value){++checks;if(!value){std::fprintf(stderr,"court startup sequence check %u failed\n",checks);std::abort();}}
constexpr std::uint32_t Ram=0x80000000u,Pad=0x1f800000u;
constexpr std::uint32_t Rosters=0x80110000u,Texture=0x80130000u;
constexpr std::uint32_t Geometry=0x80140000u,Descriptor=0x80170000u,Payload=0x80171000u;

struct Fixture {
    std::vector<std::uint8_t> ram=std::vector<std::uint8_t>(0x200000,0);
    std::vector<std::uint8_t> known=std::vector<std::uint8_t>(0x200000,1);
    std::array<std::uint8_t,1024> pad{},pad_known{};
    std::array<Nba97GameTextRegion,2> region{};
    Nba97GameImageMemory image{};
    Nba97GameCourtStartupAllocation allocation{};
    Nba97GameCourtTextureState texture_state{};
    Nba97GameCourtStartupSequenceContext context{};
    Nba97GameCourtStartupSequenceBudgets budget{};
    Nba97GameCourtStartupSequenceJournals journals{};
    Nba97GameCourtStartupSequenceProgress progress{};
    std::vector<Nba97GameCourtRosterEvent> roster=std::vector<Nba97GameCourtRosterEvent>(2000);
    std::vector<Nba97CourtInteractiveEvent> interactive=std::vector<Nba97CourtInteractiveEvent>(2000);
    std::vector<Nba97CourtPacketStartupEvent> packet=std::vector<Nba97CourtPacketStartupEvent>(512);
    std::vector<Nba97GameCourtStartupEvent> texture_select=std::vector<Nba97GameCourtStartupEvent>(100);
    std::vector<Nba97GameCourtStartupEvent> geometry_select=std::vector<Nba97GameCourtStartupEvent>(200);
    std::vector<Nba97GameTextPoolEvent> resources=std::vector<Nba97GameTextPoolEvent>(2000);
    std::vector<unsigned> service;
    unsigned loads=0;
    Fixture(){
        pad_known.fill(1);
        region={Nba97GameTextRegion{Ram,ram.data(),known.data(),ram.size()},
            Nba97GameTextRegion{Pad,pad.data(),pad_known.data(),pad.size()}};
        image={ram.data()+(Texture-Ram),known.data()+(Texture-Ram),0x10000,0,1};
        allocation={Texture,{&image,0}};
        context.memory={region.data(),region.size()};
        context.allocation=&allocation;context.allocation_count=1;
        context.interactive_io=interactiveIo;context.interactive_user=this;
        context.packet_io=packetIo;context.packet_user=this;
        context.startup_io=startupIo;context.startup_user=this;
        context.image_io=imageIo;context.image_user=this;
        context.pool_io=poolIo;context.pool_user=this;
        context.texture_state=&texture_state;
        budget={200000,100000,10000,1000,100,100,1000,10000};
        journals={roster.data(),roster.size(),interactive.data(),interactive.size(),
            packet.data(),packet.size(),texture_select.data(),texture_select.size(),
            geometry_select.data(),geometry_select.size(),resources.data(),resources.size()};
        put(0x1f800014u,0);put(0x1f800018u,0);put(0x8001ec94u,0);put(0x80021d74u,0);
        put(0x801029c0u,0);put(Descriptor,Payload);
        put(Texture+8,0); /* Exact487B8 signed count: empty container. */
        put(Geometry,0);put(Geometry+4,0); /* Exact48A4C zero-group resource. */
        text(0x800b7254u,"First\0One");text(0x800b726cu,"Second\0Two");text(0x800b7284u,"Third\0Three");
        for(unsigned i=0;i<24;++i){const auto r=Rosters+i*128u;put(0x800fc664u+i*4u,r);put(r,0,2);text(r+41,"Other\0Name");}
        for(unsigned i=0;i<32;++i){put(0x800b763cu+i*4u,0x80026404u);put(0x800b76c8u+i*4u,0x80026404u);}
    }
    std::pair<std::uint8_t*,std::uint8_t*> at(std::uint32_t address){
        if(address>=Ram&&address-Ram<ram.size())return {ram.data()+address-Ram,known.data()+address-Ram};
        if(address>=Pad&&address-Pad<pad.size())return {pad.data()+address-Pad,pad_known.data()+address-Pad};
        std::abort();
    }
    void put(std::uint32_t address,std::uint32_t value,unsigned width=4){for(unsigned i=0;i<width;++i){auto p=at(address+i);*p.first=std::uint8_t(value>>(8*i));*p.second=1;}}
    std::uint32_t get(std::uint32_t address,unsigned width=4){std::uint32_t value=0;for(unsigned i=0;i<width;++i)value|=std::uint32_t(*at(address+i).first)<<(8*i);return value;}
    void text(std::uint32_t address,const char* value){do{put(address++,std::uint8_t(*value),1);}while(*value++);}
    static int interactiveIo(void*,const Nba97GameTextMemory*,const Nba97CourtInteractiveEvent*,Nba97CourtInteractiveValue*){return 0;}
    static int packetIo(void* user,const Nba97GameTextMemory*,const Nba97CourtPacketStartupEvent* event,Nba97CourtPacketStartupValue*){
        auto& f=*static_cast<Fixture*>(user);f.service.push_back(event->entry);return event->entry==0x800994f4u;
    }
    static int startupIo(void* user,const Nba97GameTextMemory*,const Nba97GameCourtStartupEvent* event,std::uint32_t* returned){
        auto& f=*static_cast<Fixture*>(user);f.service.push_back(event->kind);
        if(event->kind==NBA97_COURT_STARTUP_LOAD_29BFC)*returned=++f.loads==1?Texture:Geometry;
        return 1;
    }
    static int imageIo(void*,const Nba97GameImageTransfer*){return 1;}
    static int poolIo(void* user,const Nba97GameTextMemory*,const Nba97GameTextPoolEvent* event,Nba97GameTextPoolValue* returned){
        auto& f=*static_cast<Fixture*>(user);f.service.push_back(event->kind);*returned={Descriptor,1};return 1;
    }
    int run(){return nba97_game_court_startup_sequence(&context,&budget,&journals,&progress);}
};

void complete_source_order(){
    Fixture f;check(f.run()==NBA97_TEXT_COMPLETE);check(f.progress.completed&&f.progress.stage==NBA97_COURT_SEQUENCE_COMPLETE);
    check(f.progress.source_intervals_completed==8&&f.progress.child_result==NBA97_TEXT_COMPLETE);
    check(f.progress.roster.completed&&f.progress.interactive.completed&&f.progress.packet.completed);
    check(f.progress.texture_select.completed&&f.progress.textures.completed&&f.progress.geometry_select.completed&&f.progress.resources.completed);
    check(f.progress.loaded_texture==Texture&&f.progress.loaded_geometry==Geometry);
    check(f.progress.texture_reference.memory==&f.image&&f.progress.texture_reference.offset==0);
    check(f.progress.textures.images_completed==0&&f.progress.resources.allocations_completed==2);
    check(f.service.size()==7);check(f.service[0]==0x800994f4u);
    check(f.service[1]==NBA97_COURT_STARTUP_LOAD_29BFC);
    check(f.service[2]==NBA97_COURT_STARTUP_SYNC_994F4&&f.service[3]==NBA97_COURT_STARTUP_FREE_90698&&f.service[4]==NBA97_COURT_STARTUP_LOAD_29BFC);
    check(f.service[5]==NBA97_TEXT_POOL_ALLOCATE_9027C&&f.service[6]==NBA97_TEXT_POOL_ALLOCATE_9027C);
    check(f.get(0x800febe4u)==Geometry&&f.get(Geometry+8)==Geometry+20u);
    check(!f.progress.natural_entry);
}

void exact_resolver(){
    Fixture f;Nba97GameImageReference result{reinterpret_cast<Nba97GameImageMemory*>(1),99};
    check(nba97_game_court_startup_resolve_image(&f.allocation,1,Texture,&result)==NBA97_IMAGE_COMPLETE);
    check(result.memory==&f.image&&result.offset==0);
    check(nba97_game_court_startup_resolve_image(&f.allocation,1,Texture+4,&result)==NBA97_IMAGE_RESOURCE);
    std::array<Nba97GameCourtStartupAllocation,2> duplicate{f.allocation,f.allocation};
    check(nba97_game_court_startup_resolve_image(duplicate.data(),2,Texture,&result)==NBA97_IMAGE_ARGUMENT);
    f.image.address_mod4_known=0;check(nba97_game_court_startup_resolve_image(&f.allocation,1,Texture,&result)==NBA97_IMAGE_UNKNOWN);
    f.image.address_mod4_known=1;f.image.address_mod4=1;check(nba97_game_court_startup_resolve_image(&f.allocation,1,Texture,&result)==NBA97_IMAGE_ARGUMENT);
    check(result.memory==&f.image&&result.offset==0); /* Failed lookup never overwrites output. */
}

void reached_validation_and_prefixes(){
    Fixture early;early.budget.roster_accesses=0;early.context.texture_state=nullptr;
    check(early.run()==NBA97_TEXT_LIMIT&&early.progress.stage==NBA97_COURT_SEQUENCE_ROSTER);
    check(early.progress.source_intervals_completed==0&&!early.progress.completed); /* No later-state preflight. */

    Fixture boundary;Fixture reference;check(reference.run()==1);boundary.journals.roster_capacity=reference.progress.roster.events;
    boundary.journals.interactive_capacity=0;
    check(boundary.run()==NBA97_TEXT_LIMIT&&boundary.progress.stage==NBA97_COURT_SEQUENCE_INTERACTIVE);
    check(boundary.progress.source_intervals_completed==1&&boundary.progress.roster.completed&&!boundary.progress.interactive.completed);
    check(boundary.get(0x800dcf10u)==0&&boundary.get(0x1f800030u)==0x1f800030u);

    Fixture missing;missing.context.allocation_count=0;
    check(missing.run()==NBA97_IMAGE_RESOURCE&&missing.progress.stage==NBA97_COURT_SEQUENCE_TEXTURE_RESOLVE);
    check(missing.progress.source_intervals_completed==4&&missing.progress.loaded_texture==Texture);
    check(!missing.progress.geometry_select.completed&&!missing.progress.resources.completed);

    Fixture allocator;allocator.context.pool_io=nullptr;
    check(allocator.run()==NBA97_TEXT_IO_REFUSED&&allocator.progress.stage==NBA97_COURT_SEQUENCE_RESOURCES);
    check(allocator.progress.source_intervals_completed==6&&allocator.progress.resources.stores>0);
    check(allocator.get(0x800febe4u)==Geometry); /* Reached resource prefix is retained. */

    Fixture args;check(nba97_game_court_startup_sequence(nullptr,&args.budget,&args.journals,&args.progress)==NBA97_TEXT_ARGUMENT);
}
}
int main(){complete_source_order();exact_resolver();reached_validation_and_prefixes();std::printf("game_court_startup_sequence: %u checks passed\n",checks);return 0;}
