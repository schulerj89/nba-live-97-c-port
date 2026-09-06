#include "frontend_load_payload_adapter.h"
#include "frontend_load_payload_capture.h"

#include <array>
#include <cctype>
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
    throw std::runtime_error("frontend-load-payload integration failed at " +
                             std::to_string(line));
}
#define CHECK(value) checkAt((value), __LINE__)

struct Fixture {
  static constexpr U Base = 0x80000000u;
  static constexpr U Size = 0x200000u;
  static constexpr U Sp = 0x801f0000u;
  static constexpr U Descriptor = 0x80170000u;
  static constexpr U Payload = 0x801e1410u;
  static constexpr U Relocated = 0x801ed000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendOverlayLoadContext overlay{};
  Nba97FrontendOverlayLoadProgress overlay_progress{};
  Nba97FrontendLoadPayloadBinding payload{};
  Nba97FrontendLoadPayloadAdapterProgress adapter{};
  std::array<Nba97FrontendOverlayLoadAccess, 4> overlay_access{};
  std::array<U, 8> overlay_instructions{};
  std::array<Nba97FrontendLoadPayloadAccess, 6> payload_access{};
  std::array<U, 16> payload_instructions{};
  Nba97FrontendLoadPayloadEvent child_event{};
  Nba97FrontendLoadPayloadWord child_return{Descriptor, 15};
  unsigned child_calls = 0;
  bool refuse_child = false;
  bool relocate = false;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      overlay.machine.registers.gpr[i] = {0x74000000u + i * 0x101u, 15};
    overlay.machine.registers.gpr[0] = {0, 15};
    overlay.machine.registers.gpr[4] = {0x80024854u, 15};
    overlay.machine.registers.gpr[5] = {0, 15};
    overlay.machine.registers.gpr[6] = {0x55667788u, 5};
    overlay.machine.registers.gpr[29] = {Sp, 15};
    overlay.machine.registers.gpr[31] = {0x80028ad4u, 15};
    overlay.machine.hi = {0x10203040u, 6};
    overlay.machine.lo = {0x50607080u, 9};
    put(Descriptor, Payload, 7);
    overlay.memory = {&region, 1};
    overlay.operation_budget = 3;
    overlay.access_journal = overlay_access.data();
    overlay.access_journal_capacity = overlay_access.size();
    overlay.instruction_journal = overlay_instructions.data();
    overlay.instruction_journal_capacity = overlay_instructions.size();
    payload.operation_budget = 4;
    payload.io = childIo;
    payload.user = this;
    payload.access_journal = payload_access.data();
    payload.access_journal_capacity = payload_access.size();
    payload.instruction_journal = payload_instructions.data();
    payload.instruction_journal_capacity = payload_instructions.size();
  }

  bool extent(U address, U width = 4) const {
    return address >= Base && width <= Size && address - Base <= Size - width;
  }
  void put(U address, U value, std::uint8_t mask = 15) {
    if (!extent(address))
      throw std::runtime_error("integration fixture write outside RAM");
    for (unsigned i = 0; i < 4; ++i) {
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
      if (region.known)
        known[address - Base + i] = std::uint8_t((mask >> i) & 1u);
    }
  }

  static int childIo(void *opaque, const Nba97GameTextMemory *,
                     const Nba97FrontendLoadPayloadEvent *event,
                     Nba97FrontendLoadPayloadMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    ++f.child_calls;
    if (!event || !machine || event->pc != 0x8007b164u ||
        event->delay_slot_pc != 0x8007b168u ||
        event->entry != 0x8007b1d0u || event->operation != 2 ||
        event->invocation != 1 || event->argument_count != 3 ||
        event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
        machine->registers.gpr[4].word != 0x80024854u ||
        machine->registers.gpr[4].known_mask != 15 ||
        machine->registers.gpr[5].word != 0 ||
        machine->registers.gpr[5].known_mask != 15 ||
        machine->registers.gpr[6].word != 1 ||
        machine->registers.gpr[6].known_mask != 15 ||
        machine->registers.gpr[31].word != 0x8007b16cu ||
        machine->registers.gpr[31].known_mask != 15)
      return 0;
    f.child_event = *event;
    if (f.relocate) {
      const U old_frame = machine->registers.gpr[29].word;
      if (!f.extent(old_frame, 48) || !f.extent(Relocated, 48)) return 0;
      for (U i = 0; i < 48; ++i) {
        f.bytes[Relocated - Base + i] = f.bytes[old_frame - Base + i];
        if (f.region.known)
          f.known[Relocated - Base + i] = f.known[old_frame - Base + i];
      }
      machine->registers.gpr[29] = {Relocated, 15};
    }
    machine->registers.gpr[2] = f.child_return;
    return f.refuse_child ? 0 : 1;
  }

  int run() {
    return nba97_frontend_overlay_load_with_recovered_payload(
        &overlay, &payload, &overlay_progress, &adapter);
  }
};

