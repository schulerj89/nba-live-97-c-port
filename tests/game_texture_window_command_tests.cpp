#include "recovered/game_texture_window_command.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

size_t checks;

void check(bool condition, const char *message) {
  ++checks;
  if (!condition) {
    std::fprintf(stderr, "game_texture_window_command_tests: %s\n", message);
    std::exit(1);
  }
}

bool same_word(const Nba97GameTextureWindowCommandWord &left,
               const Nba97GameTextureWindowCommandWord &right) {
  return left.word == right.word && left.known_mask == right.known_mask;
}

bool same_machine(const Nba97GameTextureWindowCommandMachine &left,
                  const Nba97GameTextureWindowCommandMachine &right) {
  for (unsigned index = 0u; index != 32u; ++index)
    if (!same_word(left.registers.gpr[index], right.registers.gpr[index]))
      return false;
  return same_word(left.hi, right.hi) && same_word(left.lo, right.lo);
}

void put16(uint8_t *data, size_t offset, uint16_t value) {
  data[offset] = static_cast<uint8_t>(value);
  data[offset + 1u] = static_cast<uint8_t>(value >> 8u);
}

uint32_t expected(uint8_t x, uint8_t y, uint16_t width, uint16_t height) {
  const uint32_t width_mask =
      static_cast<uint32_t>(-static_cast<int32_t>(static_cast<int16_t>(width))) &
      0xffu;
  const uint32_t height_mask =
      static_cast<uint32_t>(-static_cast<int32_t>(static_cast<int16_t>(height))) &
      0xffu;
  return UINT32_C(0xe2000000) | (static_cast<uint32_t>(x) >> 3u) << 10u |
         (static_cast<uint32_t>(y) >> 3u) << 15u |
         (height_mask >> 3u) << 5u | (width_mask >> 3u);
}

struct Fixture {
  static constexpr uint32_t base = UINT32_C(0x80000000);
  static constexpr uint32_t input = UINT32_C(0x80001000);
  static constexpr uint32_t stack = UINT32_C(0x80002010);
  std::array<uint8_t, 0x4000> data{};
  std::array<uint8_t, 0x4000> known{};
  Nba97GameTextRegion region{base, data.data(), known.data(), data.size()};
  std::array<Nba97GameTextureWindowCommandAccess, 8> journal{};
  Nba97GameTextureWindowCommandContext context{};
  Nba97GameTextureWindowCommandProgress progress{};

