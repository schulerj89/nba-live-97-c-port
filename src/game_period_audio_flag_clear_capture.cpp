#include "game_period_audio_flag_clear_capture.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>
namespace nba97 {
bool GamePeriodAudioFlagClearCapture::dispatch(const Nba97GameTextMemory* memory,const Nba97GameFirstPeriodStartupEvent* event,Nba97GameFirstPeriodStartupRegisters* registers) {
  if(!memory||!event||!registers||!receipt.empty())return false;
  constexpr std::uint32_t address=0x800b1fd5u;
  std::uint8_t* flag=nullptr;
  for(std::size_t i=0;i<memory->count;++i){auto&r=memory->region[i];
    if(address>=r.base&&std::uint64_t(address)-r.base<r.size){flag=r.data+(address-r.base);if(r.known)r.known[address-r.base]=1;break;}
  }
  if(!flag)throw std::runtime_error("audio flag capture unmapped byte");
  // Explicit nonzero flag input makes the source clear observable on the
  // caller's existing RAM; all live registers come from the actual parent.
  *flag=0xd7;const auto incoming_v0=registers->gpr[2];
  Nba97GamePeriodAudioFlagClearAccess access{};
  Nba97GamePeriodAudioFlagClearBinding b{};b.operation_budget=1;b.access_journal=&access;b.access_journal_capacity=1;
  if(nba97_game_period_audio_flag_clear_from_first_period(&b,memory,event,registers)!=1)return false;
  const auto&p=b.progress;
  if(!p.completed||*flag!=0||access.address!=address||access.pc!=0x8002a248u||registers->gpr[2].word!=incoming_v0.word||registers->gpr[2].known_mask!=incoming_v0.known_mask)throw std::runtime_error("audio flag capture state drifted");
  std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x8002A244\",\"inclusive_end\":\"0x8002A253\",\"bytes\":16,\"instructions\":4,\"classification\":\"no direct visual effect\",\"scope\":\"actual first-period caller on same synthetic retained memory with explicit nonzero flag, no advancing match\",\"completed\":true,\"same_parent_memory\":true,\"call_pc\":"<<event->pc<<",\"flag_before\":215,\"flag_after\":0,\"operations\":"<<p.operations<<",\"stores\":"<<p.stores<<",\"store_address\":"<<access.address<<",\"store_pc\":"<<access.pc<<",\"at\":"<<p.machine.registers.gpr[1].word<<",\"v0_preserved\":true,\"v0\":"<<incoming_v0.word<<",\"return_address\":"<<p.machine.registers.gpr[31].word<<",\"sp\":"<<p.machine.registers.gpr[29].word<<",\"hilo_known_masks\":["<<unsigned(p.machine.hi.known_mask)<<','<<unsigned(p.machine.lo.known_mask)<<"]}";
  receipt=o.str();return true;
}
}
