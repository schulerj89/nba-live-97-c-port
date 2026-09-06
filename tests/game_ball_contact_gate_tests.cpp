#include "recovered/game_ball_contact_gate.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "ball contact gate check %u failed at %u\n", checks,
                 line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t First = 0x80090000u;
constexpr std::uint32_t Second = 0x80090100u;
constexpr std::uint32_t EntrySp = 0x800ff000u;
constexpr std::uint32_t Return = 0x80061078u;

bool same(Nba97GameBallContactGateWord a,
          Nba97GameBallContactGateWord b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}

struct Call {
  Nba97GameBallContactGateEvent event{};
  Nba97GameBallContactGateMachine before{};
};

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x110000);
  std::vector<std::uint8_t> known =
      std::vector<std::uint8_t>(0x110000, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameBallContactGateAccess, 16> journal{};
  Nba97GameBallContactGateContext context{};
  Nba97GameBallContactGateProgress progress{};
  std::vector<Call> calls;
  bool refuse = false;
  bool mutate = false;
  bool invalidate = false;
  bool unalign_sp = false;
  bool unknown_sp = false;
  bool unmap_sp = false;

  Fixture() {
    context.memory = {&region, 1};
    context.operation_budget = 16;
    context.io = io;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {
          0x21000000u + i * 0x01010101u,
          static_cast<std::uint8_t>((i % 15) + 1)};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {First, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] = {Second, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {Return, 15};
    context.machine.hi = {0x12345678u, 5};
    context.machine.lo = {0x9abcdef0u, 10};
    put(First + 8, 0, 4);
    put(Second + 8, 0, 4);
    put(Second, 7, 4);
  }

  void put(std::uint32_t address, std::uint32_t value, unsigned width,
           std::uint8_t mask = 15) {
    auto offset = address - Ram;
    for (unsigned i = 0; i < width; ++i) {
      bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8u));
      known[offset + i] = static_cast<std::uint8_t>((mask >> i) & 1u);
    }
  }

  std::uint32_t get(std::uint32_t address, unsigned width) const {
    std::uint32_t result = 0;
    auto offset = address - Ram;
    for (unsigned i = 0; i < width; ++i)
      result |= std::uint32_t(bytes[offset + i]) << (i * 8u);
    return result;
  }

  static int io(void *user, const Nba97GameTextMemory *,
                const Nba97GameBallContactGateEvent *event,
                Nba97GameBallContactGateMachine *machine) {
    auto &f = *static_cast<Fixture *>(user);
    f.calls.push_back({*event, *machine});
    if (f.refuse)
      return 0;
    if (f.mutate) {
      constexpr std::uint32_t AlternateFrame = 0x800ff080u;
      for (unsigned i = 1; i < 32; ++i)
        machine->registers.gpr[i] = {
            0x50000000u + i,
            static_cast<std::uint8_t>((i % 15) + 1)};
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {AlternateFrame, 15};
      machine->hi = {0xaabbccddu, 9};
      machine->lo = {0x55667788u, 6};
      f.put(AlternateFrame + 0x10, 0x81234567u, 4);
    }
    if (f.unalign_sp)
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x800ff081u, 15};
    if (f.unknown_sp)
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x800ff080u, 7};
    if (f.unmap_sp)
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x90000000u, 15};
    if (f.invalidate)
      machine->registers.gpr[0] = {1, 15};
    return 1;
  }

  int run() { return nba97_game_ball_contact_gate(&context, &progress); }
};

