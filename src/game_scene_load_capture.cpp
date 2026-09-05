#include "game_scene_load_capture.h"
#include "game_scene_load_adapter.h"
#include "game_scene_random_warmup_adapter.h"
#include "game_scene_startup_capture.h"
#include "game_random_seed_adapter.h"
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
struct Children {
    std::vector<Nba97GameSceneLoadEvent> events;
    std::vector<Nba97GameSceneRandomWarmupEvent> warmup_events;
    std::vector<std::uint32_t> step_counts;
    GameSceneStartupCapture startup;
    Nba97GameRandomSeedAdapterProgress seed{};
    std::array<Nba97GameRandomSeedAccess,6> seed_journal{};
};
int syntheticWarmupChild(void* user,const Nba97GameTextMemory* memory,
    const Nba97GameSceneRandomWarmupEvent* event,
    Nba97GameSceneRandomWarmupRegisters* registers) {
    auto& children=*static_cast<Children*>(user);
    children.warmup_events.push_back(*event);
    if(event->kind==NBA97_GAME_SCENE_RANDOM_WARMUP_SEED_80093694) {
        Nba97GameRandomSeedContext seed{};seed.operation_budget=6;
        seed.access_journal=children.seed_journal.data();seed.access_journal_capacity=children.seed_journal.size();
        return nba97_game_random_seed_from_warmup(memory,event,registers,&seed,&children.seed)==NBA97_TEXT_COMPLETE;
    }
    // Explicit deterministic service fixtures, not the original RNG algorithm.
    if(event->kind==NBA97_GAME_SCENE_RANDOM_WARMUP_RANDOM_8002AB70)
        registers->gpr[NBA97_MATCH_INITIALIZE_V0]={
            event->invocation==1?0xdead0081u:0xbeefcafeu,0xf};
    if(event->kind==NBA97_GAME_SCENE_RANDOM_WARMUP_STEP_800935C4) {
        children.step_counts.push_back(registers->gpr[NBA97_MATCH_INITIALIZE_S0].word);
        registers->gpr[NBA97_MATCH_INITIALIZE_V0]={
            0x60000000u+static_cast<std::uint32_t>(event->invocation),0xf};
    }
    return 1;
}
int sceneChild(void* user,const Nba97GameTextMemory* memory,
    const Nba97GameSceneLoadEvent* event,Nba97GameSceneLoadRegisters* registers) {
    auto& children=*static_cast<Children*>(user);
    children.events.push_back(*event);
    return children.startup.dispatch(memory,event,registers);
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
    Children children;context.io=sceneChild;context.user=&children;
    std::array<Nba97GameSceneLoadAccess,2> journal{};
    context.access_journal=journal.data();context.access_journal_capacity=journal.size();
    Nba97GameSceneLoadProgress progress{};
    Nba97GameSceneRandomWarmupContext warmup{};
    warmup.operation_budget=256;warmup.io=syntheticWarmupChild;warmup.user=&children;
    std::array<Nba97GameSceneRandomWarmupAccess,4> warmup_journal{};
    warmup.access_journal=warmup_journal.data();warmup.access_journal_capacity=warmup_journal.size();
    Nba97GameSceneRandomWarmupAdapterProgress adapter{};
    const auto result=nba97_game_scene_load_with_random_warmup(&context,&warmup,&progress,&adapter);
    if(adapter.warmup_invocations==1)
        children.events.insert(children.events.begin(),adapter.warmup_event);
    if(result!=NBA97_TEXT_COMPLETE || !progress.completed || children.events.size()!=2)
        return false;
    const auto& live=progress.registers.gpr;
    *value={live[NBA97_MATCH_INITIALIZE_V0].word,
        static_cast<std::uint8_t>(live[NBA97_MATCH_INITIALIZE_V0].known_mask==0xf)};
    std::ostringstream out;
    out<<"{\n\"program\":\"GAMEONLY\",\"address\":\"0x8002DB68\","
        "\"inclusive_end\":\"0x8002DB8F\",\"bytes\":40,\"instructions\":10,"
        "\"call_pc\":\"0x8002DA84\",\"classification\":\"no direct visual effect\",\n"
        "\"scope\":\"recovered wrapper and random warm-up; synthetic service responses, no advancing match loop\","
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
    const auto& random=adapter.warmup;
    out<<"],\"random_warmup\":{\"program\":\"GAMEONLY\",\"address\":\"0x800802AC\","
        "\"inclusive_end\":\"0x80080303\",\"bytes\":88,\"instructions\":22,"
        "\"call_pc\":\"0x8002DB70\",\"classification\":\"no direct visual effect\","
        "\"scope\":\"source owner with recovered six-word seed and synthetic startup/random/step responses\","
        "\"completed\":"<<unsigned(random.completed)<<",\"operations\":"<<random.operations<<
        ",\"reads\":"<<random.reads<<",\"stores\":"<<random.stores<<
        ",\"calls_completed\":"<<random.callbacks_completed<<",\"count\":"<<random.warmup_count.word<<
        ",\"seed\":"<<random.seed_argument.word<<",\"frame_sp\":"<<random.frame_stack_pointer<<
        ",\"restored_ra\":"<<random.restored_return_address.word<<
        ",\"restored_s0\":"<<random.restored_s0.word<<",\"step_counts\":[";
    for(std::size_t i=0;i<children.step_counts.size();++i) {
        if(i)out<<',';out<<children.step_counts[i];
    }
    out<<"],\"children\":[";
    for(std::size_t i=0;i<children.warmup_events.size();++i) {
        if(i)out<<',';
        const auto& child=children.warmup_events[i];
        out<<"{\"pc\":"<<child.pc<<",\"entry\":"<<child.entry<<
            ",\"delay_slot_pc\":"<<child.delay_slot_pc<<'}';
    }
    out<<"],\"accesses\":[";
    for(std::size_t i=0;i<random.access_events;++i) {
        if(i)out<<',';const auto& access=warmup_journal[i];
        out<<"{\"pc\":"<<access.pc<<",\"address\":"<<access.address<<
            ",\"value\":"<<access.value<<",\"known_mask\":"<<unsigned(access.known_mask)<<'}';
    }
    out<<"],\"random_seed\":{\"program\":\"GAMEONLY\",\"address\":\"0x80093694\",\"inclusive_end\":\"0x80093733\",\"bytes\":160,\"instructions\":40,"
        "\"classification\":\"no direct visual effect\",\"completed\":"<<unsigned(children.seed.seed.completed)<<
        ",\"invocations\":"<<children.seed.seed_invocations<<",\"operations\":"<<children.seed.seed.operations<<
        ",\"stores\":"<<children.seed.seed.stores<<",\"call_pc\":"<<children.seed.seed_event.pc<<
        ",\"delay_slot_pc\":"<<children.seed.seed_event.delay_slot_pc<<",\"words\":[";
    for(unsigned i=0;i<6;++i){if(i)out<<',';out<<readCaptureWord(*memory,0x800c4ae8u+i*4u);}
    out<<"],\"accesses\":[";
    for(unsigned i=0;i<6;++i){if(i)out<<',';const auto& a=children.seed_journal[i];
        out<<"{\"pc\":"<<a.pc<<",\"address\":"<<a.address<<",\"value\":"<<a.value<<",\"known_mask\":"<<unsigned(a.known_mask)<<'}';}
    out<<"],\"final_a0\":"<<children.seed.seed.registers.gpr[4].word<<",\"final_a1\":"<<children.seed.seed.registers.gpr[5].word<<
        ",\"final_at\":"<<children.seed.seed.registers.gpr[1].word<<",\"final_v0\":"<<children.seed.seed.registers.gpr[2].word<<"}},\"scene_startup\":"<<children.startup.receipt<<
        ",\"routine_capture_frame_numbers\":[0,1],"
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
