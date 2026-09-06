#include "recovered/frontend_overlay_load.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using U = std::uint32_t;
unsigned checks;
void checkAt(bool value, int line) {
  ++checks;
  if (!value)
    throw std::runtime_error("frontend-overlay-load failed at " +
                             std::to_string(line));
}
#define CHECK(value) checkAt((value), __LINE__)

constexpr U Base = 0x80000000u;
constexpr U Size = 0x200000u;
constexpr U Sp = 0x801f0000u;
constexpr U ParentRa = 0x80028ad4u;

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendOverlayLoadContext context{};
  Nba97FrontendOverlayLoadProgress progress{};
  std::array<Nba97FrontendOverlayLoadAccess, 8> access{};
  std::array<U, 16> instructions{};
  Nba97FrontendOverlayLoadEvent event{};
  Nba97FrontendOverlayLoadMachine called_machine{};
  U relocated_sp = 0;
  U replacement_ra = 0;
  std::uint8_t replacement_ra_mask = 15;
  bool refuse = false;
  bool malformed = false;
  bool mutate = false;
  unsigned calls = 0;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {
          0x41000000u + i * 0x10101u, std::uint8_t((i % 15u) + 1u)};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A0] = {
        0x80024854u, 5};
    context.machine.registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A1] = {
        0x13579bdfu, 10};
    context.machine.registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A2] = {
        0xa5a5a5a5u, 3};
    context.machine.registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_SP] = {Sp, 15};
    context.machine.registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_RA] = {ParentRa,
                                                                     15};
    context.machine.hi = {0x12345678u, 5};
    context.machine.lo = {0x9abcdef0u, 10};
    context.memory = {&region, 1};
    context.operation_budget = 3;
    context.io = callback;
    context.user = this;
    context.access_journal = access.data();
    context.access_journal_capacity = access.size();
    context.instruction_journal = instructions.data();
    context.instruction_journal_capacity = instructions.size();
  }

  void put(U address, U value, std::uint8_t mask = 15) {
    for (unsigned i = 0; i < 4; ++i) {
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
      if (region.known)
        known[address - Base + i] = std::uint8_t((mask >> i) & 1u);
    }
  }
  U get(U address) const {
    U value = 0;
    for (unsigned i = 0; i < 4; ++i)
      value |= U(bytes[address - Base + i]) << (i * 8u);
    return value;
  }
  int run() { return nba97_frontend_overlay_load(&context, &progress); }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97FrontendOverlayLoadEvent *event,
                      Nba97FrontendOverlayLoadMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    ++f.calls;
    if (!event || !machine || event->pc != 0x8007b124u ||
        event->delay_slot_pc != 0x8007b128u ||
        event->entry != 0x8007b15cu || event->operation != 2 ||
        event->invocation != 1 ||
        event->site != NBA97_FRONTEND_OVERLAY_LOAD_SITE_8007B124 ||
        event->argument_count != 3 ||
        event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
        machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_RA].word !=
            0x8007b12cu ||
        machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_RA].known_mask !=
            15 ||
        machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A2].word != 1 ||
        machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A2].known_mask != 15)
      return 0;
    f.event = *event;
    f.called_machine = *machine;
    if (f.mutate) {
      for (unsigned reg = 1; reg < 31; ++reg)
        if (reg != NBA97_FRONTEND_OVERLAY_LOAD_SP)
          machine->registers.gpr[reg] = {
              0x89000000u + reg * 0x101u,
              std::uint8_t((reg % 15u) + 1u)};
      machine->hi = {0x55667788u, 6};
      machine->lo = {0xaabbccddu, 9};
    }
    machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_V0] = {0x80170000u,
                                                               6};
    if (f.relocated_sp) {
      machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_SP] = {f.relocated_sp,
                                                                 15};
      if (f.relocated_sp >= Base && f.relocated_sp - Base <= Size - 20u)
        f.put(f.relocated_sp + 16u, f.replacement_ra,
              f.replacement_ra_mask);
    }
    if (f.malformed)
      machine->registers.gpr[9].known_mask = 16;
    return !f.refuse;
  }
};

