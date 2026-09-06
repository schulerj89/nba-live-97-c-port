#include "game_display_environment_adapter.h"
#include "game_graphics_submit_capture.h"
#include "game_graphics_submit_adapter.h"
#include "game_draw_area_end_adapter.h"
#include "game_draw_area_start_adapter.h"
#include "game_draw_packet_adapter.h"
#include "game_draw_environment_adapter.h"
#include "game_gpu_control_command_adapter.h"
#include <array>
#include "game_video_mode_adapter.h"
#include <sstream>
#include <stdexcept>
#include "recovered/game_bios_memory_copy.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
using U32 = std::uint32_t;
unsigned checks;
void checkAt(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "display integration check %u failed at line %u\n",
                 checks, line);
    throw std::runtime_error("display environment capture check failed");
  }
}
#define check(value) checkAt((value), __LINE__)

struct Fixture {
  static constexpr U32 Base = 0x80000000u;
  static constexpr std::size_t Size = 0x120000u;
  static constexpr U32 Stack = 0x8010ff00u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size, 0xcd);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97GameSceneStartupContext scene{};
  Nba97GameSceneStartupProgress progress{};
  Nba97GameDisplayEnvironmentSceneBinding binding{};
  std::vector<Nba97GameSceneStartupEvent> fallbackEvents;
  std::vector<Nba97GameDisplayEnvironmentEvent> displayEvents;
  unsigned hiloCalls = 0;
  unsigned malformed = 0;

  Fixture() {
    scene.memory = {&region, 1};
    scene.operation_budget = 10000;
    scene.io = fallback;
    scene.user = this;
    for (unsigned i = 0; i < 32; ++i)
      scene.registers.gpr[i] = {0x11000000u + i, 15};
    scene.registers.gpr[0] = {0, 15};
    scene.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Stack, 15};
    scene.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x81234568u, 15};
    binding.operation_budget = 200;
    binding.hi_lo_provider = provideHiLo;
    binding.hi_lo_user = this;
    binding.io = displayIo;
    binding.user = this;
    seedScene();
    seedDisplay(0x80022070u);
    seedDisplay(0x8002205cu);
    copy(0x80022070u, 0x800c562cu, 20);
  }
  std::size_t at(U32 address) const { return address - Base; }
  void put(U32 address, U32 value, unsigned width = 4) {
    for (unsigned i = 0; i < width; ++i) {
      bytes[at(address) + i] = static_cast<std::uint8_t>(value >> (8u * i));
      known[at(address) + i] = 1;
    }
  }
  void copy(U32 source, U32 destination, unsigned size) {
    std::memmove(bytes.data() + at(destination), bytes.data() + at(source),
                 size);
    std::memmove(known.data() + at(destination), known.data() + at(source),
                 size);
  }
  void seedDisplay(U32 env) {
    put(env + 0, 10, 2);
    put(env + 2, 20, 2);
    put(env + 4, 320, 2);
    put(env + 6, 240, 2);
    put(env + 8, 0, 2);
    put(env + 10, 0, 2);
    put(env + 12, 256, 2);
    put(env + 14, 240, 2);
    put(env + 16, 0);
    put(0x800c55c2u, 0, 1);
    put(0x800c55c0u, 0, 1);
    put(0x800c55c3u, 0, 1);
    put(0x800c55bcu, 0x8009cb2cu);
    put(0x800c55b8u, 0x800c5578u);
    put(0x800c5588u, 0x8009a97cu);
  }
  void seedScene() {
    for (unsigned i = 0; i < 12; ++i) {
      const U32 home = 0x80030000u + i * 4u;
      const U32 away = 0x80030100u + i * 4u;
      put(0x80020b8cu + i * 4u, home);
      put(0x80020bbcu + i * 4u, away);
      put(home, static_cast<std::uint16_t>(-300 + static_cast<int>(i)), 2);
      put(away, static_cast<std::uint16_t>(200 + i), 2);
    }
    put(0x80020becu, 0x80030130u);
    put(0x80030130u, 212, 2);
    put(0x800fc650u, 0x80040000u);
    for (unsigned i = 0; i < 10; ++i) {
      const U32 entity = 0x80041000u + i * 0x40u;
      const U32 roster = 0x80042000u + i * 4u;
      put(0x80040000u + i * 4u, entity);
      put(entity + 0x20u, roster);
      put(roster,
          static_cast<std::uint16_t>(i & 1u ? 1000 + i
                                            : -1000 - static_cast<int>(i)),
          2);
    }
    put(0x800b729cu, 0x800abc00u);
    put(0x8001ede8u, 0);
    put(0x800fa636u, 0x55aau, 2);
  }
  static int fallback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameSceneStartupEvent *event,
                      Nba97GameSceneStartupRegisters *registers) {
    auto &f = *static_cast<Fixture *>(opaque);
    f.fallbackEvents.push_back(*event);
    if (event->kind == NBA97_GAME_SCENE_STARTUP_CONTROLLER_8008F224) {
      const U32 slot = registers->gpr[NBA97_MATCH_INITIALIZE_A0].word;
      registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {(slot & 1u) ? 0u : 0x3e1au,
                                                   15};
    }
    if (event->kind == NBA97_GAME_SCENE_STARTUP_CHILD_80056944)
      registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0xcafebabeu, 15};
    return 1;
  }
  static int provideHiLo(void *opaque, const Nba97GameSceneStartupEvent *,
                         std::size_t invocation,
                         Nba97GameDisplayEnvironmentWord *hi,
                         Nba97GameDisplayEnvironmentWord *lo) {
    auto &f = *static_cast<Fixture *>(opaque);
    ++f.hiloCalls;
    *hi = {0x10000000u + static_cast<U32>(invocation), 15};
    *lo = {0x20000000u + static_cast<U32>(invocation), 15};
    return 1;
  }
  static int displayIo(void *opaque, const Nba97GameTextMemory *,
                       const Nba97GameDisplayEnvironmentEvent *event,
                       Nba97GameDisplayEnvironmentMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    f.displayEvents.push_back(*event);
    if (event->kind == NBA97_GAME_DISPLAY_ENVIRONMENT_COPY)
      f.copy(machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].word,
             machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word,
             machine->registers.gpr[NBA97_MATCH_INITIALIZE_A2].word);
    if (f.malformed == 1)
      machine->registers.gpr[0].known_mask = 0;
    if (f.malformed >= 2) {
      machine->registers.gpr[13] = {0x13579bdfu, 7};
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x8010fe00u, 15};
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x90000004u, 15};
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {0x80023000u, 15};
    }
    if (f.malformed == 2)
      machine->hi.known_mask = 16;
    if (f.malformed == 3)
      machine->lo.known_mask = 16;
    return 1;
  }
  int run() {
    return nba97_game_scene_startup_with_display_environment(&scene, &binding,
                                                             &progress);
  }
};


