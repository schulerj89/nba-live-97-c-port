#include "frontend_overlay_load_adapter.h"
#include "frontend_overlay_load_capture.h"
#include "recovered/frontend_main.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using U = std::uint32_t;
unsigned checks;
void checkAt(bool value, int line) {
  ++checks;
  if (!value)
    throw std::runtime_error("frontend-overlay-load integration failed at " +
                             std::to_string(line));
}
#define CHECK(value) checkAt((value), __LINE__)

constexpr U Base = 0x80000000u;
constexpr U Size = 0x200000u;
constexpr U EntrySp = 0x801ff000u;
constexpr U EntryRa = 0x8007b840u;
constexpr U Handle = 0x80170000u;
constexpr U LoadSize = 0x1234u;
constexpr U DynamicEntry = 0x801e1410u;
constexpr U RelocatedOverlayFrame = 0x801ed000u;

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size, 0xcd);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendMainContext context{};
  Nba97FrontendMainProgress progress{};
  Nba97FrontendOverlayLoadBinding overlay{};
  std::array<Nba97FrontendOverlayLoadAccess, 4> overlay_access{};
  std::array<U, 8> overlay_instructions{};
  unsigned main_calls = 0;
  unsigned loader_calls = 0;
  unsigned overlay_calls = 0;
  unsigned refuse_after_overlay = 0;
  bool refuse_loader = false;
  bool relocate_loader = false;
  bool contract_failure = false;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {0x21000000u + i * 0x01010101u,
                                          15};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[NBA97_FRONTEND_MAIN_SP] = {EntrySp, 15};
    context.machine.registers.gpr[NBA97_FRONTEND_MAIN_RA] = {EntryRa, 15};
    context.machine.hi = {0x10203040u, 5};
    context.machine.lo = {0x50607080u, 10};
    put(0x80021ee4u, 1);
    put(0x8001edecu, 0, 2);
    put(0x80021568u, 0, 2);
    put(0x801e0000u, DynamicEntry);
    context.memory = {&region, 1};
    context.operation_budget = 1000;
    context.io = mainCallback;
    context.user = this;
    overlay.operation_budget = 3;
    overlay.io = loaderCallback;
    overlay.user = this;
    overlay.access_journal = overlay_access.data();
    overlay.access_journal_capacity = overlay_access.size();
    overlay.instruction_journal = overlay_instructions.data();
    overlay.instruction_journal_capacity = overlay_instructions.size();
  }

  void put(U address, U value, unsigned width = 4) {
    if (address < Base || address - Base > Size - width)
      throw std::runtime_error("fixture write outside RAM");
    for (unsigned i = 0; i < width; ++i) {
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
      if (region.known)
        known[address - Base + i] = 1;
    }
  }
  int run() { return nba97_frontend_main(&context, &progress); }

  static int loaderCallback(void *opaque, const Nba97GameTextMemory *,
                            const Nba97FrontendOverlayLoadEvent *event,
                            Nba97FrontendOverlayLoadMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    ++f.loader_calls;
    if (!event || !machine || event->pc != 0x8007b124u ||
        event->delay_slot_pc != 0x8007b128u ||
        event->entry != 0x8007b15cu || event->argument_count != 3 ||
        event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
        machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A0].word !=
            0x80024854u ||
        machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A0].known_mask !=
            15 ||
        machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A1].word != 0 ||
        machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A1].known_mask !=
            15 ||
        machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A2].word != 1 ||
        machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A2].known_mask !=
            15) {
      f.contract_failure = true;
      return 0;
    }
    machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_V0] = {Handle, 15};
    if (f.relocate_loader) {
      const U old_frame =
          machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_SP].word;
      if (old_frame < Base || old_frame - Base > Size - 64u ||
          RelocatedOverlayFrame < Base ||
          RelocatedOverlayFrame - Base > Size - 64u) {
        f.contract_failure = true;
        return 0;
      }
      for (unsigned i = 0; i < 64; ++i) {
        f.bytes[RelocatedOverlayFrame - Base + i] =
            f.bytes[old_frame - Base + i];
        if (f.region.known)
          f.known[RelocatedOverlayFrame - Base + i] =
              f.known[old_frame - Base + i];
      }
      machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_SP] = {
          RelocatedOverlayFrame, 15};
    }
    return !f.refuse_loader;
  }

  static int mainCallback(void *opaque, const Nba97GameTextMemory *memory,
                          const Nba97FrontendMainEvent *event,
                          Nba97FrontendMainMachine *machine,
                          Nba97FrontendMainCalleeOutcome *outcome) {
    auto &f = *static_cast<Fixture *>(opaque);
    ++f.main_calls;
    if (!event || !machine || !outcome || event->site == 0 ||
        event->site >= NBA97_FRONTEND_MAIN_SITE_COUNT ||
        event->delay_slot_pc != event->pc + 4u ||
        machine->registers.gpr[NBA97_FRONTEND_MAIN_RA].word != event->pc + 8u ||
        machine->registers.gpr[NBA97_FRONTEND_MAIN_RA].known_mask != 15) {
      f.contract_failure = true;
      return 0;
    }
    if (event->site == NBA97_FRONTEND_MAIN_SITE_80028ACC) {
      ++f.overlay_calls;
      return nba97_frontend_overlay_load_from_frontend_main(
          &f.overlay, memory, event, machine, outcome);
    }
    if (f.refuse_after_overlay && f.overlay_calls)
      return 0;
    if (event->site == NBA97_FRONTEND_MAIN_SITE_80028A7C)
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {0, 15};
    if (event->site == NBA97_FRONTEND_MAIN_SITE_80028AD8)
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {LoadSize, 15};
    if (event->site == NBA97_FRONTEND_MAIN_SITE_80028B54)
      f.put(0x801e0000u, DynamicEntry);
    if (event->site == NBA97_FRONTEND_MAIN_SITE_80028B68)
      return 0;
    return 1;
  }
};

