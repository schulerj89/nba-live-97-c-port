#include "game_bios_memory_copy_adapter.h"

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
  std::exit(1);
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

Nba97GameSpeechInitializeEvent copy_event() {
  Nba97GameSpeechInitializeEvent event{};
  event.pc = UINT32_C(0x8008008c);
  event.delay_slot_pc = UINT32_C(0x80080090);
  event.entry = UINT32_C(0x8009cb0c);
  event.kind = NBA97_GAME_SPEECH_INITIALIZE_COPY_8009CB0C;
  event.argument_count = 3u;
  return event;
}

void direct_registers(Nba97GameSpeechInitializeRegisters &registers) {
  for (unsigned reg = 0u; reg != 32u; ++reg)
    set_word(registers.gpr[reg], UINT32_C(0x30000000) + reg);
  set_word(registers.gpr[0], 0u);
  set_word(registers.gpr[NBA97_MATCH_INITIALIZE_A0], kPacked);
  set_word(registers.gpr[NBA97_MATCH_INITIALIZE_A1], kPayload);
  set_word(registers.gpr[NBA97_MATCH_INITIALIZE_A2], 4u);
  set_word(registers.gpr[NBA97_MATCH_INITIALIZE_RA], UINT32_C(0x80080094));
}

void test_actual_speech_caller() {
  Natural natural;
  CHECK(natural.run() == NBA97_TEXT_COMPLETE);
  CHECK(natural.speech_progress.completed == 1u);
  CHECK(natural.binding.invocations == 1u);
  CHECK(natural.binding.provider_invocations == 1u);
  CHECK(natural.binding.result == NBA97_TEXT_COMPLETE);
  CHECK(natural.binding.progress.completed == 1u);
  CHECK(natural.bios_calls == 1u);
  CHECK(natural.get(kPacked) == UINT32_C(0xefbeadde));
  CHECK(
      natural.binding.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
          .word == UINT32_C(0xcafebabe));
  CHECK(natural.binding.progress.machine.hi.word == UINT32_C(0xaabbccdd));
  CHECK(natural.binding.progress.machine.lo.word == UINT32_C(0x12345678));
  CHECK(natural.speech_progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
        UINT32_C(0x1234abcd));
}

void test_metadata_provider_and_default_hilo() {
  Natural natural;
  Nba97GameSpeechInitializeEvent event = copy_event();
  for (unsigned case_index = 0u; case_index != 7u; ++case_index) {
    Nba97GameSpeechInitializeEvent malformed = event;
    Nba97GameSpeechInitializeRegisters registers{};
    direct_registers(registers);
    if (case_index == 0u)
      malformed.pc += 4u;
    else if (case_index == 1u)
      malformed.delay_slot_pc += 4u;
    else if (case_index == 2u)
      malformed.entry += 4u;
    else if (case_index == 3u)
      malformed.kind = NBA97_GAME_SPEECH_INITIALIZE_ALLOCATE_80090160;
    else if (case_index == 4u)
      malformed.argument_count = 2u;
    else if (case_index == 5u)
      registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 7u;
    else
      registers.gpr[NBA97_MATCH_INITIALIZE_RA].word += 4u;
    const Nba97GameSpeechInitializeRegisters before = registers;
    CHECK(nba97_game_bios_memory_copy_from_speech(
              &natural.binding, &natural.memory, &malformed, &registers) == 0);
    CHECK(std::memcmp(&registers, &before, sizeof(registers)) == 0);
    CHECK(natural.binding.invocations == 0u);
  }

  Natural rejected_provider;
  rejected_provider.reject_provider = true;
  Nba97GameSpeechInitializeRegisters rejected_registers{};
  direct_registers(rejected_registers);
  CHECK(nba97_game_bios_memory_copy_from_speech(
            &rejected_provider.binding, &rejected_provider.memory, &event,
            &rejected_registers) == 0);
  CHECK(rejected_provider.binding.provider_invocations == 1u);
  CHECK(rejected_provider.binding.invocations == 0u);
  CHECK(rejected_provider.binding.result == NBA97_TEXT_IO_REFUSED);

  Natural unknown_hilo;
  unknown_hilo.binding.hilo_provider = nullptr;
  Nba97GameSpeechInitializeRegisters unknown_registers{};
  direct_registers(unknown_registers);
  CHECK(nba97_game_bios_memory_copy_from_speech(&unknown_hilo.binding,
                                                &unknown_hilo.memory, &event,
                                                &unknown_registers) == 1);
  CHECK(unknown_hilo.observed_hi.word == 0u &&
        unknown_hilo.observed_hi.known_mask == 0u);
  CHECK(unknown_hilo.observed_lo.word == 0u &&
        unknown_hilo.observed_lo.known_mask == 0u);
  CHECK(unknown_hilo.binding.provider_invocations == 0u);

  CHECK(nba97_game_bios_memory_copy_from_speech(
            nullptr, &natural.memory, &event, &unknown_registers) == 0);
  CHECK(nba97_game_bios_memory_copy_from_speech(
            &natural.binding, nullptr, &event, &unknown_registers) == 0);
  CHECK(nba97_game_bios_memory_copy_from_speech(&natural.binding,
                                                &natural.memory, nullptr,
                                                &unknown_registers) == 0);
  CHECK(nba97_game_bios_memory_copy_from_speech(
            &natural.binding, &natural.memory, &event, nullptr) == 0);
}

