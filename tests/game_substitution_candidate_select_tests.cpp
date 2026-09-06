#include "recovered/game_substitution_candidate_select.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
size_t checks;
void check(bool value, const char *message) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "game_substitution_candidate_select_tests: %s\n",
                 message);
    std::exit(1);
  }
}
void put(std::vector<uint8_t> &data, uint32_t address, uint32_t value,
         unsigned width) {
  for (unsigned b = 0u; b != width; ++b)
    data[address - UINT32_C(0x80000000) + b] =
        static_cast<uint8_t>(value >> (b * 8u));
}
uint32_t get(const std::vector<uint8_t> &data, uint32_t address,
             unsigned width) {
  uint32_t value = 0u;
  for (unsigned b = 0u; b != width; ++b)
    value |= static_cast<uint32_t>(data[address - UINT32_C(0x80000000) + b])
             << (b * 8u);
  return value;
}
bool same_word(const Nba97GameSubstitutionCandidateSelectWord &a,
               const Nba97GameSubstitutionCandidateSelectWord &b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}
bool same_machine(const Nba97GameSubstitutionCandidateSelectMachine &a,
                  const Nba97GameSubstitutionCandidateSelectMachine &b) {
  for (unsigned i = 0u; i != 32u; ++i)
    if (!same_word(a.registers.gpr[i], b.registers.gpr[i]))
      return false;
  return same_word(a.hi, b.hi) && same_word(a.lo, b.lo);
}
struct Fixture {
  static constexpr uint32_t base = UINT32_C(0x80000000);
  static constexpr size_t size = 0x200000u;
  static constexpr uint32_t team = UINT32_C(0x80010000);
  static constexpr uint32_t pointers = UINT32_C(0x80011000);
  static constexpr uint32_t player = UINT32_C(0x80012000);
  static constexpr uint32_t injury_source = UINT32_C(0x80013000);
  static constexpr uint32_t stack = UINT32_C(0x801ff000);
  std::vector<uint8_t> data = std::vector<uint8_t>(size, 0u);
  std::vector<uint8_t> known = std::vector<uint8_t>(size, 1u);
  Nba97GameTextRegion region{base, data.data(), known.data(), data.size()};
  std::array<Nba97GameSubstitutionCandidateSelectAccess, 128> journal{};
  Nba97GameSubstitutionCandidateSelectContext context{};
  Nba97GameSubstitutionCandidateSelectProgress progress{};
  Nba97GameSubstitutionCandidateSelectEvent event{};
  Nba97GameSubstitutionCandidateSelectMachine call_machine{};
  size_t calls{};
  bool reject{};
  bool relocate{};
  unsigned invalid_child{};
  bool malformed_return_load{};
  static constexpr uint32_t relocated_stack = UINT32_C(0x801fe000);

