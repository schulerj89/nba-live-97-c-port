#include "recovered/frontend_exit_drain.h"

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
    throw std::runtime_error("frontend-exit-drain failed at " +
                             std::to_string(line));
}
#define CHECK(value) checkAt((value), __LINE__)

constexpr U Base = 0x80000000u;
constexpr U Size = 0x200000u;
constexpr U Sp = 0x801f0000u;
constexpr U ParentRa = 0x8002f09cu;
constexpr U ActiveGlobal = 0x800f84c4u;
constexpr U BusyGlobal = 0x800f43b0u;
constexpr U ModeGlobal = 0x8002149cu;
constexpr std::array<U, 7> Pcs{{0x800394e8u, 0x800394f0u, 0x80039500u,
                                0x80039530u, 0x80039538u, 0x80039554u,
                                0x8003955cu}};
constexpr std::array<U, 7> Delays{{0x800394ecu, 0x800394f4u, 0x80039504u,
                                   0x80039534u, 0x8003953cu, 0x80039558u,
                                   0x80039560u}};
constexpr std::array<U, 7> Targets{{0x800393f0u, 0x800392a0u, 0x80038e84u,
                                    0x80029b64u, 0x8008c274u, 0x8006cde4u,
                                    0x8006ae60u}};
constexpr std::array<unsigned, 7> ArgCounts{{0, 0, 0, 2, 0, 1, 0}};

