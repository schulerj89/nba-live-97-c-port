#include "game_period_expiry_capture.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>
namespace nba97 {
int GamePeriodExpiryCapture::dispatch(const Nba97GameTextMemory* memory,const Nba97MatchTickCall* call,Nba97GamePeriodValue* value,const Nba97GameClockViolationsProgress* previous) {
    if(!memory || !call || !value || !previous || !previous->completed || !receipt.empty() ||
       previous->machine.registers.gpr[31].known_mask!=15 || previous->machine.registers.gpr[31].word!=0x80068d6cu)return NBA97_BODY_ARGUMENT;
    // Actual prior owner output through adjacent JAL/NOP, with no intervening
    // memory fixture changes. Initial root machine and older services remain
    // explicit synthetic fixtures, not an advancing native match.
    Nba97GamePeriodExpiryBinding b{};b.memory=*memory;b.operation_budget=40;b.entry_machine_ready=1;
    b.entry_machine=previous->machine;b.entry_machine.registers.gpr[31]={0x80068d74u,15};
    const int result=nba97_game_period_expiry_from_match_tick(&b,call,value);const auto& p=b.progress;
    if(result!=NBA97_BODY_OK || !p.completed || b.invocations!=1 || !value->known || value->word!=0 ||
       p.operations!=7 || p.reads!=4 || p.stores!=3 || p.callbacks_completed ||
       p.restored_return_address.word!=0x80068d74u || p.machine.registers.gpr[29].word!=previous->machine.registers.gpr[29].word)
        throw std::runtime_error("period expiry native CPU fixture drifted");
    std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x80067664\",\"inclusive_end\":\"0x800677D7\",\"bytes\":372,\"instructions\":93,"
      "\"classification\":\"no direct visual effect\",\"scope\":\"actual violation owner output through adjacent JAL/NOP; explicit initial machine and earlier service fixtures\","
      "\"completed\":true,\"call_pc\":"<<call->pc<<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores
     <<",\"child_calls\":"<<p.callbacks_completed<<",\"returned_value\":"<<value->word<<",\"frame_stack_pointer\":"<<p.frame_stack_pointer
     <<",\"restored_ra\":"<<p.restored_return_address.word<<"}";
    progress=p;receipt=o.str();return result;
}
}
