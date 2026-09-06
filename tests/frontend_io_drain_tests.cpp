#include "recovered/frontend_io_drain.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using U = std::uint32_t;
unsigned checks;
void checkAt(bool value, int line) {
  ++checks;
  if (!value)
    throw std::runtime_error("frontend-io-drain failed at " +
                             std::to_string(line));
}
#define CHECK(value) checkAt((value), __LINE__)

constexpr U Base = 0x80000000u;
constexpr U Size = 0x200000u;
constexpr U Sp = 0x801f0000u;
constexpr U ParentRa = 0x800394f0u;
constexpr U StatusBase = 0x800ef840u;
constexpr U PointerBase = 0x800ef844u;
constexpr U AuxBase = 0x800ef830u;
constexpr std::array<U, 3> Pcs{{0x80039458u, 0x8003949cu, 0x800394acu}};
constexpr std::array<U, 3> Delays{{0x8003945cu, 0x800394a0u, 0x800394b0u}};
constexpr std::array<U, 3> Targets{{0x80077638u, 0x800392a0u, 0x80038e84u}};
constexpr std::array<unsigned, 3> ArgCounts{{1, 0, 0}};

struct Call {
  Nba97FrontendIoDrainEvent event{};
  Nba97FrontendIoDrainMachine machine{};
};

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendIoDrainContext context{};
  Nba97FrontendIoDrainProgress progress{};
  std::array<Nba97FrontendIoDrainAccess, 128> access{};
  std::array<U, 512> instructions{};
  std::vector<Call> calls;
  std::vector<Nba97FrontendIoDrainWord> poll_results{{1, 15}};
  std::array<std::size_t, NBA97_FRONTEND_IO_DRAIN_SITE_COUNT> invocations{};
  U refuse_pc = 0;
  std::size_t refuse_invocation = 1;
  bool malformed_machine = false;
  bool mutate_machine = false;
  bool mutate_handle_registers = false;
  Nba97FrontendIoDrainWord handle_s0{};
  Nba97FrontendIoDrainWord handle_s1{};
  U handle_memory_address = 0;
  U handle_memory_value = 0;
  U relocate_pc = 0;
  U relocated_frame = 0;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {
          0x41000000u + i * 0x10101u, std::uint8_t((i % 15u) + 1u)};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[NBA97_FRONTEND_IO_DRAIN_SP] = {Sp, 15};
    context.machine.registers.gpr[NBA97_FRONTEND_IO_DRAIN_S0] = {
        0x11223344u, 5};
    context.machine.registers.gpr[NBA97_FRONTEND_IO_DRAIN_S1] = {
        0x55667788u, 10};
    context.machine.registers.gpr[NBA97_FRONTEND_IO_DRAIN_RA] = {ParentRa, 15};
    context.machine.hi = {0x12345678u, 5};
    context.machine.lo = {0x9abcdef0u, 10};
    for (unsigned slot = 0; slot < 8; ++slot) {
      put(StatusBase + slot * 36u, 0);
      put(PointerBase + slot * 36u, 0x81000000u + slot * 0x100u);
      put(AuxBase + slot * 36u, 0xa0000000u + slot);
    }
    context.memory = {&region, 1};
    context.operation_budget = 128;
    context.io = callback;
    context.user = this;
    context.access_journal = access.data();
    context.access_journal_capacity = access.size();
    context.instruction_journal = instructions.data();
    context.instruction_journal_capacity = instructions.size();
  }

  void put(U address, U value, unsigned width = 4, std::uint8_t mask = 15) {
    if (address < Base || width > Size || address - Base > Size - width)
      throw std::runtime_error("fixture write outside RAM");
    for (unsigned i = 0; i < width; ++i) {
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
      if (region.known)
        known[address - Base + i] = std::uint8_t((mask >> i) & 1u);
    }
  }
  U get(U address, unsigned width = 4) const {
    U result = 0;
    for (unsigned i = 0; i < width; ++i)
      result |= U(bytes[address - Base + i]) << (i * 8u);
    return result;
  }
  void statuses(const std::array<U, 8> &values) {
    for (unsigned i = 0; i < values.size(); ++i)
      put(StatusBase + i * 36u, values[i]);
  }
  int run() { return nba97_frontend_io_drain(&context, &progress); }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97FrontendIoDrainEvent *event,
                      Nba97FrontendIoDrainMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    if (!event || !machine || event->site == NBA97_FRONTEND_IO_DRAIN_SITE_NONE ||
        event->site >= NBA97_FRONTEND_IO_DRAIN_SITE_COUNT)
      return 0;
    const unsigned index = event->site - 1;
    const std::size_t invocation = ++f.invocations[event->site];
    if (event->pc != Pcs[index] || event->delay_slot_pc != Delays[index] ||
        event->entry != Targets[index] ||
        event->argument_count != ArgCounts[index] ||
        event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
        event->invocation != invocation ||
        machine->registers.gpr[NBA97_FRONTEND_IO_DRAIN_RA].word !=
            event->pc + 8u ||
        machine->registers.gpr[NBA97_FRONTEND_IO_DRAIN_RA].known_mask != 15)
      return 0;
    f.calls.push_back({*event, *machine});
    if (event->site == NBA97_FRONTEND_IO_DRAIN_SITE_8003949C) {
      const std::size_t index_poll = invocation - 1;
      machine->registers.gpr[NBA97_FRONTEND_IO_DRAIN_V0] =
          index_poll < f.poll_results.size() ? f.poll_results[index_poll]
                                             : f.poll_results.back();
    }
    if (event->site == NBA97_FRONTEND_IO_DRAIN_SITE_80039458 &&
        f.mutate_handle_registers) {
      machine->registers.gpr[NBA97_FRONTEND_IO_DRAIN_S0] = f.handle_s0;
      machine->registers.gpr[NBA97_FRONTEND_IO_DRAIN_S1] = f.handle_s1;
    }
    if (event->site == NBA97_FRONTEND_IO_DRAIN_SITE_80039458 &&
        f.handle_memory_address)
      f.put(f.handle_memory_address, f.handle_memory_value);
    if (event->pc == f.relocate_pc)
      machine->registers.gpr[NBA97_FRONTEND_IO_DRAIN_SP] = {
          f.relocated_frame, 15};
    if (f.mutate_machine) {
      machine->registers.gpr[8] = {0x89abcdefu, 3};
      machine->registers.gpr[24] = {0x10203040u, 12};
      machine->hi = {0x55667788u, 6};
      machine->lo = {0xaabbccddu, 9};
    }
    if (f.malformed_machine)
      machine->registers.gpr[9].known_mask = 16;
    return !(event->pc == f.refuse_pc &&
             event->invocation == f.refuse_invocation);
  }
};

