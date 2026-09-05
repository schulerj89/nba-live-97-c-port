#include "game_audio_initialize_capture.h"
#include "game_audio_initialize_adapter.h"
#include <array>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace nba97 {
namespace {
std::uint8_t* locate(const Nba97GameTextMemory& memory,std::uint32_t address,std::size_t width) {
    for(std::size_t i=0;i<memory.count;++i) {
        const auto& region=memory.region[i];
        if(address<region.base)continue;
        const auto offset=std::uint64_t(address)-region.base;
        if(offset<=region.size && width<=region.size-offset)return region.data+offset;
    }
    throw std::runtime_error("audio capture escaped retained memory");
}
std::uint32_t get(const Nba97GameTextMemory& memory,std::uint32_t address) {
    const auto* p=locate(memory,address,4);
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
        (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
void put(const Nba97GameTextMemory& memory,std::uint32_t address,std::uint32_t value) {
    auto* p=locate(memory,address,4);
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(value>>(8*i));
}
struct Fixture {
    std::vector<Nba97GameAudioInitializeEvent> calls;
    std::array<unsigned,2> attempts{};
    std::array<std::uint32_t,3> upload{};
};
int load(void* user,const Nba97GameTextMemory*,const Nba97GameResourceLoaderEvent* event,
         Nba97GameResourceLoaderValue* value) {
    auto& f=*static_cast<Fixture*>(user);
    if(event->entry!=0x800941c8u || event->argument[1] ||
       (event->argument[0]!=0x800247bcu && event->argument[0]!=0x800247c8u))return 0;
    const unsigned slot=event->argument[0]==0x800247bcu?0:1;
    const auto attempt=++f.attempts[slot];
    // Generated handle responses; no bank bytes or audible output are implied.
    *value={attempt==1?0u:(slot?0x80119000u:0x80118000u),1};return 1;
}
int child(void* user,const Nba97GameTextMemory* memory,
          const Nba97GameAudioInitializeEvent* event,Nba97GameAudioInitializeRegisters* registers) {
    auto& f=*static_cast<Fixture*>(user);f.calls.push_back(*event);
    if(event->entry==0x8008cc28u)put(*memory,0x8001502cu,0x80118100u);
    if(event->entry==0x800ad360u)
        for(unsigned i=0;i<3;++i)f.upload[i]=registers->gpr[NBA97_MATCH_INITIALIZE_A0+i].word;
    registers->gpr[NBA97_MATCH_INITIALIZE_V0]={event->entry==0x80088e84u?0xfeedbeefu:event->entry,0xf};
    return 1;
}
}
bool GameAudioInitializeCapture::dispatch(const Nba97GameTextMemory* memory,
    const Nba97GameMatchInitializeEvent* event,Nba97GameMatchInitializeRegisters* registers) {
    if(!memory || !event || !registers || !receipt.empty())return false;
    Nba97GameAudioInitializeContext context{};context.memory=*memory;context.operation_budget=64;
    if(nba97_game_audio_initialize_registers_from_match_initialize(event,registers,&context.registers)
       !=NBA97_TEXT_COMPLETE)return false;
    put(*memory,0x8001502cu,0x80117000u);put(*memory,0x80021ee0u,0xa5a5a5a5u);
    *locate(*memory,0x80021d7cu,1)=9;
    Fixture fixture;context.io=child;context.user=&fixture;
    Nba97GameAudioInitializeDependencies dependencies{32,load,&fixture};
    Nba97GameAudioInitializeProgress progress{};
    Nba97GameAudioInitializeAdapterProgress adapter{};
    const auto result=nba97_game_audio_initialize_with_recovered_dependencies(
        &context,&dependencies,&progress,&adapter);
    *registers=progress.registers;
    if(result!=NBA97_TEXT_COMPLETE || !progress.completed || adapter.resource_loader_invocations!=2)
        return false;
    std::ostringstream out;
    out<<"{\"program\":\"GAMEONLY\",\"address\":\"0x80029114\","
        "\"inclusive_end\":\"0x800291FF\",\"bytes\":236,\"instructions\":59,"
        "\"call_pc\":\"0x8002DBD0\",\"classification\":\"no direct visual effect\","
        "\"scope\":\"recovered retry loaders; synthetic bank handles and audio services; no audible output or advancing match\","
        "\"operations\":"<<progress.operations<<",\"reads\":"<<progress.reads<<
        ",\"stores\":"<<progress.stores<<",\"calls_completed\":"<<progress.callbacks_completed<<
        ",\"old_header\":"<<progress.old_bank_header.word<<",\"loaded_header\":"<<progress.new_bank_header.word<<
        ",\"live_header\":"<<get(*memory,0x8001502cu)<<",\"body\":"<<progress.bank_body.word<<
        ",\"setting\":"<<progress.volume_setting.word<<",\"scaled_volume\":"<<progress.scaled_volume.word<<
        ",\"result_before\":2779096485,\"raw_return\":"<<progress.raw_volume_return.word<<
        ",\"result_after\":"<<get(*memory,0x80021ee0u)<<",\"restored_ra\":"<<progress.restored_return_address.word<<
        ",\"sp\":"<<registers->gpr[NBA97_MATCH_INITIALIZE_SP].word<<
        ",\"upload_args\":["<<fixture.upload[0]<<','<<fixture.upload[1]<<','<<fixture.upload[2]<<"],\"loaders\":[";
    for(unsigned i=0;i<2;++i) {
        if(i)out<<',';
        out<<"{\"operations\":"<<adapter.resource_loader[i].operations<<
            ",\"attempts\":"<<adapter.resource_loader[i].load_attempts<<
            ",\"null_results\":"<<adapter.resource_loader[i].null_results<<'}';
    }
    out<<"],\"typed_children\":[";
    for(std::size_t i=0;i<fixture.calls.size();++i) {
        if(i)out<<',';
        out<<"{\"pc\":"<<fixture.calls[i].pc<<",\"entry\":"<<fixture.calls[i].entry<<'}';
    }
    out<<"]}";receipt=out.str();return true;
}
}
