#include "game_gte_reference_transform_adapter.h"
#include "game_player_geometry.hpp"

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
constexpr uint32_t kInput = UINT32_C(0x80010000);
constexpr uint32_t kOutput = UINT32_C(0x80010100);
constexpr uint32_t kFlag = UINT32_C(0x80010200);

void set_word(Nba97GameGteReferenceTransformWord &word, uint32_t value,
              uint8_t known = 15u) {
  word.word = value;
  word.known_mask = known;
}

struct Hardware {
  int result = NBA97_TEXT_COMPLETE;
  bool malformed = false;
  uint8_t output_mask = 15u;
  size_t calls = 0u;
  Nba97GameGteReferenceTransformHardwareEvent event{};
};

int hardware(void *opaque,
             const Nba97GameGteReferenceTransformHardwareEvent *event,
             Nba97GameGteReferenceTransformState *state) {
  Hardware &mock = *static_cast<Hardware *>(opaque);
  ++mock.calls;
  mock.event = *event;
  if (mock.result != NBA97_TEXT_COMPLETE)
    return mock.result;
  set_word(state->data[25], UINT32_C(0x11112222), mock.output_mask);
  set_word(state->data[26], UINT32_C(0x33334444), mock.output_mask);
  set_word(state->data[27], UINT32_C(0x55556666), mock.output_mask);
  set_word(state->data[9], 1u);
  set_word(state->data[10], 2u);
  set_word(state->data[11], 3u);
  set_word(state->control[31], UINT32_C(0x87654321), mock.output_mask);
  if (mock.malformed)
    state->data[30].known_mask = 16u;
  return NBA97_TEXT_COMPLETE;
}

struct Fixture {
  std::vector<uint8_t> ram = std::vector<uint8_t>(0x200000u, 0xa5u);
  std::vector<uint8_t> known = std::vector<uint8_t>(0x200000u, 1u);
  Nba97GameTextRegion region{};
  std::array<Nba97GameGteReferenceTransformAccess, 8> journal{};
  Nba97GameGteReferenceTransformContext context{};
  Nba97GameGteReferenceTransformProgress progress{};
  Hardware mock{};

  Fixture() {
    region = {kBase, ram.data(), known.data(), ram.size()};
    context.memory = {&region, 1u};
    context.operation_budget = 100u;
    context.hardware = hardware;
    context.hardware_user = &mock;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    for (unsigned index = 0u; index != 32u; ++index) {
      set_word(context.machine.registers.gpr[index],
               UINT32_C(0x10000000) + index);
      set_word(context.state.control[index], UINT32_C(0x20000000) + index);
      set_word(context.state.data[index], UINT32_C(0x30000000) + index);
    }
    set_word(context.machine.registers.gpr[0], 0u);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0], kInput);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1], kOutput);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2], kFlag);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
             UINT32_C(0x81234567));
    set_word(context.machine.hi, UINT32_C(0x12345678));
    set_word(context.machine.lo, UINT32_C(0x9abcdef0));
    put(kInput, UINT32_C(0x80017fff));
    put(kInput + 4u, UINT32_C(0xdead8001));
  }

  size_t offset(uint32_t address) const { return address - kBase; }
  void put(uint32_t address, uint32_t value) {
    for (unsigned byte = 0u; byte != 4u; ++byte)
      ram[offset(address) + byte] = static_cast<uint8_t>(value >> (byte * 8u));
  }
  uint32_t get(uint32_t address) const {
    uint32_t value = 0u;
    for (unsigned byte = 0u; byte != 4u; ++byte)
      value |= static_cast<uint32_t>(ram[offset(address) + byte])
               << (byte * 8u);
    return value;
  }
  int run() { return nba97_game_gte_reference_transform(&context, &progress); }
};