  Fixture() {
    known.fill(1u);
    for (unsigned index = 0u; index != 32u; ++index)
      context.machine.registers.gpr[index] =
          {UINT32_C(0x41000000) + index * UINT32_C(0x01010101),
           static_cast<uint8_t>((index % 15u) + 1u)};
    context.machine.registers.gpr[0] = {0u, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {input, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {stack, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {UINT32_C(0x80001234), 15u};
    context.machine.hi = {UINT32_C(0x11223344), 5u};
    context.machine.lo = {UINT32_C(0x55667788), 10u};
    context.memory = {&region, 1u};
    context.operation_budget = 8u;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    set_rectangle(0x3fu, 0x87u, UINT16_C(0xfff1), UINT16_C(0x8001));
  }

  void set_rectangle(uint8_t x, uint8_t y, uint16_t width, uint16_t height) {
    const size_t offset = input - base;
    data[offset] = x;
    data[offset + 1u] = 0xa5u;
    data[offset + 2u] = y;
    data[offset + 3u] = 0x5au;
    put16(data.data(), offset + 4u, width);
    put16(data.data(), offset + 6u, height);
  }

  int run() { return nba97_game_texture_window_command(&context, &progress); }
};

void normal_and_budgets() {
  Fixture complete;
  auto original = complete.context.machine;
  check(complete.run() == NBA97_TEXT_COMPLETE, "normal completes");
  check(complete.progress.operations == 8u && complete.progress.reads == 4u &&
            complete.progress.stores == 4u &&
            complete.progress.access_events == 8u,
        "normal operation counts");
  const uint32_t result = expected(0x3fu, 0x87u, UINT16_C(0xfff1),
                                   UINT16_C(0x8001));
  check(complete.progress.return_v0.word == result &&
            complete.progress.return_v0.known_mask == 15u,
        "packed E2 return");
  check(complete.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                .word == Fixture::stack,
        "stack restored");
  check(complete.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
                .word == (UINT32_C(0xe2000000) | (0x3fu >> 3u) << 10u),
        "final a1");
  check(complete.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2]
                .word == ((static_cast<uint32_t>(-static_cast<int32_t>(
                                  static_cast<int16_t>(UINT16_C(0xfff1)))) &
                            0xffu) >>
                           3u),
        "final a2");
  check(complete.progress.machine.hi.word == original.hi.word &&
            complete.progress.machine.hi.known_mask == original.hi.known_mask &&
            complete.progress.machine.lo.word == original.lo.word &&
            complete.progress.machine.lo.known_mask == original.lo.known_mask,
        "HI LO preserved");
  for (unsigned index = 0u; index != 32u; ++index) {
    if (index == NBA97_MATCH_INITIALIZE_V0 ||
        index == NBA97_MATCH_INITIALIZE_V1 ||
        index == NBA97_MATCH_INITIALIZE_A0 ||
        index == NBA97_MATCH_INITIALIZE_A1 ||
        index == NBA97_MATCH_INITIALIZE_A2)
      continue;
    check(same_word(complete.progress.machine.registers.gpr[index],
                    original.registers.gpr[index]),
          "unmodified GPR preserved");
  }

  const uint32_t pcs[8] = {UINT32_C(0x8009a834), UINT32_C(0x8009a840),
                           UINT32_C(0x8009a844), UINT32_C(0x8009a858),
                           UINT32_C(0x8009a85c), UINT32_C(0x8009a868),
                           UINT32_C(0x8009a870), UINT32_C(0x8009a898)};
  const uint32_t addresses[8] = {
      Fixture::input, Fixture::stack - 0x10u, Fixture::input + 4u,
      Fixture::stack - 8u, Fixture::input + 2u, Fixture::stack - 12u,
      Fixture::input + 6u, Fixture::stack - 4u};
  const uint8_t widths[8] = {1u, 4u, 2u, 4u, 1u, 4u, 2u, 4u};
  for (unsigned index = 0u; index != 8u; ++index) {
    check(complete.journal[index].pc == pcs[index], "journal pc order");
    check(complete.journal[index].address == addresses[index],
          "journal address order");
    check(complete.journal[index].width == widths[index], "journal width");
    check(complete.journal[index].operation == index + 1u,
          "journal operation number");
  }
  check(complete.journal[1].value == 7u &&
            complete.journal[3].value == 1u &&
            complete.journal[5].value == 16u &&
            complete.journal[7].value == 31u,
        "exact scratch store values");

  for (size_t budget = 0u; budget != 8u; ++budget) {
    Fixture limited;
    limited.context.operation_budget = budget;
    check(limited.run() == NBA97_TEXT_LIMIT, "access budget limits");
    check(limited.progress.operations == budget, "budget exact prefix");
    check(limited.progress.access_events == budget,
          "completed access prefix count");
    check(limited.progress.stopped_pc == pcs[budget], "budget stop pc");
    check(limited.progress.stopped_address == addresses[budget],
          "budget stop address");
  }
}

void null_unknown_and_ra_paths() {
  Fixture null_path;
  null_path.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
      {0u, 15u};
  null_path.context.operation_budget = 0u;
  check(null_path.run() == NBA97_TEXT_COMPLETE, "null path completes");
  check(null_path.progress.operations == 0u && null_path.progress.reads == 0u &&
            null_path.progress.stores == 0u,
        "null path no accesses");
  check(null_path.progress.return_v0.word == 0u &&
            null_path.progress.return_v0.known_mask == 15u,
        "null return zero");
  check(null_path.progress.frame_stack_pointer == Fixture::stack - 0x10u &&
            null_path.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                    .word == Fixture::stack,
        "null delay frame and restore");

  Fixture unknown;
  unknown.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {0u, 14u};
  check(unknown.run() == NBA97_TEXT_UNKNOWN, "unknown a0 branch refuses");
  check(unknown.progress.stopped_pc == UINT32_C(0x8009a824) &&
            unknown.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                    .word == Fixture::stack - 0x10u,
        "unknown branch delay prefix");

  for (unsigned byte = 0u; byte != 4u; ++byte) {
    Fixture nonzero;
    nonzero.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
        {1u << (8u * byte), static_cast<uint8_t>(1u << byte)};
    check(nonzero.run() == NBA97_TEXT_UNKNOWN,
          "known nonzero byte decides branch before address refusal");
    check(nonzero.progress.stopped_pc == UINT32_C(0x8009a834),
          "partial address stops at first load");
  }

  Fixture unknown_ra;
  unknown_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .known_mask = 14u;
  check(unknown_ra.run() == NBA97_TEXT_UNKNOWN, "unknown RA refuses");
  check(unknown_ra.progress.stopped_pc == UINT32_C(0x8009a8a0) &&
            unknown_ra.progress.machine.registers
                    .gpr[NBA97_MATCH_INITIALIZE_SP]
                    .word == Fixture::stack,
        "RA refusal after SP restore");
  check(unknown_ra.progress.return_v0.word ==
            expected(0x3fu, 0x87u, UINT16_C(0xfff1), UINT16_C(0x8001)),
        "RA refusal preserves result");
}

void coordinate_and_extent_matrix() {
  for (unsigned value = 0u; value != 256u; ++value) {
    Fixture x;
    x.set_rectangle(static_cast<uint8_t>(value), 0u, 0u, 0u);
    check(x.run() == NBA97_TEXT_COMPLETE, "x matrix completes");
    check(x.progress.return_v0.word ==
              expected(static_cast<uint8_t>(value), 0u, 0u, 0u),
          "x matrix encoding");
    Fixture y;
    y.set_rectangle(0u, static_cast<uint8_t>(value), 0u, 0u);
    check(y.run() == NBA97_TEXT_COMPLETE, "y matrix completes");
    check(y.progress.return_v0.word ==
              expected(0u, static_cast<uint8_t>(value), 0u, 0u),
          "y matrix encoding");
  }
  const uint16_t extrema[] = {0u, 1u, 7u, 8u, 255u, 256u, 0x7fffu,
                              0x8000u, 0xffffu};
  for (uint16_t width : extrema)
    for (uint16_t height : extrema) {
      Fixture fixture;
      fixture.set_rectangle(0x18u, 0x20u, width, height);
      check(fixture.run() == NBA97_TEXT_COMPLETE, "extent matrix completes");
      check(fixture.progress.return_v0.word ==
                expected(0x18u, 0x20u, width, height),
            "signed wrapping extent encoding");
    }

  Fixture ignored;
  ignored.data[Fixture::input - Fixture::base + 1u] = 0xffu;
  ignored.data[Fixture::input - Fixture::base + 3u] = 0xffu;
  check(ignored.run() == NBA97_TEXT_COMPLETE &&
            ignored.progress.return_v0.word ==
                expected(0x3fu, 0x87u, UINT16_C(0xfff1), UINT16_C(0x8001)),
        "upper coordinate bytes ignored");
}

void masks_aliases_and_atomic_failures() {
  for (unsigned mask = 0u; mask != 64u; ++mask) {
    Fixture fixture;
    const size_t input = Fixture::input - Fixture::base;
    fixture.known[input] = static_cast<uint8_t>(mask & 1u);
    fixture.known[input + 4u] = static_cast<uint8_t>((mask >> 1u) & 1u);
    fixture.known[input + 5u] = static_cast<uint8_t>((mask >> 2u) & 1u);
    fixture.known[input + 2u] = static_cast<uint8_t>((mask >> 3u) & 1u);
    fixture.known[input + 6u] = static_cast<uint8_t>((mask >> 4u) & 1u);
    fixture.known[input + 7u] = static_cast<uint8_t>((mask >> 5u) & 1u);
    check(fixture.run() == NBA97_TEXT_COMPLETE, "input mask matrix completes");
    const uint8_t expected_mask = static_cast<uint8_t>(
        8u | (((mask & 9u) == 9u) ? 4u : 0u) |
        (((mask & 25u) == 25u) ? 2u : 0u) |
        (((mask & 18u) == 18u) ? 1u : 0u));
    check(fixture.progress.return_v0.word ==
              expected(0x3fu, 0x87u, UINT16_C(0xfff1), UINT16_C(0x8001)),
          "raw result retained through unknown inputs");
    check(fixture.progress.return_v0.known_mask == expected_mask,
          "exact result byte knownness");
  }

  Fixture alias;
  const uint32_t aliased_input = Fixture::stack - 0x10u;
  alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
      {aliased_input, 15u};
  const size_t offset = aliased_input - Fixture::base;
  alias.data[offset] = 0x18u;
  alias.data[offset + 2u] = 0x20u;
  put16(alias.data.data(), offset + 4u, 8u);
  put16(alias.data.data(), offset + 6u, 16u);
  check(alias.run() == NBA97_TEXT_COMPLETE, "stack input alias completes");
  check(alias.progress.return_v0.word ==
            expected(0x18u, 0u, 8u, 0u),
        "later reads observe earlier stack stores");

  Fixture unaligned;
  unaligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word++;
  check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP,
        "unaligned halfword traps after first store");
  check(unaligned.progress.stopped_pc == UINT32_C(0x8009a844) &&
            unaligned.progress.stores == 1u,
        "unaligned exact prefix");

  Fixture unaligned_stack;
  unaligned_stack.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word++;
  check(unaligned_stack.run() == NBA97_TEXT_ALIGNMENT_TRAP,
        "unaligned stack word traps");
  check(unaligned_stack.progress.stopped_pc == UINT32_C(0x8009a840) &&
            unaligned_stack.progress.reads == 1u &&
            unaligned_stack.progress.stores == 0u,
        "unaligned stack exact prefix");

  Fixture unmapped;
  unmapped.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word =
      UINT32_C(0x90000000);
  check(unmapped.run() == NBA97_TEXT_RESOURCE, "unmapped input refuses");

  Fixture malformed;
  malformed.known[Fixture::input - Fixture::base + 5u] = 2u;
  auto original_a2 =
      malformed.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2];
  check(malformed.run() == NBA97_TEXT_ARGUMENT, "malformed known byte rejects");
  check(same_word(malformed.progress.machine.registers
                      .gpr[NBA97_MATCH_INITIALIZE_A2],
                  original_a2),
        "malformed load destination atomic");

