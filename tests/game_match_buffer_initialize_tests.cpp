#include "recovered/game_match_buffer_initialize.h"

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
    std::fprintf(stderr, "match buffer initialize check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Buffer = 0x800f9ffcu;
constexpr std::uint32_t EntrySp = 0x800ff800u;
constexpr std::uint32_t Return = 0x80065b00u;

bool same(Nba97GameMatchBufferInitializeWord a,
          Nba97GameMatchBufferInitializeWord b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}

struct Call {
  Nba97GameMatchBufferInitializeEvent event{};
  Nba97GameMatchBufferInitializeMachine machine{};
};

struct Fixture {
  std::vector<std::uint8_t> bytes =
      std::vector<std::uint8_t>(0x110000, 0xa5);
  std::vector<std::uint8_t> known =
      std::vector<std::uint8_t>(0x110000, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameMatchBufferInitializeAccess, 16> journal{};
  Nba97GameMatchBufferInitializeContext context{};
  Nba97GameMatchBufferInitializeProgress progress{};
  std::vector<Call> calls;
  unsigned refuse = 0;
  unsigned invalidate = 0;
  unsigned invalidate_hi = 0;
  unsigned invalidate_lo = 0;
  bool mutate_final = false;
  bool malformed_final_load = false;
  bool unalign_final_sp = false;
  bool unknown_final_sp = false;
  bool unmap_final_sp = false;
  std::uint32_t final_v0 = 0x2468ace0u;
  std::uint8_t final_v0_mask = 7;

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
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {Return, 15};
    context.machine.hi = {0x12345678u, 5};
    context.machine.lo = {0x9abcdef0u, 10};
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
    std::uint32_t result = 0;
    for (unsigned i = 0; i < width; ++i)
      result |= std::uint32_t(bytes[offset + i]) << (i * 8u);
    return result;
  }

  static int io(void *user, const Nba97GameTextMemory *,
                const Nba97GameMatchBufferInitializeEvent *event,
                Nba97GameMatchBufferInitializeMachine *machine) {
    auto &f = *static_cast<Fixture *>(user);
    f.calls.push_back({*event, *machine});
    unsigned n = static_cast<unsigned>(f.calls.size());
    if (f.refuse == n)
      return 0;
    if (event->entry == 0x800a3a74u) {
      for (unsigned i = 0; i < 0x378; ++i)
        f.put(Buffer + i, 0, 1);
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {0x11112222u, 3};
      machine->hi = {0xabcdef01u, 6};
    } else {
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {
          f.final_v0, f.final_v0_mask};
      if (f.mutate_final) {
        constexpr std::uint32_t Frame = 0x800ff880u;
        for (unsigned i = 1; i < 32; ++i)
          machine->registers.gpr[i] = {
              0x60000000u + i,
              static_cast<std::uint8_t>((i % 15) + 1)};
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Frame, 15};
        machine->hi = {0xaabbccddu, 9};
        machine->lo = {0x55667788u, 6};
        f.put(Frame + 0x18, 0x81234567u, 4);
        f.put(0x800fa004u, 0xdeadbeefu, 4);
      }
      if (f.unalign_final_sp)
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x800ff881u, 15};
      if (f.unknown_final_sp)
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x800ff880u, 7};
      if (f.unmap_final_sp)
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x90000000u, 15};
      if (f.malformed_final_load) {
        const auto address =
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word + 0x18u;
        f.known[address - Ram + 3] = 2;
      }
    }
    if (f.invalidate == n)
      machine->registers.gpr[0] = {1, 15};
    if (f.invalidate_hi == n)
      machine->hi.known_mask = 16;
    if (f.invalidate_lo == n)
      machine->lo.known_mask = 16;
    return 1;
  }
  int run() { return nba97_game_match_buffer_initialize(&context, &progress); }
};

