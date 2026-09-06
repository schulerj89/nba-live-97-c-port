#include "game_period_music_start_capture.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace nba97 { namespace {
std::uint32_t word(const Nba97GameTextMemory& m, std::uint32_t a, unsigned width, bool store=false, std::uint32_t value=0) {
  for (std::size_t i=0;i<m.count;++i) {
    const auto& r=m.region[i]; const auto offset=std::uint64_t(a)-r.base;
    if(a<r.base || offset>r.size || width>r.size-offset) continue;
    std::uint32_t v=0;
    for(unsigned b=0;b<width;++b) {
      if(store) {r.data[offset+b]=std::uint8_t(value>>(8*b));if(r.known)r.known[offset+b]=1;}
      else if(r.known && r.known[offset+b]!=1)throw std::runtime_error("music capture unknown byte");
      v|=std::uint32_t(r.data[offset+b])<<(8*b);
    }
    return v;
  }
  throw std::runtime_error("music capture unmapped byte");
}
struct Children {
  std::vector<std::uint32_t> pcs, entries, args;
  static int call(void* u,const Nba97GameTextMemory*,const Nba97GamePeriodMusicStartEvent* e,Nba97GamePeriodMusicStartMachine* m) {
    auto& self=*static_cast<Children*>(u);self.pcs.push_back(e->pc);self.entries.push_back(e->entry);
    for(unsigned i=0;i<e->argument_count;++i)self.args.push_back(m->registers.gpr[4+i].word);
    // Explicit service response; no music loader, driver, or audio output claimed.
    m->registers.gpr[2]={e->entry,15};return 1;
  }
};
}
bool GamePeriodMusicStartCapture::dispatch(const Nba97GameTextMemory* memory,const Nba97GameFirstPeriodStartupEvent* event,Nba97GameFirstPeriodStartupRegisters* registers,unsigned initial_flag) {
  if(!memory || !event || !registers || !receipt.empty())return false;
  const unsigned volume=initial_flag?15:14, loaded=initial_flag?1:0;
  // Synthetic music settings/descriptors are explicit diagnostic inputs on the
  // real caller's retained RAM. They do not establish a retail audio resource.
  word(*memory,0x80021d7fu,1,true,volume);word(*memory,0x800b1f38u,1,true,loaded);
  word(*memory,0x800b1f39u,1,true,0);word(*memory,0x80021d6cu,4,true,0x80150000u);
  word(*memory,0x800b1f34u,4,true,0x80160000u);
  Children children; Nba97GamePeriodMusicStartFirstPeriodBinding b{};
  b.operation_budget=32;b.io=Children::call;b.user=&children;
  if(nba97_game_period_music_start_from_first_period(&b,memory,event,registers)!=1)return false;
  const auto& p=b.progress;
  const std::vector<std::uint32_t> expected=loaded?std::vector<std::uint32_t>{0,0,127,120}:std::vector<std::uint32_t>{0x80150000,0x80160000,0,0,126,120};
  if(!p.completed || children.args!=expected || word(*memory,0x800b1f38u,1)!=1 || word(*memory,0x800b1f39u,1)!=1)throw std::runtime_error("music capture state drifted");
  std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x800295D0\",\"inclusive_end\":\"0x8002968B\",\"bytes\":188,\"instructions\":47,\"classification\":\"no direct visual effect\",\"scope\":\"actual first-period caller and music owner on same synthetic retained memory; explicit settings and typed audio services, no advancing match\",\"completed\":true,\"same_parent_memory\":true,\"call_pc\":"<<event->pc<<",\"volume_fixture\":"<<volume<<",\"loaded_before\":"<<loaded<<",\"loaded_after\":1,\"playing_after\":1,\"scaled_volume\":"<<p.scaled_volume.word<<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"callbacks\":"<<p.callbacks_completed<<",\"load_executed\":"<<unsigned(p.load_music_executed)<<",\"return_address\":"<<p.machine.registers.gpr[31].word<<",\"sp\":"<<p.machine.registers.gpr[29].word<<",\"hilo_known_masks\":["<<unsigned(p.machine.hi.known_mask)<<','<<unsigned(p.machine.lo.known_mask)<<"],\"call_pcs\":[";
  for(std::size_t i=0;i<children.pcs.size();++i){if(i)o<<',';o<<children.pcs[i];}o<<"],\"arguments\":[";
  for(std::size_t i=0;i<children.args.size();++i){if(i)o<<',';o<<children.args[i];}o<<"]}";
  receipt=o.str();return true;
}
}
