#include "recovered/frontend_exit_cleanup.h"

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
    throw std::runtime_error("frontend-exit-cleanup failed at " +
                             std::to_string(line));
}
#define CHECK(value) checkAt((value), __LINE__)

constexpr U Base = 0x80000000u;
constexpr U Size = 0x200000u;
constexpr U Sp = 0x801f0000u;
constexpr U ParentRa = 0x80028ab0u;
constexpr U PointerGlobal = 0x80021d6cu;
constexpr U ReleaseGlobal = 0x8001502cu;
constexpr U Pointer = 0x80123458u;
constexpr std::array<U, 5> Pcs{{0x8002f08cu, 0x8002f094u, 0x8002f0a4u,
                                0x8002f0c0u, 0x8002f0d0u}};
constexpr std::array<U, 5> Delays{{0x8002f090u, 0x8002f098u, 0x8002f0a8u,
                                   0x8002f0c4u, 0x8002f0d4u}};
constexpr std::array<U, 5> Targets{{0x8002efbcu, 0x800394d4u, 0x80028c90u,
                                    0x8007760cu, 0x80076540u}};

struct Call {
  Nba97FrontendExitCleanupEvent event{};
  Nba97FrontendExitCleanupMachine machine{};
};

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendExitCleanupContext context{};
  Nba97FrontendExitCleanupProgress progress{};
  std::array<Nba97FrontendExitCleanupAccess, 16> access{};
  std::array<U, 32> instructions{};
  std::vector<Call> calls;
  U refuse_pc = 0;
  U mutate_flag_pc = 0;
  U mutate_flag_value = 0;
  U relocate_pc = 0;
  U relocated_frame = 0;
  bool malformed_machine = false;
  bool mutate_machine = false;

  Fixture(U release = 1, U pointer = Pointer) {
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {0x51000000u + i * 0x10101u,
                                          std::uint8_t((i % 15u) + 1u)};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[NBA97_FRONTEND_EXIT_CLEANUP_SP] = {Sp, 15};
    context.machine.registers.gpr[NBA97_FRONTEND_EXIT_CLEANUP_RA] = {ParentRa,
                                                                     15};
    context.machine.hi = {0x12345678u, 5};
    context.machine.lo = {0x9abcdef0u, 10};
    put(PointerGlobal, pointer);
    put(ReleaseGlobal, release);
    context.memory = {&region, 1};
    context.operation_budget = 32;
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
  int run() { return nba97_frontend_exit_cleanup(&context, &progress); }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97FrontendExitCleanupEvent *event,
                      Nba97FrontendExitCleanupMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    if (!event || !machine ||
        event->site == NBA97_FRONTEND_EXIT_CLEANUP_SITE_NONE ||
        event->site >= NBA97_FRONTEND_EXIT_CLEANUP_SITE_COUNT ||
        event->pc != Pcs[event->site - 1] ||
        event->delay_slot_pc != Delays[event->site - 1] ||
        event->entry != Targets[event->site - 1] ||
        event->argument_count !=
            ((event->site == NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F0A4 ||
              event->site == NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F0C0)
                 ? 1
                 : 0) ||
        event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
        event->invocation != 1 ||
        machine->registers.gpr[NBA97_FRONTEND_EXIT_CLEANUP_RA].word !=
            event->pc + 8u ||
        machine->registers.gpr[NBA97_FRONTEND_EXIT_CLEANUP_RA].known_mask != 15)
      return 0;
    f.calls.push_back({*event, *machine});
    if (event->pc == f.mutate_flag_pc)
      f.put(ReleaseGlobal, f.mutate_flag_value);
    if (event->pc == f.relocate_pc) {
      machine->registers.gpr[NBA97_FRONTEND_EXIT_CLEANUP_SP] =
          {f.relocated_frame, 15};
    }
    if (f.mutate_machine) {
      machine->registers.gpr[8] = {0x89abcdefu, 3};
      machine->registers.gpr[24] = {0x10203040u, 12};
      machine->hi = {0x55667788u, 6};
      machine->lo = {0xaabbccddu, 9};
    }
    if (f.malformed_machine)
      machine->registers.gpr[9].known_mask = 16;
    return event->pc != f.refuse_pc;
  }
};

