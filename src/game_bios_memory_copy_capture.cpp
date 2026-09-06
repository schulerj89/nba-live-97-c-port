#include "game_bios_memory_copy_adapter.h"
#include "game_bios_memory_copy_capture.h"
#include <sstream>
#include <stdexcept>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

size_t checks = 0u;
void check(bool condition, const char *expression, int line) {
  ++checks;
  if (condition)
    return;
  std::fprintf(stderr, "check failed at line %d: %s\n", line, expression);
  throw std::runtime_error("BIOS copy capture check failed");
}
#define CHECK(expression) check((expression), #expression, __LINE__)

constexpr uint32_t kRam = UINT32_C(0x80000000);
constexpr uint32_t kStack = UINT32_C(0x807ff000);
constexpr uint32_t kEntrySp = UINT32_C(0x807fff80);
constexpr uint32_t kPayload = UINT32_C(0x80050000);
constexpr uint32_t kPacked = UINT32_C(0x80060000);

void set_word(Nba97GameBiosMemoryCopyWord &word, uint32_t value,
              uint8_t known = 15u) {
  word.word = value;
  word.known_mask = known;
}

struct Natural {
  std::vector<uint8_t> ram = std::vector<uint8_t>(UINT32_C(0x110000), 0u);
  std::vector<uint8_t> ram_known = std::vector<uint8_t>(UINT32_C(0x110000), 1u);
  std::array<uint8_t, 0x1000> stack{};
  std::array<uint8_t, 0x1000> stack_known{};
  Nba97GameTextRegion regions[2]{};
  Nba97GameTextMemory memory{};
  Nba97GameSpeechInitializeContext speech{};
  Nba97GameSpeechInitializeProgress speech_progress{};
  Nba97GameBiosMemoryCopySpeechBinding binding{};
  size_t fallback_calls = 0u;
  size_t bios_calls = 0u;
  bool reject_provider = false;
  unsigned malformed = 0u;
  Nba97GameBiosMemoryCopyWord observed_hi{};
  Nba97GameBiosMemoryCopyWord observed_lo{};

  Natural() {
    stack_known.fill(1u);
    regions[0] = {kRam, ram.data(), ram_known.data(), ram.size()};
    regions[1] = {kStack, stack.data(), stack_known.data(), stack.size()};
    memory = {regions, 2u};
    speech.memory = memory;
    speech.operation_budget = 3000u;
    for (unsigned reg = 0u; reg != 32u; ++reg)
      set_word(speech.registers.gpr[reg], UINT32_C(0x50000000) + reg,
               static_cast<uint8_t>(reg % 16u));
    set_word(speech.registers.gpr[0], 0u);
    set_word(speech.registers.gpr[NBA97_MATCH_INITIALIZE_SP], kEntrySp);
    set_word(speech.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
             UINT32_C(0x8002dbe0));
    put(UINT32_C(0x80015018), 1u);
    put(UINT32_C(0x8001edf4), 3u, 2u);
    put(UINT32_C(0x8001eeb8), 7u, 2u);
    put(UINT32_C(0x80021d74), 3u);
    put(UINT32_C(0x80021d78), 7u);
    for (unsigned index = 0u; index != 12u; ++index) {
      const uint32_t home = UINT32_C(0x80030000) + index * 0x20u;
      const uint32_t away = UINT32_C(0x80030400) + index * 0x20u;
      put(UINT32_C(0x80020b8c) + index * 4u, home);
      put(UINT32_C(0x80020bbc) + index * 4u, away);
      put(home, index, 2u);
      put(away, 100u + index, 2u);
      put(home + 7u, index, 1u);
      put(away + 7u, 100u + index, 1u);
    }
    put(kPayload, UINT32_C(0xefbeadde));
    nba97_game_bios_memory_copy_speech_binding_init(
        &binding, 1u, bios, this, provide_hilo, this, fallback, this);
  }

  uint8_t *byte(uint32_t address) {
    for (Nba97GameTextRegion &region : regions)
      if (address >= region.base &&
          static_cast<uint64_t>(address - region.base) < region.size)
        return region.data + (address - region.base);
    return nullptr;
  }

