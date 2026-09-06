#include "game_gpu_control_command_adapter.h"

#include <array>
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
    std::fprintf(stderr, "GPU control integration check %u failed at line %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) checkAt((value), __LINE__)

struct Fixture {
  static constexpr U32 Base = 0x80000000u;
  static constexpr std::size_t Size = 0x120000u;
  static constexpr U32 Env = 0x80022000u;
  static constexpr U32 Stack = 0x8010ff00u;
  static constexpr U32 Port = 0x1f801814u;
  std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(Size, 0xcd);
  std::vector<std::uint8_t> ramKnown = std::vector<std::uint8_t>(Size, 1);
  std::array<std::uint8_t, 4> port{};
  std::array<std::uint8_t, 4> portKnown{{1, 1, 1, 1}};
  std::array<Nba97GameTextRegion, 2> regions{};
  std::array<Nba97GameGpuControlCommandAccess, 3> journal{};
  Nba97GameDisplayEnvironmentContext parent{};
  Nba97GameDisplayEnvironmentProgress progress{};
  Nba97GameGpuControlCommandBinding binding{};
  std::vector<Nba97GameDisplayEnvironmentEvent> fallbackEvents;
  U32 videoMode = 0;
  bool rejectFallback = false;

  Fixture() {
    regions = {{{Base, ram.data(), ramKnown.data(), ram.size()},
                {Port, port.data(), portKnown.data(), port.size()}}};
    for (unsigned i = 0; i < 32; ++i)
      parent.machine.registers.gpr[i] = {0x22000000u + i * 0x101u, 15};
    parent.machine.registers.gpr[0] = {0, 15};
    parent.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {Env, 15};
    parent.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Stack, 15};
    parent.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x81234568u, 15};
    parent.machine.hi = {0x89abcdefu, 15};
    parent.machine.lo = {0x76543210u, 15};
    parent.memory = {regions.data(), regions.size()};
    parent.operation_budget = 200;
    parent.io = fallback;
    parent.user = this;
    binding.operation_budget = 3;
    binding.access_journal = journal.data();
    binding.access_journal_capacity = journal.size();
    seed();
  }
  std::size_t at(U32 address) const { return address - Base; }
  void put(U32 address, U32 value, unsigned width = 4) {
    for (unsigned i = 0; i < width; ++i) {
      ram[at(address) + i] = static_cast<std::uint8_t>(value >> (8u * i));
      ramKnown[at(address) + i] = 1;
    }
  }
  U32 get(U32 address, unsigned width = 4) const {
    U32 value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= U32(ram[at(address) + i]) << (8u * i);
    return value;
  }
  U32 getPort() const {
    return U32(port[0]) | (U32(port[1]) << 8u) | (U32(port[2]) << 16u) |
           (U32(port[3]) << 24u);
  }
  void copy(U32 source, U32 destination, unsigned size) {
    std::memmove(ram.data() + at(destination), ram.data() + at(source), size);
    std::memmove(ramKnown.data() + at(destination),
                 ramKnown.data() + at(source), size);
  }
  void seed() {
    put(0x800c55c2u, 0, 1);
    put(0x800c55c0u, 0, 1);
    put(0x800c55c3u, 0, 1);
    put(0x800c55bcu, 0x8009cb2cu);
    put(0x800c55b8u, 0x800c5578u);
    put(0x800c5588u, 0x8009b16cu);
    put(0x800c5694u, Port);
    put(Env + 0, 10, 2);
    put(Env + 2, 20, 2);
    put(Env + 4, 320, 2);
    put(Env + 6, 240, 2);
    put(Env + 8, 0, 2);
    put(Env + 10, 0, 2);
    put(Env + 12, 256, 2);
    put(Env + 14, 240, 2);
    put(Env + 16, 0);
    copy(Env, 0x800c562cu, 20);
  }
  static int fallback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameDisplayEnvironmentEvent *event,
                      Nba97GameDisplayEnvironmentMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    f.fallbackEvents.push_back(*event);
    if (f.rejectFallback)
      return 0;
    if (event->kind == NBA97_GAME_DISPLAY_ENVIRONMENT_VIDEO_MODE)
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {f.videoMode, 15};
    if (event->kind == NBA97_GAME_DISPLAY_ENVIRONMENT_ORIGIN_HELPER)
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {0x456u, 15};
    if (event->kind == NBA97_GAME_DISPLAY_ENVIRONMENT_COPY)
      f.copy(machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].word,
             machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word,
             machine->registers.gpr[NBA97_MATCH_INITIALIZE_A2].word);
    return 1;
  }
  int run() {
    return nba97_game_display_environment_with_gpu_control_command(
        &parent, &binding, &progress);
  }
};