  Fixture() {
    context.memory = {&region, 1u};
    context.operation_budget = 512u;
    context.io = callback;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    for (unsigned i = 0u; i != 32u; ++i)
      context.machine.registers.gpr[i] = {UINT32_C(0x21000000) + i, 15u};
    context.machine.registers.gpr[0] = {0u, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {team, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] = {7u, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2] = {injury_source,
                                                                15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A3] = {
        UINT32_C(0xdeadbeef), 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {stack, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
        UINT32_C(0x800659cc), 15u};
    context.machine.hi = {UINT32_C(0x11223344), 5u};
    context.machine.lo = {UINT32_C(0x55667788), 10u};
    put(data, team + 0x14u, 0u, 2u);
    put(data, team + 0x68u, 1u, 2u);
    put(data, team + 0x7cu, pointers, 4u);
    put(data, team + 0x80u, 5u, 2u);
    put(data, pointers, player, 4u);
    put(data, player + 8u, 7u, 1u);
    put(data, injury_source + 0x20u, 0u, 2u);
    put(data, UINT32_C(0x8001f7ec) + 0x20u, 0x7332u, 2u);
    put(data, UINT32_C(0x800b8904) + 7u, 0u, 1u);
    put(data, UINT32_C(0x800b8909) + 7u, 0u, 1u);
    put(data, UINT32_C(0x800b8910) + 7u, 0u, 1u);
    put(data, UINT32_C(0x800b8915) + 7u, 0u, 1u);
  }
  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameSubstitutionCandidateSelectEvent *event,
                      Nba97GameSubstitutionCandidateSelectMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    fixture.event = *event;
    fixture.call_machine = *machine;
    ++fixture.calls;
    if (fixture.relocate) {
      for (unsigned i = 1u; i != 32u; ++i)
        machine->registers.gpr[i] = {UINT32_C(0xca000000) + i,
                                     static_cast<uint8_t>((i % 15u) + 1u)};
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {relocated_stack,
                                                           15u};
      machine->hi = {UINT32_C(0xface0001), 3u};
      machine->lo = {UINT32_C(0xface0002), 12u};
      put(fixture.data, relocated_stack + 0x40u, UINT32_C(0x80001234), 4u);
    }
    machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {UINT32_C(0xabcdef01),
                                                         15u};
    if (fixture.invalid_child == 1)
      machine->registers.gpr[0] = {1u, 15u};
    if (fixture.invalid_child == 2)
      machine->registers.gpr[9].known_mask = 16;
    if (fixture.invalid_child == 3)
      machine->hi.known_mask = 16;
    if (fixture.invalid_child == 4)
      machine->lo.known_mask = 16;
    if (fixture.malformed_return_load)
      fixture.known[stack - 8 - base + 3] = 2;
    return fixture.reject ? 0 : 1;
  }
  int run() {
    return nba97_game_substitution_candidate_select(&context, &progress);
  }
};

void configure_pass(Fixture &fixture, unsigned pass) {
  if (pass != 1u)
    put(fixture.data, Fixture::player + 8u, 9u, 1u);
  if (pass == 2u)
    put(fixture.data, UINT32_C(0x800b8904) + 7u, 9u, 1u);
  if (pass == 3u)
    put(fixture.data, UINT32_C(0x800b8910) + 7u, 9u, 1u);
  if (pass >= 4u) {
    put(fixture.data, UINT32_C(0x8001f7ec) + 0x20u, pass == 5u ? 0u : 0x7332u,
        2u);
    put(fixture.data, Fixture::injury_source + 0x20u,
        pass == 5u ? UINT16_C(0xffff) : 0u, 2u);
  }
}

void all_pass_hits() {
  for (unsigned pass = 1u; pass != 6u; ++pass) {
    Fixture fixture;
    const auto original = fixture.context.machine;
    configure_pass(fixture, pass);
    check(fixture.run() == NBA97_TEXT_COMPLETE, "pass hit completes");
    check(fixture.calls == 1u && fixture.progress.return_v0.word == 1u,
          "pass hit calls and forces one");
    check(fixture.event.pc == UINT32_C(0x80065038) &&
              fixture.event.delay_slot_pc == UINT32_C(0x8006503c) &&
              fixture.event.entry == UINT32_C(0x800649d8) &&
              fixture.event.argument_count == 5u,
          "child exact metadata");
    check(fixture.call_machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
                  Fixture::team &&
              fixture.call_machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
                      .word == 7u &&
              fixture.call_machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2]
                      .word == 5u &&
              fixture.call_machine.registers.gpr[NBA97_MATCH_INITIALIZE_A3]
                      .word == (pass == 5u ? UINT32_MAX : 0u) &&
              get(fixture.data, Fixture::stack - 0x48u + 0x10u, 4u) ==
                  UINT32_C(0xdeadbeef),
          "child arguments and stack fifth argument");
    for (unsigned i = 0u; i != 32u; ++i) {
      const bool changed =
          i == NBA97_MATCH_INITIALIZE_V0 || i == NBA97_MATCH_INITIALIZE_V1 ||
          i == NBA97_MATCH_INITIALIZE_A2 || i == NBA97_MATCH_INITIALIZE_A3 ||
          (i >= NBA97_MATCH_INITIALIZE_T0 &&
           i <= NBA97_MATCH_INITIALIZE_T0 + 5u);
      if (!changed)
        check(same_word(fixture.progress.machine.registers.gpr[i],
                        original.registers.gpr[i]),
              "every untouched GPR word and mask preserved");
    }
  }
}