void normal_path_and_journal() {
  Fixture f;
  auto entry = f.context.machine;
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.calls.size() == 2);
  check(f.progress.operations == 7 && f.progress.accesses == 5 &&
        f.progress.reads == 1 && f.progress.stores == 4 &&
        f.progress.callbacks_completed == 2);
  check(f.calls[0].event.pc == 0x8006433cu &&
        f.calls[0].event.delay_slot_pc == 0x80064340u &&
        f.calls[0].event.entry == 0x800a3a74u &&
        f.calls[0].event.operation == 2 &&
        f.calls[0].event.invocation == 1 &&
        f.calls[0].event.argument_count == 2 &&
        f.calls[0].machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x80064344u &&
        f.calls[0].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            Buffer &&
        f.calls[0].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
            0x378);
  check(f.calls[1].event.pc == 0x80064370u &&
        f.calls[1].event.delay_slot_pc == 0x80064374u &&
        f.calls[1].event.entry == 0x80076ad0u &&
        f.calls[1].event.operation == 6 &&
        f.calls[1].event.invocation == 1 &&
        f.calls[1].event.argument_count == 0 &&
        f.calls[1].machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x80064378u);
  check(f.get(Buffer, 4) == 0 && f.get(0x800fa000u, 2) == 0x76 &&
        f.get(0x800fa002u, 2) == 0 &&
        f.get(0x800fa004u, 4) == 0x800ccc00u &&
        f.get(0x800fa008u, 4) == 0x800d5734u &&
        f.get(Buffer + 0x377, 1) == 0);
  check(f.progress.returned_value.word == f.final_v0 &&
        f.progress.returned_value.known_mask == f.final_v0_mask &&
        same(f.progress.restored_return_address,
             entry.registers.gpr[NBA97_MATCH_INITIALIZE_RA]) &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp && f.progress.machine.hi.word == 0xabcdef01u &&
        f.progress.machine.hi.known_mask == 6 &&
        same(f.progress.machine.lo, entry.lo));
  for (unsigned i = 0; i < 32; ++i) {
    if (i == NBA97_MATCH_INITIALIZE_AT || i == NBA97_MATCH_INITIALIZE_V0 ||
        i == NBA97_MATCH_INITIALIZE_V1 || i == NBA97_MATCH_INITIALIZE_A0 ||
        i == NBA97_MATCH_INITIALIZE_A1 || i == NBA97_MATCH_INITIALIZE_S0)
      continue;
    check(same(f.progress.machine.registers.gpr[i],
               entry.registers.gpr[i]));
  }
  std::array<std::uint32_t, 5> pcs{
      0x80064338u, 0x8006434cu, 0x80064360u, 0x8006436cu,
      0x80064378u};
  std::array<std::uint32_t, 5> addresses{
      EntrySp - 8, 0x800fa000u, 0x800fa004u, 0x800fa008u, EntrySp - 8};
  std::array<std::size_t, 5> operations{1, 3, 4, 5, 7};
  for (unsigned i = 0; i < pcs.size(); ++i)
    check(f.journal[i].pc == pcs[i] &&
          f.journal[i].address == addresses[i] &&
          f.journal[i].operation == operations[i] &&
          f.journal[i].width == (i == 1 ? 2 : 4) &&
          f.journal[i].kind == (i == 4 ? NBA97_GAME_MATCH_CLOCKS_READ
                                       : NBA97_GAME_MATCH_CLOCKS_STORE));
}

