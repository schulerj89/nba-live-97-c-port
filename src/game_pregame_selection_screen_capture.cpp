#include "game_pregame_selection_screen_capture.h"
#include "game_audio_stream_status_adapter.h"
#include "recovered/game_clock_read.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace nba97 { namespace {
std::uint32_t word(const Nba97GameTextMemory& m,std::uint32_t a,unsigned width,bool store=false,std::uint32_t value=0) {
  for(std::size_t i=0;i<m.count;++i){const auto&r=m.region[i];const auto offset=std::uint64_t(a)-r.base;
    if(a<r.base || offset>r.size || width>r.size-offset)continue;
    std::uint32_t v=0;for(unsigned b=0;b<width;++b){
      if(store){r.data[offset+b]=std::uint8_t(value>>(8*b));if(r.known)r.known[offset+b]=1;}
      else if(r.known&&r.known[offset+b]!=1)throw std::runtime_error("selection capture unknown byte");
      v|=std::uint32_t(r.data[offset+b])<<(8*b);
    }return v;
  }throw std::runtime_error("selection capture unmapped byte");
}
struct Children {
  std::vector<std::uint32_t> pcs,selection_pairs,inputs;
  unsigned clocks=0,pumps=0,menus=0;
  static int call(void*u,const Nba97GameTextMemory* memory,const Nba97GamePregameSelectionScreenEvent* e,Nba97GamePregameSelectionScreenMachine* m){
    auto&self=*static_cast<Children*>(u);self.pcs.push_back(e->pc);
    if(e->entry==0x800a5810u){
      Nba97GameClockReadContext c{};Nba97GameClockReadProgress p{};
      c.memory=*memory;c.operation_budget=1;c.machine=*m;
      const int result=nba97_game_clock_read(&c,&p);*m=p.machine;
      if(result!=NBA97_TEXT_COMPLETE||p.return_v0.word!=100)return 0;
      ++self.clocks;return 1;
    }
    if(e->entry==0x80083eecu){
      Nba97GameAudioStreamPumpContext c{};Nba97GameAudioStreamPumpProgress p{};
      Nba97GameAudioStreamStatusContext s{};Nba97GameAudioStreamStatusAdapterProgress a{};
      c.memory=*memory;c.operation_budget=24;c.registers=m->registers;s.operation_budget=8;
      const int result=nba97_game_audio_stream_pump_with_stream_status(&c,&s,&p,&a);
      m->registers=p.registers;m->hi.known_mask=0;m->lo.known_mask=0;
      if(result!=NBA97_TEXT_COMPLETE||a.status_completions!=1||p.returned_value.word!=0)return 0;
      ++self.pumps;return 1;
    }
    if(e->entry==0x80046738u){self.selection_pairs.push_back(m->registers.gpr[4].word);self.selection_pairs.push_back(m->registers.gpr[5].word);}
    // Explicit controller/UI dependency responses, without a rendered screen.
    if(e->entry==0x800363dcu){word(*memory,m->registers.gpr[4].word,2,true,7);word(*memory,m->registers.gpr[5].word,2,true,9);}
    if(e->entry==0x80036be4u){
      if(m->registers.gpr[4].word!=0x800b2fd4u||m->registers.gpr[5].word!=1||word(*memory,0x800fdb9cu,2)!=9)return 0;
      ++self.menus;
    }
    auto value=e->entry;
    if(e->entry==0x80036478u){
      static constexpr std::uint32_t sequence[]={4,0x20,0x80};
      if(self.inputs.size()>=3)return 0;
      value=sequence[self.inputs.size()];self.inputs.push_back(value);
    }
    m->registers.gpr[2]={value,15};return 1;
  }
};
}
bool GamePregameSelectionScreenCapture::dispatch(const Nba97GameTextMemory* memory,const Nba97GamePeriodPresentationFinishEvent* event,Nba97GamePeriodPresentationFinishMachine* machine){
  if(!memory||!event||!machine||receipt!="null")return false;
  // Retain the preceding actual card's clock, demo and stream state.
  if(word(*memory,0x800d7a70u,4)!=100||word(*memory,0x8001edecu,2)!=0||word(*memory,0x800c43b0u,1)!=0)return false;
  word(*memory,0x800fdb9cu,2,true,0x1234u);
  Children children;Nba97GamePregameSelectionScreenPresentationBinding b{};
  nba97_game_pregame_selection_screen_presentation_binding_init(&b,256,Children::call,&children,nullptr,0,nullptr,nullptr);
  if(nba97_game_pregame_selection_screen_from_presentation_finish(&b,memory,event,machine)!=1)throw std::runtime_error("pregame selection capture stopped at "+std::to_string(b.progress.stopped_pc));
  const auto&p=b.progress;
  if(!p.completed||p.redraws!=2||p.polls!=3||children.clocks!=2||children.pumps!=3||children.menus!=1||children.selection_pairs!=std::vector<std::uint32_t>{0,12,1,13}||word(*memory,0x800fdb9cu,2)!=0x1234u||word(*memory,0x800fdb78u,1)!=0)throw std::runtime_error("pregame selection capture drifted");
  std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x80046C2C\",\"inclusive_end\":\"0x80046F67\",\"bytes\":828,\"instructions\":207,\"classification\":\"BLOCKED\",\"scope\":\"actual presentation caller after actual pregame card on same synthetic retained RAM; actual clock and stream pump/status owners; typed input, menu and rendering dependencies; no advancing match\",\"missing_visual_dependencies\":[\"0x80046738\",\"0x80049018\",\"0x80036BE4\"],\"completed\":true,\"same_parent_memory\":true,\"call_pc\":"<<event->pc<<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"callbacks\":"<<p.callbacks_completed<<",\"polls\":"<<p.polls<<",\"redraws\":"<<p.redraws<<",\"clock_reads\":"<<children.clocks<<",\"stream_pumps\":"<<children.pumps<<",\"menu_calls\":"<<children.menus<<",\"clock\":100,\"input_fixture\":[4,32,128],\"selection_pairs\":[0,12,1,13],\"controller_after\":4660,\"skip_after\":0,\"return_address\":"<<p.machine.registers.gpr[31].word<<",\"sp\":"<<p.machine.registers.gpr[29].word<<",\"returned_value\":"<<p.machine.registers.gpr[2].word<<",\"hilo_known_masks\":["<<unsigned(p.machine.hi.known_mask)<<','<<unsigned(p.machine.lo.known_mask)<<"],\"call_pcs\":[";
  for(std::size_t i=0;i<children.pcs.size();++i){if(i)o<<',';o<<children.pcs[i];}o<<"]}";receipt=o.str();return true;
}
}