struct Call {
  Nba97FrontendExitDrainEvent event{};
  Nba97FrontendExitDrainMachine machine{};
};

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendExitDrainContext context{};
  Nba97FrontendExitDrainProgress progress{};
  std::array<Nba97FrontendExitDrainAccess, 32> access{};
  std::array<U, 128> instructions{};
  std::vector<Call> calls;
  std::vector<Nba97FrontendExitDrainWord> poll_results{{1, 15}};
  std::array<std::size_t, NBA97_FRONTEND_EXIT_DRAIN_SITE_COUNT> invocations{};
  U refuse_pc = 0;
  std::size_t refuse_invocation = 1;
  U mutate_global_pc = 0;
  U mutate_global_address = 0;
  U mutate_global_value = 0;
  std::uint8_t mutate_global_mask = 15;
  U relocate_pc = 0;
  U relocated_frame = 0;
  bool malformed_machine = false;
  bool mutate_machine = false;

  Fixture(U active = 1, U mode = 1) {
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {
          0x51000000u + i * 0x10101u, std::uint8_t((i % 15u) + 1u)};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_SP] = {Sp, 15};
    context.machine.registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_RA] = {ParentRa, 15};
    context.machine.hi = {0x12345678u, 5};
    context.machine.lo = {0x9abcdef0u, 10};
    put(ActiveGlobal, active);
    put(BusyGlobal, 0x89abcdefu);
    put(ModeGlobal, mode);
    context.memory = {&region, 1};
    context.operation_budget = 64;
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
  int run() { return nba97_frontend_exit_drain(&context, &progress); }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97FrontendExitDrainEvent *event,
                      Nba97FrontendExitDrainMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    if (!event || !machine ||
        event->site == NBA97_FRONTEND_EXIT_DRAIN_SITE_NONE ||
        event->site >= NBA97_FRONTEND_EXIT_DRAIN_SITE_COUNT)
      return 0;
    const unsigned index = event->site - 1;
    const std::size_t invocation = ++f.invocations[event->site];
    if (event->pc != Pcs[index] || event->delay_slot_pc != Delays[index] ||
        event->entry != Targets[index] ||
        event->argument_count != ArgCounts[index] ||
        event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
        event->invocation != invocation ||
        machine->registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_RA].word !=
            event->pc + 8u ||
        machine->registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_RA].known_mask != 15)
      return 0;
    f.calls.push_back({*event, *machine});
    if (event->site == NBA97_FRONTEND_EXIT_DRAIN_SITE_800394F0) {
      const std::size_t poll_index = invocation - 1;
      const auto value = poll_index < f.poll_results.size()
                             ? f.poll_results[poll_index]
                             : f.poll_results.back();
      machine->registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_V0] = value;
    }
    if (event->pc == f.mutate_global_pc)
      f.put(f.mutate_global_address, f.mutate_global_value, 4,
            f.mutate_global_mask);
    if (event->pc == f.relocate_pc)
      machine->registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_SP] = {
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

void normalPathsAndAllPcs() {
  Fixture full;
  CHECK(full.run() == NBA97_TEXT_COMPLETE && full.progress.completed);
  CHECK(full.progress.operations == 13 && full.progress.accesses == 7 &&
        full.progress.reads == 4 && full.progress.stores == 3 &&
        full.progress.callbacks_completed == 6 &&
        full.progress.instruction_count == 36 && full.calls.size() == 6 &&
        full.progress.poll_attempts == 1 &&
        full.progress.zero_poll_results == 0);
  CHECK(full.progress.initial_active_flag.word == 1 &&
        full.progress.first_mode_flag.word == 1 &&
        full.progress.second_mode_flag.word == 1 &&
        full.progress.saved_return_address.word == ParentRa &&
        full.progress.restored_return_address.word == ParentRa &&
        full.progress.machine.registers.gpr[29].word == Sp &&
        full.progress.machine.registers.gpr[31].word == ParentRa &&
        full.get(ActiveGlobal) == 0 && full.get(BusyGlobal) == 0);
  const std::array<unsigned, 6> full_sites{{0, 1, 3, 4, 5, 6}};
  for (unsigned i = 0; i < full_sites.size(); ++i) {
    const unsigned site = full_sites[i];
    CHECK(full.calls[i].event.pc == Pcs[site] &&
          full.calls[i].event.delay_slot_pc == Delays[site] &&
          full.calls[i].event.entry == Targets[site] &&
          full.calls[i].event.argument_count == ArgCounts[site]);
  }
  CHECK(full.calls[2].machine.registers.gpr[4].word == 0 &&
        full.calls[2].machine.registers.gpr[4].known_mask == 15 &&
        full.calls[2].machine.registers.gpr[5].word == 0 &&
        full.calls[2].machine.registers.gpr[5].known_mask == 15 &&
        full.calls[4].machine.registers.gpr[4].word == 1 &&
        full.calls[4].machine.registers.gpr[4].known_mask == 15);
  const std::array<U, 7> addresses{{ActiveGlobal, Sp - 8u, ModeGlobal,
                                    BusyGlobal, ActiveGlobal, ModeGlobal,
                                    Sp - 8u}};
  const std::array<unsigned, 7> kinds{{1, 2, 1, 2, 2, 1, 1}};
  for (unsigned i = 0; i < addresses.size(); ++i)
    CHECK(full.access[i].address == addresses[i] &&
          full.access[i].kind == kinds[i]);

  Fixture waited;
  waited.poll_results = {{0, 15}, {7, 15}};
  CHECK(waited.run() == NBA97_TEXT_COMPLETE && waited.progress.completed &&
        waited.progress.operations == 15 && waited.progress.poll_attempts == 2 &&
        waited.progress.zero_poll_results == 1 && waited.calls.size() == 8 &&
        waited.progress.call_count[NBA97_FRONTEND_EXIT_DRAIN_SITE_800394F0] ==
            2 &&
        waited.progress.call_count[NBA97_FRONTEND_EXIT_DRAIN_SITE_80039500] ==
            1 && waited.progress.instruction_count == 44);
  std::array<bool, 40> seen{};
  for (std::size_t i = 0; i < waited.progress.instruction_events; ++i) {
    const U pc = waited.instructions[i];
    CHECK(pc >= 0x800394d4u && pc <= 0x80039570u && !(pc & 3u));
    seen[(pc - 0x800394d4u) / 4u] = true;
  }
  for (bool value : seen) CHECK(value);

  Fixture inactive(0, 1);
  CHECK(inactive.run() == NBA97_TEXT_COMPLETE && inactive.progress.completed &&
        inactive.progress.operations == 3 && inactive.progress.accesses == 3 &&
        inactive.progress.reads == 2 && inactive.progress.stores == 1 &&
        inactive.progress.callbacks_completed == 0 && inactive.calls.empty() &&
        inactive.progress.instruction_count == 9 &&
        inactive.progress.restored_return_address.word == ParentRa);

  Fixture no_modes(1, 0);
  CHECK(no_modes.run() == NBA97_TEXT_COMPLETE && no_modes.progress.completed &&
        no_modes.progress.operations == 10 && no_modes.calls.size() == 3 &&
        no_modes.calls.back().event.pc == 0x80039538u &&
        no_modes.progress.instruction_count == 30);
}

void initialOrderingAndKnownness() {
  Fixture source_changed;
  source_changed.mutate_global_pc = 0x800394e8u;
  source_changed.mutate_global_address = ActiveGlobal;
  source_changed.mutate_global_value = 0;
  CHECK(source_changed.run() == NBA97_TEXT_COMPLETE &&
        source_changed.progress.initial_active_flag.word == 1 &&
        source_changed.calls.size() == 6);
  CHECK(source_changed.access[0].pc == 0x800394d8u &&
        source_changed.access[0].address == ActiveGlobal &&
        source_changed.access[1].pc == 0x800394e4u &&
        source_changed.access[1].address == Sp - 8u);

  Fixture unknown_zero;
  unknown_zero.put(ActiveGlobal, 0, 4, 7);
  CHECK(unknown_zero.run() == NBA97_TEXT_UNKNOWN &&
        unknown_zero.progress.stopped_pc == 0x800394e0u &&
        unknown_zero.progress.operations == 2 &&
        unknown_zero.progress.instruction_count == 5 &&
        unknown_zero.progress.stores == 1 && unknown_zero.calls.empty() &&
        unknown_zero.get(Sp - 8u) == ParentRa);

  Fixture known_nonzero;
  known_nonzero.put(ActiveGlobal, 0x00010000u, 4, 4);
  CHECK(known_nonzero.run() == NBA97_TEXT_COMPLETE &&
        known_nonzero.progress.initial_active_flag.known_mask == 4 &&
        known_nonzero.calls.size() == 6);

  Fixture alias;
  alias.context.machine.registers.gpr[29] = {ActiveGlobal + 8u, 15};
  CHECK(alias.run() == NBA97_TEXT_COMPLETE && alias.progress.completed &&
        alias.progress.initial_active_flag.word == 1 &&
        alias.progress.saved_return_address.word == ParentRa &&
        alias.progress.restored_return_address.word == 0 &&
        alias.progress.machine.registers.gpr[29].word == ActiveGlobal + 8u);
}

void pollingAndIndependentReloads() {
  Fixture polled;
  polled.poll_results = {{0, 15}, {0, 15}, {0x80000000u, 8}};
  CHECK(polled.run() == NBA97_TEXT_COMPLETE && polled.progress.completed &&
        polled.progress.poll_attempts == 3 &&
        polled.progress.zero_poll_results == 2 && polled.calls.size() == 10);

  Fixture unknown_poll;
  unknown_poll.poll_results = {{0, 7}};
  CHECK(unknown_poll.run() == NBA97_TEXT_UNKNOWN &&
        unknown_poll.progress.stopped_pc == 0x800394f8u &&
        unknown_poll.progress.instruction_count == 11 &&
        unknown_poll.progress.callbacks_completed == 2 &&
        unknown_poll.progress.zero_poll_results == 0);

  Fixture became_active(1, 0);
  became_active.mutate_global_pc = 0x80039538u;
  became_active.mutate_global_address = ModeGlobal;
  became_active.mutate_global_value = 0x81234560u;
  CHECK(became_active.run() == NBA97_TEXT_COMPLETE &&
        became_active.progress.first_mode_flag.word == 0 &&
        became_active.progress.second_mode_flag.word == 0x81234560u &&
        became_active.progress.call_count[
            NBA97_FRONTEND_EXIT_DRAIN_SITE_80039530] == 0 &&
        became_active.progress.call_count[
            NBA97_FRONTEND_EXIT_DRAIN_SITE_80039554] == 1 &&
        became_active.calls[became_active.calls.size() - 2]
                .machine.registers.gpr[4].word == 0x81234560u);

  Fixture became_zero(1, 0x80000000u);
  became_zero.mutate_global_pc = 0x80039538u;
  became_zero.mutate_global_address = ModeGlobal;
  became_zero.mutate_global_value = 0;
  CHECK(became_zero.run() == NBA97_TEXT_COMPLETE &&
        became_zero.progress.first_mode_flag.word == 0x80000000u &&
        became_zero.progress.second_mode_flag.word == 0 &&
        became_zero.progress.call_count[
            NBA97_FRONTEND_EXIT_DRAIN_SITE_80039530] == 1 &&
        became_zero.progress.call_count[
            NBA97_FRONTEND_EXIT_DRAIN_SITE_80039554] == 0);

  Fixture first_unknown;
  first_unknown.put(ModeGlobal, 0, 4, 7);
  CHECK(first_unknown.run() == NBA97_TEXT_UNKNOWN &&
        first_unknown.progress.stopped_pc == 0x80039528u &&
        first_unknown.progress.stores == 3 &&
        first_unknown.progress.machine.registers.gpr[4].word == 0 &&
        first_unknown.progress.machine.registers.gpr[4].known_mask == 15);

  Fixture second_unknown(1, 0);
  second_unknown.mutate_global_pc = 0x80039538u;
  second_unknown.mutate_global_address = ModeGlobal;
  second_unknown.mutate_global_value = 0;
  second_unknown.mutate_global_mask = 7;
  CHECK(second_unknown.run() == NBA97_TEXT_UNKNOWN &&
        second_unknown.progress.stopped_pc == 0x8003954cu &&
        second_unknown.progress.instruction_count == 26);
}

void callbackStateStackAndReturn() {
  Fixture mutated;
  mutated.mutate_machine = true;
  CHECK(mutated.run() == NBA97_TEXT_COMPLETE &&
        mutated.progress.machine.registers.gpr[8].word == 0x89abcdefu &&
        mutated.progress.machine.registers.gpr[8].known_mask == 3 &&
        mutated.progress.machine.registers.gpr[24].word == 0x10203040u &&
        mutated.progress.machine.hi.word == 0x55667788u &&
        mutated.progress.machine.hi.known_mask == 6 &&
        mutated.progress.machine.lo.word == 0xaabbccddu &&
        mutated.progress.machine.lo.known_mask == 9);

  Fixture moved;
  moved.relocate_pc = 0x8003955cu;
  moved.relocated_frame = 0x801e0000u;
  moved.put(moved.relocated_frame + 16u, 0x81234560u);
  CHECK(moved.run() == NBA97_TEXT_COMPLETE && moved.progress.completed &&
        moved.progress.restored_return_address.word == 0x81234560u &&
        moved.progress.machine.registers.gpr[29].word ==
            moved.relocated_frame + 24u);

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture f(0);
    f.context.machine.registers.gpr[31] = {ParentRa, std::uint8_t(mask)};
    const int result = f.run();
    CHECK(result == (mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN));
    CHECK(f.progress.restored_return_address.known_mask == mask);
  }
  Fixture bad_ra(0);
  bad_ra.context.machine.registers.gpr[31] = {ParentRa + 2u, 15};
  CHECK(bad_ra.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        bad_ra.progress.stopped_pc == 0x8003956cu &&
        bad_ra.progress.instruction_count == 9);

  Fixture late_unknown;
  late_unknown.relocate_pc = 0x8003955cu;
  late_unknown.relocated_frame = 0x801e0000u;
  late_unknown.put(late_unknown.relocated_frame + 16u, ParentRa, 4, 7);
  CHECK(late_unknown.run() == NBA97_TEXT_UNKNOWN &&
        late_unknown.progress.stopped_pc == 0x8003956cu);
  Fixture late_misaligned;
  late_misaligned.relocate_pc = 0x8003955cu;
  late_misaligned.relocated_frame = 0x801e0002u;
  CHECK(late_misaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        late_misaligned.progress.stopped_pc == 0x80039564u);
  Fixture late_unmapped;
  late_unmapped.relocate_pc = 0x8003955cu;
  late_unmapped.relocated_frame = 0x70000000u;
  CHECK(late_unmapped.run() == NBA97_TEXT_RESOURCE &&
        late_unmapped.progress.stopped_pc == 0x80039564u);
}