void every_budget_and_refusal_prefix() {
  std::array<std::uint32_t, 7> pcs{
      0x80064338u, 0x8006433cu, 0x8006434cu, 0x80064360u,
      0x8006436cu, 0x80064370u, 0x80064378u};
  for (unsigned budget = 0; budget <= 7; ++budget) {
    Fixture f;
    f.context.operation_budget = budget;
    int result = f.run();
    check((budget == 7 && result == NBA97_TEXT_COMPLETE &&
           f.progress.completed) ||
          (budget < 7 && result == NBA97_TEXT_LIMIT &&
           f.progress.stopped_pc == pcs[budget] && !f.progress.completed));
    check(f.progress.operations == budget);
  }
  Fixture zero_limit;
  zero_limit.context.operation_budget = 1;
  check(zero_limit.run() == NBA97_TEXT_LIMIT &&
        zero_limit.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
                .word == 0x80064344u &&
        zero_limit.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
                .word == 0x378);
  Fixture final_limit;
  final_limit.context.operation_budget = 5;
  check(final_limit.run() == NBA97_TEXT_LIMIT &&
        final_limit.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
                .word == 0x80064378u &&
        final_limit.get(0x800fa008u, 4) == 0x800d5734u);
  for (unsigned call = 1; call <= 2; ++call) {
    Fixture f;
    f.refuse = call;
    check(f.run() == NBA97_TEXT_IO_REFUSED &&
          f.progress.stopped_pc == (call == 1 ? 0x8006433cu : 0x80064370u) &&
          f.progress.stopped_entry ==
              (call == 1 ? 0x800a3a74u : 0x80076ad0u) &&
          f.progress.callbacks_completed == call - 1);
  }
  for (unsigned call = 1; call <= 2; ++call) {
    Fixture f;
    f.invalidate = call;
    check(f.run() == NBA97_TEXT_ARGUMENT &&
          f.progress.callbacks_completed == call - 1 &&
          f.progress.call_count[call] == 0);
  }
  for (unsigned special = 0; special < 2; ++special) {
    for (unsigned call = 1; call <= 2; ++call) {
      Fixture f;
      if (special == 0)
        f.invalidate_hi = call;
      else
        f.invalidate_lo = call;
      check(f.run() == NBA97_TEXT_ARGUMENT &&
            f.progress.stopped_pc ==
                (call == 1 ? 0x8006433cu : 0x80064370u) &&
            f.progress.callbacks_completed == call - 1 &&
            f.progress.call_count[call] == 0 &&
            (special == 0 ? f.progress.machine.hi.known_mask
                          : f.progress.machine.lo.known_mask) == 16);
    }
  }
}

void full_machine_mutation_and_aliases() {
  Fixture f;
  f.mutate_final = true;
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.get(0x800fa004u, 4) == 0xdeadbeefu &&
        f.progress.restored_return_address.word == 0x81234567u &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            0x800ff8a0u && f.progress.machine.hi.word == 0xaabbccddu &&
        f.progress.machine.lo.word == 0x55667788u);
  for (unsigned i = 1; i < 32; ++i) {
    if (i == NBA97_MATCH_INITIALIZE_SP || i == NBA97_MATCH_INITIALIZE_RA)
      continue;
    check(f.progress.machine.registers.gpr[i].word == 0x60000000u + i &&
          f.progress.machine.registers.gpr[i].known_mask == (i % 15) + 1);
  }

  Fixture alias;
  alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
      0x800fa008u, 15};
  check(alias.run() == NBA97_TEXT_COMPLETE &&
        alias.progress.frame_stack_pointer == 0x800f9fe8u &&
        alias.progress.restored_return_address.word == 0x76u &&
        alias.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            0x800fa008u);

  Fixture wrapping;
  std::array<std::uint8_t, 4> low{};
  std::array<std::uint8_t, 4> low_known{};
  low_known.fill(1);
  Nba97GameTextRegion regions[2]{
      wrapping.region, {0, low.data(), low_known.data(), low.size()}};
  wrapping.context.memory = {regions, 2};
  wrapping.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {8, 15};
  check(wrapping.run() == NBA97_TEXT_COMPLETE &&
        wrapping.progress.frame_stack_pointer == 0xffffffe8u &&
        wrapping.journal[0].address == 0 && wrapping.journal[4].address == 0 &&
        wrapping.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            8);
}

