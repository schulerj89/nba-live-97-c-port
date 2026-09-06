#include "recovered/game_actor_contact_gate.h"

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
    std::fprintf(stderr, "actor contact gate check %u failed at %u\n", checks,
                 line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t First = 0x80090000u;
constexpr std::uint32_t Second = 0x80090100u;
constexpr std::uint32_t EntrySp = 0x800ff000u;
constexpr std::uint32_t Return = 0x80061054u;

bool same(Nba97GameActorContactGateWord left,
          Nba97GameActorContactGateWord right) {
  return left.word == right.word && left.known_mask == right.known_mask;
}

struct Call {
  Nba97GameActorContactGateEvent event{};
  Nba97GameActorContactGateMachine machine{};
};

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x110000);
  std::vector<std::uint8_t> known =
      std::vector<std::uint8_t>(0x110000, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameActorContactGateAccess, 16> journal{};
  Nba97GameActorContactGateContext context{};
  Nba97GameActorContactGateProgress progress{};
  std::vector<Call> calls;
  std::uint32_t child_value = 0;
  std::uint8_t child_mask = 15;
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
          0x31000000u + i * 0x01010101u,
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
    auto offset = address - Ram;
    std::uint32_t value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= std::uint32_t(bytes[offset + i]) << (i * 8u);
    return value;
  }

  static int io(void *user, const Nba97GameTextMemory *,
                const Nba97GameActorContactGateEvent *event,
                Nba97GameActorContactGateMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(user);
    fixture.calls.push_back({*event, *machine});
    if (fixture.refuse)
      return 0;
    machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {
        fixture.child_value, fixture.child_mask};
    if (fixture.mutate) {
      constexpr std::uint32_t AlternateFrame = 0x800ff080u;
      for (unsigned i = 1; i < 32; ++i)
        machine->registers.gpr[i] = {
            0x60000000u + i,
            static_cast<std::uint8_t>((i % 15) + 1)};
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {AlternateFrame, 15};
      machine->hi = {0xaabbccddu, 9};
      machine->lo = {0x55667788u, 6};
      fixture.put(AlternateFrame + 0x10, 0x81234567u, 4);
    }
    if (fixture.unalign_sp)
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x800ff081u, 15};
    if (fixture.unknown_sp)
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x800ff080u, 7};
    if (fixture.unmap_sp)
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x90000000u, 15};
    if (fixture.invalidate)
      machine->registers.gpr[0] = {1, 15};
    return 1;
  }

  int run() { return nba97_game_actor_contact_gate(&context, &progress); }
};

void signed_gate_edges() {
  struct Case {
    std::uint32_t difference;
    bool accepted;
  };
  std::array<Case, 8> cases{{
      {0x80000000u, true}, {0xffffefffu, true}, {0xfffff000u, true},
      {0xffffffffu, true}, {0, true}, {4096, true}, {4097, false},
      {0x7fffffffu, false}}};
  for (auto item : cases) {
    Fixture f;
    f.put(Second + 8, item.difference, 4);
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
          f.progress.coordinate_difference.word == item.difference &&
          (f.calls.size() == 1) == item.accepted &&
          f.progress.returned_value.word == (item.accepted ? 1u : 0u));
  }
  Fixture wrap_reject;
  wrap_reject.put(Second + 8, 0x80000000u, 4);
  wrap_reject.put(First + 8, 1, 4);
  check(wrap_reject.run() == NBA97_TEXT_COMPLETE &&
        wrap_reject.progress.coordinate_difference.word == 0x7fffffffu &&
        wrap_reject.calls.empty());
  Fixture wrap_accept;
  wrap_accept.put(Second + 8, 0x7fffffffu, 4);
  wrap_accept.put(First + 8, 0xffffffffu, 4);
  check(wrap_accept.run() == NBA97_TEXT_COMPLETE &&
        wrap_accept.progress.coordinate_difference.word == 0x80000000u &&
        wrap_accept.calls.size() == 1);
}