void no_hit_and_boundaries() {
  Fixture count_zero;
  put(count_zero.data, Fixture::team + 0x68u, 0u, 2u);
  check(count_zero.run() == NBA97_TEXT_COMPLETE && count_zero.calls == 0u &&
            count_zero.progress.return_v0.word == 0u,
        "zero count no hit");
  for (uint16_t injury :
       {uint16_t{0x1fffu}, uint16_t{0x2000u}, uint16_t{0xffffu}}) {
    Fixture fixture;
    put(fixture.data, Fixture::player + 8u, 9u, 1u);
    put(fixture.data, UINT32_C(0x8001f7ec) + 0x20u, UINT16_C(0x8000), 2u);
    put(fixture.data, Fixture::injury_source + 0x20u, injury, 2u);
    put(fixture.data, Fixture::team + 0x80u, 4u, 2u);
    check(fixture.run() == NBA97_TEXT_COMPLETE && fixture.calls == 0u &&
              fixture.progress.return_v0.word == 0u,
          "injury no-hit boundary");
  }
  for (uint16_t inverse : {uint16_t{0xffffu}, uint16_t{4u}, uint16_t{5u},
                           uint16_t{0x7fffu}, uint16_t{0x8000u}}) {
    Fixture fixture;
    put(fixture.data, Fixture::team + 0x80u, inverse, 2u);
    put(fixture.data, Fixture::player + 8u, 9u, 1u);
    put(fixture.data, Fixture::injury_source + 0x20u, 0x2000u, 2u);
    int result = fixture.run();
    check(result == NBA97_TEXT_COMPLETE, "inverse extrema complete");
  }
}

void signed_rank_and_status_edges() {
  for (uint8_t rank : {uint8_t{127u}, uint8_t{128u}, uint8_t{255u}}) {
    Fixture fixture;
    put(fixture.data, Fixture::player + 8u, rank, 1u);
    put(fixture.data, UINT32_C(0x800b8904) + 7u, rank, 1u);
    put(fixture.data, UINT32_C(0x800b8910) + 7u, rank, 1u);
    put(fixture.data, Fixture::injury_source + 0x20u, 0x2000u, 2u);
    check(fixture.run() == NBA97_TEXT_COMPLETE, "signed rank completes");
    check(fixture.calls == (rank == 127u ? 1u : 0u),
          "signed rank versus unsigned player comparison");
  }
  for (uint16_t status : {uint16_t{0x7331u}, uint16_t{0x7332u},
                          uint16_t{0x7fffu}, uint16_t{0x8000u}}) {
    Fixture fixture;
    put(fixture.data, UINT32_C(0x8001f7ec) + 0x20u, status, 2u);
    put(fixture.data, Fixture::injury_source + 0x20u, 0x2000u, 2u);
    check(fixture.run() == NBA97_TEXT_COMPLETE, "status edge completes");
    check(fixture.calls == ((status == 0x7332u || status == 0x7fffu) ? 1u : 0u),
          "signed status threshold");
  }
}