void fullPathAndTrace() {
  Fixture f;
  f.statuses({{3, 1, 4, 5, U(-1), 0, 2, 6}});
  f.poll_results = {{0, 15}, {1, 15}};
  CHECK(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
  CHECK(f.progress.operations == 24 && f.progress.accesses == 20 &&
        f.progress.reads == 12 && f.progress.stores == 8 &&
        f.progress.callbacks_completed == 4 &&
        f.progress.slot_iterations == 8 && f.progress.poll_attempts == 2 &&
        f.progress.zero_poll_results == 1 &&
        f.progress.instruction_count == 164 && f.calls.size() == 4);
  CHECK(f.calls[0].event.pc == 0x80039458u &&
        f.calls[0].event.argument_count == 1 &&
        f.calls[0].machine.registers.gpr[4].word == 0x81000000u &&
        f.calls[1].event.pc == 0x8003949cu &&
        f.calls[2].event.pc == 0x800394acu &&
        f.calls[3].event.pc == 0x8003949cu);
  CHECK(f.get(StatusBase) == 0 && f.get(PointerBase) == 0 &&
        f.get(StatusBase + 36u) == 0 && f.get(AuxBase + 72u) == 0 &&
        f.get(AuxBase + 108u) == 0 &&
        f.get(StatusBase + 144u) == U(-1) &&
        f.get(StatusBase + 252u) == 6);
  CHECK(f.progress.saved_s0.word == 0x11223344u &&
        f.progress.saved_s0.known_mask == 5 &&
        f.progress.saved_s1.word == 0x55667788u &&
        f.progress.saved_s1.known_mask == 10 &&
        f.progress.saved_return_address.word == ParentRa &&
        f.progress.restored_s0.word == 0x11223344u &&
        f.progress.restored_s1.word == 0x55667788u &&
        f.progress.restored_return_address.word == ParentRa &&
        f.progress.machine.registers.gpr[29].word == Sp);
  const std::array<U, 20> addresses{{
      Sp - 12u, Sp - 16u, Sp - 8u, StatusBase, PointerBase, PointerBase,
      StatusBase, StatusBase + 36u, StatusBase + 36u, StatusBase + 72u,
      AuxBase + 72u, StatusBase + 108u, AuxBase + 108u,
      StatusBase + 144u, StatusBase + 180u, StatusBase + 216u,
      StatusBase + 252u, Sp - 8u, Sp - 12u, Sp - 16u}};
  for (unsigned i = 0; i < addresses.size(); ++i)
    CHECK(f.access[i].address == addresses[i]);
  std::array<bool, 57> seen{};
  for (std::size_t i = 0; i < f.progress.instruction_events; ++i) {
    const U pc = f.instructions[i];
    CHECK(pc >= 0x800393f0u && pc <= 0x800394d0u && !(pc & 3u));
    seen[(pc - 0x800393f0u) / 4u] = true;
  }
  for (bool value : seen) CHECK(value);
}

void signedStatusClasses() {
  const std::array<U, 10> statuses{{
      0x80000000u, U(-1), 0, 1, 2, 3, 4, 5, 6, 0x7fffffffu}};
  for (U status : statuses) {
    Fixture f;
    f.put(StatusBase, status);
    f.put(PointerBase, 0x81234560u);
    f.put(AuxBase, 0xfeedfaceu);
    CHECK(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
          f.progress.slot_iterations == 8);
    if (status == 3) {
      CHECK(f.calls.front().event.pc == 0x80039458u &&
            f.calls.front().machine.registers.gpr[4].word == 0x81234560u &&
            f.get(PointerBase) == 0 && f.get(StatusBase) == 0);
    } else if (status == 1) {
      CHECK(f.get(StatusBase) == 0 && f.get(PointerBase) == 0x81234560u);
    } else if (status == 4 || status == 5) {
      CHECK(f.get(AuxBase) == 0 && f.get(StatusBase) == status);
    } else {
      CHECK(f.get(StatusBase) == status && f.get(AuxBase) == 0xfeedfaceu);
    }
  }
}

void oldV0LatchingAndPartialDecisions() {
  Fixture equality_unknown;
  equality_unknown.put(StatusBase, 3, 4, 1);
  CHECK(equality_unknown.run() == NBA97_TEXT_UNKNOWN &&
        equality_unknown.progress.stopped_pc == 0x80039418u &&
        equality_unknown.progress.instruction_count == 12 &&
        equality_unknown.progress.machine.registers.gpr[2].word == 1 &&
        equality_unknown.progress.machine.registers.gpr[2].known_mask == 14 &&
        equality_unknown.progress.slot_iterations == 0);

  Fixture slti_unknown;
  slti_unknown.put(StatusBase, 6, 4, 1);
  CHECK(slti_unknown.run() == NBA97_TEXT_UNKNOWN &&
        slti_unknown.progress.stopped_pc == 0x80039420u &&
        slti_unknown.progress.instruction_count == 14 &&
        slti_unknown.progress.machine.registers.gpr[2].word == 1 &&
        slti_unknown.progress.machine.registers.gpr[2].known_mask == 15);

  Fixture known_not_three;
  known_not_three.put(StatusBase, 0x00000103u, 4, 2);
  CHECK(known_not_three.run() == NBA97_TEXT_UNKNOWN &&
        known_not_three.progress.stopped_pc == 0x80039420u);

  Fixture known_negative;
  known_negative.put(StatusBase, 0x80000000u, 4, 8);
  CHECK(known_negative.run() == NBA97_TEXT_COMPLETE &&
        known_negative.progress.completed &&
        known_negative.progress.machine.registers.gpr[2].word == 1 &&
        known_negative.progress.machine.registers.gpr[2].known_mask == 15);
}

void callbackLiveCountersAndMemory() {
  Fixture moved;
  moved.put(StatusBase, 3);
  moved.put(PointerBase, 0x81234560u);
  moved.put(StatusBase + 36u, 0xaaaaaaaa);
  moved.put(PointerBase + 36u, 0xbbbbbbbb);
  moved.mutate_handle_registers = true;
  moved.handle_s0 = {36, 15};
  moved.handle_s1 = {7, 15};
  moved.handle_memory_address = PointerBase + 36u;
  moved.handle_memory_value = 0xccccccccu;
  CHECK(moved.run() == NBA97_TEXT_COMPLETE && moved.progress.completed &&
        moved.progress.slot_iterations == 1 && moved.calls.size() == 2 &&
        moved.calls[0].machine.registers.gpr[4].word == 0x81234560u &&
        moved.get(StatusBase) == 3 && moved.get(PointerBase) == 0x81234560u &&
        moved.get(StatusBase + 36u) == 0 &&
        moved.get(PointerBase + 36u) == 0 &&
        moved.progress.last_slot_offset.word == 0);

  Fixture partial_s0;
  partial_s0.put(StatusBase, 3);
  partial_s0.mutate_handle_registers = true;
  partial_s0.handle_s0 = {0, 14};
  partial_s0.handle_s1 = {7, 15};
  CHECK(partial_s0.run() == NBA97_TEXT_UNKNOWN &&
        partial_s0.progress.stopped_pc == 0x80039468u &&
        partial_s0.progress.callbacks_completed == 1 &&
        partial_s0.progress.stores == 3);

  Fixture wrapped_counter;
  wrapped_counter.put(StatusBase, 3);
  wrapped_counter.mutate_handle_registers = true;
  wrapped_counter.handle_s0 = {0, 15};
  wrapped_counter.handle_s1 = {0x7fffffffu, 15};
  wrapped_counter.context.operation_budget = 12;
  CHECK(wrapped_counter.run() == NBA97_TEXT_LIMIT &&
        wrapped_counter.progress.operations == 12 &&
        wrapped_counter.progress.slot_iterations > 1 &&
        wrapped_counter.progress.machine.registers.gpr[17].word >=
            0x80000000u);

  Fixture mutable_machine;
  mutable_machine.mutate_machine = true;
  CHECK(mutable_machine.run() == NBA97_TEXT_COMPLETE &&
        mutable_machine.progress.machine.registers.gpr[8].word == 0x89abcdefu &&
        mutable_machine.progress.machine.registers.gpr[8].known_mask == 3 &&
        mutable_machine.progress.machine.registers.gpr[24].word == 0x10203040u &&
        mutable_machine.progress.machine.hi.word == 0x55667788u &&
        mutable_machine.progress.machine.lo.word == 0xaabbccddu);
}

void frameAliasesAndRestores() {
  Fixture alias;
  alias.context.machine.registers.gpr[NBA97_FRONTEND_IO_DRAIN_SP] = {
      StatusBase + 16u, 15};
  alias.context.machine.registers.gpr[NBA97_FRONTEND_IO_DRAIN_S0] = {3, 15};
  alias.context.machine.registers.gpr[NBA97_FRONTEND_IO_DRAIN_S1] = {
      0x11223344u, 15};
  alias.put(PointerBase, 0x81234560u);
  CHECK(alias.run() == NBA97_TEXT_COMPLETE && alias.progress.completed &&
        alias.progress.saved_s0.word == 3 && alias.calls.size() == 2 &&
        alias.calls[0].event.pc == 0x80039458u &&
        alias.progress.restored_s0.word == 0 &&
        alias.progress.machine.registers.gpr[16].word == 0 &&
        alias.progress.machine.registers.gpr[29].word == StatusBase + 16u);

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture s0;
    s0.context.machine.registers.gpr[16] = {0xa1b2c3d4u, std::uint8_t(mask)};
    CHECK(s0.run() == NBA97_TEXT_COMPLETE &&
          s0.progress.restored_s0.word == 0xa1b2c3d4u &&
          s0.progress.restored_s0.known_mask == mask);
    Fixture s1;
    s1.context.machine.registers.gpr[17] = {0x10203040u, std::uint8_t(mask)};
    CHECK(s1.run() == NBA97_TEXT_COMPLETE &&
          s1.progress.restored_s1.word == 0x10203040u &&
          s1.progress.restored_s1.known_mask == mask);
    Fixture ra;
    ra.context.machine.registers.gpr[31] = {ParentRa, std::uint8_t(mask)};
    CHECK(ra.run() == (mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN) &&
          ra.progress.restored_return_address.known_mask == mask);
  }

  Fixture relocated;
  relocated.relocate_pc = 0x8003949cu;
  relocated.relocated_frame = 0x801e0000u;
  relocated.put(relocated.relocated_frame + 24u, 0x81234560u);
  relocated.put(relocated.relocated_frame + 20u, 0x11121314u, 4, 3);
  relocated.put(relocated.relocated_frame + 16u, 0x21222324u, 4, 12);
  CHECK(relocated.run() == NBA97_TEXT_COMPLETE && relocated.progress.completed &&
        relocated.progress.restored_return_address.word == 0x81234560u &&
        relocated.progress.restored_s1.word == 0x11121314u &&
        relocated.progress.restored_s1.known_mask == 3 &&
        relocated.progress.restored_s0.word == 0x21222324u &&
        relocated.progress.restored_s0.known_mask == 12 &&
        relocated.progress.machine.registers.gpr[29].word ==
            relocated.relocated_frame + 32u);

  Fixture late_unknown;
  late_unknown.relocate_pc = 0x8003949cu;
  late_unknown.relocated_frame = 0x801e0000u;
  late_unknown.put(late_unknown.relocated_frame + 24u, ParentRa, 4, 7);
  late_unknown.put(late_unknown.relocated_frame + 20u, 0);
  late_unknown.put(late_unknown.relocated_frame + 16u, 0);
  CHECK(late_unknown.run() == NBA97_TEXT_UNKNOWN &&
        late_unknown.progress.stopped_pc == 0x800394ccu);
  Fixture late_misaligned;
  late_misaligned.relocate_pc = 0x8003949cu;
  late_misaligned.relocated_frame = 0x801e0002u;
  CHECK(late_misaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        late_misaligned.progress.stopped_pc == 0x800394bcu);
  Fixture late_unmapped;
  late_unmapped.relocate_pc = 0x8003949cu;
  late_unmapped.relocated_frame = 0x70000000u;
  CHECK(late_unmapped.run() == NBA97_TEXT_RESOURCE &&
        late_unmapped.progress.stopped_pc == 0x800394bcu);
}

