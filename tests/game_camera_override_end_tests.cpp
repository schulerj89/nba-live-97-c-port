#include "recovered/game_camera_override_end.h"

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
    std::fprintf(stderr, "camera override end check %u failed at %u\n", checks,
                 line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Flag = 0x800bc1f0u;
constexpr std::uint32_t EntrySp = 0x800ff000u;
constexpr std::uint32_t Return = 0x80065578u;

bool same(Nba97GameCameraOverrideEndWord left,
          Nba97GameCameraOverrideEndWord right) {
  return left.word == right.word && left.known_mask == right.known_mask;
}

struct Call {
  Nba97GameCameraOverrideEndEvent event{};
  Nba97GameCameraOverrideEndMachine machine{};
};

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x110000);
  std::vector<std::uint8_t> known =
      std::vector<std::uint8_t>(0x110000, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameCameraOverrideEndAccess, 16> journal{};
  Nba97GameCameraOverrideEndContext context{};
  Nba97GameCameraOverrideEndProgress progress{};
  std::vector<Call> calls;
  std::uint32_t child_value = 0x2468ace0u;
  std::uint8_t child_mask = 7;
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
          0x41000000u + i * 0x01010101u,
          static_cast<std::uint8_t>((i % 15) + 1)};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {Return, 15};
    context.machine.hi = {0x12345678u, 5};
    context.machine.lo = {0x9abcdef0u, 10};
    put(Flag, 1, 1);
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
                const Nba97GameCameraOverrideEndEvent *event,
                Nba97GameCameraOverrideEndMachine *machine) {
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
      fixture.put(Flag, 0xaau, 1);
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

  int run() { return nba97_game_camera_override_end(&context, &progress); }
};

void zero_flag_path() {
  Fixture f;
  f.put(Flag, 0, 1);
  auto entry = f.context.machine;
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.calls.empty());
  check(f.progress.operations == 3 && f.progress.accesses == 3 &&
        f.progress.reads == 2 && f.progress.stores == 1 &&
        f.progress.flag.word == 0 && f.progress.flag.known_mask == 15 &&
        f.progress.returned_value.word == 0 &&
        f.progress.returned_value.known_mask == 15);
  check(same(f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_AT],
             entry.registers.gpr[NBA97_MATCH_INITIALIZE_AT]) &&
        same(f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
             entry.registers.gpr[NBA97_MATCH_INITIALIZE_A0]) &&
        same(f.progress.restored_return_address,
             entry.registers.gpr[NBA97_MATCH_INITIALIZE_RA]) &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp &&
        same(f.progress.machine.hi, entry.hi) &&
        same(f.progress.machine.lo, entry.lo));
  for (unsigned i = 0; i < 32; ++i) {
    if (i == NBA97_MATCH_INITIALIZE_V0)
      continue;
    check(same(f.progress.machine.registers.gpr[i], entry.registers.gpr[i]));
  }
  std::array<std::uint32_t, 3> pcs{
      0x8007a370u, 0x8007a37cu, 0x8007a390u};
  std::array<std::uint32_t, 3> addresses{Flag, EntrySp - 8, EntrySp - 8};
  for (unsigned i = 0; i < pcs.size(); ++i)
    check(f.journal[i].pc == pcs[i] &&
          f.journal[i].address == addresses[i] &&
          f.journal[i].operation == i + 1 &&
          f.journal[i].width == (i == 0 ? 1 : 4) &&
          f.journal[i].kind == (i == 1 ? NBA97_GAME_MATCH_CLOCKS_STORE
                                       : NBA97_GAME_MATCH_CLOCKS_READ));
}

