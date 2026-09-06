#include "game_video_mode_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

using U32 = std::uint32_t;
size_t checks = 0u;
void check(bool condition, const char *expression, int line) {
  ++checks;
  if (condition)
    return;
  std::fprintf(stderr, "check failed at line %d: %s\n", line, expression);
  std::exit(1);
}
#define CHECK(expression) check((expression), #expression, __LINE__)

struct Fixture {
  static constexpr U32 kBase = UINT32_C(0x80000000);
  static constexpr size_t kSize = UINT32_C(0x120000);
  static constexpr U32 kEnvironment = UINT32_C(0x80022000);
  static constexpr U32 kStack = UINT32_C(0x8010ff00);
  static constexpr U32 kVideo = UINT32_C(0x800c54ac);
  std::vector<uint8_t> bytes = std::vector<uint8_t>(kSize, 0u);
  std::vector<uint8_t> known = std::vector<uint8_t>(kSize, 1u);
  Nba97GameTextRegion region{kBase, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameDisplayEnvironmentAccess, 96> display_journal{};
  std::array<Nba97GameVideoModeAccess, 2> first_journal{};
  std::array<Nba97GameVideoModeAccess, 2> second_journal{};
  Nba97GameDisplayEnvironmentContext context{};
  Nba97GameDisplayEnvironmentProgress progress{};
  Nba97GameVideoModeDisplayBinding binding{};
  std::vector<Nba97GameDisplayEnvironmentEvent> fallback_events;

  Fixture() {
    context.memory = {&region, 1u};
    context.operation_budget = 250u;
    context.io = nba97_game_video_mode_from_display;
    context.user = &binding;
    context.access_journal = display_journal.data();
    context.access_journal_capacity = display_journal.size();
    for (unsigned reg = 0u; reg != 32u; ++reg)
      context.machine.registers.gpr[reg] = {UINT32_C(0x11000000) +
                                                reg * UINT32_C(0x101),
                                            static_cast<uint8_t>(reg % 16u)};
    context.machine.registers.gpr[0] = {0u, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {kEnvironment,
                                                                15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {kStack, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
        UINT32_C(0x81234568), 15u};
    context.machine.hi = {UINT32_C(0x89abcdef), 5u};
    context.machine.lo = {UINT32_C(0x76543210), 10u};
    Nba97GameVideoModeCallConfig config[2] = {
        {1u, first_journal.data(), first_journal.size()},
        {1u, second_journal.data(), second_journal.size()}};
    nba97_game_video_mode_display_binding_init(&binding, config, fallback,
                                               this);
    seed();
  }

  size_t at(U32 address) const { return static_cast<size_t>(address - kBase); }

  void put(U32 address, U32 value, unsigned width = 4u, uint8_t mask = 15u) {
    for (unsigned byte = 0u; byte != width; ++byte) {
      bytes[at(address) + byte] = static_cast<uint8_t>(value >> (8u * byte));
      known[at(address) + byte] = static_cast<uint8_t>((mask >> byte) & 1u);
    }
  }

  U32 get(U32 address, unsigned width = 4u) const {
    U32 value = 0u;
    for (unsigned byte = 0u; byte != width; ++byte)
      value |= static_cast<U32>(bytes[at(address) + byte]) << (8u * byte);
    return value;
  }

  void copy(U32 source, U32 destination, unsigned size) {
    std::memmove(bytes.data() + at(destination), bytes.data() + at(source),
                 size);
    std::memmove(known.data() + at(destination), known.data() + at(source),
                 size);
  }

  void seed() {
    put(UINT32_C(0x800c55c2), 0u, 1u);
    put(UINT32_C(0x800c55c0), 0u, 1u);
    put(UINT32_C(0x800c55c3), 0u, 1u);
    put(UINT32_C(0x800c55bc), UINT32_C(0x8009cb2c));
    put(UINT32_C(0x800c55b8), UINT32_C(0x800c5578));
    put(UINT32_C(0x800c5588), UINT32_C(0x8009a97c));
    put(kVideo, 0u);
    put(kEnvironment + 0u, 10u, 2u);
    put(kEnvironment + 2u, 20u, 2u);
    put(kEnvironment + 4u, 320u, 2u);
    put(kEnvironment + 6u, 240u, 2u);
    put(kEnvironment + 8u, 0u, 2u);
    put(kEnvironment + 10u, 0u, 2u);
    put(kEnvironment + 12u, 256u, 2u);
    put(kEnvironment + 14u, 240u, 2u);
    put(kEnvironment + 16u, 0u);
    copy(kEnvironment, UINT32_C(0x800c562c), 20u);
  }

  void make_changed(U32 video) {
    put(kVideo, video);
    put(UINT32_C(0x800c5634), UINT32_C(0xffff), 2u);
    put(UINT32_C(0x800c563c), UINT32_C(0xffffffff));
    put(kEnvironment + 6u, 270u, 2u);
    put(kEnvironment + 8u, 50u, 2u);
    put(kEnvironment + 10u, 300u, 2u);
    put(kEnvironment + 12u, 0u, 2u);
    put(kEnvironment + 14u, 0u, 2u);
  }

  static int fallback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameDisplayEnvironmentEvent *event,
                      Nba97GameDisplayEnvironmentMachine *machine) {
    Fixture &self = *static_cast<Fixture *>(opaque);
    self.fallback_events.push_back(*event);
    if (event->kind == NBA97_GAME_DISPLAY_ENVIRONMENT_ORIGIN_HELPER)
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {UINT32_C(0x456),
                                                           15u};
    if (event->kind == NBA97_GAME_DISPLAY_ENVIRONMENT_COPY)
      self.copy(machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].word,
                machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word,
                machine->registers.gpr[NBA97_MATCH_INITIALIZE_A2].word);
    return 1;
  }

  int run() { return nba97_game_display_environment(&context, &progress); }
};

Nba97GameDisplayEnvironmentEvent video_event(unsigned site) {
  Nba97GameDisplayEnvironmentEvent event{};
  event.pc = site == 0u ? UINT32_C(0x80099de8) : UINT32_C(0x8009a034);
  event.delay_slot_pc = event.pc + 4u;
  event.entry = UINT32_C(0x800985cc);
  event.kind = NBA97_GAME_DISPLAY_ENVIRONMENT_VIDEO_MODE;
  event.argument_count = 0u;
  return event;
}

void direct_machine(Nba97GameDisplayEnvironmentMachine &machine,
                    unsigned site) {
  for (unsigned reg = 0u; reg != 32u; ++reg)
    machine.registers.gpr[reg] = {UINT32_C(0x31000000) + reg,
                                  static_cast<uint8_t>(reg % 16u)};
  machine.registers.gpr[0] = {0u, 15u};
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
      (site == 0u ? UINT32_C(0x80099df0) : UINT32_C(0x8009a03c)), 15u};
  machine.hi = {UINT32_C(0x11223344), 5u};
  machine.lo = {UINT32_C(0x55667788), 10u};
}