void normalPathsAndTrace() {
  Fixture nonzero;
  CHECK(nonzero.run() == NBA97_TEXT_COMPLETE && nonzero.progress.completed);
  CHECK(nonzero.progress.operations == 10 && nonzero.progress.accesses == 5 &&
        nonzero.progress.reads == 3 && nonzero.progress.stores == 2 &&
        nonzero.progress.callbacks_completed == 5 &&
        nonzero.progress.instruction_count == 25 && nonzero.calls.size() == 5);
  CHECK(nonzero.progress.loaded_cleanup_selector.word == Pointer &&
        nonzero.progress.loaded_cleanup_selector.known_mask == 15 &&
        nonzero.progress.loaded_release_flag.word == 1 &&
        nonzero.get(ReleaseGlobal) == 0 &&
        nonzero.progress.saved_return_address.word == ParentRa &&
        nonzero.progress.restored_return_address.word == ParentRa &&
        nonzero.progress.machine.registers.gpr[29].word == Sp &&
        nonzero.progress.machine.registers.gpr[31].word == ParentRa);
  for (unsigned i = 0; i < 5; ++i) {
    CHECK(nonzero.calls[i].event.pc == Pcs[i] &&
          nonzero.calls[i].event.delay_slot_pc == Delays[i] &&
          nonzero.calls[i].event.entry == Targets[i] &&
          nonzero.calls[i].event.argument_count == (i == 2 || i == 3 ? 1 : 0));
  }
  CHECK(nonzero.calls[2].machine.registers.gpr[4].word == Pointer &&
        nonzero.calls[2].machine.registers.gpr[4].known_mask == 15);
  const std::array<U, 5> addresses{{Sp - 8u, PointerGlobal, ReleaseGlobal,
                                    ReleaseGlobal, Sp - 8u}};
  const std::array<unsigned, 5> kinds{{2, 1, 1, 2, 1}};
  for (unsigned i = 0; i < addresses.size(); ++i)
    CHECK(nonzero.access[i].address == addresses[i] &&
          nonzero.access[i].kind == kinds[i]);

  Fixture zero(0);
  CHECK(zero.run() == NBA97_TEXT_COMPLETE && zero.progress.completed &&
        zero.progress.operations == 8 && zero.progress.accesses == 4 &&
        zero.progress.reads == 3 && zero.progress.stores == 1 &&
        zero.progress.callbacks_completed == 4 &&
        zero.progress.instruction_count == 21 && zero.calls.size() == 4);
  CHECK(zero.calls[3].event.pc == 0x8002f0d0u &&
        zero.progress.call_count[NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F0C0] ==
            0);

  std::array<bool, 25> seen{};
  for (std::size_t i = 0; i < nonzero.progress.instruction_events; ++i) {
    const U pc = nonzero.instructions[i];
    CHECK(pc >= 0x8002f084u && pc <= 0x8002f0e4u && !(pc & 3u));
    seen[(pc - 0x8002f084u) / 4u] = true;
  }
  for (bool value : seen) CHECK(value);
}

void liveReloadMachineAndAlias() {
  Fixture replenished(0);
  replenished.mutate_flag_pc = 0x8002f0a4u;
  replenished.mutate_flag_value = 0x80000000u;
  CHECK(replenished.run() == NBA97_TEXT_COMPLETE &&
        replenished.progress.loaded_release_flag.word == 0x80000000u &&
        replenished.calls.size() == 5 && replenished.get(ReleaseGlobal) == 0);

  Fixture partial(0);
  partial.put(PointerGlobal, Pointer, 4, 5);
  partial.mutate_machine = true;
  CHECK(partial.run() == NBA97_TEXT_COMPLETE && partial.calls.size() == 4 &&
        partial.calls[2].machine.registers.gpr[4].known_mask == 5 &&
        partial.progress.machine.registers.gpr[8].word == 0x89abcdefu &&
        partial.progress.machine.registers.gpr[8].known_mask == 3 &&
        partial.progress.machine.registers.gpr[24].word == 0x10203040u &&
        partial.progress.machine.hi.word == 0x55667788u &&
        partial.progress.machine.lo.word == 0xaabbccddu);

  Fixture alias(0);
  alias.context.machine.registers.gpr[29] = {0x80015034u, 15};
  CHECK(alias.run() == NBA97_TEXT_COMPLETE && alias.progress.completed &&
        alias.progress.loaded_release_flag.word == ParentRa &&
        alias.progress.restored_return_address.word == 0 &&
        alias.progress.machine.registers.gpr[31].word == 0 &&
        alias.progress.machine.registers.gpr[29].word == 0x80015034u);
}

void callbackLiveStackAndReturnKnownness() {
  Fixture moved;
  moved.relocate_pc = 0x8002f0d0u;
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
        bad_ra.progress.stopped_pc == 0x8002f0e0u &&
        bad_ra.progress.instruction_count == 21);

  Fixture late_unknown;
  late_unknown.relocate_pc = 0x8002f0d0u;
  late_unknown.relocated_frame = 0x801e0000u;
  late_unknown.put(late_unknown.relocated_frame + 16u, ParentRa, 4, 7);
  CHECK(late_unknown.run() == NBA97_TEXT_UNKNOWN &&
        late_unknown.progress.stopped_pc == 0x8002f0e0u);
  Fixture late_misaligned;
  late_misaligned.relocate_pc = 0x8002f0d0u;
  late_misaligned.relocated_frame = 0x801e0002u;
  CHECK(late_misaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        late_misaligned.progress.stopped_pc == 0x8002f0d8u);
  Fixture late_unmapped;
  late_unmapped.relocate_pc = 0x8002f0d0u;
  late_unmapped.relocated_frame = 0x70000000u;
  CHECK(late_unmapped.run() == NBA97_TEXT_RESOURCE &&
        late_unmapped.progress.stopped_pc == 0x8002f0d8u);
}