void active_flags_call_and_raw_return() {
  for (unsigned flag : {1u, 255u}) {
    Fixture f;
    f.put(Flag, flag, 1);
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
          f.calls.size() == 1 && f.get(Flag, 1) == 0 &&
          f.progress.returned_value.word == f.child_value &&
          f.progress.returned_value.known_mask == f.child_mask);
    const auto &call = f.calls[0];
    check(call.event.pc == 0x8007a380u &&
          call.event.delay_slot_pc == 0x8007a384u &&
          call.event.entry == 0x8007a114u && call.event.operation == 3 &&
          call.event.invocation == 1 && call.event.argument_count == 1 &&
          call.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
              0x8007a388u &&
          call.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0 &&
          call.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask ==
              15);
    check(f.progress.operations == 5 && f.progress.accesses == 4 &&
          f.progress.reads == 2 && f.progress.stores == 2 &&
          f.progress.callbacks_completed == 1 &&
          f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_AT].word ==
              0x800c0000u &&
          f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_AT]
                  .known_mask == 15);
    std::array<std::uint32_t, 4> pcs{
        0x8007a370u, 0x8007a37cu, 0x8007a38cu, 0x8007a390u};
    std::array<std::size_t, 4> operations{1, 2, 4, 5};
    for (unsigned i = 0; i < pcs.size(); ++i)
      check(f.journal[i].pc == pcs[i] &&
            f.journal[i].operation == operations[i]);
  }
}

void unknown_branch_and_preframe_read() {
  Fixture unknown;
  unknown.put(Flag, 1, 1, 0);
  check(unknown.run() == NBA97_TEXT_UNKNOWN &&
        unknown.progress.stopped_pc == 0x8007a378u &&
        unknown.progress.operations == 2 && unknown.progress.stores == 1 &&
        unknown.progress.flag.word == 1 &&
        unknown.progress.flag.known_mask == 0x0e &&
        unknown.progress.returned_value.word == 1 &&
        unknown.progress.returned_value.known_mask == 0x0e &&
        unknown.get(EntrySp - 8, 4) == Return && unknown.calls.empty());

  Fixture budget;
  budget.context.operation_budget = 0;
  check(budget.run() == NBA97_TEXT_LIMIT &&
        budget.progress.stopped_pc == 0x8007a370u &&
        budget.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp &&
        budget.progress.returned_value.word == 0x800c0000u);

  Fixture missing;
  const std::size_t flag_offset = Flag - Ram;
  Nba97GameTextRegion regions[2]{
      {Ram, missing.bytes.data(), missing.known.data(), flag_offset},
      {Flag + 1, missing.bytes.data() + flag_offset + 1,
       missing.known.data() + flag_offset + 1,
       missing.bytes.size() - flag_offset - 1}};
  missing.context.memory = {regions, 2};
  check(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc == 0x8007a370u &&
        missing.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp);
}

void callback_mutation_refusal_and_invalid_machine() {
  Fixture mutation;
  mutation.mutate = true;
  check(mutation.run() == NBA97_TEXT_COMPLETE && mutation.progress.completed &&
        mutation.get(Flag, 1) == 0 &&
        mutation.progress.returned_value.word == 0x60000002u &&
        mutation.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_AT].word ==
            0x800c0000u &&
        mutation.progress.machine.hi.word == 0xaabbccddu &&
        mutation.progress.machine.hi.known_mask == 9 &&
        mutation.progress.machine.lo.word == 0x55667788u &&
        mutation.progress.machine.lo.known_mask == 6 &&
        mutation.progress.restored_return_address.word == 0x81234567u &&
        mutation.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            0x800ff098u);
  for (unsigned i = 1; i < 32; ++i) {
    if (i == NBA97_MATCH_INITIALIZE_AT || i == NBA97_MATCH_INITIALIZE_SP ||
        i == NBA97_MATCH_INITIALIZE_RA)
      continue;
    check(mutation.progress.machine.registers.gpr[i].word == 0x60000000u + i &&
          mutation.progress.machine.registers.gpr[i].known_mask ==
              (i % 15) + 1);
  }

  Fixture refused;
  refused.refuse = true;
  check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.progress.stopped_pc == 0x8007a380u &&
        refused.progress.stopped_entry == 0x8007a114u &&
        refused.progress.operations == 3 &&
        refused.progress.callbacks_completed == 0 &&
        refused.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x8007a388u &&
        refused.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            0 && refused.get(Flag, 1) == 1);
  Fixture absent;
  absent.context.io = nullptr;
  check(absent.run() == NBA97_TEXT_IO_REFUSED &&
        absent.progress.stopped_pc == 0x8007a380u && absent.get(Flag, 1) == 1);
  Fixture invalid;
  invalid.invalidate = true;
  check(invalid.run() == NBA97_TEXT_ARGUMENT &&
        invalid.progress.stopped_pc == 0x8007a380u &&
        invalid.progress.callbacks_completed == 0 &&
        invalid.progress.call_count[NBA97_GAME_CAMERA_OVERRIDE_END_CHILD_8007A114] ==
            0 && invalid.get(Flag, 1) == 1);
}