void exact_paths_and_arguments() {
  Fixture f;
  auto entry = f.context.machine;
  f.put(Second + 8, 0x000001ffu, 4);
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
  check(f.progress.operations == 6 && f.progress.accesses == 5 &&
        f.progress.reads == 4 && f.progress.stores == 1 &&
        f.progress.callbacks_completed == 1 && f.calls.size() == 1);
  check(f.progress.coordinate_difference.word == 0x000001ffu &&
        f.progress.shifted_difference.word == 1 &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2].word == 1);
  auto &call = f.calls[0];
  check(call.event.pc == 0x80060ed4u &&
        call.event.delay_slot_pc == 0x80060ed8u &&
        call.event.entry == 0x800602ccu && call.event.operation == 5 &&
        call.event.invocation == 1 && call.event.argument_count == 3 &&
        call.before.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x80060edcu);
  check(call.before.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == First &&
        call.before.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word == Second &&
        call.before.registers.gpr[NBA97_MATCH_INITIALIZE_A2].word == 1);
  check(f.progress.returned_value.word == 1 &&
        f.progress.returned_value.known_mask == 15 &&
        same(f.progress.restored_return_address,
             entry.registers.gpr[NBA97_MATCH_INITIALIZE_RA]) &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp &&
        same(f.progress.machine.hi, entry.hi) &&
        same(f.progress.machine.lo, entry.lo));
  for (unsigned i = 0; i < 32; ++i) {
    if (i == NBA97_MATCH_INITIALIZE_V0 ||
        i == NBA97_MATCH_INITIALIZE_V1 ||
        i == NBA97_MATCH_INITIALIZE_A0 ||
        i == NBA97_MATCH_INITIALIZE_A1 ||
        i == NBA97_MATCH_INITIALIZE_A2 ||
        i == NBA97_MATCH_INITIALIZE_A3 ||
        i == NBA97_MATCH_INITIALIZE_SP ||
        i == NBA97_MATCH_INITIALIZE_RA)
      continue;
    check(same(f.progress.machine.registers.gpr[i],
               entry.registers.gpr[i]));
  }
  std::array<std::uint32_t, 5> pcs{
      0x80060e94u, 0x80060e98u, 0x80060e9cu, 0x80060ebcu,
      0x80060ee8u};
  std::array<std::uint32_t, 5> addresses{
      EntrySp - 8, Second + 8, First + 8, Second, EntrySp - 8};
  std::array<std::size_t, 5> operations{1, 2, 3, 4, 6};
  for (unsigned i = 0; i < pcs.size(); ++i)
    check(f.journal[i].pc == pcs[i] &&
          f.journal[i].address == addresses[i] &&
          f.journal[i].operation == operations[i] &&
          f.journal[i].width == 4 && f.journal[i].known_mask == 15 &&
          f.journal[i].kind == (i == 0 ? NBA97_GAME_MATCH_CLOCKS_STORE
                                       : NBA97_GAME_MATCH_CLOCKS_READ));

  Fixture swapped;
  swapped.put(Second, 10, 4);
  swapped.put(First + 8, 0x00000400u, 4);
  swapped.put(Second + 8, 0x00000100u, 4);
  check(swapped.run() == NBA97_TEXT_COMPLETE && swapped.calls.size() == 1);
  check(swapped.calls[0].before.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            Second &&
        swapped.calls[0].before.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
            First &&
        swapped.calls[0].before.registers.gpr[NBA97_MATCH_INITIALIZE_A2].word ==
            0xfffffffdu);
}

void coordinate_edges_fraction_and_wrap() {
  struct Edge {
    std::int32_t shifted;
    bool dispatch;
  };
  std::array<Edge, 4> edges{{{-33, false}, {-32, true}, {32, true}, {33, false}}};
  for (auto edge : edges) {
    Fixture f;
    f.put(Second + 8,
          static_cast<std::uint32_t>(edge.shifted) * 0x100u, 4);
    check(f.run() == NBA97_TEXT_COMPLETE &&
          (f.calls.size() == 1) == edge.dispatch &&
          f.progress.shifted_difference.word ==
              static_cast<std::uint32_t>(edge.shifted) &&
          f.progress.returned_value.word == (edge.dispatch ? 1u : 0u));
    check(f.progress.operations == (edge.dispatch ? 6u : 4u));
  }
  struct Fraction {
    std::uint32_t value;
    std::uint32_t shifted;
  };
  std::array<Fraction, 4> fractions{{
      {0x000000ffu, 0}, {0x00000100u, 1},
      {0xffffff01u, 0xffffffffu}, {0xffffff00u, 0xffffffffu}}};
  for (auto item : fractions) {
    Fixture f;
    f.put(Second + 8, item.value, 4);
    check(f.run() == NBA97_TEXT_COMPLETE && f.calls.size() == 1 &&
          f.progress.shifted_difference.word == item.shifted);
  }
  Fixture positive_wrap;
  positive_wrap.put(First + 8, 0xffffffffu, 4);
  positive_wrap.put(Second + 8, 0, 4);
  check(positive_wrap.run() == NBA97_TEXT_COMPLETE &&
        positive_wrap.calls.size() == 1 &&
        positive_wrap.progress.coordinate_difference.word == 1 &&
        positive_wrap.progress.shifted_difference.word == 0);
  Fixture signed_overflow;
  signed_overflow.put(First + 8, 0xffffffffu, 4);
  signed_overflow.put(Second + 8, 0x7fffffffu, 4);
  check(signed_overflow.run() == NBA97_TEXT_COMPLETE &&
        signed_overflow.calls.empty() &&
        signed_overflow.progress.coordinate_difference.word == 0x80000000u &&
        signed_overflow.progress.shifted_difference.word == 0xff800000u);
}