Nba97FrontendMainEvent parentEvent() {
  return {0x80028accu, 0x80028ad0u, 0x8007b11cu, 0, 1,
          NBA97_FRONTEND_MAIN_SITE_80028ACC, 2,
          NBA97_FRONTEND_MAIN_PROGRAM_FEONLY};
}

void naturalCallerComposition() {
  Fixture f;
  CHECK(f.run() == NBA97_TEXT_IO_REFUSED && !f.progress.completed &&
        !f.progress.transferred && !f.contract_failure &&
        f.progress.stopped_pc == 0x80028b68u &&
        f.progress.stopped_target == DynamicEntry && f.main_calls == 65 &&
        f.progress.callbacks_completed == 64 &&
        f.progress.wait_iterations == 20 &&
        f.progress.call_count[NBA97_FRONTEND_MAIN_SITE_80028ACC] == 1 &&
        f.progress.call_attempts[NBA97_FRONTEND_MAIN_SITE_80028B68] == 1 &&
        f.progress.call_count[NBA97_FRONTEND_MAIN_SITE_80028B68] == 0);
  CHECK(f.overlay_calls == 1 && f.loader_calls == 1 &&
        f.overlay.invocations == 1 && f.overlay.completions == 1 &&
        f.overlay.result == NBA97_TEXT_COMPLETE &&
        f.overlay.progress.completed);
  CHECK(f.overlay.event.pc == 0x80028accu &&
        f.overlay.event.delay_slot_pc == 0x80028ad0u &&
        f.overlay.event.entry == 0x8007b11cu &&
        f.overlay.event.argument_count == 2);
  CHECK(f.overlay.progress.forwarded_a0.word == 0x80024854u &&
        f.overlay.progress.forwarded_a0.known_mask == 15 &&
        f.overlay.progress.forwarded_a1.word == 0 &&
        f.overlay.progress.forwarded_a1.known_mask == 15 &&
        f.overlay.progress.delay_a2.word == 1 &&
        f.overlay.progress.delay_a2.known_mask == 15);
  CHECK(f.overlay.progress.child_return.word == Handle &&
        f.overlay.progress.child_return.known_mask == 15 &&
        f.progress.gameload_handle.word == Handle &&
        f.progress.gameload_size.word == LoadSize &&
        f.progress.dynamic_entry.word == DynamicEntry);
  CHECK(f.overlay.progress.operations == 3 &&
        f.overlay.progress.accesses == 2 &&
        f.overlay.progress.callbacks_completed == 1 &&
        f.overlay.progress.instruction_count == 8);
  for (unsigned i = 0; i < 8; ++i)
    CHECK(f.overlay_instructions[i] == 0x8007b11cu + 4u * i);
}