void unchangedOriginCall() {
  Fixture f;
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.binding.invocations == 1 && f.binding.completions == 1);
  check(f.binding.event[0].pc == 0x80099d6cu &&
        f.binding.event[0].delay_slot_pc == 0x80099d70u &&
        f.binding.event[0].entry == 0x8009b16cu &&
        f.binding.event[0].kind == NBA97_GAME_DISPLAY_ENVIRONMENT_GPU_COMMAND &&
        f.binding.event[0].argument_count == 1);
  check(f.binding.progress[0].completed &&
        f.binding.progress[0]
                .machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
                .word == 0x80099d74u &&
        f.getPort() == f.progress.origin_command.word &&
        f.get(0x800d8d99u, 1) == (f.progress.origin_command.word & 0xffu));
  check(f.fallbackEvents.size() == 1 &&
        f.fallbackEvents[0].kind == NBA97_GAME_DISPLAY_ENVIRONMENT_COPY &&
        f.binding.fallback_callbacks_completed == 1);
}

void changedAllFourCalls() {
  Fixture f;
  f.videoMode = 1;
  f.put(0x800c5634u, 0xffffu, 2);
  f.put(0x800c563cu, 0xffffffffu);
  f.put(Fixture::Env + 8, 50, 2);
  f.put(Fixture::Env + 10, 300, 2);
  f.put(Fixture::Env + 12, 0, 2);
  f.put(Fixture::Env + 14, 0, 2);
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.binding.invocations == 4 && f.binding.completions == 4);
  const U32 pcs[] = {0x80099d6cu, 0x80099f78u, 0x80099fa4u, 0x8009a114u};
  for (unsigned i = 0; i < 4; ++i) {
    check(f.binding.event[i].pc == pcs[i] &&
          f.binding.event[i].delay_slot_pc == pcs[i] + 4u &&
          f.binding.event[i].entry == 0x8009b16cu &&
          f.binding.call_count[i] == 1 && f.binding.progress[i].completed);
  }
  check(f.getPort() == f.progress.mode_command.word &&
        f.get(0x800d8d99u, 1) == (f.progress.origin_command.word & 0xffu) &&
        f.get(0x800d8d9au, 1) == (f.progress.horizontal_command.word & 0xffu) &&
        f.get(0x800d8d9bu, 1) == (f.progress.vertical_command.word & 0xffu) &&
        f.get(0x800d8d9cu, 1) == (f.progress.mode_command.word & 0xffu));
  check(f.fallbackEvents.size() == 3 &&
        f.fallbackEvents[0].kind == NBA97_GAME_DISPLAY_ENVIRONMENT_VIDEO_MODE &&
        f.fallbackEvents[1].kind == NBA97_GAME_DISPLAY_ENVIRONMENT_VIDEO_MODE &&
        f.fallbackEvents[2].kind == NBA97_GAME_DISPLAY_ENVIRONMENT_COPY &&
        f.binding.fallback_callbacks_completed == 3);
  check(f.journal[0].pc == 0x8009b170u && f.journal[1].pc == 0x8009b178u &&
        f.journal[2].pc == 0x8009b188u);
}

void leafFailurePrefixAndFallback() {
  Fixture limited;
  limited.binding.operation_budget = 1;
  check(
      limited.run() == NBA97_TEXT_LIMIT &&
      limited.binding.result[0] == NBA97_TEXT_LIMIT &&
      limited.binding.progress[0].stopped_pc == 0x8009b178u &&
      limited.progress.stopped_pc == 0x80099d6cu &&
      limited.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
          Fixture::Port &&
      limited.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
          0x80099d74u &&
      limited.getPort() == 0);

  Fixture fallback;
  fallback.rejectFallback = true;
  check(fallback.run() == NBA97_TEXT_IO_REFUSED &&
        fallback.binding.completions == 1 &&
        fallback.progress.stopped_pc == 0x8009a128u);
}

void exactGuardsAreImmutable() {
  Fixture f;
  Nba97GameDisplayEnvironmentEvent event{};
  event.pc = 0x80099d6cu;
  event.delay_slot_pc = 0x80099d70u;
  event.entry = 0x8009b16cu;
  event.kind = NBA97_GAME_DISPLAY_ENVIRONMENT_GPU_COMMAND;
  event.argument_count = 1;
  auto machine = f.parent.machine;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {0x05000000u, 15};
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80099d74u, 15};
  for (unsigned field = 0; field < 6; ++field) {
    Fixture bad;
    auto changed = event;
    auto invalid = machine;
    if (field == 0)
      changed.pc += 4;
    if (field == 1)
      changed.delay_slot_pc += 4;
    if (field == 2)
      changed.entry += 4;
    if (field == 3)
      changed.kind = NBA97_GAME_DISPLAY_ENVIRONMENT_VIDEO_MODE;
    if (field == 4)
      changed.argument_count = 2;
    if (field == 5)
      invalid.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word += 4;
    const auto before = invalid;
    check(nba97_game_gpu_control_command_from_display_environment(
              &bad.binding, &bad.parent.memory, &changed, &invalid) == 0 &&
          bad.binding.invocations == 0 &&
          std::memcmp(&invalid, &before, sizeof before) == 0);
  }
  Fixture invalidMemory;
  auto invalid = machine;
  const auto before = invalid;
  Nba97GameTextRegion empty = invalidMemory.regions[0];
  empty.size = 0;
  Nba97GameTextMemory memory{&empty, 1};
  check(nba97_game_gpu_control_command_from_display_environment(
            &invalidMemory.binding, &memory, &event, &invalid) == 0 &&
        invalidMemory.binding.invocations == 0 &&
        std::memcmp(&invalid, &before, sizeof before) == 0);
}


