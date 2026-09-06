#include "gameload_main_adapter.h"
#include "gameload_main_capture.h"

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
    throw std::runtime_error("gameload-main integration failed at " +
                             std::to_string(line));
}
#define CHECK(value) checkAt((value), __LINE__)

bool same(const Nba97GameloadMainWord &a,
          const Nba97GameloadMainWord &b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}

std::size_t countOccurrences(const std::string &text,
                             const std::string &needle) {
  std::size_t count = 0;
  std::size_t at = 0;
  while ((at = text.find(needle, at)) != std::string::npos) {
    ++count;
    at += needle.size();
  }
  return count;
}

constexpr std::array<U, 9> MainCallPcs{{
    0x801e1374u, 0x801e137cu, 0x801e1384u, 0x801e1394u,
    0x801e13b0u, 0x801e13c4u, 0x801e13ccu, 0x801e13e0u,
    0x801e13f4u}};

struct Fixture {
  static constexpr U Base = 0x80000000u;
  static constexpr U Size = 0x200000u;
  static constexpr U RawFrame = 0x807fffe0u;
  static constexpr U RawSp = 0x807ffff8u;
  static constexpr U CopySize = 4096u;
  static constexpr U Stage = 0x801b0000u;
  static constexpr U Workspace = 0x80015008u;
  static constexpr U Gameonly = 0x80020000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size, 0xa5);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  std::array<std::uint8_t, 24> stack{};
  std::array<std::uint8_t, 24> stack_known{};
  std::array<Nba97GameTextRegion, 2> regions{};
  std::vector<Nba97GameloadEntryAccess> entry_access =
      std::vector<Nba97GameloadEntryAccess>(2100);
  std::vector<U> entry_pcs = std::vector<U>(10500);
  std::array<Nba97GameloadMainAccess, 16> main_access{};
  std::array<U, 64> main_pcs{};
  Nba97GameloadEntryContext entry{};
  Nba97GameloadEntryProgress entry_progress{};
  Nba97GameloadMainBinding main{};
  Nba97GameloadMainAdapterProgress adapter{};
  unsigned initheap_calls = 0;
  unsigned main_calls = 0;
  unsigned refuse_main_site = 0;
  unsigned transfer_direct_site = 0;
  bool gameonly_transfer = false;
  bool gameonly_refuse = false;
  bool mutate_gameonly_machine = false;
  Nba97GameloadMainMachine dynamic_machine{};

  Fixture() {
    stack_known.fill(1);
    regions[0] = {Base, bytes.data(), known.data(), bytes.size()};
    regions[1] = {RawFrame, stack.data(), stack_known.data(), stack.size()};
    for (unsigned i = 0; i < 32; ++i)
      entry.machine.registers.gpr[i] = {0x44000000u + i * 0x101u, 15};
    entry.machine.registers.gpr[0] = {0, 15};
    entry.machine.registers.gpr[31] = {0x80028b70u, 15};
    entry.machine.hi = {0x12345678u, 5};
    entry.machine.lo = {0x9abcdef0u, 10};
    entry.memory = {regions.data(), regions.size()};
    entry.operation_budget = 3000;
    entry.io = entryIo;
    entry.user = this;
    entry.access_journal = entry_access.data();
    entry.access_journal_capacity = entry_access.size();
    entry.instruction_journal = entry_pcs.data();
    entry.instruction_journal_capacity = entry_pcs.size();
    main.operation_budget = 100;
    main.io = mainIo;
    main.user = this;
    main.access_journal = main_access.data();
    main.access_journal_capacity = main_access.size();
    main.instruction_journal = main_pcs.data();
    main.instruction_journal_capacity = main_pcs.size();
    put(0x801e8b70u, 0x00800000u);
    put(0x801e8b6cu, 0x00008000u);
    put(0x80015004u, CopySize);
    for (U i = 0; i < CopySize; ++i)
      put(Workspace + i, i * 37u + 11u, 1);
  }

  bool extent(U address, U count) const {
    return address >= Base && count <= Size && address - Base <= Size - count;
  }

  void put(U address, U value, unsigned width = 4) {
    if (!extent(address, width))
      throw std::runtime_error("integration fixture write outside main RAM");
    for (unsigned i = 0; i < width; ++i)
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
  }

  U get(U address) const {
    if (!extent(address, 4))
      throw std::runtime_error("integration fixture read outside main RAM");
    U value = 0;
    for (unsigned i = 0; i < 4; ++i)
      value |= U(bytes[address - Base + i]) << (i * 8u);
    return value;
  }

  bool copy(U destination, U source, U count) {
    if (!extent(destination, count) || !extent(source, count))
      return false;
    for (U i = 0; i < count; ++i) {
      bytes[destination - Base + i] = bytes[source - Base + i];
      if (regions[0].known)
        known[destination - Base + i] = known[source - Base + i];
    }
    return true;
  }

  static int entryIo(void *opaque, const Nba97GameTextMemory *,
                     const Nba97GameloadEntryEvent *event,
                     Nba97GameloadEntryMachine *,
                     Nba97GameloadEntryCalleeOutcome *outcome) {
    auto &f = *static_cast<Fixture *>(opaque);
    if (!event || !outcome || event->site !=
                                 NBA97_GAMELOAD_ENTRY_SITE_801E1498 ||
        event->pc != 0x801e1498u || event->delay_slot_pc != 0x801e149cu ||
        event->entry != 0x801e1590u || event->operation != 2079 ||
        event->invocation != 1 || event->argument_count != 2 ||
        event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD)
      return 0;
    ++f.initheap_calls;
    *outcome = NBA97_GAMELOAD_ENTRY_CALLEE_RETURNED;
    return 1;
  }

  static int mainIo(void *opaque, const Nba97GameTextMemory *,
                    const Nba97GameloadMainEvent *event,
                    Nba97GameloadMainMachine *machine,
                    Nba97GameloadMainCalleeOutcome *outcome) {
    auto &f = *static_cast<Fixture *>(opaque);
    Nba97GameloadMainSiteContract contract{};
    if (!event || !machine || !outcome ||
        !nba97_gameload_main_site_contract(event->site, &contract) ||
        event->pc != contract.pc ||
        event->delay_slot_pc != contract.delay_slot_pc ||
        event->argument_count != contract.argument_count ||
        event->target_program != contract.target_program ||
        (!contract.dynamic_target && event->entry != contract.target) ||
        (contract.dynamic_target && event->entry != Gameonly) ||
        machine->registers.gpr[NBA97_GAMELOAD_MAIN_RA].word != event->pc + 8u ||
        machine->registers.gpr[NBA97_GAMELOAD_MAIN_RA].known_mask != 15)
      return 0;
    ++f.main_calls;
    if (f.refuse_main_site == event->site)
      return 0;
    if (f.transfer_direct_site == event->site)
      *outcome = NBA97_GAMELOAD_MAIN_CALLEE_TRANSFERRED;
    if (event->site == NBA97_GAMELOAD_MAIN_SITE_801E1374) {
      f.put(0x80015098u, 1);
    } else if (event->site == NBA97_GAMELOAD_MAIN_SITE_801E13B0 ||
               event->site == NBA97_GAMELOAD_MAIN_SITE_801E13E0) {
      if (machine->registers.gpr[NBA97_GAMELOAD_MAIN_A2].word != CopySize ||
          !f.copy(machine->registers.gpr[NBA97_GAMELOAD_MAIN_A0].word,
                  machine->registers.gpr[NBA97_GAMELOAD_MAIN_A1].word,
                  CopySize))
        return 0;
    } else if (event->site == NBA97_GAMELOAD_MAIN_SITE_801E13C4) {
      if (machine->registers.gpr[NBA97_GAMELOAD_MAIN_A0].word != 0x801e0060u ||
          machine->registers.gpr[NBA97_GAMELOAD_MAIN_A1].word != 0x80015000u)
        return 0;
      for (U i = 0; i < CopySize; ++i)
        f.put(0x80015000u + i, i * 19u + 3u, 1);
      f.put(0x80015000u, Gameonly);
      f.put(0x80015004u, CopySize);
    } else if (event->site == NBA97_GAMELOAD_MAIN_SITE_801E13F4) {
      if (f.gameonly_refuse)
        return 0;
      if (f.mutate_gameonly_machine) {
        for (unsigned i = 1; i < 32; ++i)
          machine->registers.gpr[i] = {
              0xa0000000u + i, static_cast<std::uint8_t>(i & 15u)};
        machine->registers.gpr[NBA97_GAMELOAD_MAIN_SP] = {RawFrame, 15};
        machine->hi = {0xaabbccddu, 6};
        machine->lo = {0x11223344u, 9};
        f.dynamic_machine = *machine;
      }
      if (f.gameonly_transfer)
        *outcome = NBA97_GAMELOAD_MAIN_CALLEE_TRANSFERRED;
    }
    return 1;
  }

  int run() {
    return nba97_gameload_entry_with_recovered_main(
        &entry, &main, &entry_progress, &adapter);
  }
};