void test_exact_path_and_preservation() {
  Fixture fixture;
  const auto machine = fixture.context.machine;
  const auto state = fixture.context.state;
  CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
  CHECK(fixture.progress.completed == 1u);
  CHECK(fixture.progress.operations == 7u);
  CHECK(fixture.progress.reads == 2u && fixture.progress.stores == 4u);
  CHECK(fixture.mock.calls == 1u && fixture.progress.hardware_completed == 1u);
  CHECK(fixture.mock.event.pc == UINT32_C(0x8005665c));
  CHECK(fixture.mock.event.command == UINT32_C(0x00480012));
  CHECK(fixture.mock.event.operation == 3u &&
        fixture.mock.event.invocation == 0u);
  CHECK(fixture.progress.state.data[0].word == UINT32_C(0x80017fff));
  CHECK(fixture.progress.state.data[1].word == UINT32_C(0xffff8001));
  CHECK(fixture.progress.state.data[1].known_mask == 15u);
  CHECK(fixture.get(kOutput) == UINT32_C(0x11112222));
  CHECK(fixture.get(kOutput + 4u) == UINT32_C(0x33334444));
  CHECK(fixture.get(kOutput + 8u) == UINT32_C(0x55556666));
  CHECK(fixture.get(kFlag) == UINT32_C(0x87654321));
  CHECK(
      fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
      UINT32_C(0x87654321));
  CHECK(std::memcmp(&fixture.progress.machine.hi, &machine.hi,
                    sizeof(machine.hi)) == 0);
  CHECK(std::memcmp(&fixture.progress.machine.lo, &machine.lo,
                    sizeof(machine.lo)) == 0);
  for (unsigned index = 0u; index != 32u; ++index)
    if (index != NBA97_MATCH_INITIALIZE_V0)
      CHECK(std::memcmp(&fixture.progress.machine.registers.gpr[index],
                        &machine.registers.gpr[index],
                        sizeof(machine.registers.gpr[index])) == 0);
  for (unsigned index = 0u; index != 32u; ++index) {
    if (index != 31u)
      CHECK(std::memcmp(&fixture.progress.state.control[index],
                        &state.control[index],
                        sizeof(state.control[index])) == 0);
    if (index != 0u && index != 1u && index != 9u && index != 10u &&
        index != 11u && index != 25u && index != 26u && index != 27u)
      CHECK(std::memcmp(&fixture.progress.state.data[index], &state.data[index],
                        sizeof(state.data[index])) == 0);
  }
  const uint32_t pcs[] = {UINT32_C(0x80056650), UINT32_C(0x80056654),
                          UINT32_C(0x80056660), UINT32_C(0x80056664),
                          UINT32_C(0x80056668), UINT32_C(0x80056674)};
  const uint32_t addresses[] = {kInput,       kInput + 4u,  kOutput,
                                kOutput + 4u, kOutput + 8u, kFlag};
  for (unsigned index = 0u; index != 6u; ++index) {
    CHECK(fixture.journal[index].pc == pcs[index]);
    CHECK(fixture.journal[index].address == addresses[index]);
    CHECK(fixture.journal[index].operation ==
          (index < 2u ? index + 1u : index + 2u));
  }
}

