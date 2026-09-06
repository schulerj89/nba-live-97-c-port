#include "game_pregame_match_card_capture.h"
#include "game_audio_stream_status_adapter.h"
#include "recovered/game_clock_read.h"
#include "recovered/game_stream_readiness.h"
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
      else if(r.known&&r.known[offset+b]!=1)throw std::runtime_error("pregame card capture unknown byte");
      v|=std::uint32_t(r.data[offset+b])<<(8*b);
    }return v;
  }throw std::runtime_error("pregame card capture unmapped byte");
}
struct Children {
  std::vector<std::uint32_t> pcs;
  unsigned clocks=0,pumps=0,ready=0,layouts=0,texts=0;
  static int call(void*u,const Nba97GameTextMemory* memory,const Nba97GamePregameMatchCardEvent* e,Nba97GamePregameMatchCardMachine* m){
    auto&self=*static_cast<Children*>(u);self.pcs.push_back(e->pc);
    if(e->entry==0x800a5810u){
      Nba97GameClockReadContext c{};Nba97GameClockReadProgress p{};
      c.memory=*memory;c.operation_budget=1;c.machine=*m;
      const int result=nba97_game_clock_read(&c,&p);*m=p.machine;
      if(result!=NBA97_TEXT_COMPLETE||p.return_v0.word!=100)return 0;
      ++self.clocks;return 1;
    }
    if(e->entry==0x80088d0cu){
      Nba97GameStreamReadinessContext c{};Nba97GameStreamReadinessProgress p{};
      c.memory=*memory;c.operation_budget=16;c.machine=*m;
      const int result=nba97_game_stream_readiness(&c,&p);*m=p.machine;
      if(result!=NBA97_TEXT_COMPLETE||p.callbacks_completed||m->registers.gpr[2].word)return 0;
      ++self.ready;return 1;
    }
    if(e->entry==0x80083eecu){
      Nba97GameAudioStreamPumpContext c{};Nba97GameAudioStreamPumpProgress p{};
      Nba97GameAudioStreamStatusContext s{};Nba97GameAudioStreamStatusAdapterProgress a{};
      c.memory=*memory;c.operation_budget=24;c.registers=m->registers;s.operation_budget=8;
      const int result=nba97_game_audio_stream_pump_with_stream_status(&c,&s,&p,&a);
      m->registers=p.registers;
      // The older GPR-only owners establish no HI/LO return contract.
      m->hi.known_mask=0;m->lo.known_mask=0;
      if(result!=NBA97_TEXT_COMPLETE||a.status_completions!=1||a.status.returned_value.word!=0xfffffff2u||p.returned_value.word!=0)return 0;
      ++self.pumps;return 1;
    }
    if(e->entry==0x80031614u){
      if(e->argument_count!=8)return 0;
      for(unsigned i=0;i<4;++i)(void)word(*memory,m->registers.gpr[29].word+0x10u+4u*i,4);
      ++self.layouts;
    }
    if(e->entry==0x80030d18u){
      if(e->argument_count!=5)return 0;
      (void)word(*memory,m->registers.gpr[29].word+0x10u,4);++self.texts;
    }
    // Explicit typed UI/input responses. They do not render text or a frame.
    if(e->entry==0x800363dcu){word(*memory,m->registers.gpr[4].word,2,true,7);word(*memory,m->registers.gpr[5].word,2,true,9);}
    m->registers.gpr[2]={e->entry==0x80036478u?0x180u:e->entry,15};return 1;
  }
};
}
bool GamePregameMatchCardCapture::dispatch(const Nba97GameTextMemory* memory,const Nba97GamePeriodPresentationFinishEvent* event,Nba97GamePeriodPresentationFinishMachine* machine){
  if(!memory||!event||!machine||receipt!="null")return false;
  // Explicit synthetic resources on the actual caller's retained memory.
  // The pointer values name mapped guest fixtures, never native addresses.
  word(*memory,0x800b2048u,4,true,0x80030000u);
  word(*memory,0x8001ef24u,4,true,0x80040000u);word(*memory,0x8001ee60u,4,true,0x80041000u);
  word(*memory,0x80040040u,4,true,0x80060000u);word(*memory,0x80041040u,4,true,0x80061000u);word(*memory,0x8004104cu,4,true,0x80062000u);
  word(*memory,0x8001ec94u,4,true,0);word(*memory,0x8001edecu,2,true,0);
  word(*memory,0x800d7a70u,4,true,100);word(*memory,0x800f0fdcu,2,true,0);word(*memory,0x800c43b0u,1,true,0);
  Children children;Nba97GamePregameMatchCardBinding b{};b.operation_budget=256;b.io=Children::call;b.user=&children;
  if(nba97_game_pregame_match_card_from_period_presentation_finish(&b,memory,event,machine)!=1)throw std::runtime_error("pregame match card capture stopped at "+std::to_string(b.progress.stopped_pc));
  const auto&p=b.progress;
  if(!p.completed||!p.exited_for_input||p.exited_for_timeout||p.polling_iterations!=1||children.clocks!=2||children.pumps!=1||children.ready!=1||children.layouts!=7||children.texts!=8||word(*memory,0x80030026u,2)!=0||word(*memory,0x800eb680u,1)!=0||word(*memory,0x800fdb78u,1)!=0)throw std::runtime_error("pregame match card capture drifted");
  std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x80044550\",\"inclusive_end\":\"0x80044997\",\"bytes\":1096,\"instructions\":274,\"classification\":\"BLOCKED\",\"scope\":\"actual presentation caller on same synthetic retained RAM; UI and input services are explicit fixtures; actual clock, readiness and stream pump/status owners; no rendered card or advancing match\",\"missing_visual_dependencies\":[\"0x80031614\",\"0x80030D18\",\"0x80049018\"],\"completed\":true,\"same_parent_memory\":true,\"call_pc\":"<<event->pc<<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"callbacks\":"<<p.callbacks_completed<<",\"polls\":"<<p.polling_iterations<<",\"layouts\":"<<children.layouts<<",\"texts\":"<<children.texts<<",\"clock_reads\":"<<children.clocks<<",\"stream_pumps\":"<<children.pumps<<",\"readiness_checks\":"<<children.ready<<",\"clock\":100,\"input_fixture\":384,\"font_mode_after\":0,\"skip_after\":0,\"return_address\":"<<p.machine.registers.gpr[31].word<<",\"sp\":"<<p.machine.registers.gpr[29].word<<",\"hilo_known_masks\":["<<unsigned(p.machine.hi.known_mask)<<','<<unsigned(p.machine.lo.known_mask)<<"],\"call_pcs\":[";
  for(std::size_t i=0;i<children.pcs.size();++i){if(i)o<<',';o<<children.pcs[i];}o<<"]}";receipt=o.str();return true;
}
}