void naturalReturnedToBreak() {
  for (bool plane : {true, false}) {
    Fixture f;
    if (!plane) {
      f.regions[0].known = nullptr;
      f.regions[1].known = nullptr;
    }
    CHECK(f.run() == NBA97_GAMELOAD_ENTRY_BREAK_TRAP &&
          f.entry_progress.trapped && !f.entry_progress.completed &&
          f.entry_progress.stopped_pc == 0x801e14b4u &&
          f.entry_progress.operations == 2081 && f.initheap_calls == 1 &&
          f.main.invocations == 1 && f.main.completions == 1 &&
          f.main.result == NBA97_TEXT_COMPLETE && f.main_calls == 9 &&
          f.main.progress.completed && !f.main.progress.transferred &&
          f.main.progress.instruction_count == 41 &&
          f.main.progress.operations == 15);
    CHECK(f.adapter.invocations == 1 && f.adapter.completions == 1 &&
          f.adapter.result == NBA97_TEXT_COMPLETE &&
          f.main.parent_event.pc == 0x801e14acu &&
          f.main.parent_event.delay_slot_pc == 0x801e14b0u &&
          f.main.parent_event.entry == 0x801e136cu &&
          f.main.parent_event.operation == 2081 &&
          f.main.parent_event.invocation == 1 &&
          f.main.parent_event.argument_count == 0 &&
          f.main.parent_machine.registers.gpr[31].word == 0x801e14b4u &&
          f.main.parent_machine.registers.gpr[29].word == Fixture::RawSp);
    CHECK(f.main.progress.frame_stack_pointer == Fixture::RawFrame &&
          f.main.progress.machine.registers.gpr[29].word == Fixture::RawSp &&
          f.main.progress.restored_return_address.word == 0x801e14b4u &&
          f.get(0x80015098u) == 1 &&
          f.get(0x80015000u) == Fixture::Gameonly);
  }
}

