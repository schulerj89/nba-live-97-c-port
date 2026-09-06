#include "frontend_dispatch_entry_capture.h"
#include "frontend_dispatch_entry_adapter.h"
#include "user_setup_session.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using U = std::uint32_t;
unsigned checks;
void checkAt(bool value, int line) {
  ++checks;
  if (!value)
    throw std::runtime_error(
        "frontend-dispatch-entry integration check failed at " +
        std::to_string(line));
}
#define CHECK(value) checkAt((value), __LINE__)

constexpr std::array<U, 43> Targets{
    {0x8003fa3c, 0x8003fb6c, 0x8003fc34, 0x8003fc78, 0x8003fca8,
     0x8003fcf4, 0x8003fd74, 0x8003fe58, 0x8003fe98, 0x8003ff10,
     0x8004005c, 0x8004006c, 0x800400f0, 0x80040120, 0x80040154,
     0x80040184, 0x80040194, 0x800401c0, 0x800401fc, 0x8004028c,
     0x800402d8, 0x800402e8, 0x80040350, 0x80040360, 0x80040370,
     0x80040380, 0x80040390, 0x80040410, 0x80040474, 0x800405d8,
     0x80040558, 0x80040658, 0x800406bc, 0x800406fc, 0x8004070c,
     0x8004071c, 0x8004072c, 0x8004073c, 0x8004076c, 0x8004077c,
     0x8004009c, 0x800400ac, 0x800407d4}};