  void put(uint32_t address, uint32_t value, unsigned width = 4u) {
    for (unsigned index = 0u; index != width; ++index) {
      uint8_t *destination = byte(address + index);
      CHECK(destination != nullptr);
      *destination = static_cast<uint8_t>(value >> (8u * index));
    }
  }

  uint32_t get(uint32_t address) {
    uint32_t value = 0u;
    for (unsigned index = 0u; index != 4u; ++index) {
      uint8_t *source = byte(address + index);
      CHECK(source != nullptr);
      value |= static_cast<uint32_t>(*source) << (8u * index);
    }
    return value;
  }

  static uint8_t *find(const Nba97GameTextMemory &memory, uint32_t address) {
    for (size_t index = 0u; index != memory.count; ++index) {
      const Nba97GameTextRegion &region = memory.region[index];
      if (address >= region.base &&
          static_cast<uint64_t>(address - region.base) < region.size)
        return region.data + (address - region.base);
    }
    return nullptr;
  }

  static void put_through(const Nba97GameTextMemory &memory, uint32_t address,
                          uint32_t value) {
    for (unsigned index = 0u; index != 4u; ++index) {
      uint8_t *destination = find(memory, address + index);
      CHECK(destination != nullptr);
      *destination = static_cast<uint8_t>(value >> (8u * index));
    }
  }

  static int provide_hilo(void *opaque,
                          const Nba97GameSpeechInitializeEvent *event,
                          const Nba97GameSpeechInitializeRegisters *,
                          Nba97GameBiosMemoryCopyWord *hi,
                          Nba97GameBiosMemoryCopyWord *lo) {
    Natural &self = *static_cast<Natural *>(opaque);
    CHECK(event->pc == UINT32_C(0x8008008c));
    if (self.reject_provider)
      return 0;
    set_word(*hi, UINT32_C(0x11223344), 5u);
    set_word(*lo, UINT32_C(0x55667788), 10u);
    return 1;
  }