void every_budget_prefix() {
  std::array<std::uint32_t, 5> active_pc{
      0x8007a370u, 0x8007a37cu, 0x8007a380u, 0x8007a38cu,
      0x8007a390u};
  for (unsigned budget = 0; budget <= 5; ++budget) {
    Fixture f;
    f.context.operation_budget = budget;
    int result = f.run();
    check((budget == 5 && result == NBA97_TEXT_COMPLETE &&
           f.progress.completed) ||
          (budget < 5 && result == NBA97_TEXT_LIMIT &&
           f.progress.stopped_pc == active_pc[budget] &&
           !f.progress.completed));
    check(f.progress.operations == budget);
  }
  std::array<std::uint32_t, 3> zero_pc{
      0x8007a370u, 0x8007a37cu, 0x8007a390u};
  for (unsigned budget = 0; budget <= 3; ++budget) {
    Fixture f;
    f.put(Flag, 0, 1);
    f.context.operation_budget = budget;
    int result = f.run();
    check((budget == 3 && result == NBA97_TEXT_COMPLETE &&
           f.progress.completed) ||
          (budget < 3 && result == NBA97_TEXT_LIMIT &&
           f.progress.stopped_pc == zero_pc[budget] &&
           !f.progress.completed));
    check(f.progress.operations == budget);
  }
  Fixture call_delay;
  call_delay.context.operation_budget = 2;
  check(call_delay.run() == NBA97_TEXT_LIMIT &&
        call_delay.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
                .word == 0x8007a388u &&
        call_delay.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
                .word == 0);
  Fixture clear_prefix;
  clear_prefix.context.operation_budget = 3;
  check(clear_prefix.run() == NBA97_TEXT_LIMIT &&
        clear_prefix.progress.stopped_pc == 0x8007a38cu &&
        clear_prefix.progress.callbacks_completed == 1 &&
        clear_prefix.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_AT]
                .word == 0x800c0000u &&
        clear_prefix.get(Flag, 1) == 1);
}