void normalAndTrace() {
  Fixture f;
  CHECK(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
  CHECK(f.progress.operations == 3 && f.progress.accesses == 2 &&
        f.progress.reads == 1 && f.progress.stores == 1 &&
        f.progress.callbacks_completed == 1 && f.calls == 1);
  CHECK(f.progress.instruction_count == 8 &&
        f.progress.instruction_events == 8 && f.progress.access_events == 2);
  for (unsigned i = 0; i < 8; ++i)
    CHECK(f.instructions[i] == 0x8007b11cu + 4u * i);
  CHECK(f.access[0].pc == 0x8007b120u &&
        f.access[0].address == Sp - 8u &&
        f.access[0].value == ParentRa && f.access[0].known_mask == 15 &&
        f.access[0].kind == NBA97_FRONTEND_OVERLAY_LOAD_STORE &&
        f.access[0].operation == 1);
  CHECK(f.access[1].pc == 0x8007b12cu &&
        f.access[1].address == Sp - 8u &&
        f.access[1].value == ParentRa && f.access[1].kind == 1 &&
        f.access[1].operation == 3);
  CHECK(f.progress.forwarded_a0.word == 0x80024854u &&
        f.progress.forwarded_a0.known_mask == 5 &&
        f.progress.forwarded_a1.word == 0x13579bdfu &&
        f.progress.forwarded_a1.known_mask == 10);
  CHECK(f.progress.delay_a2.word == 1 &&
        f.progress.delay_a2.known_mask == 15 &&
        f.called_machine.registers.gpr[6].word == 1 &&
        f.called_machine.registers.gpr[6].known_mask == 15);
  CHECK(f.progress.child_return.word == 0x80170000u &&
        f.progress.child_return.known_mask == 6 &&
        f.progress.machine.registers.gpr[2].word == 0x80170000u &&
        f.progress.machine.registers.gpr[2].known_mask == 6);
  CHECK(f.progress.saved_return_address.word == ParentRa &&
        f.progress.restored_return_address.word == ParentRa &&
        f.progress.frame_stack_pointer == Sp - 24u &&
        f.progress.machine.registers.gpr[29].word == Sp &&
        f.progress.machine.registers.gpr[31].word == ParentRa);
  CHECK(f.get(Sp - 8u) == ParentRa);
}

void machineMutationAndRelocation() {
  Fixture changed;
  changed.mutate = true;
  CHECK(changed.run() == NBA97_TEXT_COMPLETE);
  for (unsigned reg = 1; reg < 31; ++reg) {
    if (reg == NBA97_FRONTEND_OVERLAY_LOAD_SP ||
        reg == NBA97_FRONTEND_OVERLAY_LOAD_V0)
      continue;
    CHECK(changed.progress.machine.registers.gpr[reg].word ==
              0x89000000u + reg * 0x101u &&
          changed.progress.machine.registers.gpr[reg].known_mask ==
              (reg % 15u) + 1u);
  }
  CHECK(changed.progress.machine.registers.gpr[2].word == 0x80170000u &&
        changed.progress.machine.registers.gpr[2].known_mask == 6 &&
        changed.progress.machine.hi.word == 0x55667788u &&
        changed.progress.machine.hi.known_mask == 6 &&
        changed.progress.machine.lo.word == 0xaabbccddu &&
        changed.progress.machine.lo.known_mask == 9);

  Fixture relocated;
  relocated.relocated_sp = 0x801e0000u;
  relocated.replacement_ra = 0x80076540u;
  CHECK(relocated.run() == NBA97_TEXT_COMPLETE &&
        relocated.progress.restored_return_address.word == 0x80076540u &&
        relocated.progress.machine.registers.gpr[29].word == 0x801e0018u &&
        relocated.progress.machine.registers.gpr[31].word == 0x80076540u);

  Fixture alias;
  alias.relocated_sp = Sp - 24u;
  alias.replacement_ra = 0x80001000u;
  CHECK(alias.run() == NBA97_TEXT_COMPLETE &&
        alias.progress.restored_return_address.word == 0x80001000u);
}

void argumentMasksForwardExactly() {
  for (unsigned a0_mask = 0; a0_mask < 16; ++a0_mask)
    for (unsigned a1_mask = 0; a1_mask < 16; ++a1_mask) {
      Fixture f;
      f.context.machine.registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A0]
          .known_mask = std::uint8_t(a0_mask);
      f.context.machine.registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A1]
          .known_mask = std::uint8_t(a1_mask);
      CHECK(f.run() == NBA97_TEXT_COMPLETE &&
            f.called_machine.registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A0]
                    .known_mask == a0_mask &&
            f.called_machine.registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A1]
                    .known_mask == a1_mask &&
            f.called_machine.registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A2]
                    .known_mask == 15);
    }
}