void test_unchanged_cache_skips_query() {
  Fixture fixture;
  CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
  CHECK(fixture.progress.completed == 1u);
  CHECK(fixture.binding.invocations == 0u);
  CHECK(fixture.binding.completions == 0u);
  CHECK(!fixture.progress.screen_rectangle_changed);
  CHECK(!fixture.progress.mode_changed);
  for (const auto &event : fixture.fallback_events)
    CHECK(event.kind != NBA97_GAME_DISPLAY_ENVIRONMENT_VIDEO_MODE);
}

void test_both_sites_and_raw_branch_distinctions() {
  for (U32 video : {0u, 1u, 2u}) {
    Fixture fixture;
    fixture.make_changed(video);
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
    CHECK(fixture.progress.completed == 1u);
    CHECK(fixture.progress.screen_rectangle_changed);
    CHECK(fixture.progress.mode_changed);
    CHECK(fixture.binding.invocations == 2u);
    CHECK(fixture.binding.completions == 2u);
    for (unsigned site = 0u; site != 2u; ++site) {
      const U32 pc = site == 0u ? UINT32_C(0x80099de8) : UINT32_C(0x8009a034);
      CHECK(fixture.binding.call_count[site] == 1u);
      CHECK(fixture.binding.event[site].pc == pc);
      CHECK(fixture.binding.event[site].delay_slot_pc == pc + 4u);
      CHECK(fixture.binding.event[site].entry == UINT32_C(0x800985cc));
      CHECK(fixture.binding.result[site] == NBA97_TEXT_COMPLETE);
      CHECK(fixture.binding.progress[site].return_v0.word == video);
      CHECK(fixture.binding.progress[site].return_v0.known_mask == 15u);
      CHECK(fixture.binding.progress[site].completed == 1u);
    }
    CHECK(fixture.get(Fixture::kEnvironment + 18u, 1u) == (video & 0xffu));
    CHECK((fixture.progress.mode_command.word & 8u) == (video == 1u ? 8u : 0u));
    CHECK((fixture.progress.mode_command.word & 36u) ==
          (video == 0u ? 36u : 0u));
  }
}

