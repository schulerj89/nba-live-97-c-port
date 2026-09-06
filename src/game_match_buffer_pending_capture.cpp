#include "game_match_buffer_pending_capture.h"
#include <sstream>
#include <stdexcept>
namespace nba97 {
bool GameMatchBufferPendingCapture::dispatch(const Nba97GameTextMemory* memory,const Nba97GamePeriodStartupEvent* event,Nba97GamePeriodStartupRegisters* registers) {
 if(!memory||!event||!registers||calls.size()>=2)return false;
 Nba97GameMatchBufferPendingPeriodBinding binding{};binding.operation_budget=1;
 Nba97GameMatchBufferPendingAccess access{};binding.access_journal=&access;binding.access_journal_capacity=1;
 if(nba97_game_match_buffer_pending_from_period_startup(&binding,memory,event,registers)!=1)return false;
 const auto& p=binding.progress;
 if(!p.completed||p.operations!=1||p.stores!=1||access.address!=0x800fe864u||access.value!=1)throw std::runtime_error("match-buffer pending capture drifted");
 std::ostringstream o;o<<"{\"call_pc\":"<<event->pc<<",\"operations\":"<<p.operations<<",\"stores\":"<<p.stores<<",\"address\":"<<access.address<<",\"value\":"<<access.value<<",\"return_v0\":"<<p.machine.registers.gpr[2].word<<",\"at\":"<<p.machine.registers.gpr[1].word<<",\"return_address\":"<<p.return_address.word<<",\"sp\":"<<p.machine.registers.gpr[29].word<<",\"hilo_known_masks\":["<<unsigned(p.machine.hi.known_mask)<<','<<unsigned(p.machine.lo.known_mask)<<"]}";
 calls.push_back(o.str());return true;
}
std::string GameMatchBufferPendingCapture::receipt() const {
 if(calls.size()!=2)throw std::runtime_error("missing period pending calls");
 return "{\"program\":\"GAMEONLY\",\"address\":\"0x80076B28\",\"inclusive_end\":\"0x80076B3B\",\"bytes\":20,\"instructions\":5,\"classification\":\"no direct visual effect\",\"scope\":\"actual period caller and pending owner share retained synthetic memory; legacy tick entry and remaining children explicit fixtures, no advancing match\",\"completed\":true,\"same_parent_memory\":true,\"calls\":["+calls[0]+","+calls[1]+"]}";
}
}