void budgetsAndCallbackFailures() {
  for (std::size_t budget = 0; budget < 3; ++budget) {
    Fixture f;
    f.context.operation_budget = budget;
    CHECK(f.run() == NBA97_TEXT_LIMIT && f.progress.operations == budget &&
          !f.progress.completed);
    if (budget == 0)
      CHECK(f.progress.instruction_count == 2 && f.progress.accesses == 0 &&
            f.calls == 0 && f.progress.stopped_pc == 0x8007b120u);
    if (budget == 1)
      CHECK(f.progress.instruction_count == 4 && f.progress.accesses == 1 &&
            f.calls == 0 && f.progress.stopped_pc == 0x8007b124u &&
            f.progress.stopped_target == 0x8007b15cu);
    if (budget == 2)
      CHECK(f.progress.instruction_count == 5 && f.progress.accesses == 1 &&
            f.calls == 1 && f.progress.callbacks_completed == 1 &&
            f.progress.stopped_pc == 0x8007b12cu);
  }

  Fixture missing;
  missing.context.io = nullptr;
  CHECK(missing.run() == NBA97_TEXT_IO_REFUSED && missing.calls == 0 &&
        missing.progress.operations == 2 &&
        missing.progress.call_attempts[1] == 1 &&
        missing.progress.callbacks_completed == 0 &&
        missing.progress.instruction_count == 4);

  Fixture refused;
  refused.refuse = true;
  CHECK(refused.run() == NBA97_TEXT_IO_REFUSED && refused.calls == 1 &&
        refused.progress.operations == 2 &&
        refused.progress.callbacks_completed == 0 &&
        refused.progress.machine.registers.gpr[2].word == 0x80170000u);

  Fixture malformed;
  malformed.malformed = true;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT && malformed.calls == 1 &&
        malformed.progress.operations == 2 &&
        malformed.progress.machine.registers.gpr[9].known_mask == 16);
}

void memoryAndKnownnessFailures() {
  Fixture partial_sp;
  partial_sp.context.machine.registers.gpr[29].known_mask = 14;
  CHECK(partial_sp.run() == NBA97_TEXT_UNKNOWN &&
        partial_sp.progress.instruction_count == 2 &&
        partial_sp.progress.operations == 0 &&
        partial_sp.progress.stopped_pc == 0x8007b120u);

  Fixture misaligned;
  misaligned.context.machine.registers.gpr[29].word = Sp + 1u;
  CHECK(misaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misaligned.progress.operations == 1 && misaligned.progress.accesses == 1 &&
        misaligned.progress.stopped_address == Sp - 7u);

  Fixture unmapped;
  unmapped.context.machine.registers.gpr[29].word = 0x80300000u;
  CHECK(unmapped.run() == NBA97_TEXT_RESOURCE &&
        unmapped.progress.operations == 1 && unmapped.progress.accesses == 1);

  Fixture no_plane;
  no_plane.region.known = nullptr;
  CHECK(no_plane.run() == NBA97_TEXT_COMPLETE);
  Fixture no_plane_partial_ra;
  no_plane_partial_ra.region.known = nullptr;
  no_plane_partial_ra.context.machine.registers.gpr[31].known_mask = 7;
  CHECK(no_plane_partial_ra.run() == NBA97_TEXT_ARGUMENT &&
        no_plane_partial_ra.progress.operations == 1 &&
        no_plane_partial_ra.progress.stores == 0);

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture saved;
    saved.context.machine.registers.gpr[31].known_mask = std::uint8_t(mask);
    const int result = saved.run();
    CHECK(result == (mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN));
    CHECK(saved.progress.restored_return_address.known_mask == mask);
  }

  Fixture late_unknown;
  late_unknown.relocated_sp = 0x801e0000u;
  late_unknown.replacement_ra = 0x80001000u;
  late_unknown.replacement_ra_mask = 7;
  CHECK(late_unknown.run() == NBA97_TEXT_UNKNOWN &&
        late_unknown.progress.instruction_count == 8 &&
        late_unknown.progress.stopped_pc == 0x8007b134u &&
        late_unknown.progress.machine.registers.gpr[29].word == 0x801e0018u);

  Fixture late_alignment;
  late_alignment.relocated_sp = 0x801e0000u;
  late_alignment.replacement_ra = 0x80001002u;
  CHECK(late_alignment.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        late_alignment.progress.stopped_target == 0x80001002u);

  Fixture live_sp_alignment;
  live_sp_alignment.relocated_sp = 0x801e0002u;
  live_sp_alignment.replacement_ra = 0x80001000u;
  CHECK(live_sp_alignment.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        live_sp_alignment.progress.operations == 3 &&
        live_sp_alignment.progress.instruction_count == 5 &&
        live_sp_alignment.progress.stopped_pc == 0x8007b12cu &&
        live_sp_alignment.progress.stopped_address == 0x801e0012u);

  Fixture live_sp_unmapped;
  live_sp_unmapped.relocated_sp = 0x80300000u;
  CHECK(live_sp_unmapped.run() == NBA97_TEXT_RESOURCE &&
        live_sp_unmapped.progress.operations == 3 &&
        live_sp_unmapped.progress.instruction_count == 5 &&
        live_sp_unmapped.progress.stopped_address == 0x80300010u);

  Fixture bad_known;
  bad_known.known[Sp - 8u - Base] = 2;
  CHECK(bad_known.run() == NBA97_TEXT_ARGUMENT &&
        bad_known.progress.operations == 1 && bad_known.progress.stores == 0);

  Fixture bad_machine;
  bad_machine.context.machine.hi.known_mask = 16;
  CHECK(bad_machine.run() == NBA97_TEXT_ARGUMENT &&
        bad_machine.progress.instruction_count == 0);

  Fixture overlap;
  Nba97GameTextRegion regions[2] = {overlap.region, overlap.region};
  overlap.context.memory = {regions, 2};
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT);
  Fixture wrapped;
  Nba97GameTextRegion wrap{0xfffffffcu, wrapped.bytes.data(),
                           wrapped.known.data(), 8};
  wrapped.context.memory = {&wrap, 1};
  CHECK(wrapped.run() == NBA97_TEXT_ARGUMENT);
  Fixture empty;
  Nba97GameTextRegion zero{Base, empty.bytes.data(), empty.known.data(), 0};
  empty.context.memory = {&zero, 1};
  CHECK(empty.run() == NBA97_TEXT_ARGUMENT);

  Fixture missing_regions;
  missing_regions.context.memory = {nullptr, 1};
  CHECK(missing_regions.run() == NBA97_TEXT_ARGUMENT);
  Fixture missing_data;
  Nba97GameTextRegion null_data{Base, nullptr, missing_data.known.data(), 4};
  missing_data.context.memory = {&null_data, 1};
  CHECK(missing_data.run() == NBA97_TEXT_ARGUMENT);
  Fixture bad_journal;
  bad_journal.context.instruction_journal = nullptr;
  bad_journal.context.instruction_journal_capacity = 1;
  CHECK(bad_journal.run() == NBA97_TEXT_ARGUMENT);
  Nba97FrontendOverlayLoadProgress output{};
  CHECK(nba97_frontend_overlay_load(nullptr, &output) == NBA97_TEXT_ARGUMENT &&
        nba97_frontend_overlay_load(&bad_journal.context, nullptr) ==
            NBA97_TEXT_ARGUMENT);
}

