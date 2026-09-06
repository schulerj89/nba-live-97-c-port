#include "recovered/game_draw_mode_command.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

size_t checks;

void check(bool condition, const char *message) {
  ++checks;
  if (!condition) {
    std::fprintf(stderr, "game_draw_mode_command_tests: %s\n", message);
    std::exit(1);
  }
}

struct Fixture {
  uint8_t type{1u};
  uint8_t known{1u};
  Nba97GameTextRegion region{UINT32_C(0x800c55c0), &type, &known, 1u};
  Nba97GameDrawModeCommandAccess journal{};
  Nba97GameDrawModeCommandContext context{};
  Nba97GameDrawModeCommandProgress progress{};

  Fixture() {
    context.memory = {&region, 1u};
    context.operation_budget = 1u;
    for (unsigned index = 0u; index != 32u; ++index) {
      context.machine.registers.gpr[index].word =
          UINT32_C(0x21000000) + index * UINT32_C(0x01020304);
      context.machine.registers.gpr[index].known_mask =
          static_cast<uint8_t>((index * 5u) & 15u);
    }
    context.machine.registers.gpr[0] = {0u, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {0u, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] = {0u, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2] =
        {UINT32_C(0xabcdef12), 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {UINT32_C(0x8009a3d0), 15u};
    context.machine.hi = {UINT32_C(0x12345678), 5u};
    context.machine.lo = {UINT32_C(0x89abcdef), 10u};
    context.access_journal = &journal;
    context.access_journal_capacity = 1u;
  }

  int run() { return nba97_game_draw_mode_command(&context, &progress); }
};

uint32_t expected(uint8_t type, uint32_t a0, uint32_t a1, uint32_t a2) {
  if (type == 1u || type == 2u)
    return UINT32_C(0xe1000000) | (a1 != 0u ? 0x800u : 0u) |
           (a2 & 0x27ffu) | (a0 != 0u ? 0x1000u : 0u);
  return UINT32_C(0xe1000000) | (a1 != 0u ? 0x200u : 0u) |
         (a2 & 0x09ffu) | (a0 != 0u ? 0x400u : 0u);
}

void all_types_and_encodings() {
  for (unsigned type = 0u; type != 256u; ++type) {
    Fixture fixture;
    fixture.type = static_cast<uint8_t>(type);
    fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
        {UINT32_C(0x80000000), 15u};
    fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] =
        {UINT32_C(0x01000000), 15u};
    check(fixture.run() == NBA97_TEXT_COMPLETE, "all types complete");
    check(fixture.progress.return_v0.word ==
              expected(static_cast<uint8_t>(type), UINT32_C(0x80000000),
                       UINT32_C(0x01000000), UINT32_C(0xabcdef12)),
          "all types exact command");
    check(fixture.progress.return_v0.known_mask == 15u,
          "all types full known command");
  }

  const uint32_t values[] = {0u, 1u, UINT32_C(0x80000000),
                             UINT32_C(0x00010000)};
  for (uint8_t type : {uint8_t{1u}, uint8_t{2u}, uint8_t{0u},
                       uint8_t{255u}})
    for (uint32_t a0 : values)
      for (uint32_t a1 : values) {
        Fixture fixture;
        fixture.type = type;
        fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
            {a0, 15u};
        fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] =
            {a1, 15u};
        check(fixture.run() == NBA97_TEXT_COMPLETE,
              "full-word Boolean matrix complete");
        check(fixture.progress.return_v0.word ==
                  expected(type, a0, a1, UINT32_C(0xabcdef12)),
              "full-word Boolean matrix command");
      }
}

void a2_masks_and_boundaries() {
  const uint32_t values[] = {0u, UINT32_C(0xffffffff), 0x200u, 0x400u,
                             0x800u, 0x1000u, 0x27ffu, 0x2800u};
  for (uint8_t type : {uint8_t{1u}, uint8_t{3u}})
    for (uint32_t value : values)
      for (unsigned mask = 0u; mask != 16u; ++mask) {
        Fixture fixture;
        fixture.type = type;
        fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2] =
            {value, static_cast<uint8_t>(mask)};
        check(fixture.run() == NBA97_TEXT_COMPLETE,
              "a2 mask matrix completes");
        check(fixture.progress.return_v0.word == expected(type, 0u, 0u, value),
              "a2 mask matrix raw command");
        check(fixture.progress.return_v0.known_mask ==
                  static_cast<uint8_t>((mask & 3u) | 12u),
              "a2 output byte knownness");
      }
}