void test_unknown_video_preserves_parent_prefix() {
  Fixture fixture;
  fixture.make_changed(1u);
  fixture.put(Fixture::kVideo, 1u, 4u, 14u);
  CHECK(fixture.run() == NBA97_TEXT_UNKNOWN);
  CHECK(fixture.progress.stopped_pc == UINT32_C(0x80099e10));
  CHECK(fixture.binding.invocations == 1u);
  CHECK(fixture.binding.progress[0].completed == 1u);
  CHECK(fixture.binding.progress[0].return_v0.word == 1u);
  CHECK(fixture.binding.progress[0].return_v0.known_mask == 14u);
  size_t read_index = fixture.progress.access_events;
  size_t store_index = fixture.progress.access_events;
  for (size_t index = 0u; index != fixture.progress.access_events; ++index) {
    if (fixture.display_journal[index].pc == UINT32_C(0x80099df0))
      read_index = index;
    if (fixture.display_journal[index].pc == UINT32_C(0x80099df4))
      store_index = index;
  }
  CHECK(read_index < store_index);
  CHECK(fixture.display_journal[read_index].kind ==
        NBA97_GAME_DISPLAY_ENVIRONMENT_READ);
  CHECK(fixture.display_journal[store_index].kind ==
        NBA97_GAME_DISPLAY_ENVIRONMENT_STORE);
  CHECK(fixture.display_journal[store_index].address ==
        Fixture::kEnvironment + 18u);
  CHECK(fixture.known[fixture.at(Fixture::kEnvironment + 18u)] == 0u);
}

void test_per_site_budget_prefixes() {
  Fixture first;
  first.make_changed(1u);
  first.binding.config[0].operation_budget = 0u;
  CHECK(first.run() == NBA97_TEXT_IO_REFUSED);
  CHECK(first.binding.invocations == 1u);
  CHECK(first.binding.result[0] == NBA97_TEXT_LIMIT);
  CHECK(first.binding.progress[0].return_v0.word == UINT32_C(0x800c0000));
  CHECK(first.binding.call_count[1] == 0u);

  Fixture second;
  second.make_changed(1u);
  second.binding.config[1].operation_budget = 0u;
  CHECK(second.run() == NBA97_TEXT_IO_REFUSED);
  CHECK(second.binding.invocations == 2u);
  CHECK(second.binding.completions == 1u);
  CHECK(second.binding.result[0] == NBA97_TEXT_COMPLETE);
  CHECK(second.binding.result[1] == NBA97_TEXT_LIMIT);
  CHECK(second.binding.progress[1].return_v0.word == UINT32_C(0x800c0000));
}