void budgets_and_failures() {
  Fixture baseline;
  check(baseline.run() == NBA97_TEXT_COMPLETE, "budget baseline");
  for (size_t budget = 0u; budget != baseline.progress.operations; ++budget) {
    Fixture fixture;
    fixture.context.operation_budget = budget;
    check(fixture.run() == NBA97_TEXT_LIMIT, "every hit budget limits");
    check(fixture.progress.operations == budget, "budget exact prefix");
  }
  for (unsigned pass = 2u; pass != 6u; ++pass) {
    Fixture pass_baseline;
    configure_pass(pass_baseline, pass);
    check(pass_baseline.run() == NBA97_TEXT_COMPLETE,
          "alternate pass budget baseline");
    for (size_t budget = 0u; budget != pass_baseline.progress.operations;
         ++budget) {
      Fixture limited;
      configure_pass(limited, pass);
      limited.context.operation_budget = budget;
      check(limited.run() == NBA97_TEXT_LIMIT,
            "every alternate-pass budget limits");
      check(limited.progress.operations == budget,
            "alternate-pass budget prefix exact");
    }
  }
  Fixture refused;
  refused.reject = true;
  check(refused.run() == NBA97_TEXT_IO_REFUSED, "child refusal");
  check(get(refused.data, Fixture::stack - 0x48u + 0x10u, 4u) ==
                UINT32_C(0xdeadbeef) &&
            refused.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
                    .word == UINT32_C(0x80065040),
        "refusal retains JAL delay prefix");
  Fixture unknown_ra;
  unknown_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .known_mask = 14u;
  check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
            unknown_ra.progress.stopped_pc == UINT32_C(0x80065068) &&
            unknown_ra.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                    .word == Fixture::stack,
        "unknown saved RA after SP restore");
  Fixture misaligned_ra;
  misaligned_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word =
      UINT32_C(0x800659cd);
  check(misaligned_ra.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
            misaligned_ra.progress.stopped_pc == UINT32_C(0x80065068) &&
            misaligned_ra.progress.machine.registers
                    .gpr[NBA97_MATCH_INITIALIZE_SP]
                    .word == Fixture::stack,
        "misaligned saved RA traps after SP restore");

  Fixture unknown_loop;
  put(unknown_loop.data, Fixture::team + 0x68u, 1u, 2u);
  unknown_loop.known[Fixture::team - Fixture::base + 0x69u] = 0u;
  put(unknown_loop.data, Fixture::team + 0x80u, 4u, 2u);
  check(unknown_loop.run() == NBA97_TEXT_UNKNOWN &&
            unknown_loop.progress.stopped_pc == UINT32_C(0x80064e54),
        "partial latched count stops at loop predicate");
  check(
      unknown_loop.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 1]
              .word == Fixture::team + 2u,
      "unknown loop predicate still executes T1 delay increment");
}