void transferredMachinePropagation() {
  Fixture f;
  f.gameonly_transfer = true;
  f.mutate_gameonly_machine = true;
  CHECK(f.run() == NBA97_TEXT_COMPLETE && f.entry_progress.completed &&
        f.entry_progress.transferred && f.main.progress.completed &&
        f.main.progress.transferred && f.main.completions == 1 &&
        f.adapter.completions == 1);
  for (unsigned i = 0; i < 32; ++i) {
    CHECK(f.entry_progress.machine.registers.gpr[i].word ==
              f.main.progress.machine.registers.gpr[i].word &&
          f.entry_progress.machine.registers.gpr[i].known_mask ==
              f.main.progress.machine.registers.gpr[i].known_mask);
    CHECK(f.entry_progress.machine.registers.gpr[i].word ==
              f.dynamic_machine.registers.gpr[i].word &&
          f.entry_progress.machine.registers.gpr[i].known_mask ==
              f.dynamic_machine.registers.gpr[i].known_mask);
  }
  CHECK(same(f.main.progress.machine.hi, f.dynamic_machine.hi) &&
        same(f.main.progress.machine.lo, f.dynamic_machine.lo) &&
        f.entry_progress.machine.hi.word == f.dynamic_machine.hi.word &&
        f.entry_progress.machine.hi.known_mask ==
            f.dynamic_machine.hi.known_mask &&
        f.entry_progress.machine.lo.word == f.dynamic_machine.lo.word &&
        f.entry_progress.machine.lo.known_mask ==
            f.dynamic_machine.lo.known_mask);
}

void returnedMachinePropagation() {
  Fixture f;
  f.mutate_gameonly_machine = true;
  CHECK(f.run() == NBA97_GAMELOAD_ENTRY_BREAK_TRAP &&
        !f.main.progress.transferred && f.main.progress.completed &&
        f.main.progress.machine.registers.gpr[NBA97_GAMELOAD_MAIN_SP].word ==
            Fixture::RawSp &&
        f.main.progress.machine.registers.gpr[NBA97_GAMELOAD_MAIN_RA].word ==
            0x801e14b4u);
  for (unsigned i = 1; i < 32; ++i)
    if (i != NBA97_GAMELOAD_MAIN_S0 && i != NBA97_GAMELOAD_MAIN_SP &&
        i != NBA97_GAMELOAD_MAIN_RA)
      CHECK(f.entry_progress.machine.registers.gpr[i].word ==
                f.dynamic_machine.registers.gpr[i].word &&
            f.entry_progress.machine.registers.gpr[i].known_mask ==
                f.dynamic_machine.registers.gpr[i].known_mask);
  CHECK(f.entry_progress.machine.hi.word == f.dynamic_machine.hi.word &&
        f.entry_progress.machine.hi.known_mask ==
            f.dynamic_machine.hi.known_mask &&
        f.entry_progress.machine.lo.word == f.dynamic_machine.lo.word &&
        f.entry_progress.machine.lo.known_mask ==
            f.dynamic_machine.lo.known_mask);
}