// The real copy trampoline is shared with speech; BIOS byte transfer remains
// an explicit synthetic host service in this asset-free scene fixture.
struct Composed : Fixture {
  Nba97GameBiosMemoryCopyProgress copyProgress{};
  std::vector<U32> commands;
  unsigned copies=0, videos=0;
  std::size_t copyBudget=1;
  Composed() {
    put(0x800c5588u,0x8009b16cu); // Original retail GP1 dispatch address.
    put(0x8002205cu,100,2);put(0x8002205eu,240,2);
    put(0x80022060u,512,2);put(0x80022062u,289,2);
    put(0x80022064u,20,2);put(0x80022066u,30,2);
    binding.io=child;binding.user=this;
  }
  static int bios(void* opaque,const Nba97GameTextMemory*,const Nba97GameBiosMemoryCopyEvent* e,Nba97GameBiosMemoryCopyMachine* m) {
    auto& self=*static_cast<Composed*>(opaque);
    check(e->pc==0x8009cb10u&&e->delay_slot_pc==0x8009cb14u&&e->entry==0xa0u&&e->service==0x2a);
    self.copy(m->registers.gpr[5].word,m->registers.gpr[4].word,m->registers.gpr[6].word);
    return 1;
  }
  static int child(void* opaque,const Nba97GameTextMemory* memory,const Nba97GameDisplayEnvironmentEvent* e,Nba97GameDisplayEnvironmentMachine* m) {
    auto& self=*static_cast<Composed*>(opaque);self.displayEvents.push_back(*e);
    if(e->kind==NBA97_GAME_DISPLAY_ENVIRONMENT_GPU_COMMAND) {
      check(e->entry==0x8009b16cu);self.commands.push_back(m->registers.gpr[4].word);return 1;
    }
    if(e->kind==NBA97_GAME_DISPLAY_ENVIRONMENT_VIDEO_MODE) {++self.videos;m->registers.gpr[2]={1u,15u};return 1;}
    if(e->kind==NBA97_GAME_DISPLAY_ENVIRONMENT_COPY) {
      check(e->pc==0x8009a128u&&e->delay_slot_pc==0x8009a12cu&&e->entry==0x8009cb0cu&&e->argument_count==3);
      Nba97GameBiosMemoryCopyContext c{};c.memory=*memory;c.operation_budget=self.copyBudget;
      c.machine.registers=m->registers;c.machine.hi=m->hi;c.machine.lo=m->lo;c.io=bios;c.user=&self;
      ++self.copies;const int result=nba97_game_bios_memory_copy(&c,&self.copyProgress);
      m->registers=self.copyProgress.machine.registers;m->hi=self.copyProgress.machine.hi;m->lo=self.copyProgress.machine.lo;
      return result==NBA97_TEXT_COMPLETE?1:0;
    }
    return 0;
  }
};