void full_machine_preservation() {
  Fixture fixture;
  auto original = fixture.context.machine;
  check(fixture.run() == NBA97_TEXT_COMPLETE, "preservation completes");
  for (unsigned index = 0u; index != 32u; ++index) {
    if (index == NBA97_MATCH_INITIALIZE_V0 ||
        index == NBA97_MATCH_INITIALIZE_V1)
      continue;
    check(fixture.progress.machine.registers.gpr[index].word ==
              original.registers.gpr[index].word,
          "preserved gpr word");
    check(fixture.progress.machine.registers.gpr[index].known_mask ==
              original.registers.gpr[index].known_mask,
          "preserved gpr mask");
  }
  check(std::memcmp(&fixture.progress.machine.hi, &original.hi,
                    sizeof(original.hi)) == 0,
        "hi preserved");
  check(std::memcmp(&fixture.progress.machine.lo, &original.lo,
                    sizeof(original.lo)) == 0,
        "lo preserved");
  check(fixture.progress.operations == 1u && fixture.progress.reads == 1u &&
            fixture.progress.accesses == 1u,
        "one read only");
  check(fixture.journal.pc == UINT32_C(0x8009a5f0) &&
            fixture.journal.address == UINT32_C(0x800c55c0) &&
            fixture.journal.width == 1u &&
            fixture.journal.kind == NBA97_GAME_MATCH_CLOCKS_READ,
        "exact read journal");
}

void known_and_unknown_branches() {
  for (uint8_t type : {uint8_t{1u}, uint8_t{3u}}) {
    const uint32_t a1_pc =
        type == 1u ? UINT32_C(0x8009a608) : UINT32_C(0x8009a624);
    const uint32_t a0_pc =
        type == 1u ? UINT32_C(0x8009a614) : UINT32_C(0x8009a630);
    const uint32_t payload_mask = type == 1u ? 0x27ffu : 0x09ffu;

    for (unsigned mask = 0u; mask != 15u; ++mask) {
      Fixture unknown_a1;
      unknown_a1.type = type;
      unknown_a1.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] =
          {0u, static_cast<uint8_t>(mask)};
      check(unknown_a1.run() == NBA97_TEXT_UNKNOWN,
            "partial known-zero a1 refuses");
      check(unknown_a1.progress.stopped_pc == a1_pc,
            "partial a1 branch pc");
      check(unknown_a1.progress.machine.registers
                    .gpr[NBA97_MATCH_INITIALIZE_V1]
                    .word == UINT32_C(0xe1000000),
            "a1 branch LUI delay prefix");

      Fixture unknown_a0;
      unknown_a0.type = type;
      unknown_a0.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
          {0u, static_cast<uint8_t>(mask)};
      check(unknown_a0.run() == NBA97_TEXT_UNKNOWN,
            "partial known-zero a0 refuses");
      check(unknown_a0.progress.stopped_pc == a0_pc,
            "partial a0 branch pc");
      check(unknown_a0.progress.machine.registers
                    .gpr[NBA97_MATCH_INITIALIZE_V0]
                    .word == (UINT32_C(0xabcdef12) & payload_mask),
            "a0 branch ANDI delay prefix");
    }

    for (unsigned byte = 0u; byte != 4u; ++byte) {
      Fixture known_nonzero;
      known_nonzero.type = type;
      known_nonzero.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
          {1u << (8u * byte), static_cast<uint8_t>(1u << byte)};
      known_nonzero.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] =
          {1u << (8u * byte), static_cast<uint8_t>(1u << byte)};
      check(known_nonzero.run() == NBA97_TEXT_COMPLETE,
            "known nonzero byte decides full-word branches");
    }
  }

  Fixture unknown_type;
  unknown_type.known = 0u;
  check(unknown_type.run() == NBA97_TEXT_UNKNOWN, "unknown type refuses");
  check(unknown_type.progress.stopped_pc == UINT32_C(0x8009a600),
        "unknown type branch pc");
  check(unknown_type.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .known_mask == 14u,
        "unknown type Boolean upper bytes known");
}