void naturalNullAndNonnull() {
  Fixture nonnull;
  CHECK(nonnull.run() == NBA97_TEXT_COMPLETE &&
        nonnull.overlay_progress.completed && nonnull.payload.progress.completed &&
        nonnull.payload.invocations == 1 && nonnull.payload.completions == 1 &&
        nonnull.adapter.invocations == 1 && nonnull.adapter.completions == 1 &&
        nonnull.child_calls == 1);
  CHECK(nonnull.overlay_progress.operations == 3 &&
        nonnull.overlay_progress.accesses == 2 &&
        nonnull.overlay_progress.callbacks_completed == 1 &&
        nonnull.overlay_progress.instruction_count == 8 &&
        nonnull.payload.progress.operations == 4 &&
        nonnull.payload.progress.accesses == 3 &&
        nonnull.payload.progress.callbacks_completed == 1 &&
        nonnull.payload.progress.instruction_count == 11);
  CHECK(nonnull.adapter.parent_event.pc == 0x8007b124u &&
        nonnull.adapter.parent_event.delay_slot_pc == 0x8007b128u &&
        nonnull.adapter.parent_event.entry == 0x8007b15cu &&
        nonnull.adapter.parent_event.argument_count == 3 &&
        nonnull.adapter.parent_machine.registers.gpr[6].word == 1 &&
        nonnull.adapter.parent_machine.registers.gpr[31].word == 0x8007b12cu);
  CHECK(nonnull.payload.progress.child_return.word == Fixture::Descriptor &&
        nonnull.payload.progress.payload_result.word == Fixture::Payload &&
        nonnull.payload.progress.payload_result.known_mask == 7 &&
        nonnull.overlay_progress.child_return.word == Fixture::Payload &&
        nonnull.overlay_progress.child_return.known_mask == 7 &&
        nonnull.overlay_progress.machine.registers.gpr[2].word ==
            Fixture::Payload &&
        nonnull.overlay_progress.machine.registers.gpr[2].known_mask == 7 &&
        nonnull.overlay_progress.machine.registers.gpr[29].word == Fixture::Sp &&
        nonnull.overlay_progress.machine.registers.gpr[31].word == 0x80028ad4u);

  Fixture null_result;
  null_result.child_return = {0, 15};
  null_result.payload.operation_budget = 3;
  CHECK(null_result.run() == NBA97_TEXT_COMPLETE &&
        null_result.payload.progress.completed &&
        null_result.payload.progress.payload_result.word == 0 &&
        null_result.payload.progress.payload_result.known_mask == 15 &&
        null_result.payload.progress.operations == 3 &&
        null_result.overlay_progress.child_return.word == 0 &&
        null_result.overlay_progress.machine.registers.gpr[2].word == 0);
}

void absentPlaneRelocationAndFailure() {
  Fixture absent;
  absent.region.known = nullptr;
  CHECK(absent.run() == NBA97_TEXT_COMPLETE &&
        absent.payload.progress.payload_result.known_mask == 15 &&
        absent.overlay_progress.machine.registers.gpr[2].known_mask == 15);

  Fixture relocated;
  relocated.relocate = true;
  CHECK(relocated.run() == NBA97_TEXT_COMPLETE &&
        relocated.payload.progress.restored_return_address.word == 0x8007b12cu &&
        relocated.payload.progress.machine.registers.gpr[29].word ==
            Fixture::Relocated + 24u &&
        relocated.overlay_progress.restored_return_address.word ==
            0x80028ad4u &&
        relocated.overlay_progress.machine.registers.gpr[29].word ==
            Fixture::Relocated + 48u);

  Fixture refused;
  refused.refuse_child = true;
  CHECK(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.payload.result == NBA97_TEXT_IO_REFUSED &&
        refused.payload.invocations == 1 && refused.payload.completions == 0 &&
        refused.payload.progress.operations == 2 &&
        refused.overlay_progress.stopped_pc == 0x8007b124u &&
        refused.overlay_progress.callbacks_completed == 0);
  Fixture limit;
  limit.payload.operation_budget = 2;
  CHECK(limit.run() == NBA97_TEXT_IO_REFUSED &&
        limit.payload.result == NBA97_TEXT_LIMIT &&
        limit.payload.progress.stopped_pc == 0x8007b17cu &&
        limit.payload.progress.operations == 2 &&
        limit.adapter.result == NBA97_TEXT_LIMIT);

  Fixture reused;
  CHECK(reused.run() == NBA97_TEXT_COMPLETE && reused.payload.invocations == 1);
  const auto prior_operations = reused.payload.progress.operations;
  reused.payload.access_journal = nullptr;
  reused.payload.access_journal_capacity = 1;
  CHECK(reused.run() == NBA97_TEXT_IO_REFUSED &&
        reused.payload.invocations == 1 && reused.payload.completions == 1 &&
        reused.payload.result == NBA97_TEXT_ARGUMENT &&
        reused.payload.progress.operations == prior_operations &&
        reused.adapter.invocations == 0 && reused.adapter.completions == 0);
}

