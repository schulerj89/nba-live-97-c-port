#include "recovered/frontend_exit_wait.h"

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
    throw std::runtime_error("frontend-exit-wait failed at " +
                             std::to_string(line));
}
#define CHECK(value) checkAt((value), __LINE__)

constexpr U Base = 0x80000000u;
constexpr U Size = 0x200000u;
constexpr U Sp = 0x801f0000u;
constexpr U ParentRa = 0x8002f094u;
constexpr U HandleGlobal = 0x80017268u;
constexpr U SecondaryGlobal = 0x8002149cu;
constexpr U Handle = 0x80145678u;
constexpr U Secondary = 0x80123458u;
constexpr std::array<U, 10> Pcs{{
    0x8002efdcu, 0x8002efe4u, 0x8002eff0u, 0x8002f000u, 0x8002f010u,
    0x8002f018u, 0x8002f034u, 0x8002f048u, 0x8002f050u, 0x8002f060u}};
constexpr std::array<U, 10> Delays{{
    0x8002efe0u, 0x8002efe8u, 0x8002eff4u, 0x8002f004u, 0x8002f014u,
    0x8002f01cu, 0x8002f038u, 0x8002f04cu, 0x8002f054u, 0x8002f064u}};
constexpr std::array<U, 10> Targets{{
    0x8007b2bcu, 0x8008da5cu, 0x8006b6a0u, 0x8006fcf0u, 0x80039260u,
    0x8008da5cu, 0x80092c34u, 0x80028c28u, 0x8006faa0u, 0x80028cf4u}};
constexpr std::array<unsigned, 10> Args{{3, 0, 0, 0, 0, 0, 1, 0, 0, 1}};

struct Call {
  Nba97FrontendExitWaitEvent event{};
  Nba97FrontendExitWaitMachine machine{};
};

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendExitWaitContext context{};
  Nba97FrontendExitWaitProgress progress{};
  std::array<Nba97FrontendExitWaitAccess, 16> access{};
  std::array<U, 256> instructions{};
  std::vector<Call> calls;
  std::vector<Nba97FrontendExitWaitWord> poll_a{{0, 15}};
  std::vector<Nba97FrontendExitWaitWord> poll_b{{0, 15}};
  std::vector<Nba97FrontendExitWaitWord> clocks{{1000, 15}, {1361, 15}};
  U refuse_pc = 0;
  U relocate_pc = 0;
  U relocated_frame = 0;
  U mutate_s0_pc = 0;
  Nba97FrontendExitWaitWord mutated_s0{};
  U mutate_handle_pc = 0;
  U mutate_handle_value = 0;
  U mutate_secondary_pc = 0;
  U mutate_secondary_value = 0;
  bool malformed_machine = false;
  std::array<unsigned, NBA97_FRONTEND_EXIT_WAIT_SITE_COUNT> site_calls{};

  Fixture(U handle = Handle, U secondary = Secondary) {
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {0x41000000u + i * 0x101u,
                                          std::uint8_t((i % 15u) + 1u)};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[NBA97_FRONTEND_EXIT_WAIT_SP] = {Sp, 15};
    context.machine.registers.gpr[NBA97_FRONTEND_EXIT_WAIT_RA] = {ParentRa, 15};
    context.machine.registers.gpr[NBA97_FRONTEND_EXIT_WAIT_S0] =
        {0x10203040u, 5};
    context.machine.hi = {0x55667788u, 6};
    context.machine.lo = {0x99aabbccu, 9};
    put(HandleGlobal, handle);
    put(SecondaryGlobal, secondary);
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
  U get(U address) const {
    U result = 0;
    for (unsigned i = 0; i < 4; ++i)
      result |= U(bytes[address - Base + i]) << (i * 8u);
    return result;
  }
  int run() { return nba97_frontend_exit_wait(&context, &progress); }

  static Nba97FrontendExitWaitWord response(
      const std::vector<Nba97FrontendExitWaitWord> &values, unsigned index) {
    return values[index < values.size() ? index : values.size() - 1];
  }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97FrontendExitWaitEvent *event,
                      Nba97FrontendExitWaitMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    if (!event || !machine || event->site == 0 || event->site >= 11 ||
        event->pc != Pcs[event->site - 1] ||
        event->delay_slot_pc != Delays[event->site - 1] ||
        event->entry != Targets[event->site - 1] ||
        event->argument_count != Args[event->site - 1] ||
        event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
        machine->registers.gpr[31].word != event->pc + 8u ||
        machine->registers.gpr[31].known_mask != 15)
      return 0;
    f.calls.push_back({*event, *machine});
    const unsigned index = f.site_calls[event->site]++;
    if (event->site == NBA97_FRONTEND_EXIT_WAIT_SITE_8002EFE4)
      machine->registers.gpr[2] = response(f.clocks, 0);
    else if (event->site == NBA97_FRONTEND_EXIT_WAIT_SITE_8002F018)
      machine->registers.gpr[2] = response(f.clocks, index + 1);
    else if (event->site == NBA97_FRONTEND_EXIT_WAIT_SITE_8002EFF0)
      machine->registers.gpr[2] = response(f.poll_a, index);
    else if (event->site == NBA97_FRONTEND_EXIT_WAIT_SITE_8002F000)
      machine->registers.gpr[2] = response(f.poll_b, index);
    if (event->pc == f.mutate_s0_pc)
      machine->registers.gpr[16] = f.mutated_s0;
    if (event->pc == f.mutate_handle_pc)
      f.put(HandleGlobal, f.mutate_handle_value);
    if (event->pc == f.mutate_secondary_pc)
      f.put(SecondaryGlobal, f.mutate_secondary_value);
    if (event->pc == f.relocate_pc)
      machine->registers.gpr[29] = {f.relocated_frame, 15};
    if (f.malformed_machine)
      machine->registers.gpr[24].known_mask = 16;
    return event->pc != f.refuse_pc;
  }
};