void delay_slots_and_partial_knownness() {
  Fixture gate;
  gate.put(Second + 8, 0, 4, 0);
  check(gate.run() == NBA97_TEXT_UNKNOWN &&
        gate.progress.stopped_pc == 0x80060eb4u && gate.calls.empty() &&
        gate.progress.operations == 3 &&
        gate.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            10 &&
        gate.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .known_mask == 15);
  Fixture id;
  id.put(Second, 10, 4, 0x0e);
  check(id.run() == NBA97_TEXT_UNKNOWN &&
        id.progress.stopped_pc == 0x80060ec4u && id.calls.empty() &&
        id.progress.operations == 4 &&
        id.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            First &&
        id.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
                .known_mask == 15);
  Fixture disproved;
  disproved.put(Second, 11, 4, 1);
  check(disproved.run() == NBA97_TEXT_COMPLETE &&
        disproved.calls.size() == 1 &&
        disproved.calls[0]
                .before.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
                .word == First);
}

void callback_mutation_refusal_and_result() {
  Fixture f;
  f.mutate = true;
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
  check(f.progress.returned_value.word == 1 &&
        f.progress.returned_value.known_mask == 15 &&
        f.progress.machine.hi.word == 0xaabbccddu &&
        f.progress.machine.hi.known_mask == 9 &&
        f.progress.machine.lo.word == 0x55667788u &&
        f.progress.machine.lo.known_mask == 6 &&
        f.progress.restored_return_address.word == 0x81234567u &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            0x800ff098u);
  for (unsigned i = 1; i < 32; ++i) {
    if (i == NBA97_MATCH_INITIALIZE_V0 ||
        i == NBA97_MATCH_INITIALIZE_SP ||
        i == NBA97_MATCH_INITIALIZE_RA)
      continue;
    check(f.progress.machine.registers.gpr[i].word == 0x50000000u + i &&
          f.progress.machine.registers.gpr[i].known_mask == (i % 15) + 1);
  }
  Fixture refused;
  refused.refuse = true;
  check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.progress.stopped_pc == 0x80060ed4u &&
        refused.progress.stopped_entry == 0x800602ccu &&
        refused.progress.operations == 5 &&
        refused.progress.callbacks_completed == 0 &&
        refused.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            10);
  Fixture missing;
  missing.context.io = nullptr;
  check(missing.run() == NBA97_TEXT_IO_REFUSED &&
        missing.progress.stopped_pc == 0x80060ed4u);
  Fixture invalid;
  invalid.invalidate = true;
  check(invalid.run() == NBA97_TEXT_ARGUMENT &&
        invalid.progress.stopped_pc == 0x80060ed4u &&
        invalid.progress.machine.registers.gpr[0].word == 1 &&
        invalid.progress.returned_value.word != 1);
}

void every_budget_prefix() {
  std::array<std::uint32_t, 6> normal_pc{
      0x80060e94u, 0x80060e98u, 0x80060e9cu,
      0x80060ebcu, 0x80060ed4u, 0x80060ee8u};
  for (unsigned budget = 0; budget <= 6; ++budget) {
    Fixture f;
    f.context.operation_budget = budget;
    int result = f.run();
    check((budget == 6 && result == NBA97_TEXT_COMPLETE &&
           f.progress.completed) ||
          (budget < 6 && result == NBA97_TEXT_LIMIT &&
           f.progress.stopped_pc == normal_pc[budget] &&
           !f.progress.completed));
    check(f.progress.operations == budget);
  }
  std::array<std::uint32_t, 4> outside_pc{
      0x80060e94u, 0x80060e98u, 0x80060e9cu, 0x80060ee8u};
  for (unsigned budget = 0; budget <= 4; ++budget) {
    Fixture f;
    f.put(Second + 8, 33u * 256u, 4);
    f.context.operation_budget = budget;
    int result = f.run();
    check((budget == 4 && result == NBA97_TEXT_COMPLETE &&
           f.progress.completed) ||
          (budget < 4 && result == NBA97_TEXT_LIMIT &&
           f.progress.stopped_pc == outside_pc[budget] &&
           !f.progress.completed));
    check(f.progress.operations == budget);
  }
}