void adapterGuardsReuseAndCapture() {
  Fixture f;
  Nba97FrontendOverlayLoadEvent event{
      0x8007b124u, 0x8007b128u, 0x8007b15cu, 2, 1,
      NBA97_FRONTEND_OVERLAY_LOAD_SITE_8007B124, 3,
      NBA97_FRONTEND_MAIN_PROGRAM_FEONLY};
  auto machine = f.overlay.machine;
  machine.registers.gpr[6] = {1, 15};
  machine.registers.gpr[31] = {0x8007b12cu, 15};
  Nba97GameTextMemory memory{&f.region, 1};
  for (unsigned field = 0; field < 11; ++field) {
    Fixture bad;
    auto bad_event = event;
    auto bad_machine = machine;
    if (field == 0) bad_event.pc ^= 4;
    else if (field == 1) bad_event.delay_slot_pc ^= 4;
    else if (field == 2) bad_event.entry ^= 4;
    else if (field == 3) bad_event.invocation = 2;
    else if (field == 4) bad_event.argument_count = 2;
    else if (field == 5) bad_event.site = NBA97_FRONTEND_OVERLAY_LOAD_SITE_NONE;
    else if (field == 6) bad_event.target_program = NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD;
    else if (field == 7) bad_machine.registers.gpr[31].word ^= 4;
    else if (field == 8) bad_machine.registers.gpr[31].known_mask = 14;
    else if (field == 9) bad_machine.registers.gpr[6].word = 2;
    else bad_machine.registers.gpr[6].known_mask = 14;
    const auto before = bad_machine;
    CHECK(nba97_frontend_load_payload_from_frontend_overlay_load(
              &bad.payload, &memory, &bad_event, &bad_machine) == 0 &&
          bad.payload.invocations == 0 &&
          bad.payload.result == NBA97_TEXT_ARGUMENT);
    for (unsigned reg = 0; reg < 32; ++reg)
      CHECK(bad_machine.registers.gpr[reg].word ==
                before.registers.gpr[reg].word &&
            bad_machine.registers.gpr[reg].known_mask ==
                before.registers.gpr[reg].known_mask);
  }

  CHECK(nba97_frontend_load_payload_from_frontend_overlay_load(
            &f.payload, &memory, &event, &machine) == 1 &&
        f.payload.invocations == 1 && f.payload.completions == 1);
  const auto prior = f.payload.progress;
  f.payload.access_journal = nullptr;
  f.payload.access_journal_capacity = 1;
  CHECK(nba97_frontend_load_payload_from_frontend_overlay_load(
            &f.payload, &memory, &event, &machine) == 0 &&
        f.payload.invocations == 1 && f.payload.completions == 1 &&
        f.payload.result == NBA97_TEXT_ARGUMENT &&
        f.payload.progress.operations == prior.operations);

  Nba97FrontendLoadPayloadParentContract contract{};
  CHECK(nba97_frontend_load_payload_parent_contract(&contract) == 1 &&
        contract.pc == 0x8007b124u && contract.delay_slot_pc == 0x8007b128u &&
        contract.target == 0x8007b15cu &&
        contract.return_address == 0x8007b12cu &&
        contract.argument_count == 3);

  const std::string receipt = nba97::captureFrontendLoadPayload();
  CHECK(!receipt.empty() && receipt.front() == '{' && receipt.back() == '}');
  for (unsigned char byte : receipt) CHECK(std::isprint(byte));
  CHECK(receipt.find("\"contract_failure\":0") != std::string::npos &&
        receipt.find("\"bytes\":52,\"instructions\":13") !=
            std::string::npos &&
        receipt.find("\"kind\":\"null\"") != std::string::npos &&
        receipt.find("\"kind\":\"nonnull\"") != std::string::npos &&
        receipt.find("\"gameplay_shown\":\"BLOCKED\"") !=
            std::string::npos);
}
} // namespace

int main() {
  try {
    naturalNullAndNonnull();
    absentPlaneRelocationAndFailure();
    adapterGuardsReuseAndCapture();
    std::printf("frontend_load_payload_integration_tests: PASS (%u checks)\n",
                checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