void test_prefixes_and_failures() {
  for (size_t budget = 0u; budget != 7u; ++budget) {
    Fixture fixture;
    fixture.context.operation_budget = budget;
    CHECK(fixture.run() == NBA97_TEXT_LIMIT);
    CHECK(fixture.progress.operations == budget);
  }
  Fixture exact;
  exact.context.operation_budget = 7u;
  CHECK(exact.run() == NBA97_TEXT_COMPLETE);

  Fixture final_delay;
  final_delay.context.operation_budget = 6u;
  CHECK(final_delay.run() == NBA97_TEXT_LIMIT);
  CHECK(final_delay.progress.stores == 3u);
  CHECK(final_delay.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
            .word == UINT32_C(0x87654321));
  CHECK(final_delay.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
            .known_mask == 15u);
  CHECK(final_delay.progress.stopped_pc == UINT32_C(0x80056674));

  Fixture refused;
  refused.mock.result = NBA97_TEXT_IO_REFUSED;
  CHECK(refused.run() == NBA97_TEXT_IO_REFUSED);
  CHECK(refused.progress.reads == 2u && refused.progress.stores == 0u);
  CHECK(refused.progress.stopped_pc == UINT32_C(0x8005665c));

  Fixture malformed_callback;
  malformed_callback.mock.malformed = true;
  CHECK(malformed_callback.run() == NBA97_TEXT_ARGUMENT);
  CHECK(malformed_callback.progress.state.data[30].known_mask == 16u);
  CHECK(malformed_callback.progress.stores == 0u);

  Fixture late;
  late.known[late.offset(kInput + 7u)] = 2u;
  const auto old_data1 = late.context.state.data[1];
  CHECK(late.run() == NBA97_TEXT_ARGUMENT);
  CHECK(late.progress.state.data[0].word == UINT32_C(0x80017fff));
  CHECK(std::memcmp(&late.progress.state.data[1], &old_data1,
                    sizeof(old_data1)) == 0);
  CHECK(late.progress.reads == 1u && late.mock.calls == 0u);

  Fixture unknown_address;
  unknown_address.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
      .known_mask = 14u;
  CHECK(unknown_address.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknown_address.progress.operations == 0u);

  Fixture unknown_a1;
  unknown_a1.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
      .known_mask = 7u;
  CHECK(unknown_a1.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknown_a1.progress.operations == 3u);

  Fixture unknown_a2;
  unknown_a2.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2]
      .known_mask = 7u;
  CHECK(unknown_a2.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknown_a2.progress.stores == 3u);

  Fixture unknown_ra;
  unknown_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .known_mask = 7u;
  CHECK(unknown_ra.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknown_ra.progress.stores == 4u);
  CHECK(unknown_ra.get(kFlag) == UINT32_C(0x87654321));
  CHECK(unknown_ra.progress.stopped_pc == UINT32_C(0x80056670));

  Fixture alignment;
  alignment.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word += 2u;
  CHECK(alignment.run() == NBA97_TEXT_ALIGNMENT_TRAP);
  CHECK(alignment.progress.operations == 1u);

  Fixture unmapped;
  unmapped.context.memory.count = 0u;
  CHECK(unmapped.run() == NBA97_TEXT_RESOURCE);

  Fixture invalid_gpr;
  invalid_gpr.context.machine.registers.gpr[14].known_mask = 16u;
  CHECK(invalid_gpr.run() == NBA97_TEXT_ARGUMENT);
  CHECK(invalid_gpr.progress.operations == 0u);

  Fixture invalid_state;
  invalid_state.context.state.control[7].known_mask = 16u;
  CHECK(invalid_state.run() == NBA97_TEXT_ARGUMENT);

  Fixture no_hardware;
  no_hardware.context.hardware = nullptr;
  CHECK(no_hardware.run() == NBA97_TEXT_IO_REFUSED);

  Fixture unknown_store;
  unknown_store.mock.output_mask = 7u;
  unknown_store.region.known = nullptr;
  const uint32_t before = unknown_store.get(kOutput);
  CHECK(unknown_store.run() == NBA97_TEXT_ARGUMENT);
  CHECK(unknown_store.get(kOutput) == before);
  CHECK(unknown_store.progress.operations == 4u);

  Fixture short_journal;
  short_journal.context.access_journal_capacity = 2u;
  CHECK(short_journal.run() == NBA97_TEXT_COMPLETE);
  CHECK(short_journal.progress.access_events == 6u);
}

void test_all_byte_masks() {
  for (uint8_t mask = 0u; mask != 16u; ++mask) {
    Fixture input;
    for (unsigned byte = 0u; byte != 4u; ++byte) {
      input.known[input.offset(kInput) + byte] =
          static_cast<uint8_t>((mask >> byte) & 1u);
      input.known[input.offset(kInput + 4u) + byte] =
          static_cast<uint8_t>((mask >> byte) & 1u);
    }
    CHECK(input.run() == NBA97_TEXT_COMPLETE);
    CHECK(input.progress.state.data[0].known_mask == mask);
    const uint8_t signed_mask =
        static_cast<uint8_t>((mask & 3u) | ((mask & 2u) != 0u ? 12u : 0u));
    CHECK(input.progress.state.data[1].known_mask == signed_mask);

    Fixture output;
    output.mock.output_mask = mask;
    CHECK(output.run() == NBA97_TEXT_COMPLETE);
    for (unsigned byte = 0u; byte != 4u; ++byte) {
      const uint8_t expected = static_cast<uint8_t>((mask >> byte) & 1u);
      CHECK(output.known[output.offset(kOutput) + byte] == expected);
      CHECK(output.known[output.offset(kOutput + 4u) + byte] == expected);
      CHECK(output.known[output.offset(kOutput + 8u) + byte] == expected);
      CHECK(output.known[output.offset(kFlag) + byte] == expected);
    }
  }
}