struct VideoComposed : Composed {
 Nba97GameVideoModeDisplayBinding videoBinding{};
 VideoComposed() {
   put(0x800c54acu,1u);
   Nba97GameVideoModeCallConfig config[2]={{1,nullptr,0},{1,nullptr,0}};
   nba97_game_video_mode_display_binding_init(&videoBinding,config,Composed::child,this);
   binding.io=nba97_game_video_mode_from_display;binding.user=&videoBinding;
 }
};

struct GpuComposed : VideoComposed {
 std::array<std::uint8_t,4> port{},portKnown{{1,1,1,1}};
 std::array<Nba97GameTextRegion,2> mapped{};
 Nba97GameGpuControlCommandBinding gpu{};
 std::vector<Nba97GameGpuControlCommandProgress> leaves;
 GpuComposed() {
   mapped={region,Nba97GameTextRegion{0x1f801814u,port.data(),portKnown.data(),4}};
   scene.memory={mapped.data(),mapped.size()};put(0x800c5694u,0x1f801814u);
   gpu.operation_budget=3;binding.io=dispatch;binding.user=this;
 }
 static int dispatch(void* opaque,const Nba97GameTextMemory* memory,const Nba97GameDisplayEnvironmentEvent* e,Nba97GameDisplayEnvironmentMachine* m) {
   auto& self=*static_cast<GpuComposed*>(opaque);
   if(e->kind!=NBA97_GAME_DISPLAY_ENVIRONMENT_GPU_COMMAND)
     return nba97_game_video_mode_from_display(&self.videoBinding,memory,e,m);
   self.commands.push_back(m->registers.gpr[4].word);
   const int accepted=nba97_game_gpu_control_command_from_display_environment(&self.gpu,memory,e,m);
   unsigned index=e->pc==0x80099d6cu?0:e->pc==0x80099f78u?1:e->pc==0x80099fa4u?2:3;
   self.leaves.push_back(self.gpu.progress[index]);return accepted;
 }
};