void call_delay_fraction_and_order() {
  struct Fraction {
    std::uint32_t raw;
    std::uint32_t shifted;
  };
  std::array<Fraction, 4> fractions{{
      {0xffu, 0}, {0x100u, 1}, {0xffffff01u, 0xffffffffu},
      {0xffffff00u, 0xffffffffu}}};
  for (auto item : fractions) {
    Fixture f;
    f.put(Second + 8, item.raw, 4);
    auto entry = f.context.machine;
    check(f.run() == NBA97_TEXT_COMPLETE && f.calls.size() == 1 &&
          f.progress.coordinate_difference.word == item.raw &&
          f.progress.shifted_difference.word == item.shifted);
    const auto &call = f.calls[0];
    check(call.event.pc == 0x8005faccu &&
          call.event.delay_slot_pc == 0x8005fad0u &&
          call.event.entry == 0x8005f948u && call.event.operation == 4 &&
          call.event.invocation == 1 && call.event.argument_count == 3 &&
          call.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
              0x8005fad4u &&
          call.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == First &&
          call.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word == Second &&
          call.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2].word ==
              item.shifted);
    check(f.progress.operations == 5 && f.progress.accesses == 4 &&
          f.progress.reads == 3 && f.progress.stores == 1 &&
          f.progress.callbacks_completed == 1 &&
          f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
              EntrySp &&
          same(f.progress.restored_return_address,
               entry.registers.gpr[NBA97_MATCH_INITIALIZE_RA]));
  }
  Fixture order;
  auto order_entry = order.context.machine;
  check(order.run() == NBA97_TEXT_COMPLETE);
  std::array<std::uint32_t, 4> pcs{
      0x8005faacu, 0x8005fab0u, 0x8005fab4u, 0x8005fad8u};
  std::array<std::uint32_t, 4> addresses{
      EntrySp - 8, Second + 8, First + 8, EntrySp - 8};
  std::array<std::size_t, 4> operations{1, 2, 3, 5};
  for (unsigned i = 0; i < pcs.size(); ++i)
    check(order.journal[i].pc == pcs[i] &&
          order.journal[i].address == addresses[i] &&
          order.journal[i].operation == operations[i] &&
          order.journal[i].width == 4 && order.journal[i].known_mask == 15 &&
          order.journal[i].kind == (i == 0 ? NBA97_GAME_MATCH_CLOCKS_STORE
                                           : NBA97_GAME_MATCH_CLOCKS_READ));
  for (unsigned i = 0; i < 32; ++i) {
    if (i == NBA97_MATCH_INITIALIZE_V0 ||
        i == NBA97_MATCH_INITIALIZE_V1 ||
        i == NBA97_MATCH_INITIALIZE_A2)
      continue;
    check(same(order.progress.machine.registers.gpr[i],
               order_entry.registers.gpr[i]));
  }
  check(same(order.progress.machine.hi, order_entry.hi) &&
        same(order.progress.machine.lo, order_entry.lo));
}

void child_returns_mutation_and_refusal() {
  for (std::uint32_t value : {0u, 1u, 2u, 0xffffffffu}) {
    Fixture f;
    f.child_value = value;
    f.child_mask = static_cast<std::uint8_t>((value % 15) + 1);
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.returned_value.word == 1 &&
          f.progress.returned_value.known_mask == 15);
  }
  Fixture mutation;
  mutation.mutate = true;
  check(mutation.run() == NBA97_TEXT_COMPLETE && mutation.progress.completed &&
        mutation.progress.machine.hi.word == 0xaabbccddu &&
        mutation.progress.machine.hi.known_mask == 9 &&
        mutation.progress.machine.lo.word == 0x55667788u &&
        mutation.progress.machine.lo.known_mask == 6 &&
        mutation.progress.restored_return_address.word == 0x81234567u &&
        mutation.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            0x800ff098u && mutation.progress.returned_value.word == 1);
  for (unsigned i = 1; i < 32; ++i) {
    if (i == NBA97_MATCH_INITIALIZE_V0 || i == NBA97_MATCH_INITIALIZE_SP ||
        i == NBA97_MATCH_INITIALIZE_RA)
      continue;
    check(mutation.progress.machine.registers.gpr[i].word == 0x60000000u + i &&
          mutation.progress.machine.registers.gpr[i].known_mask ==
              (i % 15) + 1);
  }
  Fixture refusal;
  refusal.refuse = true;
  check(refusal.run() == NBA97_TEXT_IO_REFUSED &&
        refusal.progress.stopped_pc == 0x8005faccu &&
        refusal.progress.stopped_entry == 0x8005f948u &&
        refusal.progress.operations == 4 &&
        refusal.progress.callbacks_completed == 0 &&
        refusal.progress.returned_value.word == 0);
  Fixture missing;
  missing.context.io = nullptr;
  check(missing.run() == NBA97_TEXT_IO_REFUSED &&
        missing.progress.stopped_pc == 0x8005faccu);
  Fixture invalid;
  invalid.invalidate = true;
  check(invalid.run() == NBA97_TEXT_ARGUMENT &&
        invalid.progress.stopped_pc == 0x8005faccu &&
        invalid.progress.machine.registers.gpr[0].word == 1);
}