void test_nested_failure_statuses() {
  Natural bounded;
  bounded.binding.operation_budget = 0u;
  CHECK(bounded.run() == NBA97_TEXT_IO_REFUSED);
  CHECK(bounded.binding.result == NBA97_TEXT_LIMIT);
  CHECK(bounded.binding.progress.machine.registers
            .gpr[NBA97_MATCH_INITIALIZE_T0 + 1u]
            .word == 0x2au);
  CHECK(bounded.binding.progress.machine.registers
            .gpr[NBA97_MATCH_INITIALIZE_T0 + 2u]
            .word == 0xa0u);

  Natural invalid_gpr;
  invalid_gpr.malformed = 1u;
  CHECK(invalid_gpr.run() == NBA97_TEXT_ARGUMENT);
  CHECK(invalid_gpr.binding.result == NBA97_TEXT_ARGUMENT);
  CHECK(invalid_gpr.binding.progress.machine.registers.gpr[17].known_mask ==
        16u);

  Natural invalid_hi;
  invalid_hi.malformed = 2u;
  CHECK(invalid_hi.run() == NBA97_TEXT_ARGUMENT);
  CHECK(invalid_hi.binding.result == NBA97_TEXT_ARGUMENT);
  CHECK(invalid_hi.binding.progress.machine.hi.known_mask == 16u);

  Natural invalid_lo;
  invalid_lo.malformed = 3u;
  CHECK(invalid_lo.run() == NBA97_TEXT_ARGUMENT);
  CHECK(invalid_lo.binding.result == NBA97_TEXT_ARGUMENT);
  CHECK(invalid_lo.binding.progress.machine.lo.known_mask == 16u);

  CHECK(nba97_game_bios_memory_copy_with_speech(nullptr, &invalid_lo.binding,
                                                &invalid_lo.speech_progress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_bios_memory_copy_with_speech(&invalid_lo.speech, nullptr,
                                                &invalid_lo.speech_progress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_bios_memory_copy_with_speech(&invalid_lo.speech,
                                                &invalid_lo.binding, nullptr) ==
        NBA97_TEXT_ARGUMENT);
}

void test_fallback() {
  Natural natural;
  Nba97GameSpeechInitializeEvent event{};
  event.pc = UINT32_C(0x800800a0);
  event.delay_slot_pc = UINT32_C(0x800800a4);
  event.entry = UINT32_C(0x800ae54c);
  event.kind = NBA97_GAME_SPEECH_INITIALIZE_CONVERT_800AE54C;
  event.argument_count = 1u;
  Nba97GameSpeechInitializeRegisters registers{};
  direct_registers(registers);
  CHECK(nba97_game_bios_memory_copy_from_speech(
            &natural.binding, &natural.memory, &event, &registers) == 1);
  CHECK(natural.fallback_calls == 1u);
  CHECK(natural.binding.invocations == 0u);
  natural.binding.fallback = nullptr;
  CHECK(nba97_game_bios_memory_copy_from_speech(
            &natural.binding, &natural.memory, &event, &registers) == 0);
}

} // namespace

int main() {
  test_actual_speech_caller();
  test_metadata_provider_and_default_hilo();
  test_nested_failure_statuses();
  test_fallback();
  std::printf("game_bios_memory_copy_integration_tests: %zu checks passed\n",
              checks);
  return 0;
}