void budgetsRefusalsAndRunaway() {
  for (std::size_t budget = 0; budget <= 15; ++budget) {
    Fixture f;
    f.context.operation_budget = budget;
    CHECK(f.run() == (budget == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_LIMIT) &&
          f.progress.operations == budget);
  }
  for (std::size_t budget = 0; budget <= 24; ++budget) {
    Fixture f;
    f.statuses({{3, 1, 4, 5, U(-1), 0, 2, 6}});
    f.poll_results = {{0, 15}, {1, 15}};
    f.context.operation_budget = budget;
    CHECK(f.run() == (budget == 24 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_LIMIT) &&
          f.progress.operations == budget);
  }

  for (unsigned i = 0; i < Pcs.size(); ++i) {
    Fixture f;
    if (Pcs[i] == 0x80039458u)
      f.put(StatusBase, 3);
    if (Pcs[i] == 0x800394acu)
      f.poll_results = {{0, 15}, {1, 15}};
    f.refuse_pc = Pcs[i];
    f.mutate_machine = true;
    CHECK(f.run() == NBA97_TEXT_IO_REFUSED &&
          f.progress.stopped_pc == Pcs[i] &&
          f.progress.stopped_target == Targets[i] &&
          f.progress.machine.registers.gpr[8].word == 0x89abcdefu &&
          f.progress.machine.registers.gpr[31].word == Pcs[i] + 8u);
  }

  Fixture second_poll;
  second_poll.poll_results = {{0, 15}, {1, 15}};
  second_poll.refuse_pc = 0x8003949cu;
  second_poll.refuse_invocation = 2;
  CHECK(second_poll.run() == NBA97_TEXT_IO_REFUSED &&
        second_poll.progress.call_count[
            NBA97_FRONTEND_IO_DRAIN_SITE_8003949C] == 1 &&
        second_poll.progress.call_attempts[
            NBA97_FRONTEND_IO_DRAIN_SITE_8003949C] == 2);

  Fixture malformed;
  malformed.malformed_machine = true;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.callbacks_completed == 0 &&
        malformed.progress.machine.registers.gpr[9].known_mask == 16);

  Fixture poll_runaway;
  poll_runaway.poll_results = {{0, 15}};
  poll_runaway.context.operation_budget = 22;
  CHECK(poll_runaway.run() == NBA97_TEXT_LIMIT &&
        poll_runaway.progress.operations == 22 &&
        poll_runaway.progress.poll_attempts == 6 &&
        poll_runaway.progress.zero_poll_results == 6 &&
        poll_runaway.progress.stopped_pc == 0x800394acu);
}