struct MaskFixture : Fixture {
  Nba97GameDisplayMaskSetContext mask{};
  Nba97GameDisplayMaskSetProgress maskProgress{};
  Nba97GameGpuControlCommandProgress leaf{};
  size_t budget=3;
  int leafResult=NBA97_TEXT_ARGUMENT;
  MaskFixture(U32 enabled) {
    mask.memory=parent.memory;mask.operation_budget=30;mask.mask=enabled;
    mask.stack_pointer=Stack;mask.return_address=0x81234568u;
    mask.saved_register[0]=0x11223344u;mask.saved_register[1]=0x55667788u;
    mask.io=child;mask.user=this;
  }
  static int child(void* opaque,const Nba97GameTextMemory* memory,
      const Nba97GameDisplayMaskSetEvent* event,Nba97GameDisplayMaskSetValue* value) {
    auto& self=*static_cast<MaskFixture*>(opaque);
    if(event->kind==NBA97_GAME_DISPLAY_MASK_CLEAR_ENVIRONMENTS) {
      check(event->pc==0x800994acu&&event->argument[2]==20);
      for(unsigned i=0;i<20;++i)self.put(event->argument[0]+i,event->argument[1],1);
      return 1;
    }
    self.leafResult=nba97_game_gpu_control_command_from_display_mask(memory,event,self.budget,&self.leaf,value);
    return self.leafResult==NBA97_TEXT_COMPLETE;
  }
  int runMask(){return nba97_game_display_mask_set(&mask,&maskProgress);}
};

void actualDisplayMaskCaller() {
  for(U32 enabled:{0u,1u,0x80000000u,0xffffffffu}) {
    MaskFixture f(enabled);
    check(f.runMask()==NBA97_TEXT_COMPLETE&&f.maskProgress.completed);
    check(f.leaf.completed&&f.leaf.reads==1&&f.leaf.stores==2);
    check(f.getPort()==(enabled?0x03000000u:0x03000001u));
    check(f.get(0x800d8d97u,1)==(enabled?0u:1u)&&f.maskProgress.return_v0==3&&f.maskProgress.return_v0_known);
    check(f.leaf.machine.registers.gpr[29].word==Fixture::Stack-0x20&&f.leaf.machine.registers.gpr[31].word==0x800994dcu);
    check(f.leaf.machine.registers.gpr[16].word==enabled&&f.leaf.machine.registers.gpr[17].word==0x800c55c2u);
    check(!f.leaf.machine.hi.known_mask&&!f.leaf.machine.lo.known_mask&&!f.leaf.machine.registers.gpr[13].known_mask);
    check(f.maskProgress.restored_saved_register[0]==0x11223344u&&f.maskProgress.restored_saved_register[1]==0x55667788u&&f.maskProgress.stack_pointer==Fixture::Stack);
  }
  MaskFixture prefix(1);prefix.budget=1;
  check(prefix.runMask()==NBA97_TEXT_IO_REFUSED&&prefix.leafResult==NBA97_TEXT_LIMIT);
  check(prefix.maskProgress.stopped_pc==0x800994d4u&&prefix.leaf.stopped_pc==0x8009b178u&&prefix.getPort()==0);
  MaskFixture guards(1);Nba97GameDisplayMaskSetEvent event{};
  Nba97GameDisplayMaskSetValue value{0x11223344u,1};
  check(nba97_game_gpu_control_command_from_display_mask(&guards.mask.memory,&event,3,&guards.leaf,&value)==NBA97_TEXT_ARGUMENT);
  check(value.word==0x11223344u&&value.known==1&&guards.getPort()==0);
}

} // namespace

int main() {
  unchangedOriginCall();
  actualDisplayMaskCaller();
  changedAllFourCalls();
  leafFailurePrefixAndFallback();
  exactGuardsAreImmutable();
  std::printf("game GPU control command integration tests passed (%u checks)\n",
              checks);
  return 0;
}