void test_alias_wrap_and_partial_knownness() {
  Fixture alias;
  set_word(alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1],
           kInput);
  set_word(alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2],
           kInput + 4u);
  CHECK(alias.run() == NBA97_TEXT_COMPLETE);
  CHECK(alias.get(kInput) == UINT32_C(0x11112222));
  CHECK(alias.get(kInput + 4u) == UINT32_C(0x87654321));
  CHECK(alias.progress.state.data[1].word == UINT32_C(0xffff8001));

  Fixture partial;
  partial.known[partial.offset(kInput + 6u)] = 0u;
  partial.known[partial.offset(kInput + 7u)] = 0u;
  CHECK(partial.run() == NBA97_TEXT_COMPLETE);
  CHECK(partial.progress.state.data[1].known_mask == 15u);

  std::array<uint8_t, 4> high{{1u, 0u, 0xffu, 0x7fu}};
  std::array<uint8_t, 4> high_known{{1u, 1u, 1u, 1u}};
  std::array<uint8_t, 16> low{};
  std::array<uint8_t, 16> low_known{};
  low.fill(0u);
  low_known.fill(1u);
  Nba97GameTextRegion regions[2] = {
      {UINT32_C(0xfffffffc), high.data(), high_known.data(), high.size()},
      {0u, low.data(), low_known.data(), low.size()}};
  Fixture wrap;
  wrap.context.memory = {regions, 2u};
  set_word(wrap.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
           UINT32_C(0xfffffffc));
  set_word(wrap.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1], 4u);
  set_word(wrap.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2], 0u);
  CHECK(wrap.run() == NBA97_TEXT_COMPLETE);
  CHECK(wrap.progress.state.data[0].word == UINT32_C(0x7fff0001));
  CHECK(wrap.progress.state.data[1].word == 0u);
}

void seed_identity(Nba97GameGteReferenceTransformState &state) {
  for (auto &word : state.control)
    set_word(word, UINT32_C(0x40000000));
  for (auto &word : state.data)
    set_word(word, UINT32_C(0x50000000));
  set_word(state.control[0], 4096u);
  set_word(state.control[1], 0u);
  set_word(state.control[2], 4096u);
  set_word(state.control[3], 0u);
  set_word(state.control[4], 4096u, 3u);
  set_word(state.control[5], 10u);
  set_word(state.control[6], 20u);
  set_word(state.control[7], 30u);
  set_word(state.data[0], UINT32_C(0x00020001));
  set_word(state.data[1], UINT32_C(0xffff0003));
}