void memoryAndArguments() {
  Fixture unknown_sp;
  unknown_sp.context.machine.registers.gpr[29].known_mask = 14;
  CHECK(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stopped_pc == 0x800393f4u &&
        unknown_sp.progress.operations == 0);
  Fixture misaligned_sp;
  misaligned_sp.context.machine.registers.gpr[29] = {Sp + 2u, 15};
  CHECK(misaligned_sp.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misaligned_sp.progress.operations == 1 &&
        misaligned_sp.progress.stopped_pc == 0x800393f4u);
  Fixture unmapped_sp;
  unmapped_sp.context.machine.registers.gpr[29] = {0x70000020u, 15};
  CHECK(unmapped_sp.run() == NBA97_TEXT_RESOURCE &&
        unmapped_sp.progress.stopped_address == 0x70000014u);

  Fixture no_plane;
  no_plane.region.known = nullptr;
  no_plane.context.machine.registers.gpr[16].known_mask = 15;
  no_plane.context.machine.registers.gpr[17].known_mask = 15;
  CHECK(no_plane.run() == NBA97_TEXT_COMPLETE);
  Fixture no_plane_s1;
  no_plane_s1.region.known = nullptr;
  CHECK(no_plane_s1.run() == NBA97_TEXT_ARGUMENT &&
        no_plane_s1.progress.stopped_pc == 0x800393f4u);
  Fixture no_plane_s0;
  no_plane_s0.region.known = nullptr;
  no_plane_s0.context.machine.registers.gpr[17].known_mask = 15;
  CHECK(no_plane_s0.run() == NBA97_TEXT_ARGUMENT &&
        no_plane_s0.progress.stopped_pc == 0x800393fcu &&
        no_plane_s0.progress.stores == 1);

  Fixture malformed_status;
  malformed_status.known[StatusBase - Base + 3] = 2;
  CHECK(malformed_status.run() == NBA97_TEXT_ARGUMENT &&
        malformed_status.progress.stopped_pc == 0x80039410u &&
        malformed_status.progress.operations == 4);

  Fixture overlap;
  std::array<std::uint8_t, 32> data{};
  std::array<Nba97GameTextRegion, 2> regions{{
      {0x90000000u, data.data(), nullptr, 24},
      {0x90000010u, data.data() + 16, nullptr, 16},
  }};
  overlap.context.memory = {regions.data(), regions.size()};
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT);
  Fixture wrapping_region;
  Nba97GameTextRegion bad{0xfffffff0u, data.data(), nullptr, 32};
  wrapping_region.context.memory = {&bad, 1};
  CHECK(wrapping_region.run() == NBA97_TEXT_ARGUMENT);
  Fixture malformed_machine;
  malformed_machine.context.machine.lo.known_mask = 16;
  CHECK(malformed_machine.run() == NBA97_TEXT_ARGUMENT);
  Fixture bad_journal;
  bad_journal.context.instruction_journal = nullptr;
  bad_journal.context.instruction_journal_capacity = 1;
  CHECK(bad_journal.run() == NBA97_TEXT_ARGUMENT);
  Nba97FrontendIoDrainProgress out{};
  CHECK(nba97_frontend_io_drain(nullptr, &out) == NBA97_TEXT_ARGUMENT &&
        nba97_frontend_io_drain(&overlap.context, nullptr) ==
            NBA97_TEXT_ARGUMENT);
}
} // namespace

int main() {
  try {
    fullPathAndTrace();
    signedStatusClasses();
    oldV0LatchingAndPartialDecisions();
    callbackLiveCountersAndMemory();
    frameAliasesAndRestores();
    budgetsRefusalsAndRunaway();
    memoryAndArguments();
    std::printf("frontend_io_drain_tests: PASS (%u checks)\n", checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