void addresses_alias_wrap_and_unknown_return() {
  Fixture alias;
  alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
      Second + 0x10, 15};
  alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x100, 15};
  check(alias.run() == NBA97_TEXT_COMPLETE && alias.calls.size() == 1 &&
        alias.get(Second + 8, 4) == 0x100 &&
        alias.progress.second_coordinate.word == 0x100 &&
        alias.progress.shifted_difference.word == 1 &&
        alias.progress.restored_return_address.word == 0x100);

  Fixture wrapping;
  std::array<std::uint8_t, 4> low{};
  std::array<std::uint8_t, 4> low_known{};
  low_known.fill(1);
  Nba97GameTextRegion regions[2]{
      wrapping.region, {0, low.data(), low_known.data(), low.size()}};
  wrapping.context.memory = {regions, 2};
  wrapping.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {8, 15};
  check(wrapping.run() == NBA97_TEXT_COMPLETE &&
        wrapping.progress.frame_stack_pointer == 0xfffffff0u &&
        wrapping.journal[0].address == 0 && wrapping.journal[4].address == 0 &&
        wrapping.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                .word == 8 &&
        wrapping.progress.restored_return_address.word == Return);

  Fixture ra;
  ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 7;
  check(ra.run() == NBA97_TEXT_UNKNOWN &&
        ra.progress.stopped_pc == 0x80060ef0u &&
        ra.progress.operations == 6 &&
        ra.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp &&
        ra.progress.restored_return_address.known_mask == 7);
}