void prefixFailuresCompose() {
  Fixture next;
  next.refuse_after_overlay = 1;
  CHECK(next.run() == NBA97_TEXT_IO_REFUSED && next.overlay_calls == 1 &&
        next.loader_calls == 1 && next.overlay.progress.completed &&
        next.progress.stopped_pc == 0x80028ad8u &&
        next.progress.stopped_target == 0x80077cd4u &&
        next.progress.gameload_handle.word == Handle && next.main_calls == 35 &&
        next.progress.callbacks_completed == 34 &&
        next.progress.call_count[NBA97_FRONTEND_MAIN_SITE_80028ACC] == 1 &&
        next.progress.call_attempts[NBA97_FRONTEND_MAIN_SITE_80028AD8] == 1 &&
        next.progress.call_count[NBA97_FRONTEND_MAIN_SITE_80028AD8] == 0);

  Fixture child;
  child.refuse_loader = true;
  CHECK(child.run() == NBA97_TEXT_IO_REFUSED && child.overlay_calls == 1 &&
        child.loader_calls == 1 && child.overlay.invocations == 1 &&
        child.overlay.completions == 0 &&
        child.overlay.result == NBA97_TEXT_IO_REFUSED &&
        child.overlay.progress.operations == 2 &&
        child.overlay.progress.callbacks_completed == 0 &&
        child.progress.stopped_pc == 0x80028accu && child.main_calls == 34 &&
        child.progress.callbacks_completed == 33 &&
        child.progress.call_attempts[NBA97_FRONTEND_MAIN_SITE_80028ACC] == 1 &&
        child.progress.call_count[NBA97_FRONTEND_MAIN_SITE_80028ACC] == 0);
}

void optionalPlaneAndCombinedRelocation() {
  Fixture absent;
  absent.region.known = nullptr;
  CHECK(absent.run() == NBA97_TEXT_IO_REFUSED && !absent.progress.completed &&
        absent.progress.stopped_pc == 0x80028b68u &&
        absent.progress.stopped_target == DynamicEntry &&
        absent.overlay.progress.completed && !absent.contract_failure);

  Fixture relocated;
  relocated.relocate_loader = true;
  CHECK(relocated.run() == NBA97_TEXT_IO_REFUSED &&
        !relocated.progress.completed && relocated.overlay.progress.completed &&
        relocated.progress.stopped_pc == 0x80028b68u &&
        relocated.progress.stopped_target == DynamicEntry &&
        relocated.overlay.progress.restored_return_address.word ==
            0x80028ad4u &&
        relocated.overlay.progress.machine.registers.gpr
                [NBA97_FRONTEND_OVERLAY_LOAD_SP]
                    .word == RelocatedOverlayFrame + 24u &&
        relocated.progress.machine.registers.gpr[NBA97_FRONTEND_MAIN_SP].word ==
            RelocatedOverlayFrame + 24u &&
        !relocated.contract_failure);
}

