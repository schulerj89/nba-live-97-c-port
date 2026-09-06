#include "game_draw_environment_adapter.h"
#include "recovered/game_bios_memory_copy.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

unsigned checks = 0u;
void check(bool condition, const char *expression, int line) {
  ++checks;
  if (condition)
    return;
  std::fprintf(stderr, "check failed at line %d: %s\n", line, expression);
  std::exit(1);
}
#define CHECK(expression) check((expression), #expression, __LINE__)

constexpr uint32_t kBase = UINT32_C(0x80000000);
constexpr uint32_t kStack = UINT32_C(0x8010ff00);
constexpr uint32_t kSelector = UINT32_C(0x8001ede8);
constexpr uint32_t kDispatch = UINT32_C(0x80030000);
constexpr uint32_t kSubmit = UINT32_C(0x80050000);

void set_word(Nba97GameDrawEnvironmentWord &word, uint32_t value,
              uint8_t mask = 15u) {
  word.word = value;
  word.known_mask = mask;
}

struct Natural {
  std::vector<uint8_t> ram = std::vector<uint8_t>(0x120000u, 0u);
  std::vector<uint8_t> known = std::vector<uint8_t>(0x120000u, 1u);
  Nba97GameTextRegion region{};
  Nba97GameSceneStartupContext scene{};
  Nba97GameSceneStartupProgress scene_progress{};
  Nba97GameDrawEnvironmentSceneBinding binding{};
  std::array<Nba97GameDrawEnvironmentAccess, 32> draw_journal{};
  std::vector<Nba97GameDrawEnvironmentEvent> draw_calls;
  std::vector<Nba97GameDrawEnvironmentMachine> draw_machines;
  size_t draw_boundaries = 0u;
  size_t hi_lo_calls = 0u;
  size_t fallback_calls = 0u;
  size_t submits = 0u;
  size_t copies = 0u;
  size_t copy_budget = 1u;
  Nba97GameBiosMemoryCopyProgress copy_progress{};

  Natural();
  size_t offset(uint32_t address) const { return address - kBase; }
  void put(uint32_t address, uint32_t value, unsigned width = 4u) {
    for (unsigned byte = 0u; byte != width; ++byte) {
      ram[offset(address) + byte] = static_cast<uint8_t>(value >> (byte * 8u));
      known[offset(address) + byte] = 1u;
    }
  }
  uint32_t get(uint32_t address) const {
    uint32_t value = 0u;
    for (unsigned byte = 0u; byte != 4u; ++byte)
      value |= static_cast<uint32_t>(ram[offset(address) + byte])
               << (byte * 8u);
    return value;
  }
};


int bios_copy(void* opaque,const Nba97GameTextMemory*,const Nba97GameBiosMemoryCopyEvent* event,Nba97GameBiosMemoryCopyMachine* machine) {
  auto& natural=*static_cast<Natural*>(opaque);
  CHECK(event->pc==0x8009cb10u&&event->delay_slot_pc==0x8009cb14u&&event->entry==0xa0u&&event->service==0x2a);
  const auto destination=machine->registers.gpr[4].word;
  const auto source=machine->registers.gpr[5].word;
  const auto size=machine->registers.gpr[6].word;
  CHECK(size==0x5c);
  std::memmove(natural.ram.data()+natural.offset(destination),natural.ram.data()+natural.offset(source),size);
  std::memmove(natural.known.data()+natural.offset(destination),natural.known.data()+natural.offset(source),size);
  set_word(machine->registers.gpr[2],0xdeadc0deu);
  return 1;
}

int draw_io(void *opaque, const Nba97GameTextMemory *,
            const Nba97GameDrawEnvironmentEvent *event,
            Nba97GameDrawEnvironmentMachine *machine) {
  Natural &natural = *static_cast<Natural *>(opaque);
  natural.draw_calls.push_back(*event);
  natural.draw_machines.push_back(*machine);
  if (event->kind == NBA97_GAME_DRAW_ENVIRONMENT_PACKET_8009A344) {
    const uint32_t packet =
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].word;
    natural.put(packet + 0x1cu,
                natural.get(packet + 0x1cu) & UINT32_C(0xff000000));
  } else if (event->kind == NBA97_GAME_DRAW_ENVIRONMENT_SUBMIT_INDIRECT) {
    ++natural.submits;
  } else if (event->kind == NBA97_GAME_DRAW_ENVIRONMENT_COPY_8009CB0C) {
    ++natural.copies;
    Nba97GameBiosMemoryCopyContext context{};
    context.memory=natural.scene.memory;context.operation_budget=natural.copy_budget;
    context.machine=*machine;context.io=bios_copy;context.user=&natural;
    const int result=nba97_game_bios_memory_copy(&context,&natural.copy_progress);
    *machine=natural.copy_progress.machine;
    return result==NBA97_TEXT_COMPLETE;

  }
  return 1;
}

