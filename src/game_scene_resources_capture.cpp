#include "game_scene_resources_capture.h"
#include <array>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace nba97 {
namespace {
std::uint32_t word(const Nba97GameTextMemory& memory,std::uint32_t address,
    bool store=false,std::uint32_t value=0) {
    for(std::size_t i=0;i<memory.count;++i) {
        const auto& region=memory.region[i];
        if(address<region.base)continue;
        const auto offset=std::uint64_t(address)-region.base;
        if(offset>region.size || 4>region.size-offset)continue;
        std::uint32_t result=0;
        for(unsigned j=0;j<4;++j) {
            if(store) {
                region.data[offset+j]=static_cast<std::uint8_t>(value>>(8*j));
                if(region.known)region.known[offset+j]=1;
            }
            result|=std::uint32_t(region.data[offset+j])<<(8*j);
        }
        return result;
    }
    throw std::runtime_error("scene-resource capture escaped retained memory");
}
struct Child { Nba97GameSceneResourcesEvent event;std::uint32_t a0,a1,result; };
struct Attempt { Nba97GameResourceLoaderEvent event;unsigned ordinal; };
struct Fixture {
    std::vector<Child> children;
    std::vector<Attempt> attempts;
};
int child(void* user,const Nba97GameTextMemory*,
    const Nba97GameSceneResourcesEvent* event,Nba97GameSceneResourcesRegisters* registers) {
    auto& fixture=*static_cast<Fixture*>(user);
    const auto a0=registers->gpr[4].word,a1=registers->gpr[5].word;
    // Synthetic service results only; no loader, archive or allocator algorithm.
    auto result=0x71000000u^(event->pc&0xffffu);
    if(event->entry==0x800a3fecu)result=a0+4*a1;
    if(event->entry==0x80090160u)result=event->pc==0x80052f2cu?0x8011d000u:0x8011e000u;
    registers->gpr[2]={result,0xf};
    fixture.children.push_back({*event,a0,a1,result});
    return 1;
}
int load(void* user,const Nba97GameTextMemory*,const Nba97GameResourceLoaderEvent* event,
    Nba97GameResourceLoaderValue* value) {
    auto& fixture=*static_cast<Fixture*>(user);
    unsigned ordinal=1;
    for(const auto& previous:fixture.attempts)
        ordinal+=previous.event.argument[0]==event->argument[0];
    fixture.attempts.push_back({*event,ordinal});
    // Each real retry-wrapper executes one known-null attempt before success.
    *value={ordinal==1?0u:0x80140000u+(event->argument[0]&0xffffu),1};
    return 1;
}
}
int GameSceneResourcesCapture::dispatch(const Nba97GameTextMemory* memory,
    const Nba97GameSceneStartupEvent* event,Nba97GameSceneStartupRegisters* registers) {
    if(!memory || !event || !registers || !receipt.empty())return 0;
    word(*memory,0x80021d74u,true,2);word(*memory,0x80021d78u,true,3);
    word(*memory,0x800b739cu,true,0x8011b000u);
    word(*memory,0x800b7428u,true,0x8011b100u);
    word(*memory,0x800faba0u,true,0x8011c000u);
    word(*memory,0x80102918u,true,0x8011c100u);
    Fixture fixture;
    std::array<Nba97GameSceneResourcesAccess,192> journal{};
    Nba97GameSceneResourcesBinding binding{};
    binding.operation_budget=512;binding.io=child;binding.user=&fixture;
    binding.resource_loader_operation_budget=16;binding.resource_loader_io=load;
    binding.resource_loader_user=&fixture;
    binding.access_journal=journal.data();binding.access_journal_capacity=journal.size();
    const auto accepted=nba97_game_scene_resources_from_scene_startup(&binding,memory,event,registers);
    if(accepted!=1 || !binding.progress.completed)return 0;
    const auto& p=binding.progress;
    std::ostringstream out;
    out<<"{\"program\":\"GAMEONLY\",\"address\":\"0x80052C20\","
        "\"inclusive_end\":\"0x800530FB\",\"bytes\":1244,\"instructions\":311,"
        "\"call_pc\":"<<event->pc<<",\"classification\":\"no direct visual effect\","
        "\"scope\":\"recovered owner and retry loader with synthetic archive/allocator/render/I-O services\","
        "\"completed\":"<<unsigned(p.completed)<<",\"operations\":"<<p.operations<<
        ",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"calls_completed\":"<<p.callbacks_completed<<
        ",\"typed_calls\":"<<binding.unresolved_callbacks_completed<<
        ",\"frame_sp\":"<<p.frame_stack_pointer<<",\"restored_ra\":"<<p.restored_return_address.word<<
        ",\"loader_count\":"<<binding.resource_loader_invocations<<",\"loaders\":[";
    for(std::size_t i=0;i<binding.resource_loader_invocations;++i) {
        if(i)out<<',';const auto& loader=binding.resource_loader[i];
        out<<"{\"operations\":"<<loader.operations<<",\"attempts\":"<<loader.load_attempts<<
            ",\"nulls\":"<<loader.null_results<<",\"return_v0\":"<<loader.return_v0<<'}';
    }
    out<<"],\"attempts\":[";
    for(std::size_t i=0;i<fixture.attempts.size();++i) {
        if(i)out<<',';const auto& a=fixture.attempts[i].event;
        out<<"{\"pc\":"<<a.pc<<",\"entry\":"<<a.entry<<",\"filename\":"<<a.argument[0]<<
            ",\"flags\":"<<a.argument[1]<<",\"attempt\":"<<fixture.attempts[i].ordinal<<'}';
    }
    out<<"],\"publications\":[";
    const std::uint32_t globals[]={0x800b72dcu,0x800fb820u,0x800fac20u,0x800f9fc0u,
        0x800f0edcu,0x800f0facu,0x800ebc38u,0x800f0f64u,0x800fabccu,0x800d9284u,
        0x801041a0u,0x800fdb34u,0x800dcbe8u,0x80103f44u};
    for(unsigned i=0;i<14;++i) {if(i)out<<',';out<<"["<<globals[i]<<","<<word(*memory,globals[i])<<"]";}
    out<<"],\"lookup_tables\":[";
    const std::uint32_t bases[]={0x800fac24u,0x800fb154u,0x800feca8u};
    for(unsigned i=0;i<3;++i) {
        if(i)out<<',';out<<'[';
        for(unsigned j=0;j<(i==2?26u:10u);++j){if(j)out<<',';out<<word(*memory,bases[i]+4*j);}
        out<<']';
    }
    out<<"],\"children\":[";
    for(std::size_t i=0;i<fixture.children.size();++i) {
        if(i)out<<',';const auto& c=fixture.children[i];
        out<<"{\"pc\":"<<c.event.pc<<",\"entry\":"<<c.event.entry<<
            ",\"a0\":"<<c.a0<<",\"a1\":"<<c.a1<<",\"result\":"<<c.result<<'}';
    }
    out<<"]}";receipt=out.str();return 1;
}
}