  static int bios(void *opaque, const Nba97GameTextMemory *memory,
                  const Nba97GameBiosMemoryCopyEvent *event,
                  Nba97GameBiosMemoryCopyMachine *machine) {
    Natural &self = *static_cast<Natural *>(opaque);
    ++self.bios_calls;
    CHECK(event->pc == UINT32_C(0x8009cb10));
    CHECK(event->delay_slot_pc == UINT32_C(0x8009cb14));
    CHECK(event->entry == UINT32_C(0x000000a0));
    CHECK(event->service == 0x2au && event->argument_count == 3u);
    CHECK(machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
          UINT32_C(0x80080094));
    CHECK(machine->registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 1u].word == 0x2au);
    CHECK(machine->registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 2u].word == 0xa0u);
    self.observed_hi = machine->hi;
    self.observed_lo = machine->lo;
    if (self.binding.hilo_provider != nullptr) {
      CHECK(machine->hi.word == UINT32_C(0x11223344));
      CHECK(machine->hi.known_mask == 5u);
      CHECK(machine->lo.word == UINT32_C(0x55667788));
      CHECK(machine->lo.known_mask == 10u);
    } else {
      CHECK(machine->hi.word == 0u && machine->hi.known_mask == 0u);
      CHECK(machine->lo.word == 0u && machine->lo.known_mask == 0u);
    }
    const Nba97GameBiosMemoryCopyWord destination =
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0];
    const Nba97GameBiosMemoryCopyWord source =
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1];
    const Nba97GameBiosMemoryCopyWord count =
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_A2];
    CHECK(destination.known_mask == 15u && source.known_mask == 15u &&
          count.known_mask == 15u);
    for (uint32_t offset = 0u; offset != count.word; ++offset) {
      uint8_t *from = find(*memory, source.word + offset);
      uint8_t *to = find(*memory, destination.word + offset);
      CHECK(from != nullptr && to != nullptr);
      *to = *from;
    }
    set_word(machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0],
             UINT32_C(0xcafebabe), 7u);
    set_word(machine->hi, UINT32_C(0xaabbccdd), 3u);
    set_word(machine->lo, UINT32_C(0x12345678), 12u);
    if (self.malformed == 1u)
      machine->registers.gpr[17].known_mask = 16u;
    else if (self.malformed == 2u)
      machine->hi.known_mask = 16u;
    else if (self.malformed == 3u)
      machine->lo.known_mask = 16u;
    return 1;
  }

  static int fallback(void *opaque, const Nba97GameTextMemory *memory,
                      const Nba97GameSpeechInitializeEvent *event,
                      Nba97GameSpeechInitializeRegisters *registers) {
    Natural &self = *static_cast<Natural *>(opaque);
    ++self.fallback_calls;
    switch (event->kind) {
    case NBA97_GAME_SPEECH_INITIALIZE_RESOURCE_LOAD_80029BFC:
      set_word(registers->gpr[NBA97_MATCH_INITIALIZE_V0],
               UINT32_C(0x80040000) +
                   static_cast<uint32_t>(event->invocation) * 0x1000u);
      break;
    case NBA97_GAME_SPEECH_INITIALIZE_LOOKUP_8007FC08: {
      const Nba97GameSpeechInitializeWord destination =
          registers->gpr[NBA97_MATCH_INITIALIZE_A3];
      CHECK(destination.known_mask == 15u);
      if (event->invocation == 1u) {
        put_through(*memory, destination.word, kPayload);
        put_through(*memory, destination.word + 4u, 4u);
      } else {
        put_through(*memory, destination.word, 0u);
        put_through(*memory, destination.word + 4u, 0u);
      }
      break;
    }
    case NBA97_GAME_SPEECH_INITIALIZE_ALLOCATE_80090160:
      set_word(registers->gpr[NBA97_MATCH_INITIALIZE_V0], kPacked);
      break;
    case NBA97_GAME_SPEECH_INITIALIZE_CONVERT_800AE54C:
      set_word(registers->gpr[NBA97_MATCH_INITIALIZE_V0],
               registers->gpr[NBA97_MATCH_INITIALIZE_A0].word + 0x1000u);
      break;
    case NBA97_GAME_SPEECH_INITIALIZE_RELEASE_80090698:
      set_word(registers->gpr[NBA97_MATCH_INITIALIZE_V0], UINT32_C(0x1234abcd));
      break;
    default:
      break;
    }
    return 1;
  }

  int run() {
    return nba97_game_bios_memory_copy_with_speech(&speech, &binding,
                                                   &speech_progress);
  }
};


} // namespace
namespace nba97 {
std::string captureGameBiosMemoryCopy() {
 Natural c;const uint32_t before=c.get(kPacked);
 if(c.run()!=NBA97_TEXT_COMPLETE || !c.binding.progress.completed || !c.speech_progress.completed)
   throw std::runtime_error("BIOS copy native speech composition failed");
 const auto& q=c.binding.progress;std::ostringstream o;
 o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x8009CB0C\",\"inclusive_end\":\"0x8009CB17\",\"bytes\":12,\"instructions\":3,"
   "\"classification\":\"no direct visual effect\",\"scope\":\"actual speech initializer and BIOS trampoline; synthetic resource and BIOS services; explicit HI/LO provider\","
   "\"completed\":true,\"parent_completed\":true,\"operations\":"<<q.operations<<",\"callbacks\":"<<q.callbacks_completed
   <<",\"call_pc\":"<<q.event.pc<<",\"delay_pc\":"<<q.event.delay_slot_pc<<",\"bios_vector\":"<<q.event.entry<<",\"service\":"<<unsigned(q.event.service)
   <<",\"destination_before\":"<<before<<",\"destination_after\":"<<c.get(kPacked)
   <<",\"returned_t1\":"<<q.machine.registers.gpr[9].word<<",\"returned_t2\":"<<q.machine.registers.gpr[10].word
   <<",\"returned_v0\":"<<q.machine.registers.gpr[2].word<<",\"returned_v0_mask\":"<<unsigned(q.machine.registers.gpr[2].known_mask)
   <<",\"returned_hi\":"<<q.machine.hi.word<<",\"returned_lo\":"<<q.machine.lo.word
   <<",\"hi_mask\":"<<unsigned(q.machine.hi.known_mask)<<",\"lo_mask\":"<<unsigned(q.machine.lo.known_mask)
   <<",\"return_address\":"<<q.machine.registers.gpr[31].word<<",\"parent_v0\":"<<c.speech_progress.registers.gpr[2].word<<"}";
 return o.str();
}
}