int hi_lo(void *opaque, const Nba97GameSceneStartupEvent *event,
          Nba97GameDrawEnvironmentWord *hi, Nba97GameDrawEnvironmentWord *lo) {
  Natural &natural = *static_cast<Natural *>(opaque);
  ++natural.hi_lo_calls;
  set_word(*hi, event->pc);
  set_word(*lo, event->pc + 4u);
  return 1;
}

int scene_fallback(void *opaque, const Nba97GameTextMemory *,
                   const Nba97GameSceneStartupEvent *event,
                   Nba97GameSceneStartupRegisters *registers) {
  Natural &natural = *static_cast<Natural *>(opaque);
  ++natural.fallback_calls;
  if (event->kind == NBA97_GAME_SCENE_STARTUP_CONTROLLER_8008F224) {
    const uint32_t slot = registers->gpr[NBA97_MATCH_INITIALIZE_A0].word;
    set_word(registers->gpr[NBA97_MATCH_INITIALIZE_V0],
             (slot & 1u) != 0u ? 0u : UINT32_C(0x3e1a));
  } else if (event->kind == NBA97_GAME_SCENE_STARTUP_CHILD_80056944) {
    set_word(registers->gpr[NBA97_MATCH_INITIALIZE_V0], UINT32_C(0xcafebabe));
  }
  return 1;
}

int scene_io(void *opaque, const Nba97GameTextMemory *memory,
             const Nba97GameSceneStartupEvent *event,
             Nba97GameSceneStartupRegisters *registers) {
  Natural &natural = *static_cast<Natural *>(opaque);
  const int accepted = nba97_game_draw_environment_from_scene(
      &natural.binding, memory, event, registers);
  if (event->kind == NBA97_GAME_SCENE_STARTUP_DRAW_80099ACC)
    ++natural.draw_boundaries;
  return accepted;
}

Natural::Natural() {
  region = {kBase, ram.data(), known.data(), ram.size()};
  scene.memory = {&region, 1u};
  scene.operation_budget = 10000u;
  scene.io = scene_io;
  scene.user = this;
  for (unsigned index = 0u; index != 32u; ++index)
    set_word(scene.registers.gpr[index], UINT32_C(0x11000000) + index);
  set_word(scene.registers.gpr[0], 0u);
  set_word(scene.registers.gpr[NBA97_MATCH_INITIALIZE_SP], kStack);
  set_word(scene.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
           UINT32_C(0x81234568));

  for (unsigned index = 0u; index != 12u; ++index) {
    const uint32_t home = UINT32_C(0x80030000) + index * 4u;
    const uint32_t away = UINT32_C(0x80030100) + index * 4u;
    put(UINT32_C(0x80020b8c) + index * 4u, home);
    put(UINT32_C(0x80020bbc) + index * 4u, away);
    put(home, static_cast<uint16_t>(-300 + static_cast<int>(index)), 2u);
    put(away, 200u + index, 2u);
  }
  put(UINT32_C(0x80020bec), UINT32_C(0x80030130));
  put(UINT32_C(0x80030130), 212u, 2u);
  put(UINT32_C(0x800fc650), UINT32_C(0x80040000));
  for (unsigned index = 0u; index != 10u; ++index) {
    const uint32_t entity = UINT32_C(0x80041000) + index * 0x40u;
    const uint32_t roster = UINT32_C(0x80042000) + index * 4u;
    put(UINT32_C(0x80040000) + index * 4u, entity);
    put(entity + 0x20u, roster);
    put(roster,
        index & 1u ? 1000u + index
                   : static_cast<uint16_t>(-1000 - static_cast<int>(index)),
        2u);
  }
  put(UINT32_C(0x800b729c), UINT32_C(0x800abc00));
  put(kSelector, 0u);
  put(UINT32_C(0x800fa636), UINT32_C(0x55aa), 2u);

  put(UINT32_C(0x800c55b8), kDispatch);
  put(UINT32_C(0x800c55bc), UINT32_C(0x80040000));
  put(UINT32_C(0x800c55c2), 0u, 1u);
  put(kDispatch + 0x18u, UINT32_C(0x87654321));
  put(kDispatch + 8u, kSubmit);
  put(UINT32_C(0x80021eec) + 0x1cu, UINT32_C(0x12000000));
  put(UINT32_C(0x80021f48) + 0x1cu, UINT32_C(0x34000000));

  nba97_game_draw_environment_scene_binding_init(
      &binding, 100u, draw_io, this, hi_lo, this, draw_journal.data(),
      draw_journal.size(), scene_fallback, this);
}