void budgetsRefusalsAndMalformed() {
  for (std::size_t budget = 0; budget <= 13; ++budget) {
    Fixture f;
    f.context.operation_budget = budget;
    const int result = f.run();
    CHECK(result == (budget == 13 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_LIMIT));
    CHECK(f.progress.operations == budget);
  }
  for (unsigned i = 0; i < Pcs.size(); ++i) {
    Fixture f;
    if (Pcs[i] == 0x80039500u)
      f.poll_results = {{0, 15}, {1, 15}};
    f.refuse_pc = Pcs[i];
    f.mutate_global_pc = Pcs[i];
    f.mutate_global_address = ModeGlobal;
    f.mutate_global_value = 0xdeadbeefu;
    CHECK(f.run() == NBA97_TEXT_IO_REFUSED &&
          f.progress.stopped_pc == Pcs[i] &&
          f.progress.stopped_target == Targets[i] &&
          f.get(ModeGlobal) == 0xdeadbeefu &&
          f.progress.machine.registers.gpr[31].word == Pcs[i] + 8u);
  }
  Fixture second_poll_refusal;
  second_poll_refusal.poll_results = {{0, 15}, {1, 15}};
  second_poll_refusal.refuse_pc = 0x800394f0u;
  second_poll_refusal.refuse_invocation = 2;
  CHECK(second_poll_refusal.run() == NBA97_TEXT_IO_REFUSED &&
        second_poll_refusal.progress.call_count[
            NBA97_FRONTEND_EXIT_DRAIN_SITE_800394F0] == 1 &&
        second_poll_refusal.progress.call_attempts[
            NBA97_FRONTEND_EXIT_DRAIN_SITE_800394F0] == 2);

  Fixture malformed;
  malformed.malformed_machine = true;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.callbacks_completed == 0 &&
        malformed.progress.machine.registers.gpr[9].known_mask == 16);

  Fixture runaway;
  runaway.poll_results = {{0, 15}};
  runaway.context.operation_budget = 9;
  CHECK(runaway.run() == NBA97_TEXT_LIMIT &&
        runaway.progress.operations == 9 &&
        runaway.progress.poll_attempts == 4 &&
        runaway.progress.zero_poll_results == 3 &&
        runaway.progress.stopped_pc == 0x800394f0u &&
        runaway.progress.call_attempts[
            NBA97_FRONTEND_EXIT_DRAIN_SITE_800394F0] == 3);
}