void budgetsRefusalsAndMalformed() {
  for (std::size_t budget = 0; budget <= 10; ++budget) {
    Fixture f;
    f.context.operation_budget = budget;
    const int result = f.run();
    CHECK(result == (budget == 10 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_LIMIT));
    CHECK(f.progress.operations == budget);
  }
  for (unsigned i = 0; i < Pcs.size(); ++i) {
    Fixture f;
    f.refuse_pc = Pcs[i];
    f.mutate_flag_pc = Pcs[i];
    f.mutate_flag_value = 0xdeadbeefu;
    CHECK(f.run() == NBA97_TEXT_IO_REFUSED &&
          f.progress.stopped_pc == Pcs[i] &&
          f.progress.stopped_target == Targets[i] &&
          f.get(ReleaseGlobal) == 0xdeadbeefu &&
          f.progress.machine.registers.gpr[31].word == Pcs[i] + 8u);
  }
  Fixture malformed;
  malformed.malformed_machine = true;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.callbacks_completed == 0 &&
        malformed.progress.machine.registers.gpr[9].known_mask == 16);
}

void memoryKnownnessAndArguments() {
  Fixture unknown_sp;
  unknown_sp.context.machine.registers.gpr[29].known_mask = 14;
  CHECK(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stopped_pc == 0x8002f088u &&
        unknown_sp.progress.operations == 0);
  Fixture misaligned_sp;
  misaligned_sp.context.machine.registers.gpr[29] = {Sp + 2u, 15};
  CHECK(misaligned_sp.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misaligned_sp.progress.operations == 1);
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
  low_data[0] = std::uint8_t(ParentRa & 0xffu);
  low_data[1] = std::uint8_t((ParentRa >> 8) & 0xffu);
  low_data[2] = std::uint8_t((ParentRa >> 16) & 0xffu);
  low_data[3] = std::uint8_t((ParentRa >> 24) & 0xffu);
  CHECK(wrapped.run() == NBA97_TEXT_COMPLETE &&
        wrapped.progress.frame_stack_pointer == 0xfffffff0u &&
        wrapped.access[0].address == 0 &&
        wrapped.progress.machine.registers.gpr[29].word == 8);

  Fixture no_plane(0);
  no_plane.region.known = nullptr;
  CHECK(no_plane.run() == NBA97_TEXT_COMPLETE);
  Fixture no_plane_partial(0);
  no_plane_partial.region.known = nullptr;
  no_plane_partial.context.machine.registers.gpr[31].known_mask = 7;
  CHECK(no_plane_partial.run() == NBA97_TEXT_ARGUMENT &&
        no_plane_partial.progress.stopped_pc == 0x8002f088u);

  Fixture branch_unknown(0);
  branch_unknown.put(ReleaseGlobal, 0, 4, 7);
  CHECK(branch_unknown.run() == NBA97_TEXT_UNKNOWN &&
        branch_unknown.progress.stopped_pc == 0x8002f0b8u &&
        branch_unknown.progress.instruction_count == 15);
  Fixture branch_known_nonzero(0);
  branch_known_nonzero.put(ReleaseGlobal, 0x00010000u, 4, 4);
  CHECK(branch_known_nonzero.run() == NBA97_TEXT_COMPLETE &&
        branch_known_nonzero.calls.size() == 5 &&
        branch_known_nonzero.calls[3].machine.registers.gpr[4].word ==
            0x00010000u &&
        branch_known_nonzero.calls[3].machine.registers.gpr[4].known_mask == 4);
  Fixture branch_all_unknown(0);
  branch_all_unknown.put(ReleaseGlobal, 0, 4, 0);
  CHECK(branch_all_unknown.run() == NBA97_TEXT_UNKNOWN &&
        branch_all_unknown.progress.stopped_pc == 0x8002f0b8u &&
        branch_all_unknown.progress.instruction_count == 15);
  Fixture malformed_known;
  malformed_known.known[ReleaseGlobal - Base + 3] = 2;
  CHECK(malformed_known.run() == NBA97_TEXT_ARGUMENT &&
        malformed_known.progress.stopped_pc == 0x8002f0b0u);

  Fixture overlap;
  std::array<std::uint8_t, 16> data{};
  std::array<Nba97GameTextRegion, 2> regions{{
      {0x90000000u, data.data(), nullptr, 12},
      {0x90000008u, data.data() + 8, nullptr, 8},
  }};
  overlap.context.memory = {regions.data(), regions.size()};
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT);
  Nba97FrontendExitCleanupProgress out{};
  CHECK(nba97_frontend_exit_cleanup(nullptr, &out) == NBA97_TEXT_ARGUMENT &&
        nba97_frontend_exit_cleanup(&overlap.context, nullptr) ==
            NBA97_TEXT_ARGUMENT);
}
} // namespace

int main() {
  try {
    normalPathsAndTrace();
    liveReloadMachineAndAlias();
    callbackLiveStackAndReturnKnownness();
    budgetsRefusalsAndMalformed();
    memoryKnownnessAndArguments();
    std::printf("frontend_exit_cleanup_tests: PASS (%u checks)\n", checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