void test_actual_scene_both_sites() {
  Natural natural;
  CHECK(nba97_game_scene_startup(&natural.scene, &natural.scene_progress) ==
        NBA97_TEXT_COMPLETE);
  CHECK(natural.scene_progress.completed == 1u);
  CHECK(natural.draw_boundaries == 2u && natural.binding.invocations == 2u);
  CHECK(natural.hi_lo_calls == 2u);
  CHECK(natural.submits == 2u && natural.copies == 2u);
  CHECK(natural.binding.result == NBA97_TEXT_COMPLETE);
  CHECK(natural.fallback_calls == 17u);
  CHECK(natural.get(UINT32_C(0x80021f48) + 0x1cu) == UINT32_C(0x34ffffff));
  CHECK(natural.get(UINT32_C(0x80021eec) + 0x1cu) == UINT32_C(0x12ffffff));
  CHECK(natural.get(UINT32_C(0x800c55d0) + 0x1cu) == UINT32_C(0x12ffffff));

  std::vector<uint32_t> packet_pcs;
  for (size_t index = 0u; index != natural.draw_calls.size(); ++index) {
    if (natural.draw_calls[index].kind ==
        NBA97_GAME_DRAW_ENVIRONMENT_PACKET_8009A344) {
      packet_pcs.push_back(natural.draw_machines[index].hi.word);
      CHECK(natural.draw_machines[index].hi.known_mask == 15u);
      CHECK(natural.draw_machines[index].lo.word ==
            natural.draw_machines[index].hi.word + 4u);
    }
  }
  CHECK(packet_pcs.size() == 2u);
  CHECK(packet_pcs[0] == UINT32_C(0x80048f4c));
  CHECK(packet_pcs[1] == UINT32_C(0x80048fa0));
}

void test_nested_argument_prefix() {
  Natural natural;
  natural.known[natural.offset(UINT32_C(0x800c55b8))] = 2u;
  CHECK(nba97_game_scene_startup(&natural.scene, &natural.scene_progress) ==
        NBA97_TEXT_IO_REFUSED);
  CHECK(natural.binding.result == NBA97_TEXT_ARGUMENT);
  CHECK(natural.binding.progress.stopped_pc == UINT32_C(0x80099b40));
  CHECK(natural.draw_boundaries == 1u);
}


void test_composed_copy_failure_prefix() {
  Natural limited;limited.copy_budget=0;
  CHECK(nba97_game_scene_startup(&limited.scene,&limited.scene_progress)==NBA97_TEXT_IO_REFUSED);
  CHECK(limited.scene_progress.stopped_pc==0x80048f4cu&&limited.binding.progress.stopped_pc==0x80099b68u);
  CHECK(limited.copy_progress.machine.registers.gpr[9].word==0x2a&&limited.copy_progress.machine.registers.gpr[10].word==0xa0);
  CHECK(limited.submits==1&&limited.copies==1&&limited.binding.progress.callbacks_completed==2);
  CHECK(limited.binding.progress.machine.registers.gpr[31].word==0x80099b70u);
}

} // namespace

int main() {
  test_actual_scene_both_sites();
  test_composed_copy_failure_prefix();
  test_nested_argument_prefix();
  std::printf("game_draw_environment_integration_tests: %u checks passed\n",
              checks);
  return 0;
}