void test_geometry_hardware() {
  nba97::GamePlayerGeometry geometry{};
  Nba97GameGteReferenceTransformGeometryBinding binding{};
  nba97_game_gte_reference_transform_geometry_binding_init(&binding, &geometry);
  Nba97GameGteReferenceTransformHardwareEvent event{
      UINT32_C(0x8005665c), UINT32_C(0x00480012), 3u, 0u};
  Nba97GameGteReferenceTransformState state{};
  seed_identity(state);
  const auto before = state;
  CHECK(nba97_game_gte_reference_transform_geometry_hardware(
            &binding, &event, &state) == NBA97_TEXT_COMPLETE);
  CHECK(state.data[25].word == 11u && state.data[26].word == 22u &&
        state.data[27].word == 33u);
  CHECK(state.data[9].word == 11u && state.data[10].word == 22u &&
        state.data[11].word == 33u);
  CHECK(state.control[31].word == 0u);
  for (unsigned index = 0u; index != 32u; ++index) {
    if (index != 31u)
      CHECK(std::memcmp(&state.control[index], &before.control[index],
                        sizeof(state.control[index])) == 0);
    if (index != 9u && index != 10u && index != 11u && index != 25u &&
        index != 26u && index != 27u)
      CHECK(std::memcmp(&state.data[index], &before.data[index],
                        sizeof(state.data[index])) == 0);
  }

  Nba97GameGteReferenceTransformState general{};
  seed_identity(general);
  set_word(general.control[0], UINT32_C(0x00001000));
  set_word(general.control[1], UINT32_C(0x10000000));
  set_word(general.control[2], UINT32_C(0x00001000));
  set_word(general.control[3], UINT32_C(0xffff0000));
  set_word(general.control[4], UINT32_C(0xdead1000), 3u);
  set_word(general.data[0], UINT32_C(0xffff7fff));
  set_word(general.data[1], UINT32_C(0xaaaa8000), 3u);
  CHECK(nba97_game_gte_reference_transform_geometry_hardware(
            &binding, &event, &general) == NBA97_TEXT_COMPLETE);
  CHECK(general.data[25].known_mask == 15u &&
        general.data[9].known_mask == 15u);

  Nba97GameGteReferenceTransformState wide{};
  seed_identity(wide);
  for (unsigned index = 0u; index != 8u; ++index)
    set_word(wide.control[index], UINT32_C(0x7fffffff));
  set_word(wide.control[4], UINT32_C(0x00007fff), 3u);
  set_word(wide.data[0], UINT32_C(0x80007fff));
  set_word(wide.data[1], UINT32_C(0x0000ffff), 3u);
  CHECK(nba97_game_gte_reference_transform_geometry_hardware(
            &binding, &event, &wide) == NBA97_TEXT_COMPLETE);
  CHECK(wide.data[25].word == UINT32_C(0x7ffbffff));
  CHECK(wide.data[26].word == UINT32_C(0x8003ffef));
  CHECK(wide.data[27].word == UINT32_C(0x7ffbfff7));
  CHECK(wide.data[9].word == UINT32_C(0x00007fff));
  CHECK(wide.data[10].word == UINT32_C(0xffff8000));
  CHECK(wide.data[11].word == UINT32_C(0x00007fff));
  CHECK(wide.control[31].word == UINT32_C(0xa1c00000));

  Nba97GameGteReferenceTransformState unknown{};
  seed_identity(unknown);
  unknown.control[0].known_mask = 14u;
  const auto unknown_before = unknown;
  CHECK(nba97_game_gte_reference_transform_geometry_hardware(
            &binding, &event, &unknown) == NBA97_TEXT_UNKNOWN);
  CHECK(std::memcmp(&unknown, &unknown_before, sizeof(unknown)) == 0);

  Nba97GameGteReferenceTransformState malformed{};
  seed_identity(malformed);
  malformed.data[31].known_mask = 16u;
  CHECK(nba97_game_gte_reference_transform_geometry_hardware(
            &binding, &event, &malformed) == NBA97_TEXT_ARGUMENT);

  event.command ^= 1u;
  CHECK(nba97_game_gte_reference_transform_geometry_hardware(
            &binding, &event, &state) == NBA97_TEXT_ARGUMENT);
}

void test_determinism() {
  Fixture first;
  Fixture second;
  CHECK(first.run() == NBA97_TEXT_COMPLETE);
  CHECK(second.run() == NBA97_TEXT_COMPLETE);
  CHECK(std::memcmp(&first.progress, &second.progress,
                    sizeof(first.progress)) == 0);
  CHECK(first.ram == second.ram);
  CHECK(first.known == second.known);
}

} // namespace

int main() {
  test_exact_path_and_preservation();
  test_prefixes_and_failures();
  test_all_byte_masks();
  test_alias_wrap_and_partial_knownness();
  test_geometry_hardware();
  test_determinism();
  std::printf("game_gte_reference_transform_tests: %u checks passed\n", checks);
  return 0;
}