void deadlineAndSentinelPaths() {
  Fixture f;
  CHECK(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.progress.exit_path == NBA97_FRONTEND_EXIT_WAIT_EXIT_DEADLINE &&
        f.progress.operations == 19 && f.progress.accesses == 9 &&
        f.progress.reads == 5 && f.progress.stores == 4 &&
        f.progress.callbacks_completed == 10 &&
        f.progress.instruction_count == 50 && f.calls.size() == 10);
  CHECK(f.progress.initial_handle.word == Handle &&
        f.progress.deadline.word == 1360 &&
        f.progress.clock_result.word == 1361 &&
        f.progress.loop_iterations == 1 &&
        f.progress.reloaded_handle.word == Handle &&
        f.progress.secondary_word.word == Secondary &&
        f.get(HandleGlobal) == UINT32_MAX && f.get(SecondaryGlobal) == 0 &&
        f.progress.restored_return_address.word == ParentRa &&
        f.progress.restored_s0.word == 0x10203040u &&
        f.progress.restored_s0.known_mask == 5 &&
        f.progress.machine.registers.gpr[29].word == Sp);
  CHECK(f.calls[0].machine.registers.gpr[4].word == Handle &&
        f.calls[0].machine.registers.gpr[5].word == 100 &&
        f.calls[0].machine.registers.gpr[6].word == UINT32_MAX &&
        f.calls[6].machine.registers.gpr[4].word == Handle &&
        f.calls[9].machine.registers.gpr[4].word == Secondary);
  for (unsigned i = 0; i < Pcs.size(); ++i)
    CHECK(f.calls[i].event.pc == Pcs[i] &&
          f.calls[i].event.delay_slot_pc == Delays[i] &&
          f.calls[i].event.entry == Targets[i] &&
          f.calls[i].event.argument_count == Args[i]);
  std::array<bool, 50> seen{};
  for (std::size_t i = 0; i < f.progress.instruction_events; ++i)
    seen[(f.instructions[i] - 0x8002efbcu) / 4u] = true;
  for (bool value : seen) CHECK(value);
  const std::array<U, 9> addresses{{HandleGlobal, Sp - 4u, Sp - 8u,
                                    HandleGlobal, HandleGlobal, SecondaryGlobal,
                                    SecondaryGlobal, Sp - 4u, Sp - 8u}};
  for (unsigned i = 0; i < addresses.size(); ++i)
    CHECK(f.access[i].address == addresses[i]);

  Fixture sentinel(UINT32_MAX);
  CHECK(sentinel.run() == NBA97_TEXT_COMPLETE && sentinel.progress.completed &&
        sentinel.progress.exit_path == NBA97_FRONTEND_EXIT_WAIT_EXIT_SENTINEL &&
        sentinel.progress.operations == 5 && sentinel.progress.accesses == 5 &&
        sentinel.progress.callbacks_completed == 0 &&
        sentinel.progress.instruction_count == 12 && sentinel.calls.empty() &&
        sentinel.get(HandleGlobal) == UINT32_MAX &&
        sentinel.get(SecondaryGlobal) == Secondary &&
        sentinel.progress.saved_s0.word == 0x10203040u);
}