  Fixture no_known;
  no_known.region.known = nullptr;
  no_known.known[Fixture::input - Fixture::base] = 0u;
  check(no_known.run() == NBA97_TEXT_COMPLETE,
        "known-null map accepts full-known stores");

  Fixture partial_store;
  const size_t input_offset = Fixture::input - Fixture::base;
  const size_t frame_offset = Fixture::stack - 0x10u - Fixture::base;
  std::array<Nba97GameTextRegion, 2> split = {
      Nba97GameTextRegion{Fixture::input, partial_store.data.data() + input_offset,
                          partial_store.known.data() + input_offset, 8u},
      Nba97GameTextRegion{Fixture::stack - 0x10u,
                          partial_store.data.data() + frame_offset, nullptr,
                          16u}};
  partial_store.known[input_offset] = 0u;
  partial_store.context.memory = {split.data(), split.size()};
  std::array<uint8_t, 16> before{};
  std::memcpy(before.data(), partial_store.data.data() + frame_offset,
              before.size());
  check(partial_store.run() == NBA97_TEXT_ARGUMENT,
        "partial word refuses known-null store");
  check(partial_store.progress.stopped_pc == UINT32_C(0x8009a840) &&
            partial_store.progress.reads == 1u &&
            partial_store.progress.stores == 0u,
        "known-null refusal prefix");
  check(std::memcmp(before.data(), partial_store.data.data() + frame_offset,
                    before.size()) == 0,
        "rejected partial store leaves bytes unchanged");
}

