#include "gameload_entry_adapter.h"
#include "gameload_entry_capture.h"

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
    throw std::runtime_error("gameload-entry integration failed at " +
                             std::to_string(line));
}
#define CHECK(value) checkAt((value), __LINE__)

struct Fixture {
  static constexpr U Base = 0x80000000u;
  static constexpr U Size = 0x200000u;
  static constexpr U EntrySp = 0x801ff000u;
  static constexpr U Handle = 0x80140000u;
  static constexpr U Destination = 0x801e0000u;
  static constexpr U LoadSize = 4096u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  std::vector<Nba97FrontendMainAccess> main_access =
      std::vector<Nba97FrontendMainAccess>(256);
  std::vector<U> main_pc = std::vector<U>(2048);
  std::vector<Nba97FrontendMemoryCopyAccess> copy_access =
      std::vector<Nba97FrontendMemoryCopyAccess>(4096);
  std::vector<U> copy_pc = std::vector<U>(8192);
  std::vector<Nba97GameloadEntryAccess> entry_access =
      std::vector<Nba97GameloadEntryAccess>(2100);
  std::vector<U> entry_pc = std::vector<U>(10500);
  Nba97FrontendMainContext main{};
  Nba97FrontendMainProgress main_progress{};
  Nba97FrontendMemoryCopyBinding copy{};
  Nba97GameloadEntryBinding entry{};
  Nba97GameloadEntryAdapterProgress adapter{};
  unsigned children = 0;
  bool refuse_first_child = false;
  bool transfer_second_child = false;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      main.machine.registers.gpr[i] = {0x22000000u + i * 0x101u, 15};
    main.machine.registers.gpr[0] = {0, 15};
    main.machine.registers.gpr[NBA97_FRONTEND_MAIN_SP] = {EntrySp, 15};
    main.machine.registers.gpr[NBA97_FRONTEND_MAIN_RA] = {0x8007b840u, 15};
    main.machine.hi = {0x12345678u, 5};
    main.machine.lo = {0x9abcdef0u, 10};
    main.memory = {&region, 1};
    main.operation_budget = 1000;
    main.io = frontendService;
    main.user = this;
    main.access_journal = main_access.data();
    main.access_journal_capacity = main_access.size();
    main.instruction_journal = main_pc.data();
    main.instruction_journal_capacity = main_pc.size();
    copy.operation_budget = 10000;
    copy.access_journal = copy_access.data();
    copy.access_journal_capacity = copy_access.size();
    copy.instruction_journal = copy_pc.data();
    copy.instruction_journal_capacity = copy_pc.size();
    entry.operation_budget = 2081;
    entry.io = entryService;
    entry.user = this;
    entry.access_journal = entry_access.data();
    entry.access_journal_capacity = entry_access.size();
    entry.instruction_journal = entry_pc.data();
    entry.instruction_journal_capacity = entry_pc.size();
    put(0x80021ee4u, 1);
    put(0x8001edecu, 0, 2);
    put(0x80021568u, 0, 2);
    put(0x80015098u, 0);
    put(0x801e8b70u, 0x00800000u);
    put(0x801e8b6cu, 0x00008000u);
    for (U i = 0; i < LoadSize; ++i)
      put(Handle + i, i * 37u + (i >> 5u) + 11u, 1);
    put(Handle, 0x801e1410u);
    for (U i = 0; i < LoadSize; ++i) put(Destination + i, 0xa5u, 1);
  }

  void put(U address, U value, unsigned width = 4) {
    if (address < Base || std::uint64_t(address - Base) + width > Size)
      throw std::runtime_error("integration fixture write outside RAM");
    for (unsigned i = 0; i < width; ++i)
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
  }
  U get(U address) const {
    if (address < Base || address - Base > Size - 4)
      throw std::runtime_error("integration fixture read outside RAM");
    U value = 0;
    for (unsigned i = 0; i < 4; ++i)
      value |= U(bytes[address - Base + i]) << (i * 8u);
    return value;
  }

  static int frontendService(void *opaque, const Nba97GameTextMemory *,
                             const Nba97FrontendMainEvent *event,
                             Nba97FrontendMainMachine *machine,
                             Nba97FrontendMainCalleeOutcome *) {
    auto &f = *static_cast<Fixture *>(opaque);
    if (!event || !machine || event->site == NBA97_FRONTEND_MAIN_SITE_80028B54 ||
        event->site == NBA97_FRONTEND_MAIN_SITE_80028B68)
      return 0;
    if (event->site == NBA97_FRONTEND_MAIN_SITE_80028810)
      f.put(0x80015098u, 1);
    else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028880 ||
             event->site == NBA97_FRONTEND_MAIN_SITE_80028974)
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {0x80130000u, 15};
    else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028A7C)
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {0, 15};
    else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028ACC)
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {Handle, 15};
    else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028AD8)
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {LoadSize, 15};
    return 1;
  }

  static int entryService(void *opaque, const Nba97GameTextMemory *,
                          const Nba97GameloadEntryEvent *event,
                          Nba97GameloadEntryMachine *,
                          Nba97GameloadEntryCalleeOutcome *outcome) {
    auto &f = *static_cast<Fixture *>(opaque);
    if (!event || !outcome) return 0;
    ++f.children;
    *outcome = f.transfer_second_child && f.children == 2 ? NBA97_GAMELOAD_ENTRY_CALLEE_TRANSFERRED : NBA97_GAMELOAD_ENTRY_CALLEE_RETURNED;
    return f.refuse_first_child ? 0 : 1;
  }

  int run() {
    return nba97_frontend_main_with_recovered_memory_copy_and_gameload(
        &main, &copy, &entry, &main_progress, &adapter);
  }
};