void latchedPollAndLoopSemantics() {
  Fixture negative;
  negative.poll_a = {{0x80000001u, 8}};
  CHECK(negative.run() == NBA97_TEXT_COMPLETE &&
        negative.progress.exit_path ==
            NBA97_FRONTEND_EXIT_WAIT_EXIT_POLL_NEGATIVE &&
        negative.progress.first_poll_result.word == 0x80000001u &&
        negative.progress.first_poll_result.known_mask == 8 &&
        negative.get(HandleGlobal) == UINT32_MAX && negative.calls.size() == 6);

  Fixture nonzero;
  nonzero.poll_b = {{0x00000001u, 1}};
  CHECK(nonzero.run() == NBA97_TEXT_COMPLETE &&
        nonzero.progress.exit_path ==
            NBA97_FRONTEND_EXIT_WAIT_EXIT_POLL_NONZERO &&
        nonzero.progress.second_poll_result.word == 1 &&
        nonzero.progress.second_poll_result.known_mask == 1 &&
        nonzero.get(HandleGlobal) == UINT32_MAX && nonzero.calls.size() == 7);

  Fixture equality;
  equality.clocks = {{100, 15}, {460, 15}, {461, 15}};
  CHECK(equality.run() == NBA97_TEXT_COMPLETE &&
        equality.progress.exit_path == NBA97_FRONTEND_EXIT_WAIT_EXIT_DEADLINE &&
        equality.progress.loop_iterations == 2 && equality.calls.size() == 14 &&
        equality.progress.deadline.word == 460 &&
        equality.progress.clock_result.word == 461);

  Fixture wrapped;
  wrapped.clocks = {{0x7ffffff0u, 15}, {0x80000159u, 15}};
  CHECK(wrapped.run() == NBA97_TEXT_COMPLETE &&
        wrapped.progress.deadline.word == 0x80000158u &&
        wrapped.progress.exit_path == NBA97_FRONTEND_EXIT_WAIT_EXIT_DEADLINE);

  Fixture live_s0;
  live_s0.clocks = {{1000, 15}, {2, 15}};
  live_s0.mutate_s0_pc = 0x8002f010u;
  live_s0.mutated_s0 = {1, 15};
  CHECK(live_s0.run() == NBA97_TEXT_COMPLETE &&
        live_s0.progress.deadline.word == 1360 &&
        live_s0.progress.exit_path == NBA97_FRONTEND_EXIT_WAIT_EXIT_DEADLINE);

  Fixture bounded;
  bounded.clocks = {{1000, 15}, {1000, 15}};
  bounded.context.operation_budget = 24;
  CHECK(bounded.run() == NBA97_TEXT_LIMIT && bounded.progress.operations == 24 &&
        bounded.progress.loop_iterations >= 2);
}

void knownnessAndLiveMemory() {
  Fixture partial_handle;
  partial_handle.put(HandleGlobal, 0xffffff00u, 4, 1);
  CHECK(partial_handle.run() == NBA97_TEXT_COMPLETE &&
        partial_handle.calls[0].machine.registers.gpr[4].word == 0xffffff00u &&
        partial_handle.calls[0].machine.registers.gpr[4].known_mask == 1);
  Fixture unknown_sentinel(UINT32_MAX);
  unknown_sentinel.put(HandleGlobal, UINT32_MAX, 4, 7);
  CHECK(unknown_sentinel.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sentinel.progress.stopped_pc == 0x8002efd0u &&
        unknown_sentinel.progress.operations == 3 &&
        unknown_sentinel.progress.instruction_count == 7);
  Fixture unknown_poll;
  unknown_poll.poll_a = {{0, 7}};
  CHECK(unknown_poll.run() == NBA97_TEXT_UNKNOWN &&
        unknown_poll.progress.stopped_pc == 0x8002eff8u &&
        unknown_poll.progress.machine.registers.gpr[2].word == UINT32_MAX);

  Fixture reloaded;
  reloaded.mutate_handle_pc = 0x8002f018u;
  reloaded.mutate_handle_value = 0x80111110u;
  reloaded.mutate_secondary_pc = 0x8002f060u;
  reloaded.mutate_secondary_value = 0xdeadbeefu;
  CHECK(reloaded.run() == NBA97_TEXT_COMPLETE &&
        reloaded.progress.reloaded_handle.word == 0x80111110u &&
        reloaded.calls[6].machine.registers.gpr[4].word == 0x80111110u &&
        reloaded.progress.secondary_word.word == Secondary &&
        reloaded.get(SecondaryGlobal) == 0);

  Fixture no_plane;
  no_plane.context.machine.registers.gpr[16].known_mask = 15;
  no_plane.region.known = nullptr;
  CHECK(no_plane.run() == NBA97_TEXT_COMPLETE);
  Fixture no_plane_partial;
  no_plane_partial.context.machine.registers.gpr[16].known_mask = 7;
  no_plane_partial.region.known = nullptr;
  CHECK(no_plane_partial.run() == NBA97_TEXT_ARGUMENT &&
        no_plane_partial.progress.stopped_pc == 0x8002efd4u);
  Fixture malformed;
  malformed.known[SecondaryGlobal - Base + 3] = 2;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.stopped_pc == 0x8002f05cu);
}