void invalid_context_determinism_and_wrap() {
  Fixture bad_zero;
  bad_zero.context.machine.registers.gpr[0].word = 1u;
  check(bad_zero.run() == NBA97_TEXT_ARGUMENT, "bad zero rejects");
  Fixture bad_mask;
  bad_mask.context.machine.hi.known_mask = 16u;
  check(bad_mask.run() == NBA97_TEXT_ARGUMENT, "bad machine mask rejects");
  Fixture bad_journal;
  bad_journal.context.access_journal = nullptr;
  check(bad_journal.run() == NBA97_TEXT_ARGUMENT, "missing journal rejects");
  Fixture bad_map;
  bad_map.context.memory.region = nullptr;
  check(bad_map.run() == NBA97_TEXT_ARGUMENT, "missing map rejects");
  Fixture overlap;
  std::array<Nba97GameTextRegion, 2> overlapping = {
      overlap.region,
      Nba97GameTextRegion{Fixture::base + 1u, overlap.data.data() + 1u,
                          overlap.known.data() + 1u, 1u}};
  overlap.context.memory = {overlapping.data(), overlapping.size()};
  check(overlap.run() == NBA97_TEXT_ARGUMENT, "overlapping map rejects");

  Fixture wrap;
  wrap.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {0u, 15u};
  wrap.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {8u, 15u};
  check(wrap.run() == NBA97_TEXT_COMPLETE, "SP wrap null completes");
  check(wrap.progress.frame_stack_pointer == UINT32_C(0xfffffff8) &&
            wrap.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
                8u,
        "SP wraps and restores");

  Fixture mapped_wrap;
  std::array<uint8_t, 8> high_data{};
  std::array<uint8_t, 8> high_known{};
  std::array<uint8_t, 8> low_data{};
  std::array<uint8_t, 8> low_known{};
  high_known.fill(1u);
  low_known.fill(1u);
  const size_t mapped_input_offset = Fixture::input - Fixture::base;
  std::array<Nba97GameTextRegion, 3> wrap_regions = {
      Nba97GameTextRegion{Fixture::input,
                          mapped_wrap.data.data() + mapped_input_offset,
                          mapped_wrap.known.data() + mapped_input_offset, 8u},
      Nba97GameTextRegion{UINT32_C(0xfffffff8), high_data.data(),
                          high_known.data(), high_data.size()},
      Nba97GameTextRegion{0u, low_data.data(), low_known.data(),
                          low_data.size()}};
  mapped_wrap.context.memory = {wrap_regions.data(), wrap_regions.size()};
  mapped_wrap.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
      {8u, 15u};
  check(mapped_wrap.run() == NBA97_TEXT_COMPLETE,
        "mapped SP wrap completes");
  check(mapped_wrap.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                .word == 8u &&
            mapped_wrap.journal[1].address == UINT32_C(0xfffffff8) &&
            mapped_wrap.journal[3].address == 0u &&
            mapped_wrap.journal[5].address == UINT32_C(0xfffffffc) &&
            mapped_wrap.journal[7].address == 4u,
        "mapped wrap store addresses and restoration");

  Fixture first;
  Fixture second;
  first.context.access_journal_capacity = 0u;
  second.context.access_journal_capacity = 0u;
  check(first.run() == NBA97_TEXT_COMPLETE &&
            second.run() == NBA97_TEXT_COMPLETE,
        "truncated journal runs");
  check(first.progress.access_events == 8u &&
            same_machine(first.progress.machine, second.progress.machine),
        "deterministic full machine");
}

} // namespace

int main() {
  normal_and_budgets();
  null_unknown_and_ra_paths();
  coordinate_and_extent_matrix();
  masks_aliases_and_atomic_failures();
  invalid_context_determinism_and_wrap();
  std::printf("game_texture_window_command_tests: %zu checks passed\n",
              checks);
  return 0;
}