void wrapAndDeterminism() {
  Fixture wrapped;
  std::array<std::uint8_t, 4> low{};
  std::array<std::uint8_t, 4> low_known{{1, 1, 1, 1}};
  Nba97GameTextRegion regions[2] = {
      wrapped.region, {0, low.data(), low_known.data(), low.size()}};
  wrapped.context.memory = {regions, 2};
  wrapped.context.machine.registers.gpr[29] = {8, 15};
  CHECK(wrapped.run() == NBA97_TEXT_COMPLETE &&
        wrapped.progress.frame_stack_pointer == 0xfffffff0u &&
        wrapped.progress.machine.registers.gpr[29].word == 8u &&
        U(low[0]) == (ParentRa & 0xffu));

  Fixture signed_edge;
  signed_edge.context.machine.registers.gpr[29] = {0x80000010u, 15};
  CHECK(signed_edge.run() == NBA97_TEXT_COMPLETE &&
        signed_edge.progress.frame_stack_pointer == 0x7ffffff8u &&
        signed_edge.access[0].address == 0x80000008u &&
        signed_edge.progress.machine.registers.gpr[29].word == 0x80000010u);

  Fixture a;
  Fixture b;
  CHECK(a.run() == NBA97_TEXT_COMPLETE && b.run() == NBA97_TEXT_COMPLETE);
  CHECK(a.progress.operations == b.progress.operations &&
        a.progress.instruction_count == b.progress.instruction_count &&
        a.progress.machine.registers.gpr[2].word ==
            b.progress.machine.registers.gpr[2].word);
}
} // namespace

int main() {
  try {
    normalAndTrace();
    machineMutationAndRelocation();
    argumentMasksForwardExactly();
    budgetsAndCallbackFailures();
    memoryAndKnownnessFailures();
    wrapAndDeterminism();
    std::printf("frontend_overlay_load_tests passed %u checks\n", checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