void failuresBudgetsStackAndAliases() {
  for (std::size_t budget = 0; budget <= 19; ++budget) {
    Fixture f;
    f.context.operation_budget = budget;
    CHECK(f.run() ==
          (budget == 19 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_LIMIT));
    CHECK(f.progress.operations == budget);
  }
  for (unsigned i = 0; i < Pcs.size(); ++i) {
    Fixture f;
    f.refuse_pc = Pcs[i];
    CHECK(f.run() == NBA97_TEXT_IO_REFUSED &&
          f.progress.stopped_pc == Pcs[i] &&
          f.progress.stopped_target == Targets[i]);
  }
  Fixture malformed_callback;
  malformed_callback.malformed_machine = true;
  CHECK(malformed_callback.run() == NBA97_TEXT_ARGUMENT &&
        malformed_callback.progress.callbacks_completed == 0 &&
        malformed_callback.progress.machine.registers.gpr[24].known_mask == 16);

  Fixture initial_unmapped;
  initial_unmapped.region.base = 0x80100000u;
  initial_unmapped.region.size = 0x100000u;
  CHECK(initial_unmapped.run() == NBA97_TEXT_RESOURCE &&
        initial_unmapped.progress.stopped_pc == 0x8002efc0u &&
        initial_unmapped.progress.machine.registers.gpr[29].word == Sp);
  Fixture bad_sp;
  bad_sp.context.machine.registers.gpr[29] = {Sp + 2u, 15};
  CHECK(bad_sp.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        bad_sp.progress.stopped_pc == 0x8002efccu);
  Fixture unknown_sp;
  unknown_sp.context.machine.registers.gpr[29].known_mask = 14;
  CHECK(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stopped_pc == 0x8002efccu);

  Fixture moved;
  moved.relocate_pc = 0x8002f060u;
  moved.relocated_frame = 0x801e0000u;
  moved.put(moved.relocated_frame + 20u, 0x81234560u);
  moved.put(moved.relocated_frame + 16u, 0xabcdef01u, 4, 3);
  CHECK(moved.run() == NBA97_TEXT_COMPLETE &&
        moved.progress.restored_return_address.word == 0x81234560u &&
        moved.progress.restored_s0.word == 0xabcdef01u &&
        moved.progress.restored_s0.known_mask == 3 &&
        moved.progress.machine.registers.gpr[29].word ==
            moved.relocated_frame + 24u);

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture f(UINT32_MAX);
    f.context.machine.registers.gpr[31] = {ParentRa, std::uint8_t(mask)};
    CHECK(f.run() == (mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN));
    CHECK(f.progress.restored_return_address.known_mask == mask);
  }
  Fixture ra_alias;
  ra_alias.context.machine.registers.gpr[29] = {0x8001726cu, 15};
  CHECK(ra_alias.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        ra_alias.progress.reloaded_handle.word == ParentRa &&
        ra_alias.progress.restored_return_address.word == UINT32_MAX);
  Fixture secondary_alias;
  secondary_alias.context.machine.registers.gpr[29] = {0x800214a0u, 15};
  CHECK(secondary_alias.run() == NBA97_TEXT_COMPLETE &&
        secondary_alias.progress.restored_return_address.word == 0 &&
        secondary_alias.progress.machine.registers.gpr[31].word == 0);

  Fixture overlap;
  std::array<std::uint8_t, 16> data{};
  std::array<Nba97GameTextRegion, 2> regions{{
      {0x90000000u, data.data(), nullptr, 12},
      {0x90000008u, data.data() + 8, nullptr, 8},
  }};
  overlap.context.memory = {regions.data(), regions.size()};
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT);
}
} // namespace

int main() {
  try {
    deadlineAndSentinelPaths();
    latchedPollAndLoopSemantics();
    knownnessAndLiveMemory();
    failuresBudgetsStackAndAliases();
    std::printf("frontend_exit_wait_tests: PASS (%u checks)\n", checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