struct DrawComposed : GpuComposed {
 Nba97GameDrawEnvironmentSceneBinding draw{};
 std::vector<Nba97GameDrawEnvironmentProgress> draws;
 std::vector<Nba97GameSceneStartupEvent> drawEvents;
 unsigned packets=0,submits=0,drawCopies=0;
 DrawComposed() {
   put(0x800c5580u,0x8009b298u);put(0x800c5590u,0x8009b1f8u);
   put(0x80021eecu+0x1c,0x12000000u);put(0x80021f48u+0x1c,0x34000000u);
   nba97_game_draw_environment_scene_binding_init(&draw,100,child,this,hiLo,this,nullptr,0,Fixture::fallback,this);
   scene.io=dispatchDraw;scene.user=this;
 }
 static int hiLo(void*,const Nba97GameSceneStartupEvent* e,Nba97GameDrawEnvironmentWord* hi,Nba97GameDrawEnvironmentWord* lo) {
   *hi={e->pc,15};*lo={e->pc+4,15};return 1;
 }
 static int dispatchDraw(void* opaque,const Nba97GameTextMemory* memory,const Nba97GameSceneStartupEvent* e,Nba97GameSceneStartupRegisters* registers) {
   auto& self=*static_cast<DrawComposed*>(opaque);
   const int accepted=nba97_game_draw_environment_from_scene(&self.draw,memory,e,registers);
   if(e->kind==NBA97_GAME_SCENE_STARTUP_DRAW_80099ACC){self.draws.push_back(self.draw.progress);self.drawEvents.push_back(*e);}
   return accepted;
 }
 static int child(void* opaque,const Nba97GameTextMemory* memory,const Nba97GameDrawEnvironmentEvent* e,Nba97GameDrawEnvironmentMachine* m) {
   auto& self=*static_cast<DrawComposed*>(opaque);
   if(e->kind==NBA97_GAME_DRAW_ENVIRONMENT_PACKET_8009A344) {
     check(e->pc==0x80099b20u&&e->argument_count==2);++self.packets;return 1;
   }
   if(e->kind==NBA97_GAME_DRAW_ENVIRONMENT_SUBMIT_INDIRECT) {
     check(e->pc==0x80099b58u&&e->entry==0x8009b298u&&m->registers.gpr[4].word==0x8009b1f8u&&m->registers.gpr[6].word==0x40&&m->registers.gpr[7].word==0);++self.submits;return 1;
   }
   if(e->kind==NBA97_GAME_DRAW_ENVIRONMENT_COPY_8009CB0C) {
     check(e->pc==0x80099b68u&&m->registers.gpr[6].word==0x5c);
     Nba97GameBiosMemoryCopyContext c{};c.memory=*memory;c.operation_budget=1;c.machine=*m;c.io=Composed::bios;c.user=&self;
     ++self.copies;++self.drawCopies;const int result=nba97_game_bios_memory_copy(&c,&self.copyProgress);
     *m=self.copyProgress.machine;return result==NBA97_TEXT_COMPLETE;
   }
   return 0;
 }
};

struct PacketComposed : DrawComposed {
 Nba97GameDrawPacketEnvironmentBinding packet{};
 std::vector<Nba97GameDrawPacketProgress> packetRuns;
 PacketComposed() {
   put(0x800c55c4u,640,2);put(0x800c55c6u,480,2);
   for(U32 e:{0x80021eecu,0x80021f48u}) {
     put(e,64,2);put(e+2,32,2);put(e+4,64,2);put(e+6,64,2);
     put(e+8,2,2);put(e+10,3,2);put(e+0x18,e==0x80021eecu?1:0,1);
     put(e+0x19,0x11,1);put(e+0x1a,0x22,1);put(e+0x1b,0x33,1);
   }
   nba97_game_draw_packet_environment_binding_init(&packet,256,wordChild,this,nullptr,0,DrawComposed::child,this);
   draw.io=packetDispatch;draw.user=this;
 }
 static int wordChild(void*,const Nba97GameTextMemory*,const Nba97GameDrawPacketEvent* e,Nba97GameDrawPacketMachine* m) {
   m->registers.gpr[2]={0x90000000u+e->kind,15};return 1;
 }
 static int packetDispatch(void* opaque,const Nba97GameTextMemory* memory,const Nba97GameDrawEnvironmentEvent* e,Nba97GameDrawEnvironmentMachine* m) {
   auto& self=*static_cast<PacketComposed*>(opaque);
   const int accepted=nba97_game_draw_packet_from_draw_environment(&self.packet,memory,e,m);
   if(e->kind==NBA97_GAME_DRAW_ENVIRONMENT_PACKET_8009A344)self.packetRuns.push_back(self.packet.progress);
   return accepted;
 }
 U32 get(U32 a) const {U32 v=0;for(unsigned b=0;b<4;++b)v|=U32(bytes[at(a+b)])<<(8*b);return v;}
};