void budgets_failures_and_final_delay() {
  Fixture limited;
  limited.context.operation_budget = 0u;
  auto original_v1 = limited.context.machine.registers.gpr[3];
  check(limited.run() == NBA97_TEXT_LIMIT, "zero budget limits");
  check(limited.progress.stopped_pc == UINT32_C(0x8009a5f0) &&
            limited.progress.operations == 0u,
        "zero budget read prefix");
  check(limited.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            UINT32_C(0x800c55c0),
        "zero budget retains address calculation");
  check(std::memcmp(&limited.progress.machine.registers.gpr[3], &original_v1,
                    sizeof(original_v1)) == 0,
        "zero budget leaves v1 untouched");

  Fixture unknown_ra;
  unknown_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .known_mask = 14u;
  check(unknown_ra.run() == NBA97_TEXT_UNKNOWN, "unknown ra refuses");
  check(unknown_ra.progress.stopped_pc == UINT32_C(0x8009a63c),
        "unknown ra jr pc");
  check(unknown_ra.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .word ==
            expected(1u, 0u, 0u, UINT32_C(0xabcdef12)),
        "jr delay OR retained before unknown ra");
  check(unknown_ra.progress.return_v0.word ==
            expected(1u, 0u, 0u, UINT32_C(0xabcdef12)),
        "unknown ra publishes return word");
}

void invalid_contexts_and_determinism() {
  Fixture malformed;
  malformed.known = 2u;
  check(malformed.run() == NBA97_TEXT_ARGUMENT,
        "malformed source knownness rejects");
  check(malformed.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .word == UINT32_C(0x800c55c0),
        "malformed load destination atomic");

  Fixture unmapped;
  unmapped.context.memory = {nullptr, 0u};
  check(unmapped.run() == NBA97_TEXT_RESOURCE, "unmapped type refuses");

  Fixture bad_zero;
  bad_zero.context.machine.registers.gpr[0].word = 1u;
  check(bad_zero.run() == NBA97_TEXT_ARGUMENT, "bad zero register rejects");

  Fixture bad_mask;
  bad_mask.context.machine.hi.known_mask = 16u;
  check(bad_mask.run() == NBA97_TEXT_ARGUMENT, "bad machine mask rejects");

  Fixture bad_journal;
  bad_journal.context.access_journal = nullptr;
  check(bad_journal.run() == NBA97_TEXT_ARGUMENT,
        "missing journal rejects");

  Fixture overlap;
  uint8_t extra = 0u;
  uint8_t extra_known = 1u;
  std::array<Nba97GameTextRegion, 2> regions = {
      overlap.region,
      Nba97GameTextRegion{UINT32_C(0x800c55c0), &extra, &extra_known, 1u}};
  overlap.context.memory = {regions.data(), regions.size()};
  check(overlap.run() == NBA97_TEXT_ARGUMENT, "overlap map rejects");

  Fixture first;
  Fixture second;
  first.context.access_journal_capacity = 0u;
  second.context.access_journal_capacity = 0u;
  check(first.run() == NBA97_TEXT_COMPLETE &&
            second.run() == NBA97_TEXT_COMPLETE,
        "journal truncation runs");
  check(first.progress.access_events == 1u && second.progress.access_events == 1u,
        "truncated logical journal count");
  check(std::memcmp(&first.progress.machine, &second.progress.machine,
                    sizeof(first.progress.machine)) == 0,
        "deterministic machine");
}

} // namespace

int main() {
  all_types_and_encodings();
  a2_masks_and_boundaries();
  full_machine_preservation();
  known_and_unknown_branches();
  budgets_failures_and_final_delay();
  invalid_contexts_and_determinism();
  std::printf("game_draw_mode_command_tests: %zu checks passed\n", checks);
  return 0;
}