void naturalMainCopyAndBreak() {
  for (bool plane : {true, false}) {
    Fixture f;
    if (!plane) f.region.known = nullptr;
    CHECK(f.run() == NBA97_TEXT_IO_REFUSED &&
          f.main_progress.stopped_pc == 0x80028b68u &&
          f.main_progress.stopped_target == 0x801e1410u &&
          f.copy.invocations == 1 && f.copy.completions == 1 &&
          f.copy.result == NBA97_TEXT_COMPLETE &&
          f.entry.invocations == 1 && f.entry.completions == 0 &&
          f.entry.result == NBA97_GAMELOAD_ENTRY_BREAK_TRAP &&
          f.adapter.invocations == 1 && f.adapter.completions == 0 &&
          f.adapter.result == NBA97_GAMELOAD_ENTRY_BREAK_TRAP);
    CHECK(f.get(Fixture::Destination) == 0x801e1410u &&
          f.main_progress.dynamic_entry.word == 0x801e1410u &&
          f.entry.parent_event.pc == 0x80028b68u &&
          f.entry.parent_event.delay_slot_pc == 0x80028b6cu &&
          f.entry.parent_event.entry == 0x801e1410u &&
          f.entry.parent_machine.registers.gpr[31].word == 0x80028b70u);
    CHECK(f.entry.progress.operations == 2081 &&
          f.entry.progress.words_cleared == 2073 && f.children == 2 &&
          f.entry.progress.stopped_pc == 0x801e14b4u &&
          f.get(0x801e8b4cu) == 0x801eb0a0u &&
          f.get(0x801e8b50u) == 0x0060cf58u);
  }
}