struct AreaStartComposed : PacketComposed {
 Nba97GameDrawAreaStartPacketBinding area{};
 std::vector<Nba97GameDrawAreaStartProgress> areas;
 AreaStartComposed() {
   put(0x800c55c0u,0,1);
   nba97_game_draw_area_start_packet_binding_init(&area,3,nullptr,0,PacketComposed::wordChild,this);
   packet.io=areaDispatch;packet.user=this;
 }
 static int areaDispatch(void* opaque,const Nba97GameTextMemory* memory,const Nba97GameDrawPacketEvent* e,Nba97GameDrawPacketMachine* m) {
   auto& self=*static_cast<AreaStartComposed*>(opaque);
   const int accepted=nba97_game_draw_area_start_from_packet(&self.area,memory,e,m);
   if(e->kind==NBA97_GAME_DRAW_PACKET_CHILD_8009A644)self.areas.push_back(self.area.progress);
   return accepted;
 }
};

struct AreaEndComposed : AreaStartComposed {
 Nba97GameDrawAreaEndPacketBinding area{};
 std::vector<Nba97GameDrawAreaEndProgress> areas;
 AreaEndComposed() {
   put(0x800c55c0u,0,1);
   nba97_game_draw_area_end_packet_binding_init(&area,3,nullptr,0,PacketComposed::wordChild,this);
   AreaStartComposed::area.fallback=areaDispatch;AreaStartComposed::area.fallback_user=this;
 }
 static int areaDispatch(void* opaque,const Nba97GameTextMemory* memory,const Nba97GameDrawPacketEvent* e,Nba97GameDrawPacketMachine* m) {
   auto& self=*static_cast<AreaEndComposed*>(opaque);
   const int accepted=nba97_game_draw_area_end_from_packet(&self.area,memory,e,m);
   if(e->kind==NBA97_GAME_DRAW_PACKET_CHILD_8009A710)self.areas.push_back(self.area.progress);
   return accepted;
 }
};