void machine_knownness_mapping_and_counts() {
  Fixture live;
  live.relocate = true;
  check(live.run() == NBA97_TEXT_COMPLETE,
        "callback live relocation completes");
  for (unsigned i = 1u; i != 32u; ++i) {
    if (i == NBA97_MATCH_INITIALIZE_V0 || i == NBA97_MATCH_INITIALIZE_SP ||
        i == NBA97_MATCH_INITIALIZE_RA)
      continue;
    check(live.progress.machine.registers.gpr[i].word ==
                  UINT32_C(0xca000000) + i &&
              live.progress.machine.registers.gpr[i].known_mask ==
                  static_cast<uint8_t>((i % 15u) + 1u),
          "all callback GPR mutations retained");
  }
  check(
      live.progress.machine.hi.word == UINT32_C(0xface0001) &&
          live.progress.machine.lo.word == UINT32_C(0xface0002) &&
          live.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
              Fixture::relocated_stack + 0x48u &&
          live.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
              UINT32_C(0x80001234),
      "callback GPR HI LO and live SP/RA retained");

  for (unsigned mask = 0u; mask != 15u; ++mask) {
    Fixture ra;
    ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask =
        static_cast<uint8_t>(mask);
    check(ra.run() == NBA97_TEXT_UNKNOWN &&
              ra.progress.stopped_pc == UINT32_C(0x80065068),
          "all partial saved RA masks refuse at JR");
  }

  Fixture malformed;
  malformed.known[Fixture::team - Fixture::base + 0x7fu] = 2u;
  check(malformed.run() == NBA97_TEXT_ARGUMENT,
        "malformed late LW byte rejects");
  check(malformed.progress.stopped_pc == UINT32_C(0x80064e10) &&
            malformed.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                    .word == 0u &&
            malformed.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                    .known_mask == 15u,
        "malformed load destination remains atomic");

  Fixture partial_store;
  partial_store.region.known = nullptr;
  partial_store.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A3]
      .known_mask = 14u;
  std::array<uint8_t, 4> before{};
  const size_t stack_arg = Fixture::stack - 0x48u + 0x10u - Fixture::base;
  for (unsigned b = 0u; b != 4u; ++b)
    before[b] = partial_store.data[stack_arg + b];
  check(partial_store.run() == NBA97_TEXT_ARGUMENT &&
            partial_store.progress.stopped_pc == UINT32_C(0x8006503c),
        "partial fifth argument refuses known-null stack store");
  for (unsigned b = 0u; b != 4u; ++b)
    check(partial_store.data[stack_arg + b] == before[b],
          "rejected fifth argument store leaves byte unchanged");

  Fixture unaligned;
  unaligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word++;
  check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
            unaligned.progress.stopped_pc == UINT32_C(0x80064dc0),
        "unaligned frame traps");
  Fixture unmapped;
  unmapped.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word =
      UINT32_C(0x90000000);
  check(unmapped.run() == NBA97_TEXT_RESOURCE, "unmapped team refuses");
  Fixture overlap;
  std::array<Nba97GameTextRegion, 2> overlapping = {
      overlap.region,
      Nba97GameTextRegion{Fixture::base + 1u, overlap.data.data() + 1u,
                          overlap.known.data() + 1u, 1u}};
  overlap.context.memory = {overlapping.data(), overlapping.size()};
  check(overlap.run() == NBA97_TEXT_ARGUMENT, "overlapping map rejects");
  Fixture bad_zero;
  bad_zero.context.machine.registers.gpr[0].word = 1u;
  check(bad_zero.run() == NBA97_TEXT_ARGUMENT, "malformed zero rejects");
  Fixture bad_mask;
  bad_mask.context.machine.hi.known_mask = 16u;
  check(bad_mask.run() == NBA97_TEXT_ARGUMENT, "malformed mask rejects");
  Fixture bad_journal;
  bad_journal.context.access_journal = nullptr;
  check(bad_journal.run() == NBA97_TEXT_ARGUMENT,
        "missing configured journal rejects");

  Fixture wrapped;
  put(wrapped.data, Fixture::team + 0x68u, 0u, 2u);
  wrapped.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {8u, 15u};
  std::array<uint8_t, 64> high_data{};
  std::array<uint8_t, 64> high_known{};
  std::array<uint8_t, 8> low_data{};
  std::array<uint8_t, 8> low_known{};
  high_known.fill(1u);
  low_known.fill(1u);
  std::array<Nba97GameTextRegion, 3> wrap_regions = {
      wrapped.region,
      Nba97GameTextRegion{UINT32_C(0xffffffc0), high_data.data(),
                          high_known.data(), high_data.size()},
      Nba97GameTextRegion{0u, low_data.data(), low_known.data(),
                          low_data.size()}};
  wrapped.context.memory = {wrap_regions.data(), wrap_regions.size()};
  check(wrapped.run() == NBA97_TEXT_COMPLETE &&
            wrapped.progress.frame_stack_pointer == UINT32_C(0xffffffc0) &&
            wrapped.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                    .word == 8u,
        "mapped high-low stack wrap completes");

  Fixture full_id;
  full_id.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] = {
      UINT32_C(0x100), 15u};
  put(full_id.data, Fixture::injury_source + 0x20u, 0x2000u, 2u);
  check(full_id.run() == NBA97_TEXT_COMPLETE && full_id.calls == 0u,
        "pass one compares full a1 against unsigned byte");

  Fixture away;
  put(away.data, Fixture::team + 0x14u, 0x8001u, 2u);
  put(away.data, UINT32_C(0x8001f984) + 0x20u, 0x7332u, 2u);
  check(away.run() == NBA97_TEXT_COMPLETE && away.calls == 1u,
        "raw nonzero side selects away status table");

  Fixture twelve;
  put(twelve.data, Fixture::team + 0x68u, 12u, 2u);
  put(twelve.data, Fixture::injury_source + 0x20u, 0x2000u, 2u);
  for (unsigned i = 0u; i != 12u; ++i)
    put(twelve.data, Fixture::team + 0x80u + i * 2u, 4u, 2u);
  check(twelve.run() == NBA97_TEXT_COMPLETE && twelve.calls == 0u,
        "count twelve scans without normalization");
  Fixture huge;
  put(huge.data, Fixture::team + 0x68u, UINT16_C(0xffff), 2u);
  put(huge.data, Fixture::team + 0x80u, 4u, 2u);
  huge.context.operation_budget = 10u;
  check(huge.run() == NBA97_TEXT_LIMIT && huge.progress.operations == 10u,
        "count 65535 remains bounded only by explicit budget");

  Fixture reload;
  configure_pass(reload, 5u);
  put(reload.data, Fixture::team + 0x68u, 2u, 2u);
  put(reload.data, Fixture::team + 0x80u, 4u, 2u);
  put(reload.data, Fixture::team + 0x82u, 5u, 2u);
  put(reload.data, Fixture::pointers + 4u, Fixture::player + 0x100u, 4u);
  put(reload.data, Fixture::player + 0x108u, 9u, 1u);
  put(reload.data, UINT32_C(0x8001f7ec) + 0x20u, UINT16_C(0xffff), 2u);
  put(reload.data, UINT32_C(0x8001f7ec) + 0x42u, 0u, 2u);
  check(reload.run() == NBA97_TEXT_COMPLETE && reload.calls == 1u,
        "fifth pass second candidate completes");
  size_t reloads = 0u;
  for (size_t i = 0u; i != reload.progress.access_events; ++i)
    if (reload.journal[i].pc == UINT32_C(0x80065048))
      ++reloads;
  check(reloads == 1u, "fifth pass rereads count after rejected candidate");

  Fixture first;
  Fixture second;
  check(first.run() == NBA97_TEXT_COMPLETE &&
            second.run() == NBA97_TEXT_COMPLETE,
        "deterministic repeats complete");
  check(first.data == second.data && first.known == second.known &&
            same_machine(first.progress.machine, second.progress.machine),
        "deterministic bytes knownness and return");
}