struct Fixture {
  static constexpr U Base = 0x80000000u;
  static constexpr U Size = 0x200000u;
  static constexpr U Context = 0x80110000u;
  static constexpr U Allocation = 0x80140000u;
  static constexpr U Roster = 0x80160000u;
  static constexpr U Sp = 0x801f0000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendDispatchEntryMachine machine{};
  Nba97FrontendDispatchEntryBinding binding{};
  Nba97FrontendDispatchEntryCallerEvent parent{
      0x80028aa0u, 0x80028aa4u, 0x800360d4u, 1, 1, 0};
  std::array<Nba97FrontendDispatchEntryAccess, 8> wrapper_journal{};
  std::array<Nba97FrontendDispatchAccess, 4096> dispatch_journal{};
  std::vector<Nba97FrontendDispatchEvent> calls;
  nba97::UserSetupSession session;
  bool setup_accepted = false;
  bool relocate_combined_frame = false;
  U relocated_ra = 0x13579bdfu;
  U refuse_pc = 0;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      machine.registers.gpr[i] = {0x22000000u + i * 0x101u, 15};
    machine.registers.gpr[0] = {0, 15};
    machine.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_S0] = {0, 15};
    machine.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_SP] = {Sp, 15};
    machine.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_RA] = {0x80028aa8u,
                                                                15};
    machine.hi = {0x12345678u, 5};
    machine.lo = {0x9abcdef0u, 10};
    put(0x800170c0u, Context);
    put(Context + 0x14u, 0x80120000u);
    put(0x8009821cu, Allocation);
    put(0x800982e0u, Allocation);
    put(0x8009352cu, Allocation);
    put(0x80015098u, 0);
    put(0x80021d74u, 0);
    put(0x80021d78u, 1);
    put(0x8001edecu, 0, 2);
    put(Context + 0x720u, 5, 2);
    for (unsigned i = 0; i < Targets.size(); ++i)
      put(0x80024f80u + i * 4u, Targets[i]);
    for (unsigned team = 0; team < 2; ++team)
      for (unsigned slot = 0; slot < 12; ++slot) {
        const U source = Roster + (team + 1u) * 0x600u + slot * 0x80u;
        put(0x80023ab0u + team * 104u + slot * 4u, source);
        for (unsigned byte = 0; byte < 110; ++byte)
          put(source + byte, team * 17u + slot + byte, 1);
      }
    binding.operation_budget = 5;
    binding.access_journal = wrapper_journal.data();
    binding.access_journal_capacity = wrapper_journal.size();
    binding.dispatcher.operation_budget = 20000;
    binding.dispatcher.io = callback;
    binding.dispatcher.user = this;
    binding.dispatcher.access_journal = dispatch_journal.data();
    binding.dispatcher.access_journal_capacity = dispatch_journal.size();
  }

  void put(U address, U value, unsigned width = 4) {
    for (unsigned byte = 0; byte < width; ++byte)
      bytes[address - Base + byte] = std::uint8_t(value >> (byte * 8u));
  }
  U get(U address) const {
    U value = 0;
    for (unsigned byte = 0; byte < 4; ++byte)
      value |= U(bytes[address - Base + byte]) << (byte * 8u);
    return value;
  }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97FrontendDispatchEvent *event,
                      Nba97FrontendDispatchMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    Nba97FrontendDispatchSiteContract contract{};
    if (!event || !machine ||
        !nba97_frontend_dispatch_site_contract(event->site, &contract) ||
        event->pc != contract.pc ||
        event->delay_slot_pc != contract.delay_slot_pc ||
        event->target != contract.target ||
        event->argument_count != contract.argument_count ||
        machine->registers.gpr[31].known_mask != 15 ||
        machine->registers.gpr[31].word != event->pc + 8u)
      return 0;
    f.calls.push_back(*event);
    if (f.refuse_pc == event->pc)
      return 0;
    if (f.relocate_combined_frame && f.calls.size() == 1) {
      const U old_dispatch_frame = machine->registers.gpr[29].word;
      const U new_dispatch_frame = old_dispatch_frame - 0x200u;
      for (unsigned i = 0; i < 0xa0; ++i) {
        f.bytes[new_dispatch_frame - Base + i] =
            f.bytes[old_dispatch_frame - Base + i];
        if (f.region.known)
          f.known[new_dispatch_frame - Base + i] =
              f.known[old_dispatch_frame - Base + i];
      }
      machine->registers.gpr[29] = {new_dispatch_frame, 15};
      f.put(new_dispatch_frame + 0x98u, f.relocated_ra);
    }
    if (event->target == 0x800770d4u)
      machine->registers.gpr[2] = {Allocation, 15};
    else if (event->target == 0x800459c8u)
      machine->registers.gpr[2] = {5, 15};
    else if (event->target == 0x80031a88u)
      f.put(Context + 0x720u, 5, 2);
    else if (event->target == 0x80037010u) {
      std::array<std::uint8_t, 8> assignments{{1, 2, 0, 0, 0, 0, 0, 0}};
      f.session.open(assignments, {}, 0);
      f.session.setControllers(0, 1);
      f.session.key(0, 0x80, true);
      const auto actions = f.session.step(100);
      f.setup_accepted =
          !actions.empty() && actions.back().event == NBA97_USER_CONFIRMED &&
          f.session.state().result == 6;
      machine->registers.gpr[2] = {f.setup_accepted ? 6u : 0u, 15};
    } else if (event->target == 0x800909a8u) {
      const U source = machine->registers.gpr[4].word;
      const U target = machine->registers.gpr[5].word;
      const U count = machine->registers.gpr[6].word;
      if (count != 0x6e || source < Base || target < Base ||
          source - Base > f.bytes.size() - count ||
          target - Base > f.bytes.size() - count)
        return 0;
      for (U i = 0; i < count; ++i) {
        f.bytes[target - Base + i] = f.bytes[source - Base + i];
        f.known[target - Base + i] = f.known[source - Base + i];
      }
    } else
      machine->registers.gpr[2] = {UINT32_MAX, 15};
    return 1;
  }

  int run() {
    Nba97GameTextMemory memory{&region, 1};
    return nba97_frontend_dispatch_entry_from_frontend_main(
        &binding, &memory, &parent, &machine);
  }
};