void address_knownness_and_metadata_failures() {
  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture saved_ra;
    saved_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
        .known_mask = static_cast<std::uint8_t>(mask);
    const int result = saved_ra.run();
    check((mask == 15 && result == NBA97_TEXT_COMPLETE &&
           saved_ra.progress.completed) ||
          (mask != 15 && result == NBA97_TEXT_UNKNOWN &&
           saved_ra.progress.stopped_pc == 0x80064380u &&
           !saved_ra.progress.completed));
    check(saved_ra.progress.machine.registers
                  .gpr[NBA97_MATCH_INITIALIZE_SP]
                  .word == EntrySp &&
          saved_ra.progress.machine.registers
                  .gpr[NBA97_MATCH_INITIALIZE_RA]
                  .known_mask == mask);
  }
  Fixture unknown_sp;
  unknown_sp.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
      .known_mask = 7;
  check(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stopped_pc == 0x80064338u);
  Fixture unaligned;
  unaligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word =
      EntrySp + 1;
  check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x80064338u);
  Fixture missing;
  missing.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word =
      0x90000008u;
  check(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc == 0x80064338u);
  Fixture live_unaligned;
  live_unaligned.unalign_final_sp = true;
  check(live_unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        live_unaligned.progress.stopped_pc == 0x80064378u);
  Fixture live_unknown;
  live_unknown.unknown_final_sp = true;
  check(live_unknown.run() == NBA97_TEXT_UNKNOWN &&
        live_unknown.progress.stopped_pc == 0x80064378u &&
        live_unknown.progress.operations == 6);
  Fixture live_unmapped;
  live_unmapped.unmap_final_sp = true;
  check(live_unmapped.run() == NBA97_TEXT_RESOURCE &&
        live_unmapped.progress.stopped_pc == 0x80064378u);

  Fixture missing_global;
  const std::size_t gap = 0x800fa004u - Ram;
  Nba97GameTextRegion split[2]{
      {Ram, missing_global.bytes.data(), missing_global.known.data(), gap},
      {0x800fa008u, missing_global.bytes.data() + gap + 4,
       missing_global.known.data() + gap + 4,
       missing_global.bytes.size() - gap - 4}};
  missing_global.context.memory = {split, 2};
  check(missing_global.run() == NBA97_TEXT_RESOURCE &&
        missing_global.progress.stopped_pc == 0x80064360u &&
        missing_global.get(0x800fa000u, 2) == 0x76);

  Fixture raw;
  raw.region.known = nullptr;
  raw.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 7;
  check(raw.run() == NBA97_TEXT_ARGUMENT &&
        raw.progress.stopped_pc == 0x80064338u);
  Fixture malformed;
  auto stack = EntrySp - 8;
  malformed.put(stack, 0x11223344u, 4);
  malformed.known[stack - Ram + 3] = 2;
  check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.stopped_pc == 0x80064338u &&
        malformed.get(stack, 4) == 0x11223344u);
  Fixture malformed_final;
  malformed_final.malformed_final_load = true;
  check(malformed_final.run() == NBA97_TEXT_ARGUMENT &&
        malformed_final.progress.stopped_pc == 0x80064378u &&
        malformed_final.progress.operations == 7 &&
        malformed_final.progress.stores == 4 &&
        malformed_final.progress.reads == 0 &&
        malformed_final.progress.callbacks_completed == 2 &&
        malformed_final.progress.machine
                .registers.gpr[NBA97_MATCH_INITIALIZE_RA]
                .word == 0x80064378u &&
        malformed_final.get(0x800fa008u, 4) == 0x800d5734u);

  Nba97GameMatchBufferInitializeProgress progress{};
  Fixture argument;
  check(nba97_game_match_buffer_initialize(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
  check(nba97_game_match_buffer_initialize(&argument.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
  argument.context.machine.registers.gpr[0] = {1, 15};
  check(argument.run() == NBA97_TEXT_ARGUMENT);
  Fixture bad_mask;
  bad_mask.context.machine.hi.known_mask = 16;
  check(bad_mask.run() == NBA97_TEXT_ARGUMENT);
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
  Fixture a;
  Fixture b;
  check(a.run() == NBA97_TEXT_COMPLETE && b.run() == NBA97_TEXT_COMPLETE &&
        a.progress.operations == b.progress.operations &&
        a.progress.returned_value.word == b.progress.returned_value.word &&
        a.get(0x800fa008u, 4) == b.get(0x800fa008u, 4));
  for (unsigned i = 0; i < 32; ++i)
    check(same(a.progress.machine.registers.gpr[i],
               b.progress.machine.registers.gpr[i]));
  check(same(a.progress.machine.hi, b.progress.machine.hi) &&
        same(a.progress.machine.lo, b.progress.machine.lo));
  check(a.bytes == b.bytes && a.known == b.known);
}
} // namespace

int main() {
  normal_path_and_journal();
  every_budget_and_refusal_prefix();
  full_machine_mutation_and_aliases();
  address_knownness_and_metadata_failures();
  repeatability();
  std::printf("game match buffer initialize: %u checks passed\n", checks);
}
