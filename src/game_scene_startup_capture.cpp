#include "game_scene_startup_capture.h"
#include <array>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace nba97 {
namespace {
std::uint32_t word(const Nba97GameTextMemory& memory,std::uint32_t address,
    unsigned width=4,bool store=false,std::uint32_t value=0) {
    for(std::size_t i=0;i<memory.count;++i) {
        const auto& region=memory.region[i];
        if(address<region.base)continue;
        const auto offset=std::uint64_t(address)-region.base;
        if(offset>region.size || width>region.size-offset)continue;
        std::uint32_t result=0;
        for(unsigned j=0;j<width;++j) {
            if(store) {
                region.data[offset+j]=static_cast<std::uint8_t>(value>>(8*j));
                if(region.known)region.known[offset+j]=1;
            }
            result|=std::uint32_t(region.data[offset+j])<<(8*j);
        }
        return result;
    }
    throw std::runtime_error("scene-startup capture escaped retained memory");
}
struct Child {
    Nba97GameSceneStartupEvent event;
    Nba97GameSceneStartupRegisters registers;
};
int fixture(void* user,const Nba97GameTextMemory*,
    const Nba97GameSceneStartupEvent* event,Nba97GameSceneStartupRegisters* registers) {
    static_cast<std::vector<Child>*>(user)->push_back({*event,*registers});
    // Explicit CPU evidence fixture. No resource loader or GPU behavior is inferred.
    if(event->entry==0x8008f224u)
        registers->gpr[NBA97_MATCH_INITIALIZE_V0]={
            registers->gpr[NBA97_MATCH_INITIALIZE_A0].word%2?0u:0x3e1au,0xf};
    if(event->entry==0x80056944u)
        registers->gpr[NBA97_MATCH_INITIALIZE_V0]={0x80048d5cu,0xf};
    return 1;
}
}
int GameSceneStartupCapture::dispatch(const Nba97GameTextMemory* memory,
    const Nba97GameSceneLoadEvent* event,Nba97GameSceneLoadRegisters* registers) {
    if(!memory || !event || !registers || !receipt.empty())return 0;
    // Runtime-generated CPU records at reserved diagnostic addresses. These
    // replace the scene's unresolved roster/entity producers only in this test.
    for(unsigned i=0;i<12;++i) {
        word(*memory,0x80020b8cu+4*i,4,true,0x8011a000u+4*i);
        word(*memory,0x80020bbcu+4*i,4,true,0x8011a100u+4*i);
        word(*memory,0x8011a000u+4*i,2,true,static_cast<std::uint16_t>(-300+int(i)));
        word(*memory,0x8011a100u+4*i,2,true,200+i);
    }
    word(*memory,0x800fc650u,4,true,0x8011a200u);
    for(unsigned i=0;i<10;++i) {
        word(*memory,0x8011a200u+4*i,4,true,0x8011a400u+0x40*i);
        word(*memory,0x8011a420u+0x40*i,4,true,0x8011a800u+4*i);
        word(*memory,0x8011a800u+4*i,2,true,
            static_cast<std::uint16_t>(i%2?1000+int(i):-1000-int(i)));
    }
    word(*memory,0x800b729cu,4,true,0x8011abc0u);
    word(*memory,0x8001ede8u,4,true,7);
    word(*memory,0x800fa636u,2,true,0x55aau);
    std::vector<Child> calls;
    std::array<Nba97GameSceneStartupAccess,200> journal{};
    Nba97GameSceneStartupBinding binding{};
    binding.operation_budget=512;binding.io=fixture;binding.user=&calls;
    binding.access_journal=journal.data();binding.access_journal_capacity=journal.size();
    const auto accepted=nba97_game_scene_startup_from_scene_load(
        &binding,memory,event,registers);
    if(accepted!=1 || !binding.progress.completed)return 0;
    const auto& p=binding.progress;
    std::ostringstream out;
    out<<"{\"program\":\"GAMEONLY\",\"address\":\"0x80048D5C\","
        "\"inclusive_end\":\"0x80048FE3\",\"bytes\":648,\"instructions\":162,"
        "\"call_pc\":"<<event->pc<<",\"classification\":\"no direct visual effect\","
        "\"scope\":\"recovered owner with synthetic roster/entity records and typed resource/render service fixtures\","
        "\"completed\":"<<unsigned(p.completed)<<",\"operations\":"<<p.operations<<
        ",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"calls_completed\":"<<p.callbacks_completed<<
        ",\"controller_iterations\":"<<p.controller_iterations<<",\"controller_matches\":"<<p.controller_matches<<
        ",\"roster_iterations\":"<<p.roster_iterations<<",\"entity_iterations\":"<<p.entity_iterations<<
        ",\"frame_sp\":"<<p.frame_stack_pointer<<",\"restored_ra\":"<<p.restored_return_address.word<<
        ",\"selector_before\":7,\"selector_after\":"<<word(*memory,0x8001ede8u)<<
        ",\"render_enable\":"<<word(*memory,0x80021498u,2)<<",\"camera\":[";
    for(unsigned i=0;i<7;++i) {if(i)out<<',';out<<word(*memory,0x800fa630u+2*i,2);}
    out<<"],\"controllers\":[";
    for(unsigned i=0;i<8;++i) {if(i)out<<',';out<<word(*memory,0x800faba4u+4*i);}
    out<<"],\"home_ids\":[";
    for(unsigned i=0;i<12;++i) {if(i)out<<',';out<<word(*memory,0x8010424cu+4*i);}
    out<<"],\"away_ids\":[";
    for(unsigned i=0;i<12;++i) {if(i)out<<',';out<<word(*memory,0x8010427cu+4*i);}
    out<<"],\"entity_ids\":[";
    for(unsigned i=0;i<10;++i) {if(i)out<<',';out<<word(*memory,0x800fee90u+4*i);}
    out<<"],\"children\":[";
    for(std::size_t i=0;i<calls.size();++i) {
        if(i)out<<',';const auto& c=calls[i];
        out<<"{\"pc\":"<<c.event.pc<<",\"entry\":"<<c.event.entry<<
            ",\"delay_slot_pc\":"<<c.event.delay_slot_pc<<",\"a0\":"<<c.registers.gpr[4].word<<
            ",\"a1\":"<<c.registers.gpr[5].word<<",\"ra\":"<<c.registers.gpr[31].word<<'}';
    }
    out<<"],\"access_count\":"<<p.access_events<<'}';
    receipt=out.str();return 1;
}
}