void aliases_stack_wrap_and_failures() {
  Fixture active_alias;
  active_alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
      Flag + 8, 15};
  check(active_alias.run() == NBA97_TEXT_COMPLETE &&
        active_alias.calls.size() == 1 && active_alias.get(Flag, 1) == 0 &&
        active_alias.progress.restored_return_address.word == 0x80065500u);
  Fixture zero_alias;
  zero_alias.put(Flag, 0, 1);
  zero_alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
      Flag + 8, 15};
  check(zero_alias.run() == NBA97_TEXT_COMPLETE && zero_alias.calls.empty() &&
        zero_alias.get(Flag, 1) == (Return & 0xffu) &&
        zero_alias.progress.restored_return_address.word == Return);

  Fixture wrapping;
  std::array<std::uint8_t, 4> low{};
  std::array<std::uint8_t, 4> low_known{};
  low_known.fill(1);
  Nba97GameTextRegion wrap_regions[2]{
      wrapping.region, {0, low.data(), low_known.data(), low.size()}};
  wrapping.context.memory = {wrap_regions, 2};
  wrapping.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {8, 15};
  check(wrapping.run() == NBA97_TEXT_COMPLETE &&
        wrapping.progress.frame_stack_pointer == 0xfffffff0u &&
        wrapping.journal[1].address == 0 && wrapping.journal[3].address == 0 &&
        wrapping.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            8);

  Fixture unknown_ra;
  unknown_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .known_mask = 7;
  check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.stopped_pc == 0x8007a398u &&
        unknown_ra.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                .word == EntrySp);
  Fixture unknown_sp;
  unknown_sp.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
      .known_mask = 7;
  check(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stopped_pc == 0x8007a37cu &&
        unknown_sp.progress.operations == 1 && unknown_sp.progress.reads == 1);
  Fixture unaligned_sp;
  unaligned_sp.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word =
      EntrySp + 1;
  check(unaligned_sp.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_sp.progress.stopped_pc == 0x8007a37cu);
  Fixture missing_stack;
  missing_stack.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word =
      0x90000018u;
  check(missing_stack.run() == NBA97_TEXT_RESOURCE &&
        missing_stack.progress.stopped_pc == 0x8007a37cu);
  Fixture live_unaligned;
  live_unaligned.unalign_sp = true;
  check(live_unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        live_unaligned.progress.stopped_pc == 0x8007a390u &&
        live_unaligned.get(Flag, 1) == 0);
  Fixture live_unknown;
  live_unknown.unknown_sp = true;
  check(live_unknown.run() == NBA97_TEXT_UNKNOWN &&
        live_unknown.progress.stopped_pc == 0x8007a390u &&
        live_unknown.progress.operations == 4 && live_unknown.get(Flag, 1) == 0);
  Fixture live_unmapped;
  live_unmapped.unmap_sp = true;
  check(live_unmapped.run() == NBA97_TEXT_RESOURCE &&
        live_unmapped.progress.stopped_pc == 0x8007a390u &&
        live_unmapped.progress.operations == 5 && live_unmapped.get(Flag, 1) == 0);

  Fixture raw_store;
  raw_store.region.known = nullptr;
  raw_store.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask =
      7;
  check(raw_store.run() == NBA97_TEXT_ARGUMENT &&
        raw_store.progress.stopped_pc == 0x8007a37cu &&
        raw_store.progress.operations == 2);
}

void metadata_and_repeatability() {
  Nba97GameCameraOverrideEndProgress progress{};
  Fixture f;
  check(nba97_game_camera_override_end(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
  check(nba97_game_camera_override_end(&f.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
  f.context.machine.registers.gpr[0] = {1, 15};
  check(f.run() == NBA97_TEXT_ARGUMENT);
  Fixture mask;
  mask.context.machine.lo.known_mask = 16;
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
  bad_known.known[Flag - Ram] = 2;
  check(bad_known.run() == NBA97_TEXT_ARGUMENT &&
        bad_known.progress.stopped_pc == 0x8007a370u);

  Fixture one;
  Fixture two;
  one.child_value = two.child_value = 0xffffffffu;
  check(one.run() == NBA97_TEXT_COMPLETE &&
        two.run() == NBA97_TEXT_COMPLETE &&
        one.progress.operations == two.progress.operations &&
        one.progress.returned_value.word == two.progress.returned_value.word &&
        one.get(Flag, 1) == two.get(Flag, 1));
}
} // namespace

int main() {
  zero_flag_path();
  active_flags_call_and_raw_return();
  unknown_branch_and_preframe_read();
  callback_mutation_refusal_and_invalid_machine();
  every_budget_prefix();
  aliases_stack_wrap_and_failures();
  metadata_and_repeatability();
  std::printf("game camera override end: %u checks passed\n", checks);
}