void refusalAndAdapterGuards() {
  Fixture refused;
  refused.refuse_first_child = true;
  CHECK(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.entry.result == NBA97_TEXT_IO_REFUSED &&
        refused.entry.progress.operations == 2079 &&
        refused.entry.progress.callbacks_completed == 0 &&
        refused.main_progress.stopped_pc == 0x80028b68u);

  Fixture direct;
  Nba97FrontendMainEvent event{0x80028b68u, 0x80028b6cu, 0x801e1410u,
                               55, 1, NBA97_FRONTEND_MAIN_SITE_80028B68, 0,
                               NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD};
  auto machine = direct.main.machine;
  machine.registers.gpr[31] = {0x80028b70u, 15};
  Nba97FrontendMainCalleeOutcome outcome = NBA97_FRONTEND_MAIN_CALLEE_RETURNED;
  for (unsigned field = 0; field < 9; ++field) {
    auto bad = event;
    auto bad_machine = machine;
    if (field == 0) bad.pc ^= 4;
    else if (field == 1) bad.delay_slot_pc ^= 4;
    else if (field == 2) bad.entry ^= 4;
    else if (field == 3) bad.invocation = 2;
    else if (field == 4) bad.site = NBA97_FRONTEND_MAIN_SITE_NONE;
    else if (field == 5) bad.target_program = NBA97_FRONTEND_MAIN_PROGRAM_FEONLY;
    else if (field == 6) bad_machine.registers.gpr[31].word ^= 4;
    else if (field == 7) bad_machine.registers.gpr[31].known_mask = 14;
    else bad.argument_count = 1;
    CHECK(nba97_gameload_entry_from_frontend_main(
              &direct.entry, &direct.main.memory, &bad, &bad_machine,
              &outcome) == 0 &&
          direct.entry.invocations == 0 &&
          direct.entry.result == NBA97_TEXT_ARGUMENT);
  }
  Nba97GameloadEntryParentContract contract{};
  CHECK(nba97_gameload_entry_parent_contract(&contract) == 1 &&
        contract.pc == 0x80028b68u && contract.delay_slot_pc == 0x80028b6cu &&
        contract.target == 0x801e1410u &&
        contract.return_address == 0x80028b70u &&
        contract.argument_count == 0 &&
        contract.target_program == NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD);

  Fixture stale;
  CHECK(stale.run() == NBA97_TEXT_IO_REFUSED && stale.entry.invocations == 1);
  const auto prior = stale.entry.progress.operations;
  stale.entry.access_journal = nullptr;
  stale.entry.access_journal_capacity = 1;
  CHECK(stale.run() == NBA97_TEXT_IO_REFUSED && stale.entry.invocations == 1 &&
        stale.entry.progress.operations == prior &&
        stale.adapter.invocations == 0 && stale.adapter.completions == 0 &&
        stale.entry.result == NBA97_TEXT_ARGUMENT);
}

void transferredComposition() {
  Fixture f;
  f.transfer_second_child = true;
  CHECK(f.run() == NBA97_TEXT_COMPLETE && f.main_progress.transferred &&
        f.entry.completions == 1 && f.adapter.completions == 1 &&
        f.entry.progress.transferred && !f.entry.progress.trapped);
  for (unsigned i=0; i<32; ++i) {
    CHECK(f.main_progress.machine.registers.gpr[i].word == f.entry.progress.machine.registers.gpr[i].word);
    CHECK(f.main_progress.machine.registers.gpr[i].known_mask == f.entry.progress.machine.registers.gpr[i].known_mask);
  }
  CHECK(f.main_progress.machine.hi.word == f.entry.progress.machine.hi.word &&
        f.main_progress.machine.hi.known_mask == f.entry.progress.machine.hi.known_mask &&
        f.main_progress.machine.lo.word == f.entry.progress.machine.lo.word &&
        f.main_progress.machine.lo.known_mask == f.entry.progress.machine.lo.known_mask);
}

void captureSmoke() {
  const std::string first = nba97::captureGameloadEntry();
  const std::string second = nba97::captureGameloadEntry();
  CHECK(first == second && !first.empty() && first.front() == '{' &&
        first.back() == '}');
  for (unsigned char byte : first) CHECK(std::isprint(byte));
  CHECK(first.find("\"program\":\"GAMELOAD\"") != std::string::npos &&
        first.find("\"bytes\":168,\"instructions\":42") !=
            std::string::npos &&
        first.find("\"contract_failure\":0") != std::string::npos &&
        first.find("\"operations\":2081") != std::string::npos &&
        first.find("\"accesses\":2079") != std::string::npos &&
        first.find("\"pc_events\":10402") != std::string::npos &&
        first.find("\"call_sequence\":[") != std::string::npos &&
        first.find("Unbound full-machine child fixtures") !=
            std::string::npos &&
        first.find("\"gameplay_shown\":\"BLOCKED\"") !=
            std::string::npos);
}
} // namespace

int main() {
  try {
    naturalMainCopyAndBreak();
    refusalAndAdapterGuards();
    transferredComposition();
    captureSmoke();
    std::printf("gameload_entry_integration_tests: PASS (%u checks)\n", checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
