#include "game_loop_entry_capture.h"
#include "game_loop_entry_adapter.h"
#include "game_match_hot_start_capture.h"
#include "game_period_startup_capture.h"
#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>
namespace nba97 {
bool GameLoopEntryCapture::probe(const Nba97GameTextMemory* memory,
    const Nba97GameMatchSessionEvent* event) {
    if(!memory || !event || !receipt.empty())return false;
    // This isolated diagnostic terminates at refusal. The older coverage
    // harness retains its explicitly synthetic RUN_LOOP response separately.
    std::vector<std::vector<std::uint8_t>> bytes(memory->count),known(memory->count);
    std::vector<Nba97GameTextRegion> regions(memory->count);
    for(std::size_t i=0;i<memory->count;++i) {
        const auto& source=memory->region[i];
        bytes[i].assign(source.data,source.data+source.size);
        if(source.known)known[i].assign(source.known,source.known+source.size);
        regions[i]={source.base,bytes[i].data(),source.known?known[i].data():nullptr,source.size};
    }
    Nba97GameLoopEntryContext context{};
    context.memory={regions.data(),regions.size()};context.operation_budget=3;
    if(nba97_game_loop_entry_registers_from_session(event,&context.registers)
       !=NBA97_TEXT_COMPLETE)return false;
    std::array<Nba97GameLoopEntryAccess,2> journal{};
    context.access_journal=journal.data();context.access_journal_capacity=journal.size();
    Nba97GameLoopEntryMatchTickServices services{};services.operation_budget=256;
    Nba97GameLoopEntryProgress progress{};
    Nba97GameLoopEntryAdapterProgress adapter{};
    const auto result=nba97_game_loop_entry_with_match_tick(&context,&services,&progress,&adapter);
    if(result!=NBA97_TEXT_IO_REFUSED || progress.completed ||
       adapter.match_tick_result!=NBA97_MATCH_TICK_SERVICE_REQUIRED ||
       adapter.match_tick.stopped_pc!=0x80068c24u ||
       adapter.match_tick.stopped_entry!=0x80066f88u)return false;
    unsigned unknown=0;
    for(unsigned i=1;i<32;++i)unknown+=progress.registers.gpr[i].known_mask==0;
    std::ostringstream out;
    out<<"{\"program\":\"GAMEONLY\",\"address\":\"0x8002DC38\","
        "\"inclusive_end\":\"0x8002DC57\",\"bytes\":32,\"instructions\":8,"
        "\"call_pc\":\"0x8002DA8C\",\"classification\":\"BLOCKED\","
        "\"scope\":\"isolated retained-memory probe from natural caller event; terminated at missing service; legacy coverage continuation remains synthetic\","
        "\"completed\":false,\"operations\":"<<progress.operations<<
        ",\"reads\":"<<progress.reads<<",\"stores\":"<<progress.stores<<
        ",\"calls_completed\":"<<progress.callbacks_completed<<
        ",\"stopped_pc\":"<<progress.stopped_pc<<",\"stopped_entry\":"<<progress.stopped_entry<<
        ",\"saved_pc\":"<<journal[0].pc<<",\"saved_address\":"<<journal[0].address<<
        ",\"saved_value\":"<<journal[0].value<<",\"unknown_output_gprs\":"<<unknown<<
        ",\"tick\":{\"entry\":2147912696,\"completed\":false,\"operations\":"<<adapter.match_tick.operations<<
        ",\"stopped_pc\":"<<adapter.match_tick.stopped_pc<<",\"stopped_entry\":"<<adapter.match_tick.stopped_entry<<
        ",\"simulation_steps\":"<<adapter.match_tick.simulation_steps<<
        ",\"frame_pumps\":"<<adapter.match_tick.frame_pumps<<"},"
        "\"hot_start\":"<<captureGameMatchHotStart()<<","
        "\"period_startup\":"<<captureGamePeriodStartup()<<","
        "\"routine_capture_frame_numbers\":[0,1],"
        "\"captures\":[\"loop-entry-before.ppm\",\"loop-entry-after.ppm\"]}\n";
    receipt=out.str();return true;
}
void GameLoopEntryCapture::writeReceipt(const std::filesystem::path& path) const {
    if(receipt.empty() || before!=after)
        throw std::runtime_error("loop-entry evidence missing or scanout changed");
    std::ofstream out(path);out<<receipt;
    if(!out)throw std::runtime_error("cannot write loop-entry evidence");
}
}