struct SubmitComposed : AreaEndComposed {
 Nba97GameGraphicsSubmitBinding submit{};
 std::vector<Nba97GameGraphicsSubmitProgress> submissions;
 std::vector<Nba97GameGraphicsSubmitEvent> services;
 std::vector<U32> submittedPackets;
 bool queueMode;
 explicit SubmitComposed(bool queue) : queueMode(queue) {
   put(0x800c55c1u,queue?1:0,1);put(0x800c56c4u,1);put(0x800c56c8u,0);
   put(0x800c56a0u,0x80050000u);put(0x80050000u,0);put(0x800c55ccu,0);
   submit.operation_budget=300;submit.io=service;submit.user=this;
   packet.fallback=submitDispatch;packet.fallback_user=this;
 }
 static int submitDispatch(void* opaque,const Nba97GameTextMemory* memory,const Nba97GameDrawEnvironmentEvent* e,Nba97GameDrawEnvironmentMachine* m) {
   auto& self=*static_cast<SubmitComposed*>(opaque);
   if(e->kind!=NBA97_GAME_DRAW_ENVIRONMENT_SUBMIT_INDIRECT)return DrawComposed::child(&self,memory,e,m);
   // Explicit synthetic GPU read status at this device boundary. GP1 writes
   // above are commands; their raw stored value is not hardware read status.
   self.port={0,0,0,4};self.submittedPackets.push_back(m->registers.gpr[5].word);
   const int accepted=nba97_game_graphics_submit_from_draw_environment(&self.submit,memory,e,m);
   self.submissions.push_back(self.submit.progress);
   if(accepted)++self.submits;
   return accepted;
 }
 static int service(void* opaque,const Nba97GameTextMemory*,const Nba97GameGraphicsSubmitEvent* e,Nba97GameGraphicsSubmitMachine* m) {
   auto& self=*static_cast<SubmitComposed*>(opaque);self.services.push_back(*e);
   if(e->kind==NBA97_GAME_GRAPHICS_SUBMIT_CRITICAL&&e->pc==0x8009b304u)m->registers.gpr[2]={0x55,15};
   if(e->kind==NBA97_GAME_GRAPHICS_SUBMIT_INDIRECT)
     check(e->pc==0x8009b3a8u&&e->entry==0x8009b1f8u&&m->registers.gpr[5].word==0);
   return 1;
 }
};
} // namespace
namespace nba97 {
std::string captureGameGraphicsSubmit() {
 std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x8009B298\",\"inclusive_end\":\"0x8009B57B\",\"bytes\":740,\"instructions\":185,"
  "\"classification\":\"no direct visual effect\",\"scope\":\"actual scene/display/draw/packet/draw-area-start/end/video/GPU-command/BIOS-trampoline/graphics-submit owners; three synthetic packet helpers, scheduler/critical services, GPU read status and packet consumption\",\"scenarios\":[";
 for(unsigned scenario=0;scenario<2;++scenario) {
   SubmitComposed c(scenario!=0);
   if(c.run()!=NBA97_TEXT_COMPLETE||!c.progress.completed||c.submissions.size()!=2||c.packetRuns.size()!=2||c.copies!=4||c.submits!=2)
     throw std::runtime_error("graphics submission scene composition failed");
   const bool cacheEqual=std::memcmp(c.bytes.data()+c.at(0x800c55d0u),c.bytes.data()+c.at(0x80021eecu),0x5c)==0;check(cacheEqual);
   if(scenario)for(unsigned i=0;i<2;++i) {
     const U32 q=0x80104748u+(i+1)*96;
     check(c.get(q)==0x8009b1f8u&&c.get(q+4)==q+12&&c.get(q+8)==0);
     check(std::memcmp(c.bytes.data()+c.at(q+12),c.bytes.data()+c.at(c.submittedPackets[i]),64)==0);
   }
   else check(c.get(0x800c56b4u)==0x8009b1f8u&&c.get(0x800c56b8u)==0x80021f08u&&c.get(0x800c56bcu)==0);
   if(scenario)o<<',';
   o<<"{\"queued\":"<<(scenario?"true":"false")<<",\"completed\":true,\"parent_completed\":true,\"submit_calls\":"<<c.submits<<",\"copy_calls\":"<<c.copies
     <<",\"service_calls\":"<<c.services.size()<<",\"cache_matches_last_environment\":"<<(cacheEqual?"true":"false")<<",\"head_after\":"<<c.get(0x800c56c4u)<<",\"tail_after\":"<<c.get(0x800c56c8u)<<",\"submissions\":[";
   for(size_t i=0;i<c.submissions.size();++i){if(i)o<<',';const auto&q=c.submissions[i];
     o<<"{\"operations\":"<<q.operations<<",\"reads\":"<<q.reads<<",\"stores\":"<<q.stores<<",\"callbacks\":"<<q.callbacks_completed<<",\"copied_words\":"<<q.copy_iterations<<",\"return_v0\":"<<q.machine.registers.gpr[2].word<<",\"return_address\":"<<q.machine.registers.gpr[31].word<<",\"hi\":"<<q.machine.hi.word<<"}";}
   o<<"]}";
 }
 o<<"]}";return o.str();
}
}