void syntheticParentAndNaturalDispatcherComposition() {
  Fixture f;
  CHECK(f.run() == 1 && f.binding.result == NBA97_TEXT_COMPLETE &&
        f.binding.progress.completed && f.binding.invocations == 1 &&
        f.binding.completions == 1 && f.setup_accepted &&
        f.session.state().result == 6);
  CHECK(f.binding.event.pc == 0x80028aa0u &&
        f.binding.event.delay_slot_pc == 0x80028aa4u &&
        f.binding.event.entry == 0x800360d4u &&
        f.machine.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_S0].word == 0 &&
        f.machine.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_SP].word ==
            Fixture::Sp &&
        f.machine.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_RA].word ==
            0x80028aa8u);
  CHECK(f.get(0x80021ee4u) == 1 && f.get(0x800c6e68u) == 32 &&
        f.get(Fixture::Sp - 8u) == 0x80028aa8u);
  CHECK(f.binding.progress.operations == 5 &&
        f.binding.progress.instruction_count == 14 &&
        f.binding.progress.frame_stack_pointer == Fixture::Sp - 24u &&
        f.binding.progress.callbacks_completed == 1 &&
        f.binding.adapter.dispatcher_invocations == 1 &&
        f.binding.adapter.dispatcher_completions == 1 &&
        f.binding.adapter.dispatcher_result == NBA97_TEXT_COMPLETE &&
        f.binding.adapter.dispatcher_event.pc == 0x800360f4u &&
        f.binding.adapter.dispatcher_event.delay_slot_pc == 0x800360f8u &&
        f.binding.adapter.dispatcher_event.entry == 0x8003f7c8u &&
        f.binding.adapter.dispatcher_progress.completed &&
        f.binding.dispatcher.invocations == 1 &&
        f.binding.dispatcher.completions == 1 && !f.calls.empty());
  const std::array<U, 4> addresses{{0x80021ee4u, Fixture::Sp - 8u,
                                    0x800c6e68u, Fixture::Sp - 8u}};
  for (unsigned i = 0; i < addresses.size(); ++i)
    CHECK(f.wrapper_journal[i].address == addresses[i] &&
          f.wrapper_journal[i].operation == (i < 3 ? i + 1u : 5u));
}

void combinedFrameRelocationAndNestedBudget() {
  for (bool known_plane : {true, false}) {
    Fixture moved;
    moved.relocate_combined_frame = true;
    if (!known_plane)
      moved.region.known = nullptr;
    CHECK(moved.run() == 1 && moved.binding.progress.completed &&
          moved.binding.adapter.dispatcher_progress.completed &&
          moved.machine.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_SP].word ==
              Fixture::Sp - 0x200u &&
          moved.machine.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_RA].word ==
              moved.relocated_ra &&
          moved.binding.progress.restored_return_address.word ==
              moved.relocated_ra &&
          moved.wrapper_journal[3].address == Fixture::Sp - 0x200u - 8u);
  }

  Fixture limited;
  limited.binding.dispatcher.operation_budget = 1;
  CHECK(limited.run() == 0 &&
        limited.binding.result == NBA97_TEXT_IO_REFUSED &&
        limited.binding.progress.operations == 4 &&
        limited.binding.progress.stores == 3 &&
        limited.binding.progress.callbacks_completed == 0 &&
        limited.binding.adapter.dispatcher_result == NBA97_TEXT_LIMIT &&
        limited.binding.adapter.dispatcher_progress.operations == 1 &&
        limited.binding.adapter.dispatcher_progress.completed == 0);
}