void childFailuresAndDirectTransfer() {
  for (unsigned site = 1; site < NBA97_GAMELOAD_MAIN_SITE_COUNT; ++site) {
    Fixture f;
    f.refuse_main_site = site;
    CHECK(f.run() == NBA97_TEXT_IO_REFUSED &&
          f.entry_progress.stopped_pc == 0x801e14acu &&
          f.main.result == NBA97_TEXT_IO_REFUSED &&
          f.main.progress.stopped_pc == MainCallPcs[site - 1u] &&
          f.main.invocations == 1 && f.main.completions == 0 &&
          f.adapter.invocations == 1 && f.adapter.completions == 0);
  }
  for (unsigned site = 1; site < NBA97_GAMELOAD_MAIN_SITE_801E13F4; ++site) {
    Fixture f;
    f.transfer_direct_site = site;
    CHECK(f.run() == NBA97_TEXT_IO_REFUSED &&
          f.main.result == NBA97_TEXT_ARGUMENT &&
          f.entry_progress.stopped_pc == 0x801e14acu &&
          f.main.progress.call_attempts[site] == 1 &&
          f.main.progress.call_count[site] == 0);
  }
}

void adapterGuardsContractsAndReuse() {
  Fixture f;
  Nba97GameloadEntryEvent event{
      0x801e14acu, 0x801e14b0u, 0x801e136cu, 2081, 1,
      NBA97_GAMELOAD_ENTRY_SITE_801E14AC, 0,
      NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD};
  auto machine = f.entry.machine;
  machine.registers.gpr[31] = {0x801e14b4u, 15};
  Nba97GameloadEntryCalleeOutcome outcome =
      NBA97_GAMELOAD_ENTRY_CALLEE_RETURNED;
  for (unsigned field = 0; field < 12; ++field) {
    auto bad = event;
    auto bad_machine = machine;
    if (field == 0) bad.pc ^= 4;
    else if (field == 1) bad.delay_slot_pc ^= 4;
    else if (field == 2) bad.entry ^= 4;
    else if (field == 3) ++bad.operation;
    else if (field == 4) ++bad.invocation;
    else if (field == 5) bad.site = NBA97_GAMELOAD_ENTRY_SITE_NONE;
    else if (field == 6) bad.argument_count = 1;
    else if (field == 7) bad.target_program = NBA97_FRONTEND_MAIN_PROGRAM_FEONLY;
    else if (field == 8) bad_machine.registers.gpr[31].word ^= 4;
    else if (field == 9) bad_machine.registers.gpr[31].known_mask = 14;
    else if (field == 10) bad_machine.registers.gpr[8].known_mask = 16;
    else bad_machine.registers.gpr[0].word = 1;
    const auto before = bad_machine;
    CHECK(nba97_gameload_main_from_entry(&f.main, &f.entry.memory, &bad,
                                          &bad_machine, &outcome) == 0 &&
          f.main.invocations == 0 && f.main.result == NBA97_TEXT_ARGUMENT);
    for (unsigned i = 0; i < 32; ++i)
      CHECK(bad_machine.registers.gpr[i].word ==
                before.registers.gpr[i].word &&
            bad_machine.registers.gpr[i].known_mask ==
                before.registers.gpr[i].known_mask);
  }
  Nba97GameloadMainParentContract parent{};
  CHECK(nba97_gameload_main_parent_contract(&parent) == 1 &&
        parent.pc == event.pc && parent.delay_slot_pc == event.delay_slot_pc &&
        parent.target == event.entry && parent.return_address == 0x801e14b4u &&
        parent.operation == 2081 && parent.invocation == 1 &&
        parent.site == event.site && parent.argument_count == 0 &&
        parent.target_program == NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD);
  CHECK(nba97_gameload_main_parent_contract(nullptr) == 0);
  for (unsigned site = 1; site < NBA97_GAMELOAD_MAIN_SITE_COUNT; ++site) {
    Nba97GameloadMainSiteContract contract{};
    CHECK(nba97_gameload_main_site_contract(
              static_cast<std::uint8_t>(site), &contract) == 1 &&
          contract.pc != 0 && contract.delay_slot_pc == contract.pc + 4u &&
          contract.dynamic_target == (site == NBA97_GAMELOAD_MAIN_SITE_801E13F4));
  }
  Nba97GameloadMainSiteContract contract{};
  CHECK(nba97_gameload_main_site_contract(0, &contract) == 0 &&
        nba97_gameload_main_site_contract(NBA97_GAMELOAD_MAIN_SITE_COUNT,
                                           &contract) == 0 &&
        nba97_gameload_main_site_contract(1, nullptr) == 0);

  Fixture reused;
  reused.gameonly_refuse = true;
  CHECK(reused.run() == NBA97_TEXT_IO_REFUSED && reused.main.invocations == 1);
  const auto prior = reused.main.progress.operations;
  reused.main.access_journal = nullptr;
  reused.main.access_journal_capacity = 1;
  CHECK(reused.run() == NBA97_TEXT_IO_REFUSED && reused.main.invocations == 1 &&
        reused.main.progress.operations == prior && reused.adapter.invocations == 0 &&
        reused.main.result == NBA97_TEXT_ARGUMENT);
}