void adapterGuardsAndContract() {
  Fixture f;
  Nba97GameTextMemory memory{&f.region, 1};
  auto event = parentEvent();
  auto machine = f.context.machine;
  machine.registers.gpr[NBA97_FRONTEND_MAIN_A0] = {0x80024854u, 15};
  machine.registers.gpr[NBA97_FRONTEND_MAIN_A1] = {0, 15};
  machine.registers.gpr[NBA97_FRONTEND_MAIN_RA] = {0x80028ad4u, 15};
  Nba97FrontendOverlayLoadSiteContract contract{};
  CHECK(nba97_frontend_overlay_load_site_contract(
            NBA97_FRONTEND_OVERLAY_LOAD_SITE_8007B124, &contract) == 1 &&
        contract.pc == 0x8007b124u && contract.delay_slot_pc == 0x8007b128u &&
        contract.target == 0x8007b15cu && contract.argument_count == 3 &&
        contract.target_program == NBA97_FRONTEND_MAIN_PROGRAM_FEONLY);
  CHECK(!nba97_frontend_overlay_load_site_contract(0, &contract) &&
        !nba97_frontend_overlay_load_site_contract(
            NBA97_FRONTEND_OVERLAY_LOAD_SITE_COUNT, &contract) &&
        !nba97_frontend_overlay_load_site_contract(
            NBA97_FRONTEND_OVERLAY_LOAD_SITE_8007B124, nullptr));

  const auto reject = [&](Nba97FrontendMainEvent bad_event,
                          Nba97FrontendMainMachine bad_machine) {
    Nba97FrontendOverlayLoadBinding binding{};
    binding.operation_budget = 3;
    binding.io = Fixture::loaderCallback;
    binding.user = &f;
    Nba97FrontendMainCalleeOutcome rejected_outcome =
        NBA97_FRONTEND_MAIN_CALLEE_TRANSFERRED;
    const auto before = bad_machine;
    CHECK(!nba97_frontend_overlay_load_from_frontend_main(
              &binding, &memory, &bad_event, &bad_machine,
              &rejected_outcome) &&
          binding.result == NBA97_TEXT_ARGUMENT && binding.invocations == 0 &&
          binding.completions == 0 &&
          rejected_outcome == NBA97_FRONTEND_MAIN_CALLEE_TRANSFERRED &&
          std::memcmp(&before, &bad_machine, sizeof before) == 0);
  };

  auto bad_event = event;
  bad_event.argument_count = 3;
  reject(bad_event, machine);
  bad_event = event;
  bad_event.pc ^= 4u;
  reject(bad_event, machine);
  bad_event = event;
  bad_event.delay_slot_pc ^= 4u;
  reject(bad_event, machine);
  bad_event = event;
  bad_event.entry ^= 4u;
  reject(bad_event, machine);
  bad_event = event;
  bad_event.invocation = 2;
  reject(bad_event, machine);
  bad_event = event;
  bad_event.site = NBA97_FRONTEND_MAIN_SITE_80028AA0;
  reject(bad_event, machine);
  bad_event = event;
  bad_event.target_program = NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD;
  reject(bad_event, machine);

  auto bad_machine = machine;
  bad_machine.registers.gpr[NBA97_FRONTEND_MAIN_RA].word ^= 4u;
  reject(event, bad_machine);
  bad_machine = machine;
  bad_machine.registers.gpr[NBA97_FRONTEND_MAIN_RA].known_mask = 7;
  reject(event, bad_machine);
  bad_machine = machine;
  bad_machine.registers.gpr[NBA97_FRONTEND_MAIN_A0].word ^= 4u;
  reject(event, bad_machine);
  bad_machine = machine;
  bad_machine.registers.gpr[NBA97_FRONTEND_MAIN_A0].known_mask = 7;
  reject(event, bad_machine);
  bad_machine = machine;
  bad_machine.registers.gpr[NBA97_FRONTEND_MAIN_A1].word = 1;
  reject(event, bad_machine);
  bad_machine = machine;
  bad_machine.registers.gpr[NBA97_FRONTEND_MAIN_A1].known_mask = 7;
  reject(event, bad_machine);
}

void captureSchema() {
  const std::string json = nba97::captureFrontendOverlayLoad();
  CHECK(json.find("\"address\":\"0x8007b11c\"") != std::string::npos &&
        json.find("\"inclusive_end\":\"0x8007b13b\"") !=
            std::string::npos &&
        json.find("\"argument_count\":3") != std::string::npos &&
        json.find("\"contract_failure\":0") != std::string::npos &&
        json.find("\"gameplay_shown\":\"BLOCKED\"") !=
            std::string::npos &&
        json.find("0x8007B124 -> 0x8007B15C") != std::string::npos);
}
} // namespace

int main() {
  try {
    naturalCallerComposition();
    prefixFailuresCompose();
    optionalPlaneAndCombinedRelocation();
    adapterGuardsAndContract();
    captureSchema();
    std::printf("frontend_overlay_load_integration_tests passed %u checks\n",
                checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
