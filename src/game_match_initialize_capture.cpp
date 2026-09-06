#include "game_match_state_reset_capture.h"
#include "game_match_initialize_capture.h"
#include "game_match_initialize_adapter.h"
#include "recovered/game_roster_bindings.h"
#include "game_audio_initialize_capture.h"
#include "game_speech_initialize_capture.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace nba97 {
namespace {
// Native fixture preparation/inspection only; all translated work stays in C.
std::uint8_t* locate(const Nba97GameTextMemory& memory,std::uint32_t address,
                     std::size_t width) {
    for(std::size_t i=0;i<memory.count;++i) {
        auto& region=memory.region[i];
        if(address<region.base)continue;
        const auto offset=std::uint64_t(address)-region.base;
        if(offset>region.size || width>region.size-offset)continue;
        return region.data+offset;
    }
    throw std::runtime_error("match initializer capture escaped retained memory");
}
std::uint32_t get(const Nba97GameTextMemory& memory,std::uint32_t address) {
    const auto* p=locate(memory,address,4);
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
struct ChildLog {
    std::vector<Nba97GameMatchInitializeEvent> events;
    std::vector<std::uint32_t> a0;
    bool clear_seen=false;
    bool clear_after_zero=false;
    GameMatchStateResetCapture reset;
    Nba97GameRosterBindingsProgress roster{};
    GameAudioInitializeCapture audio;
    GameSpeechInitializeCapture speech;
};
int child(void* user,const Nba97GameTextMemory* memory,
          const Nba97GameMatchInitializeEvent* event,
          Nba97GameMatchInitializeRegisters* registers) {
    auto& log=*static_cast<ChildLog*>(user);
    if(log.events.empty()) {
        const auto* bytes=locate(*memory,0x800fdb4cu,0xe7cu);
        log.clear_after_zero=std::all_of(bytes,bytes+0xe7cu,[](std::uint8_t b){return b==0;});
    }
    log.events.push_back(*event);
    log.a0.push_back(registers->gpr[NBA97_MATCH_INITIALIZE_A0].word);
    if(event->entry==0x80063d58u) {
        Nba97GameRosterBindingsContext context{};
        context.memory=*memory;context.operation_budget=512;
        context.registers=*registers;
        const auto result=nba97_game_roster_bindings(&context,&log.roster);
        *registers=log.roster.registers;
        return result==NBA97_TEXT_COMPLETE && log.roster.completed;
    }
    if(event->entry==0x800659f0u)return log.reset.dispatch(memory,event,registers);
    if(event->entry==0x80029114u)return log.audio.dispatch(memory,event,registers);
    if(event->entry==0x8007fd40u)return log.speech.dispatch(memory,event,registers);
    if(event->entry==0x800763f4u)log.clear_seen=get(*memory,0x80020c18u)==0;
    // Explicit nonretail child response. Do not invent simulation or rendering
    // work for these still-incompatible/unresolved complete-call interfaces.
    registers->gpr[NBA97_MATCH_INITIALIZE_V0]={event->entry,0x0f};
    return 1;
}
}

bool GameMatchInitializeCapture::dispatch(const Nba97GameTextMemory* memory,
    const Nba97GameMatchSessionEvent* event,Nba97GameMatchSessionValue* value) {
    if(!memory || !event || !value || !receipt.empty())return false;
    Nba97GameMatchInitializeContext context{};
    context.memory=*memory;context.operation_budget=40;
    if(nba97_game_match_initialize_registers_from_session(event,&context.registers)
       !=NBA97_TEXT_COMPLETE)return false;
    // Runtime sentinel fixture makes the CPU transition measurable even when
    // the caller's selected teams and original snapshot values are both zero.
    std::fill_n(locate(*memory,0x800fdb4cu,0xe7cu),0xe7cu,std::uint8_t{0x5a});
    std::fill_n(locate(*memory,0x80020c18u,4),4,std::uint8_t{0xa5});
    const std::array<std::uint32_t,2> teams={get(*memory,0x80021d74u),get(*memory,0x80021d78u)};
    // Synthetic counts exercise a short roster and a full twelve-slot roster.
    *locate(*memory,0x80023aecu+teams[0]*0x68u,1)=3;
    *locate(*memory,0x80023aecu+teams[1]*0x68u,1)=12;
    ChildLog log;context.io=child;context.user=&log;
    std::array<Nba97GameMatchInitializeAccess,7> accesses{};
    context.access_journal=accesses.data();context.access_journal_capacity=accesses.size();
    Nba97GameMatchInitializeProgress progress{};
    Nba97GameMatchInitializeAdapterProgress adapter{};
    const auto result=nba97_game_match_initialize_with_zero(&context,1100,&progress,&adapter);
    const bool cleared=log.clear_after_zero;
    if(result!=NBA97_TEXT_COMPLETE || !progress.completed || !cleared ||
       !adapter.memory_zero.completed || adapter.memory_zero_invocations!=1 || log.reset.receipt.empty() ||
       log.events.size()!=11 || !log.clear_seen || !log.roster.completed ||
       get(*memory,0x80022084u)!=teams[0] || get(*memory,0x80022adcu)!=teams[1])return false;
    const auto& live=progress.registers.gpr;
    *value={live[NBA97_MATCH_INITIALIZE_V0].word,
        static_cast<std::uint8_t>(live[NBA97_MATCH_INITIALIZE_V0].known_mask==0xf)};
    std::ostringstream out;
    out<<"{\n  \"program\": \"GAMEONLY\", \"address\": \"0x8002DB90\", "
        "\"inclusive_end\": \"0x8002DC37\", \"bytes\": 168, \"instructions\": 42,\n"
        "  \"instruction_sha256\": \"c1569d2ae6b58be97cd7511f5dd2bee7be70684d9e1fc9ba9abd3ad9f83ce6f3\",\n"
        "  \"call_pc\": \"0x8002DA7C\", \"classification\": \"no direct visual effect\",\n"
        "  \"scope\": \"recovered reset, zero, roster, audio and speech owners; synthetic dependent services, no advancing match loop\",\n"
        "  \"driver\": \"native recovered-input handlers: Game Setup, Team Select, User Setup\",\n"
        "  \"operations\": "<<progress.operations<<", \"reads\": "<<progress.reads<<
        ", \"stores\": "<<progress.stores<<", \"calls_completed\": "<<progress.callbacks_completed<<",\n"
        "  \"team_snapshots\": ["<<teams[0]<<','<<teams[1]<<"], "
        "\"zero_bytes\": 3708, \"zero_before_byte\": 90, \"zero_after\": true, \"zero_after_checkpoint\": \"immediately after parent zero before first child\", "
        "\"zero_stores\": "<<adapter.memory_zero.stores<<",\n"
        "  \"final_flag_before\": 2779096485, \"final_flag_after\": "<<get(*memory,0x80020c18u)<<
        ", \"final_child_saw_clear\": true, \"return_v0\": "<<value->word<<
        ", \"restored_ra\": "<<progress.restored_return_address.word<<
        ", \"sp\": "<<live[NBA97_MATCH_INITIALIZE_SP].word<<",\n"
        "  \"typed_children\": [";
    for(std::size_t i=0;i<log.events.size();++i) {
        if(i)out<<',';
        out<<"{\"pc\": "<<log.events[i].pc<<", \"entry\": "<<log.events[i].entry<<
            ", \"a0\": "<<log.a0[i]<<'}';
    }
    out<<"],\n  \"parent_accesses\": [";
    for(std::size_t i=0;i<progress.access_events;++i) {
        if(i)out<<',';
        out<<"{\"pc\": "<<accesses[i].pc<<", \"address\": "<<accesses[i].address<<
            ", \"value\": "<<accesses[i].value<<'}';
    }
    out<<"],\n  \"roster_bindings\": {\"program\": \"GAMEONLY\", "
        "\"address\": \"0x80063D58\", \"inclusive_end\": \"0x80063EDB\", "
        "\"bytes\": 388, \"instructions\": 97, \"call_pc\": \"0x8002DBC8\", "
        "\"classification\": \"no direct visual effect\", "
        "\"counts\": [3,12], \"completed\": true, \"operations\": "<<log.roster.operations<<
        ", \"reads\": "<<log.roster.reads<<", \"stores\": "<<log.roster.stores<<
        ", \"published_table\": "<<get(*memory,0x80015030u)<<", \"home\": [";
    for(unsigned i=0;i<12;++i) {if(i)out<<',';out<<get(*memory,0x80020b8cu+i*4);}
    out<<"], \"away\": [";
    for(unsigned i=0;i<12;++i) {if(i)out<<',';out<<get(*memory,0x80020bbcu+i*4);}
    out<<"], \"mirror_home\": [";
    for(unsigned i=0;i<12;++i) {if(i)out<<',';out<<get(*memory,0x80015034u+i*4);}
    out<<"], \"mirror_away\": [";
    for(unsigned i=0;i<12;++i) {if(i)out<<',';out<<get(*memory,0x80015064u+i*4);}
    out<<"]},\n  \"audio_initialize\": "<<log.audio.receipt<<
        ",\n  \"speech_initialize\": "<<log.speech.receipt<<
        ",\n  \"match_state_reset\": "<<log.reset.receipt<<
        ",\n  \"routine_capture_frame_numbers\": [0, 1],\n"
        "  \"captures\": [\"match-initialize-before.ppm\", \"match-initialize-after.ppm\"]\n}\n";
    receipt=out.str();return true;
}
void GameMatchInitializeCapture::writeReceipt(const std::filesystem::path& path) const {
    if(receipt.empty() || before!=after)
        throw std::runtime_error("match initializer capture missing or scanout changed");
    std::ofstream out(path);out<<receipt;
    if(!out)throw std::runtime_error("cannot write match initializer receipt");
}
}