void captureSmoke() {
  const std::string first = nba97::captureGameloadMain();
  const std::string second = nba97::captureGameloadMain();
  CHECK(first == second && !first.empty() && first.front() == '{' &&
        first.back() == '}');
  bool printable = true;
  for (unsigned char byte : first)
    printable = printable && std::isprint(byte) != 0;
  CHECK(printable);
  CHECK(first.find("\"routine\":\"GAMELOAD:801E136C-801E140F\"") !=
            std::string::npos &&
        first.find("a2d2a4b742c47b1c72d89e7c8b2ddbada0fee604cef947e11914515653e82398") !=
            std::string::npos &&
        first.find("\"result\":-5,\"completed\":0,\"transferred\":0") !=
            std::string::npos &&
        first.find("\"gameplay_shown\":\"BLOCKED\"") != std::string::npos &&
        first.find("\"stopped_pc\":2149454836") != std::string::npos &&
        first.find("\"operations\":13,\"accesses\":4") !=
            std::string::npos &&
        first.find("\"callbacks_completed\":8,\"call_attempts\":9") !=
            std::string::npos &&
        first.find("\"instruction_count\":36") != std::string::npos &&
        first.find("\"copies\":2") != std::string::npos &&
        first.find("\"staged_checksum\":2633698756") != std::string::npos &&
        first.find("\"loaded_checksum\":3725267423") != std::string::npos &&
        first.find("\"restored_checksum\":2633698756") !=
            std::string::npos &&
        first.find("\"call_overflow\":0") != std::string::npos &&
        first.find("\"contract_failure\":0") != std::string::npos &&
        first.find("34*(u32 word,u8 known)=170 bytes") != std::string::npos &&
        first.find("\"input_machine\":\"b7445f74d378ab6d\"") !=
            std::string::npos &&
        first.find("\"output_machine\":\"3caf63f127be5718\"") !=
            std::string::npos &&
        first.find("\"accesses\":\"c950ec5fd8ef15f5\"") !=
            std::string::npos &&
        first.find("\"pcs\":\"9bf1f2acbe8aaf75\"") != std::string::npos &&
        first.find("\"calls\":\"5fb80388d5d596eb\"") !=
            std::string::npos &&
        first.find("\"input_machine\":{") != std::string::npos &&
        first.find("\"output_machine\":{") != std::string::npos);
  CHECK(countOccurrences(first, ",\"machine\":{\"gpr\":[") == 9 &&
        first.find("\"call_sequence\":[{\"pc\":2149454708,\"delay\":2149454712,\"target\":2149455032,\"operation\":3,\"invocation\":1,\"site\":1,\"argc\":0,\"program\":1,\"machine\":{") !=
            std::string::npos &&
        first.find("\"pc\":2149454836,\"delay\":2149454840,\"target\":2147614720,\"operation\":13,\"invocation\":1,\"site\":9,\"argc\":0,\"program\":2,\"machine\":{") !=
            std::string::npos);
  CHECK(first.find("\"next_unbound_boundary\":{\"first_production\":\"801E1374->801E14B8 startup\"") !=
            std::string::npos &&
        first.find("801E13C4->801E1300 GAMEONLY loader") !=
            std::string::npos &&
        first.find("801E13F4->80020000 refused synthetic GAMEONLY") !=
            std::string::npos &&
        first.find("\"memory\":\"synthetic standalone 2 MiB main memory") !=
            std::string::npos);
}
} // namespace

int main() {
  try {
    naturalReturnedToBreak();
    transferredMachinePropagation();
    returnedMachinePropagation();
    childFailuresAndDirectTransfer();
    adapterGuardsContractsAndReuse();
    captureSmoke();
    std::printf("gameload_main_integration_tests: PASS (%u checks)\n", checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