void address_failures_and_metadata() {
  Fixture sp;
  sp.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].known_mask = 7;
  check(sp.run() == NBA97_TEXT_UNKNOWN &&
        sp.progress.stopped_pc == 0x80060e94u && sp.progress.operations == 0);
  Fixture a1;
  a1.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].known_mask = 7;
  check(a1.run() == NBA97_TEXT_UNKNOWN &&
        a1.progress.stopped_pc == 0x80060e98u && a1.progress.operations == 1);
  Fixture a0;
  a0.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask = 7;
  check(a0.run() == NBA97_TEXT_UNKNOWN &&
        a0.progress.stopped_pc == 0x80060e9cu && a0.progress.operations == 2);

  Fixture unaligned_sp;
  unaligned_sp.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word =
      EntrySp + 1;
  check(unaligned_sp.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_sp.progress.stopped_pc == 0x80060e94u);
  Fixture unaligned_a1;
  unaligned_a1.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word =
      Second + 1;
  check(unaligned_a1.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_a1.progress.stopped_pc == 0x80060e98u);
  Fixture unaligned_a0;
  unaligned_a0.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word =
      First + 1;
  check(unaligned_a0.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_a0.progress.stopped_pc == 0x80060e9cu);
  Fixture live_sp;
  live_sp.unalign_sp = true;
  check(live_sp.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        live_sp.progress.stopped_pc == 0x80060ee8u &&
        live_sp.progress.callbacks_completed == 1);
  Fixture unknown_live_sp;
  unknown_live_sp.unknown_sp = true;
  check(unknown_live_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_live_sp.progress.stopped_pc == 0x80060ee8u &&
        unknown_live_sp.progress.operations == 5);
  Fixture unmapped_live_sp;
  unmapped_live_sp.unmap_sp = true;
  check(unmapped_live_sp.run() == NBA97_TEXT_RESOURCE &&
        unmapped_live_sp.progress.stopped_pc == 0x80060ee8u &&
        unmapped_live_sp.progress.operations == 6);

  Fixture unmapped_stack;
  unmapped_stack.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word =
      0x90000018u;
  check(unmapped_stack.run() == NBA97_TEXT_RESOURCE &&
        unmapped_stack.progress.stopped_pc == 0x80060e94u);
  Fixture unmapped_second;
  unmapped_second.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
      .word = 0x90000000u;
  check(unmapped_second.run() == NBA97_TEXT_RESOURCE &&
        unmapped_second.progress.stopped_pc == 0x80060e98u);
  Fixture unmapped_first;
  unmapped_first.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word =
      0x90000000u;
  check(unmapped_first.run() == NBA97_TEXT_RESOURCE &&
        unmapped_first.progress.stopped_pc == 0x80060e9cu);
  Fixture unmapped_identifier;
  const std::size_t gap = Second - Ram;
  Nba97GameTextRegion split_regions[2]{
      {Ram, unmapped_identifier.bytes.data(), unmapped_identifier.known.data(),
       gap},
      {Second + 4, unmapped_identifier.bytes.data() + gap + 4,
       unmapped_identifier.known.data() + gap + 4,
       unmapped_identifier.bytes.size() - gap - 4}};
  unmapped_identifier.context.memory = {split_regions, 2};
  check(unmapped_identifier.run() == NBA97_TEXT_RESOURCE &&
        unmapped_identifier.progress.stopped_pc == 0x80060ebcu &&
        unmapped_identifier.progress.operations == 4);

  Fixture bad_known;
  bad_known.known[Second + 8 - Ram] = 2;
  check(bad_known.run() == NBA97_TEXT_ARGUMENT &&
        bad_known.progress.stopped_pc == 0x80060e98u);
  Fixture raw;
  raw.region.known = nullptr;
  raw.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 7;
  check(raw.run() == NBA97_TEXT_ARGUMENT &&
        raw.progress.stopped_pc == 0x80060e94u);

  Nba97GameBallContactGateProgress progress{};
  Fixture argument;
  check(nba97_game_ball_contact_gate(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
  check(nba97_game_ball_contact_gate(&argument.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
  argument.context.machine.registers.gpr[0] = {1, 15};
  check(argument.run() == NBA97_TEXT_ARGUMENT);
  Fixture mask;
  mask.context.machine.hi.known_mask = 16;
  check(mask.run() == NBA97_TEXT_ARGUMENT);
  Fixture missing_regions;
  missing_regions.context.memory = {nullptr, 1};
  check(missing_regions.run() == NBA97_TEXT_ARGUMENT);
  Fixture null_data;
  null_data.region.data = nullptr;
  check(null_data.run() == NBA97_TEXT_ARGUMENT);
  Fixture zero_size;
  zero_size.region.size = 0;
  check(zero_size.run() == NBA97_TEXT_ARGUMENT);
  Fixture overflowing;
  overflowing.region.base = 2;
  overflowing.region.size = static_cast<std::size_t>(UINT32_MAX);
  check(overflowing.run() == NBA97_TEXT_ARGUMENT);
  Fixture overlap;
  Nba97GameTextRegion overlap_regions[2]{overlap.region, overlap.region};
  overlap.context.memory = {overlap_regions, 2};
  check(overlap.run() == NBA97_TEXT_ARGUMENT);
  Fixture journal;
  journal.context.access_journal = nullptr;
  journal.context.access_journal_capacity = 1;
  check(journal.run() == NBA97_TEXT_ARGUMENT);
}

void repeatability() {
  Fixture one;
  Fixture two;
  one.put(Second, 10, 4);
  two.put(Second, 10, 4);
  check(one.run() == NBA97_TEXT_COMPLETE &&
        two.run() == NBA97_TEXT_COMPLETE &&
        one.progress.returned_value.word == two.progress.returned_value.word &&
        one.progress.shifted_difference.word ==
            two.progress.shifted_difference.word &&
        one.progress.operations == two.progress.operations &&
        one.calls.size() == two.calls.size());
}
} // namespace

int main() {
  exact_paths_and_arguments();
  coordinate_edges_fraction_and_wrap();
  delay_slots_and_partial_knownness();
  callback_mutation_refusal_and_result();
  every_budget_prefix();
  addresses_alias_wrap_and_unknown_return();
  address_failures_and_metadata();
  repeatability();
  std::printf("game ball contact gate: %u checks passed\n", checks);
}