void malformed_child_and_final_load_prefixes() {
  for (unsigned kind = 1; kind <= 4; ++kind) {
    Fixture f;
    f.invalid_child = kind;
    check(f.run() == NBA97_TEXT_ARGUMENT &&
              f.progress.stopped_pc == 0x80065038u &&
              f.progress.callbacks_completed == 0,
          "accepted malformed child refused");
    check(f.progress.machine.registers.gpr[2].word == 0xabcdef01u &&
              get(f.data, Fixture::stack - 0x38, 4) == 0xdeadbeefu,
          "malformed child retains pre-force V0 and delay store");
  }
  Fixture f;
  f.malformed_return_load = true;
  check(f.run() == NBA97_TEXT_ARGUMENT &&
            f.progress.stopped_pc == 0x80065060u &&
            f.progress.callbacks_completed == 1,
        "late final LW malformed byte refused");
  check(f.progress.machine.registers.gpr[31].word == 0x80065040u &&
            f.progress.machine.registers.gpr[2].word == 1 &&
            f.progress.machine.registers.gpr[29].word == Fixture::stack - 0x48,
        "final LW atomic after forced return one");
}
} // namespace

int main() {
  malformed_child_and_final_load_prefixes();
  all_pass_hits();
  no_hit_and_boundaries();
  signed_rank_and_status_edges();
  budgets_and_failures();
  machine_knownness_mapping_and_counts();
  std::printf("game_substitution_candidate_select_tests: %zu checks passed\n",
              checks);
  return 0;
}