void partial_knownness_and_delay() {
  Fixture f;
  f.put(First + 8, 1, 4);
  f.put(Second + 8, 0, 4, 0x0e);
  check(f.run() == NBA97_TEXT_UNKNOWN &&
        f.progress.stopped_pc == 0x8005fac4u && f.progress.operations == 3 &&
        f.progress.coordinate_difference.word == 0xffffffffu &&
        f.progress.coordinate_difference.known_mask == 0 &&
        f.progress.coordinate_gate.word == 1 &&
        f.progress.coordinate_gate.known_mask == 0x0e &&
        f.progress.returned_value.word == 0 &&
        f.progress.returned_value.known_mask == 15 && f.calls.empty());
}

void every_budget_prefix() {
  std::array<std::uint32_t, 5> accepted_pc{
      0x8005faacu, 0x8005fab0u, 0x8005fab4u, 0x8005faccu,
      0x8005fad8u};
  for (unsigned budget = 0; budget <= 5; ++budget) {
    Fixture f;
    f.context.operation_budget = budget;
    int result = f.run();
    check((budget == 5 && result == NBA97_TEXT_COMPLETE &&
           f.progress.completed) ||
          (budget < 5 && result == NBA97_TEXT_LIMIT &&
           f.progress.stopped_pc == accepted_pc[budget] &&
           !f.progress.completed));
    check(f.progress.operations == budget);
  }
  std::array<std::uint32_t, 4> rejected_pc{
      0x8005faacu, 0x8005fab0u, 0x8005fab4u, 0x8005fad8u};
  for (unsigned budget = 0; budget <= 4; ++budget) {
    Fixture f;
    f.put(Second + 8, 4097, 4);
    f.context.operation_budget = budget;
    int result = f.run();
    check((budget == 4 && result == NBA97_TEXT_COMPLETE &&
           f.progress.completed) ||
          (budget < 4 && result == NBA97_TEXT_LIMIT &&
           f.progress.stopped_pc == rejected_pc[budget] &&
           !f.progress.completed));
    check(f.progress.operations == budget);
  }
  Fixture call_delay;
  call_delay.put(Second + 8, 0x100, 4);
  call_delay.context.operation_budget = 3;
  check(call_delay.run() == NBA97_TEXT_LIMIT &&
        call_delay.progress.stopped_pc == 0x8005faccu &&
        call_delay.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
                .word == 0x8005fad4u &&
        call_delay.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2]
                .word == 1 &&
        call_delay.calls.empty());
  Fixture restore_prefix;
  restore_prefix.context.operation_budget = 4;
  check(restore_prefix.run() == NBA97_TEXT_LIMIT &&
        restore_prefix.progress.stopped_pc == 0x8005fad8u &&
        restore_prefix.progress.callbacks_completed == 1 &&
        restore_prefix.progress.returned_value.word == 1);
}

