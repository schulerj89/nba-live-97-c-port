#include "game_team_strategy_apply_capture.h"
#include "game_team_header_initialize_capture.h"
#include "game_controller_profile_reset_capture.h"
#include "game_match_state_reset_capture.h"
#include "game_match_state_reset_adapter.h"
#include <sstream>
#include <stdexcept>
#include <vector>

namespace nba97 {
namespace {
std::uint32_t read(const Nba97GameTextMemory& memory,std::uint32_t address,unsigned width) {
 for(std::size_t i=0;i<memory.count;++i) {
  const auto&r=memory.region[i];const std::uint64_t offset=std::uint64_t(address)-r.base;
  if(address<r.base||offset>r.size||width>r.size-offset)continue;
  std::uint32_t value=0;
  for(unsigned b=0;b<width;++b){if(r.known&&r.known[offset+b]!=1)throw std::runtime_error("reset capture unknown byte");value|=std::uint32_t(r.data[offset+b])<<(8*b);}return value;
 }
 throw std::runtime_error("reset capture unmapped read");
}
struct Fixture {
 GameTeamStrategyApplyCapture strategy;
 GameTeamHeaderInitializeCapture teams;
 GameControllerProfileResetCapture profile;
 std::vector<Nba97GameMatchStateResetEvent> calls;
 static int service(void* opaque,const Nba97GameTextMemory* memory,const Nba97GameMatchStateResetEvent* event,Nba97GameMatchStateResetMachine* machine) {
  auto&self=*static_cast<Fixture*>(opaque);
  if(event->entry==0x80083490u)return self.profile.dispatch(memory,event,machine);
  if(event->entry==0x800655b0u)return self.teams.dispatch(memory,event,machine);
  if(event->entry==0x80065820u)return self.strategy.dispatch(memory,event,machine);
  self.calls.push_back(*event);
  // Explicit synthetic full-machine response. Later input/team/period owners
  // must replace these services before this path can initialize a retail match.
  machine->registers.gpr[2]={event->entry,15};return 1;
 }
};
}
bool GameMatchStateResetCapture::dispatch(const Nba97GameTextMemory* memory,const Nba97GameMatchInitializeEvent* event,Nba97GameMatchInitializeRegisters* registers) {
 if(!memory||!event||!registers||!receipt.empty())return false;
 const auto mode=read(*memory,0x8001edecu,2);
 Fixture fixture;Nba97GameMatchStateResetBinding binding{};
 binding.operation_budget=64;for(auto& n:binding.zero_operation_budget)n=2048;binding.roster_operation_budget=512;
 binding.io=Fixture::service;binding.user=&fixture;
 // The caller exposes GPRs only: omitted HI/LO remain explicitly unknown.
 if(nba97_game_match_state_reset_from_match_initialize(&binding,memory,event,registers)!=1||binding.result!=NBA97_TEXT_COMPLETE||!binding.progress.completed||binding.zero_invocations!=4||binding.roster_invocations!=1||fixture.calls.size()!=4||fixture.strategy.receipt.empty()||fixture.profile.receipt.empty()||fixture.teams.receipt.empty())
  return false;
 const auto& p=binding.progress;
 if(read(*memory,0x8001edf2u,2)!=0||read(*memory,0x800fdb9cu,2)!=65535||read(*memory,0x8001eeccu,2)!=5||read(*memory,0x800fdb54u,2)!=0||p.spin_iterations!=24)
  throw std::runtime_error("reset capture source state mismatch");
 std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x800659F0\",\"inclusive_end\":\"0x80065B17\",\"bytes\":296,\"instructions\":74,\"call_pc\":\"0x8002DBF8\","
 "\"classification\":\"no direct visual effect\",\"scope\":\"actual initializer and match-state reset on the same retained memory, real zero-fill and roster owners; four typed controller/period services, no advancing native match\",\"completed\":true,\"same_parent_memory\":true,"
 "\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"calls_completed\":"<<p.callbacks_completed<<",\"spin_iterations\":"<<p.spin_iterations<<",\"mode\":"<<mode<<",\"mode_98\":"<<unsigned(p.mode_98)
 <<",\"zero_calls\":"<<binding.zero_invocations<<",\"roster_calls\":"<<binding.roster_invocations<<",\"hilo_known_masks\":["<<unsigned(p.machine.hi.known_mask)<<','<<unsigned(p.machine.lo.known_mask)<<"],\"restored_ra\":"<<p.machine.registers.gpr[31].word<<",\"sp\":"<<p.machine.registers.gpr[29].word<<",\"final_halfwords\":["<<read(*memory,0x8001edf2u,2)<<','<<read(*memory,0x800fdb9cu,2)<<','<<read(*memory,0x8001eeccu,2)<<','<<read(*memory,0x800fdb54u,2)<<"],\"zero_ranges\":[";
 for(unsigned i=0;i<4;++i){if(i)o<<',';const auto&z=binding.zero_progress[i];o<<"{\"address\":"<<z.destination<<",\"length\":"<<z.requested_length<<",\"stores\":"<<z.stores<<",\"completed\":"<<unsigned(z.completed)<<"}";}
 o<<"],\"typed_children\":[";for(std::size_t i=0;i<fixture.calls.size();++i){if(i)o<<',';const auto&e=fixture.calls[i];o<<"{\"pc\":"<<e.pc<<",\"entry\":"<<e.entry<<"}";}
 o<<"],\"controller_profile_reset\":"<<fixture.profile.receipt<<",\"team_header_initialize\":"<<fixture.teams.receipt<<",\"team_strategy_apply\":"<<fixture.strategy.receipt<<"}";receipt=o.str();return true;
}
}