void directCompositionAndNestedFailure() {
  Fixture direct;
  Nba97FrontendDispatchEntryContext context{};
  context.memory = {&direct.region, 1};
  context.operation_budget = 5;
  context.machine = direct.machine;
  context.access_journal = direct.wrapper_journal.data();
  context.access_journal_capacity = direct.wrapper_journal.size();
  Nba97FrontendDispatchEntryProgress progress{};
  Nba97FrontendDispatchEntryAdapterProgress adapter{};
  CHECK(nba97_frontend_dispatch_entry_with_recovered_dispatch(
            &context, &direct.binding.dispatcher, &progress, &adapter) ==
            NBA97_TEXT_COMPLETE &&
        progress.completed && adapter.dispatcher_completions == 1 &&
        adapter.dispatcher_progress.completed);

  Fixture refused;
  refused.refuse_pc = 0x8003fcf4u;
  CHECK(refused.run() == 0 &&
        refused.binding.result == NBA97_TEXT_IO_REFUSED &&
        refused.binding.progress.operations == 4 &&
        refused.binding.progress.stores == 3 &&
        refused.binding.progress.instruction_count == 10 &&
        refused.binding.adapter.dispatcher_result == NBA97_TEXT_IO_REFUSED &&
        refused.binding.adapter.dispatcher_progress.stopped_pc ==
            0x8003fcf4u &&
        refused.machine.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_RA].word ==
            0x8003fcfcu);
}

void parentGuardsAndFreshRepeatedBindingEvidence() {
  for (unsigned field = 0; field < 9; ++field) {
    Fixture f;
    if (field == 0)
      f.parent.pc ^= 4;
    else if (field == 1)
      f.parent.delay_slot_pc ^= 4;
    else if (field == 2)
      f.parent.entry ^= 4;
    else if (field == 3)
      f.parent.invocation = 2;
    else if (field == 4)
      f.parent.argument_count = 1;
    else if (field == 5)
      f.machine.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_RA].word ^= 4;
    else if (field == 6)
      f.machine.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_RA].known_mask = 14;
    else if (field == 7)
      f.machine.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_S0].word = 1;
    else
      f.machine.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_S0].known_mask = 14;
    const auto before = f.machine;
    CHECK(f.run() == 0 && f.binding.result == NBA97_TEXT_ARGUMENT &&
          f.binding.invocations == 0);
    for (unsigned i = 0; i < 32; ++i)
      CHECK(f.machine.registers.gpr[i].word ==
                before.registers.gpr[i].word &&
            f.machine.registers.gpr[i].known_mask ==
                before.registers.gpr[i].known_mask);
  }

  Fixture reused;
  CHECK(reused.run() == 1 && reused.binding.dispatcher.invocations == 1);
  reused.binding.dispatcher.access_journal = nullptr;
  reused.binding.dispatcher.access_journal_capacity = 1;
  CHECK(reused.run() == 0 &&
        reused.binding.result == NBA97_TEXT_IO_REFUSED &&
        reused.binding.invocations == 2 && reused.binding.completions == 1 &&
        reused.binding.progress.stores == 3 &&
        reused.binding.progress.callbacks_completed == 0 &&
        reused.binding.adapter.dispatcher_result == NBA97_TEXT_ARGUMENT &&
        reused.binding.adapter.dispatcher_invocations == 1 &&
        reused.binding.adapter.dispatcher_completions == 0 &&
        reused.binding.adapter.dispatcher_progress.operations == 0 &&
        reused.binding.adapter.dispatcher_progress.instruction_count == 0 &&
        reused.binding.dispatcher.invocations == 1);
}
} // namespace

int main() {
  try {
    syntheticParentAndNaturalDispatcherComposition();
    combinedFrameRelocationAndNestedBudget();
    directCompositionAndNestedFailure();
    parentGuardsAndFreshRepeatedBindingEvidence();
    const auto receipt = nba97::captureFrontendDispatchEntry();
    CHECK(receipt.find("\"user_setup\":{\"accepted\":1,\"result\":6}") != std::string::npos);
    CHECK(receipt.find("\"contract_failure\":0") != std::string::npos);
    bool printable = true;
    for (unsigned char byte : receipt)
      printable = printable && byte >= 0x20 && byte <= 0x7e;
    CHECK(printable);

    std::printf("frontend_dispatch_entry_integration_tests: PASS (%u checks)\n",
                checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