void aliases_wrap_and_address_failures() {
  Fixture alias;
  alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
      Second + 0x10, 15};
  alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x100, 15};
  check(alias.run() == NBA97_TEXT_COMPLETE && alias.calls.size() == 1 &&
        alias.get(Second + 8, 4) == 0x100 &&
        alias.progress.second_coordinate.word == 0x100 &&
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
        wrapping.journal[0].address == 0 && wrapping.journal[3].address == 0 &&
        wrapping.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            8 && wrapping.progress.restored_return_address.word == Return);

  Fixture unknown_ra;
  unknown_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .known_mask = 7;
  check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.stopped_pc == 0x8005fae0u &&
        unknown_ra.progress.operations == 5 &&
        unknown_ra.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                .word == EntrySp);

  Fixture unknown_sp;
  unknown_sp.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
      .known_mask = 7;
  check(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stopped_pc == 0x8005faacu &&
        unknown_sp.progress.operations == 0);
  Fixture unknown_a1;
  unknown_a1.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
      .known_mask = 7;
  check(unknown_a1.run() == NBA97_TEXT_UNKNOWN &&
        unknown_a1.progress.stopped_pc == 0x8005fab0u);
  Fixture unknown_a0;
  unknown_a0.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
      .known_mask = 7;
  check(unknown_a0.run() == NBA97_TEXT_UNKNOWN &&
        unknown_a0.progress.stopped_pc == 0x8005fab4u);

  Fixture unaligned_sp;
  unaligned_sp.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word =
      EntrySp + 1;
  check(unaligned_sp.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_sp.progress.stopped_pc == 0x8005faacu);
  Fixture unaligned_a1;
  unaligned_a1.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word =
      Second + 1;
  check(unaligned_a1.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_a1.progress.stopped_pc == 0x8005fab0u);
  Fixture unaligned_a0;
  unaligned_a0.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word =
      First + 1;
  check(unaligned_a0.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_a0.progress.stopped_pc == 0x8005fab4u);
  Fixture live_unaligned;
  live_unaligned.unalign_sp = true;
  check(live_unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        live_unaligned.progress.stopped_pc == 0x8005fad8u);
  Fixture live_unknown;
  live_unknown.unknown_sp = true;
  check(live_unknown.run() == NBA97_TEXT_UNKNOWN &&
        live_unknown.progress.stopped_pc == 0x8005fad8u &&
        live_unknown.progress.operations == 4);
  Fixture live_unmapped;
  live_unmapped.unmap_sp = true;
  check(live_unmapped.run() == NBA97_TEXT_RESOURCE &&
        live_unmapped.progress.stopped_pc == 0x8005fad8u &&
        live_unmapped.progress.operations == 5);

  Fixture missing_stack;
  missing_stack.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word =
      0x90000018u;
  check(missing_stack.run() == NBA97_TEXT_RESOURCE &&
        missing_stack.progress.stopped_pc == 0x8005faacu);
  Fixture missing_second;
  missing_second.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
      .word = 0x90000000u;
  check(missing_second.run() == NBA97_TEXT_RESOURCE &&
        missing_second.progress.stopped_pc == 0x8005fab0u);
  Fixture missing_first;
  missing_first.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word =
      0x90000000u;
  check(missing_first.run() == NBA97_TEXT_RESOURCE &&
        missing_first.progress.stopped_pc == 0x8005fab4u);
}

void metadata_and_repeatability() {
  Nba97GameActorContactGateProgress progress{};
  Fixture f;
  check(nba97_game_actor_contact_gate(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
  check(nba97_game_actor_contact_gate(&f.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
  f.context.machine.registers.gpr[0] = {1, 15};
  check(f.run() == NBA97_TEXT_ARGUMENT);
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
  Fixture bad_known;
  bad_known.known[Second + 8 - Ram] = 2;
  check(bad_known.run() == NBA97_TEXT_ARGUMENT &&
        bad_known.progress.stopped_pc == 0x8005fab0u);
  Fixture raw;
  raw.region.known = nullptr;
  raw.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 7;
  check(raw.run() == NBA97_TEXT_ARGUMENT &&
        raw.progress.stopped_pc == 0x8005faacu);

  Fixture one;
  Fixture two;
  one.put(Second + 8, 0xfffff000u, 4);
  two.put(Second + 8, 0xfffff000u, 4);
  check(one.run() == NBA97_TEXT_COMPLETE &&
        two.run() == NBA97_TEXT_COMPLETE &&
        one.progress.operations == two.progress.operations &&
        one.progress.returned_value.word == two.progress.returned_value.word &&
        one.progress.shifted_difference.word ==
            two.progress.shifted_difference.word);
}
} // namespace

int main() {
  signed_gate_edges();
  call_delay_fraction_and_order();
  child_returns_mutation_and_refusal();
  partial_knownness_and_delay();
  every_budget_prefix();
  aliases_wrap_and_address_failures();
  metadata_and_repeatability();
  std::printf("game actor contact gate: %u checks passed\n", checks);
}