void test_adapter_machine_copy_and_guards() {
  for (unsigned site = 0u; site != 2u; ++site) {
    Fixture fixture;
    fixture.put(Fixture::kVideo, UINT32_C(0x80000002));
    Nba97GameDisplayEnvironmentEvent event = video_event(site);
    Nba97GameDisplayEnvironmentMachine machine{};
    direct_machine(machine, site);
    const Nba97GameDisplayEnvironmentMachine before = machine;
    CHECK(nba97_game_video_mode_from_display(&fixture.binding,
                                             &fixture.context.memory, &event,
                                             &machine) == 1);
    CHECK(machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
          UINT32_C(0x80000002));
    for (unsigned reg = 0u; reg != 32u; ++reg) {
      if (reg == NBA97_MATCH_INITIALIZE_V0)
        continue;
      CHECK(machine.registers.gpr[reg].word == before.registers.gpr[reg].word);
      CHECK(machine.registers.gpr[reg].known_mask ==
            before.registers.gpr[reg].known_mask);
    }
    CHECK(machine.hi.word == before.hi.word &&
          machine.hi.known_mask == before.hi.known_mask);
    CHECK(machine.lo.word == before.lo.word &&
          machine.lo.known_mask == before.lo.known_mask);
  }

  for (unsigned case_index = 0u; case_index != 9u; ++case_index) {
    Fixture fixture;
    Nba97GameDisplayEnvironmentEvent event = video_event(0u);
    Nba97GameDisplayEnvironmentMachine machine{};
    direct_machine(machine, 0u);
    if (case_index == 0u)
      event.pc = UINT32_C(0x80099dec);
    else if (case_index == 1u)
      event.delay_slot_pc += 4u;
    else if (case_index == 2u)
      event.entry += 4u;
    else if (case_index == 3u)
      event.kind = NBA97_GAME_DISPLAY_ENVIRONMENT_GPU_COMMAND;
    else if (case_index == 4u)
      event.argument_count = 1u;
    else if (case_index == 5u)
      machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 7u;
    else if (case_index == 6u)
      machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word += 4u;
    else if (case_index == 7u)
      machine.hi.known_mask = 16u;
    else
      fixture.binding.config[0].access_journal = nullptr;
    const Nba97GameDisplayEnvironmentMachine before = machine;
    CHECK(nba97_game_video_mode_from_display(&fixture.binding,
                                             &fixture.context.memory, &event,
                                             &machine) == 0);
    CHECK(std::memcmp(&machine, &before, sizeof(machine)) == 0);
    CHECK(fixture.binding.invocations == 0u);
  }

  Fixture invalid_memory;
  Nba97GameDisplayEnvironmentEvent event = video_event(0u);
  Nba97GameDisplayEnvironmentMachine machine{};
  direct_machine(machine, 0u);
  invalid_memory.region.size = 0u;
  const Nba97GameDisplayEnvironmentMachine before = machine;
  CHECK(nba97_game_video_mode_from_display(&invalid_memory.binding,
                                           &invalid_memory.context.memory,
                                           &event, &machine) == 0);
  CHECK(std::memcmp(&machine, &before, sizeof(machine)) == 0);

  Fixture fallback;
  Nba97GameDisplayEnvironmentEvent other{};
  other.pc = UINT32_C(0x80099d6c);
  other.entry = UINT32_C(0x8009a97c);
  other.kind = NBA97_GAME_DISPLAY_ENVIRONMENT_GPU_COMMAND;
  CHECK(nba97_game_video_mode_from_display(&fallback.binding,
                                           &fallback.context.memory, &other,
                                           &machine) == 1);
  CHECK(fallback.fallback_events.size() == 1u);
  fallback.binding.fallback = nullptr;
  CHECK(nba97_game_video_mode_from_display(&fallback.binding,
                                           &fallback.context.memory, &other,
                                           &machine) == 0);

  CHECK(nba97_game_video_mode_from_display(nullptr, &fallback.context.memory,
                                           &event, &machine) == 0);
  CHECK(nba97_game_video_mode_from_display(&fallback.binding, nullptr, &event,
                                           &machine) == 0);
  CHECK(nba97_game_video_mode_from_display(&fallback.binding,
                                           &fallback.context.memory, nullptr,
                                           &machine) == 0);
  CHECK(nba97_game_video_mode_from_display(
            &fallback.binding, &fallback.context.memory, &event, nullptr) == 0);
}

} // namespace

int main() {
  test_unchanged_cache_skips_query();
  test_both_sites_and_raw_branch_distinctions();
  test_unknown_video_preserves_parent_prefix();
  test_per_site_budget_prefixes();
  test_adapter_machine_copy_and_guards();
  std::printf("game_video_mode_integration_tests: %zu checks passed\n", checks);
  return 0;
}