void memoryAndArgumentFailures() {
  Fixture unknown_sp;
  unknown_sp.context.machine.registers.gpr[29].known_mask = 14;
  CHECK(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stopped_pc == 0x800394e4u &&
        unknown_sp.progress.operations == 1 && unknown_sp.progress.reads == 1);
  Fixture misaligned_sp;
  misaligned_sp.context.machine.registers.gpr[29] = {Sp + 2u, 15};
  CHECK(misaligned_sp.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misaligned_sp.progress.operations == 2 &&
        misaligned_sp.progress.stopped_pc == 0x800394e4u);
  Fixture unmapped_sp;
  unmapped_sp.context.machine.registers.gpr[29] = {0x70000018u, 15};
  CHECK(unmapped_sp.run() == NBA97_TEXT_RESOURCE &&
        unmapped_sp.progress.stopped_address == 0x70000010u);

  Fixture wrapped(0);
  std::array<std::uint8_t, 64> low_data{};
  std::array<std::uint8_t, 64> low_known{};
  low_known.fill(1);
  std::array<Nba97GameTextRegion, 2> wrapped_regions{{
      {0, low_data.data(), low_known.data(), low_data.size()}, wrapped.region}};
  wrapped.context.memory = {wrapped_regions.data(), wrapped_regions.size()};
  wrapped.context.machine.registers.gpr[29] = {8, 15};
  CHECK(wrapped.run() == NBA97_TEXT_COMPLETE &&
        wrapped.progress.frame_stack_pointer == 0xfffffff0u &&
        wrapped.access[1].address == 0 &&
        wrapped.progress.machine.registers.gpr[29].word == 8);

  Fixture no_plane;
  no_plane.region.known = nullptr;
  CHECK(no_plane.run() == NBA97_TEXT_COMPLETE);
  Fixture no_plane_partial;
  no_plane_partial.region.known = nullptr;
  no_plane_partial.context.machine.registers.gpr[31].known_mask = 7;
  CHECK(no_plane_partial.run() == NBA97_TEXT_ARGUMENT &&
        no_plane_partial.progress.stopped_pc == 0x800394e4u &&
        no_plane_partial.progress.operations == 2);

  Fixture malformed_initial;
  malformed_initial.known[ActiveGlobal - Base + 3] = 2;
  CHECK(malformed_initial.run() == NBA97_TEXT_ARGUMENT &&
        malformed_initial.progress.stopped_pc == 0x800394d8u &&
        malformed_initial.progress.operations == 1);
  Fixture malformed_late;
  malformed_late.known[ModeGlobal - Base + 2] = 2;
  CHECK(malformed_late.run() == NBA97_TEXT_ARGUMENT &&
        malformed_late.progress.stopped_pc == 0x80039514u &&
        malformed_late.progress.callbacks_completed == 2);

  Fixture overlap;
  std::array<std::uint8_t, 16> data{};
  std::array<Nba97GameTextRegion, 2> regions{{
      {0x90000000u, data.data(), nullptr, 12},
      {0x90000008u, data.data() + 8, nullptr, 8},
  }};
  overlap.context.memory = {regions.data(), regions.size()};
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT);

  Fixture wrapping_region;
  Nba97GameTextRegion bad{0xfffffff0u, data.data(), nullptr, 32};
  wrapping_region.context.memory = {&bad, 1};
  CHECK(wrapping_region.run() == NBA97_TEXT_ARGUMENT);

  Fixture malformed_machine;
  malformed_machine.context.machine.hi.known_mask = 16;
  CHECK(malformed_machine.run() == NBA97_TEXT_ARGUMENT);
  Fixture bad_journal;
  bad_journal.context.access_journal = nullptr;
  bad_journal.context.access_journal_capacity = 1;
  CHECK(bad_journal.run() == NBA97_TEXT_ARGUMENT);
  Nba97FrontendExitDrainProgress out{};
  CHECK(nba97_frontend_exit_drain(nullptr, &out) == NBA97_TEXT_ARGUMENT &&
        nba97_frontend_exit_drain(&overlap.context, nullptr) ==
            NBA97_TEXT_ARGUMENT);
}
} // namespace

int main() {
  try {
    normalPathsAndAllPcs();
    initialOrderingAndKnownness();
    pollingAndIndependentReloads();
    callbackStateStackAndReturn();
    budgetsRefusalsAndMalformed();
    memoryAndArgumentFailures();
    std::printf("frontend_exit_drain_tests: PASS (%u checks)\n", checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
