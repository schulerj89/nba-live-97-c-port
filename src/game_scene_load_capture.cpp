#include "game_scene_load_capture.h"
#include "game_scene_load_adapter.h"
#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace nba97 {
namespace {
std::uint32_t readCaptureWord(const Nba97GameTextMemory& memory,std::uint32_t address) {
    for(std::size_t i=0;i<memory.count;++i) {
        const auto& region=memory.region[i];
        if(address<region.base)continue;
        const auto offset=std::uint64_t(address)-region.base;
        if(offset>region.size || 4>region.size-offset)continue;
        const auto* p=region.data+offset;
        return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
            (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
    }
    throw std::runtime_error("scene wrapper capture escaped retained memory");
}
struct Children { std::vector<Nba97GameSceneLoadEvent> events; };
int syntheticChild(void* user,const Nba97GameTextMemory*,
    const Nba97GameSceneLoadEvent* event,Nba97GameSceneLoadRegisters* registers) {
    static_cast<Children*>(user)->events.push_back(*event);
    // Explicit fixture response only. The wrapper owns no scene or RNG work.
    registers->gpr[NBA97_MATCH_INITIALIZE_V0]={event->entry,0xf};
    return 1;
}
}
bool GameSceneLoadCapture::dispatch(const Nba97GameTextMemory* memory,
    const Nba97GameMatchSessionEvent* event,Nba97GameMatchSessionValue* value) {
    if(!memory || !event || !value || !receipt.empty())return false;
    Nba97GameSceneLoadContext context{};
    context.memory=*memory;context.operation_budget=8;
    if(nba97_game_scene_load_registers_from_session(event,&context.registers)
       !=NBA97_TEXT_COMPLETE)return false;
    const auto saved_address=event->stack_pointer-8u;
    const auto saved_before=readCaptureWord(*memory,saved_address);
    Children children;context.io=syntheticChild;context.user=&children;
    std::array<Nba97GameSceneLoadAccess,2> journal{};
    context.access_journal=journal.data();context.access_journal_capacity=journal.size();
    Nba97GameSceneLoadProgress progress{};
    const auto result=nba97_game_scene_load(&context,&progress);
    if(result!=NBA97_TEXT_COMPLETE || !progress.completed || children.events.size()!=2)
        return false;
    const auto& live=progress.registers.gpr;
    *value={live[NBA97_MATCH_INITIALIZE_V0].word,
        static_cast<std::uint8_t>(live[NBA97_MATCH_INITIALIZE_V0].known_mask==0xf)};
    std::ostringstream out;
    out<<"{\n\"program\":\"GAMEONLY\",\"address\":\"0x8002DB68\","
        "\"inclusive_end\":\"0x8002DB8F\",\"bytes\":40,\"instructions\":10,"
        "\"call_pc\":\"0x8002DA84\",\"classification\":\"no direct visual effect\",\n"
        "\"scope\":\"recovered wrapper; two synthetic child responses, no advancing match loop\","
        "\"operations\":"<<progress.operations<<",\"reads\":"<<progress.reads<<
        ",\"stores\":"<<progress.stores<<",\"calls_completed\":"<<progress.callbacks_completed<<
        ",\"saved_address\":"<<saved_address<<",\"saved_before\":"<<saved_before<<
        ",\"saved_after\":"<<readCaptureWord(*memory,saved_address)<<
        ",\"restored_ra\":"<<progress.restored_return_address.word<<
        ",\"sp\":"<<live[NBA97_MATCH_INITIALIZE_SP].word<<",\"return_v0\":"<<value->word<<
        ",\"children\":[";
    for(std::size_t i=0;i<children.events.size();++i) {
        if(i)out<<',';
        out<<"{\"pc\":"<<children.events[i].pc<<",\"entry\":"<<children.events[i].entry<<
            ",\"delay_slot_pc\":"<<children.events[i].delay_slot_pc<<'}';
    }
    out<<"],\"accesses\":[";
    for(std::size_t i=0;i<progress.access_events;++i) {
        if(i)out<<',';
        out<<"{\"pc\":"<<journal[i].pc<<",\"address\":"<<journal[i].address<<
            ",\"value\":"<<journal[i].value<<'}';
    }
    out<<"],\"routine_capture_frame_numbers\":[0,1],"
        "\"captures\":[\"scene-load-before.ppm\",\"scene-load-after.ppm\"]}\n";
    receipt=out.str();return true;
}
void GameSceneLoadCapture::writeReceipt(const std::filesystem::path& path) const {
    if(receipt.empty() || before!=after)
        throw std::runtime_error("scene wrapper capture missing or scanout changed");
    std::ofstream out(path);out<<receipt;
    if(!out)throw std::runtime_error("cannot write scene wrapper receipt");
}
}
