#include "game_period_audio_noop_capture.h"
#include <algorithm>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace nba97 {
bool GamePeriodAudioNoopCapture::dispatch(const Nba97GameTextMemory* memory,const Nba97GameFirstPeriodStartupEvent* event,Nba97GameFirstPeriodStartupRegisters* registers) {
  if(!memory||!event||!registers||!receipt.empty())return false;
  const auto incoming=*registers;
  std::vector<std::vector<std::uint8_t>> bytes,known;
  for(std::size_t i=0;i<memory->count;++i){const auto&r=memory->region[i];bytes.emplace_back(r.data,r.data+r.size);known.emplace_back();if(r.known)known.back().assign(r.known,r.known+r.size);}
  Nba97GamePeriodAudioNoopBinding b{};
  if(nba97_game_period_audio_noop_from_first_period_startup(&b,memory,event,registers)!=1)return false;
  bool unchanged=true;
  for(std::size_t i=0;i<memory->count;++i){const auto&r=memory->region[i];unchanged=unchanged&&std::equal(bytes[i].begin(),bytes[i].end(),r.data);if(r.known)unchanged=unchanged&&std::equal(known[i].begin(),known[i].end(),r.known);}
  for(unsigned i=0;i<32;++i)unchanged=unchanged&&incoming.gpr[i].word==registers->gpr[i].word&&incoming.gpr[i].known_mask==registers->gpr[i].known_mask;
  const auto&p=b.progress;
  if(!p.completed||!unchanged||p.operations||p.accesses)throw std::runtime_error("audio no-op capture state drifted");
  std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x8002A254\",\"inclusive_end\":\"0x8002A25B\",\"bytes\":8,\"instructions\":2,\"classification\":\"no direct visual effect\",\"scope\":\"actual first-period caller and retained memory, no additional fixture or advancing match\",\"completed\":true,\"same_parent_memory\":true,\"call_pc\":"<<event->pc<<",\"operations\":0,\"accesses\":0,\"memory_and_registers_unchanged\":true,\"a0\":"<<registers->gpr[4].word<<",\"v0\":"<<registers->gpr[2].word<<",\"return_address\":"<<registers->gpr[31].word<<",\"sp\":"<<registers->gpr[29].word<<",\"hilo_known_masks\":["<<unsigned(p.machine.hi.known_mask)<<','<<unsigned(p.machine.lo.known_mask)<<"]}";
  receipt=o.str();return true;
}
}
