#include "game_display_environment_adapter.h"
#include "game_display_environment_capture.h"
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


} // namespace
namespace nba97 {
std::string captureGameDisplayEnvironment() {
 Composed c;
 if(c.run()!=NBA97_TEXT_COMPLETE||!c.progress.completed||!c.binding.progress[1].completed)
   throw std::runtime_error("display environment scene composition failed");
 std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x80099CA4\",\"inclusive_end\":\"0x8009A153\",\"bytes\":1200,\"instructions\":300,"
  "\"classification\":\"no direct visual effect\",\"scope\":\"actual scene startup and BIOS copy trampoline; synthetic GPU/video/BIOS services and explicit HI/LO provider\","
  "\"completed\":true,\"parent_completed\":true,\"display_calls\":"<<c.binding.completions<<",\"copy_calls\":"<<c.copies<<",\"video_calls\":"<<c.videos<<",\"commands\":[";
 for(size_t i=0;i<c.commands.size();++i){if(i)o<<',';o<<c.commands[i];}
 o<<"],\"invocations\":[";
 for(unsigned i=0;i<2;++i){if(i)o<<',';const auto&q=c.binding.progress[i];
  o<<"{\"call_pc\":"<<c.binding.event[i].pc<<",\"operations\":"<<q.operations<<",\"reads\":"<<q.reads<<",\"stores\":"<<q.stores<<",\"callbacks\":"<<q.callbacks_completed
   <<",\"screen_changed\":"<<unsigned(q.screen_rectangle_changed)<<",\"mode_changed\":"<<unsigned(q.mode_changed)<<",\"return_v0\":"<<q.return_v0.word
   <<",\"returned_sp\":"<<q.machine.registers.gpr[29].word<<",\"return_address\":"<<q.machine.registers.gpr[31].word<<"}";}
 const bool equal=std::memcmp(c.bytes.data()+c.at(0x800c562cu),c.bytes.data()+c.at(0x8002205cu),20)==0;
 o<<"],\"cache_matches_final_environment\":"<<(equal?"true":"false")<<",\"copy_t1\":"<<c.copyProgress.machine.registers.gpr[9].word<<",\"copy_t2\":"<<c.copyProgress.machine.registers.gpr[10].word<<"}";
 return o.str();
}
}
