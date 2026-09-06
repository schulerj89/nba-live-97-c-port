#include "frontend_memory_copy_adapter.h"
#include "frontend_memory_copy_capture.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
using U = std::uint32_t;
unsigned checks;
void checkAt(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr,
                 "frontend memory-copy integration check %u failed at line %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define CHECK(value) checkAt((value), __LINE__)

std::size_t occurrences(const std::string &text, const std::string &needle) {
  std::size_t count = 0;
  std::size_t position = 0;
  while ((position = text.find(needle, position)) != std::string::npos) {
    ++count;
    position += needle.size();
  }
  return count;
}

struct Fixture {
  static constexpr U Base = 0x80000000u;
  static constexpr U Size = 0x200000u;
  static constexpr U EntrySp = 0x801ff000u;
  static constexpr U EntryRa = 0x8007b840u;
  static constexpr U Handle = 0x80140000u;
  static constexpr U Destination = 0x801e0000u;
  static constexpr U LoadSize = 4096u;
  static constexpr U GameEntry = 0x801e1410u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  std::vector<Nba97FrontendMainAccess> main_accesses =
      std::vector<Nba97FrontendMainAccess>(256);
  std::vector<U> main_instructions = std::vector<U>(2048);
  std::vector<Nba97FrontendMemoryCopyAccess> copy_accesses =
      std::vector<Nba97FrontendMemoryCopyAccess>(4096);
  std::vector<U> copy_instructions = std::vector<U>(8192);
  Nba97FrontendMainContext context{};
  Nba97FrontendMainProgress main_progress{};
  Nba97FrontendMemoryCopyBinding binding{};
  std::vector<Nba97FrontendMainEvent> services;
  bool startup_published = false;
  bool loader_returned = false;
  bool size_returned = false;
  bool gameload_refused = false;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {0x22000000u + i * 0x101u, 15};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[NBA97_FRONTEND_MAIN_SP] = {EntrySp, 15};
    context.machine.registers.gpr[NBA97_FRONTEND_MAIN_RA] = {EntryRa, 15};
    context.machine.hi = {0x12345678u, 5};
    context.machine.lo = {0x9abcdef0u, 10};
    context.memory = {&region, 1};
    context.operation_budget = 1000;
    context.io = service;
    context.user = this;
    context.access_journal = main_accesses.data();
    context.access_journal_capacity = main_accesses.size();
    context.instruction_journal = main_instructions.data();
    context.instruction_journal_capacity = main_instructions.size();
    binding.operation_budget = 10000;
    binding.access_journal = copy_accesses.data();
    binding.access_journal_capacity = copy_accesses.size();
    binding.instruction_journal = copy_instructions.data();
    binding.instruction_journal_capacity = copy_instructions.size();

    put(0x80021ee4u, 1);
    put(0x8001edecu, 0, 2);
    put(0x80021568u, 0, 2);
    put(0x80015098u, 0);
    for (U i = 0; i < LoadSize; ++i)
      put(Handle + i, i * 37u + (i >> 5u) + 11u, 1);
    put(Handle, GameEntry);
    for (U i = 0; i < LoadSize; ++i)
      put(Destination + i, 0xa5u, 1);
  }

  void put(U address, U value, unsigned width = 4) {
    CHECK(address >= Base && std::uint64_t(address - Base) + width <= Size);
    for (unsigned i = 0; i < width; ++i)
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
  }
  U get(U address, unsigned width = 4) const {
    CHECK(address >= Base && std::uint64_t(address - Base) + width <= Size);
    U value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= U(bytes[address - Base + i]) << (i * 8u);
    return value;
  }

  static int service(void *opaque, const Nba97GameTextMemory *,
                     const Nba97FrontendMainEvent *event,
                     Nba97FrontendMainMachine *machine,
                     Nba97FrontendMainCalleeOutcome *outcome) {
    auto &f = *static_cast<Fixture *>(opaque);
    if (!event || !machine || !outcome ||
        event->site == NBA97_FRONTEND_MAIN_SITE_80028B54)
      return 0;
    f.services.push_back(*event);
    if (event->site == NBA97_FRONTEND_MAIN_SITE_80028810) {
      f.put(0x80015098u, 1);
      f.startup_published = true;
    } else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028880 ||
               event->site == NBA97_FRONTEND_MAIN_SITE_80028974) {
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {0x80130000u, 15};
    } else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028A7C) {
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {0, 15};
    } else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028ACC) {
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {Handle, 15};
      f.loader_returned = true;
    } else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028AD8) {
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {LoadSize, 15};
      f.size_returned = true;
    } else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028B68) {
      f.gameload_refused = true;
      return 0;
    }
    return 1;
  }

  int run() {
    return nba97_frontend_main_with_recovered_memory_copy(
        &context, &binding, &main_progress);
  }
};

void naturalMainReachesHonestUnboundTransfer() {
  for (bool known_plane : {true, false}) {
    Fixture f;
    if (!known_plane)
      f.region.known = nullptr;
    CHECK(f.run() == NBA97_TEXT_IO_REFUSED && !f.main_progress.completed &&
          f.main_progress.stopped_pc == 0x80028b68u && f.gameload_refused);
    CHECK(f.startup_published && f.loader_returned && f.size_returned &&
          f.binding.invocations == 1 && f.binding.completions == 1 &&
          f.binding.result == NBA97_TEXT_COMPLETE && f.binding.progress.completed);
    CHECK(f.binding.event.pc == 0x80028b54u &&
          f.binding.event.delay_slot_pc == 0x80028b58u &&
          f.binding.event.entry == 0x800909a8u &&
          f.binding.event.argument_count == 3 &&
          f.binding.input_machine.registers.gpr[NBA97_FRONTEND_MAIN_A0].word ==
              Fixture::Handle &&
          f.binding.input_machine.registers.gpr[NBA97_FRONTEND_MAIN_A1].word ==
              Fixture::Destination &&
          f.binding.input_machine.registers.gpr[NBA97_FRONTEND_MAIN_A2].word ==
              Fixture::LoadSize &&
          f.binding.input_machine.registers.gpr[NBA97_FRONTEND_MAIN_RA].word ==
              0x80028b5cu);
    CHECK(f.binding.progress.operations == 2048 &&
          f.binding.progress.reads == 1024 &&
          f.binding.progress.stores == 1024 &&
          f.binding.progress.bytes_read == Fixture::LoadSize &&
          f.binding.progress.bytes_stored == Fixture::LoadSize &&
          f.binding.progress.working_source == Fixture::Handle + Fixture::LoadSize &&
          f.binding.progress.working_destination ==
              Fixture::Destination + Fixture::LoadSize &&
          f.binding.progress.working_count == UINT32_MAX);
    CHECK(f.get(Fixture::Destination) == Fixture::GameEntry &&
          f.main_progress.dynamic_entry.word == Fixture::GameEntry &&
          f.services.back().site == NBA97_FRONTEND_MAIN_SITE_80028B68 &&
          f.services.back().target_program == NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD);
    for (U i = 0; i < Fixture::LoadSize; ++i)
      CHECK(f.bytes[Fixture::Destination - Fixture::Base + i] ==
            f.bytes[Fixture::Handle - Fixture::Base + i]);
    for (unsigned reg : {0u, 3u, 16u, 17u, 18u, 19u, 20u, 21u, 22u, 23u,
                         24u, 25u, 26u, 27u, 28u, 29u, 30u, 31u})
      CHECK(f.binding.progress.machine.registers.gpr[reg].word ==
                f.binding.input_machine.registers.gpr[reg].word &&
            f.binding.progress.machine.registers.gpr[reg].known_mask ==
                f.binding.input_machine.registers.gpr[reg].known_mask);
    CHECK(f.binding.progress.machine.hi.word == f.binding.input_machine.hi.word &&
          f.binding.progress.machine.hi.known_mask ==
              f.binding.input_machine.hi.known_mask &&
          f.binding.progress.machine.lo.word == f.binding.input_machine.lo.word &&
          f.binding.progress.machine.lo.known_mask ==
              f.binding.input_machine.lo.known_mask);
  }
}

void ownerFailureRemainsVisibleAtNaturalSite() {
  Fixture f;
  f.binding.operation_budget = 7;
  CHECK(f.run() == NBA97_TEXT_IO_REFUSED &&
        f.main_progress.stopped_pc == 0x80028b54u &&
        f.binding.invocations == 1 && f.binding.completions == 0 &&
        f.binding.result == NBA97_TEXT_LIMIT &&
        f.binding.progress.operations == 7 &&
        f.binding.progress.stopped_pc == 0x800909e8u &&
        f.binding.progress.machine.registers.gpr[31].word == 0x80028b5cu);
}

void captureReceiptIsDeterministicAndHonest() {
  const std::string first = nba97::captureFrontendMemoryCopy();
  const std::string second = nba97::captureFrontendMemoryCopy();
  if (first != second || first.empty() || first.front() != '{' ||
      first.back() != '}')
    std::fprintf(stderr, "capture1=%s\ncapture2=%s\n", first.c_str(),
                 second.c_str());
  CHECK(first == second && !first.empty() && first.front() == '{' &&
        first.back() == '}');
  for (unsigned char byte : first)
    CHECK(std::isprint(byte));
  CHECK(first.find("\"program\":\"FEONLY\"") != std::string::npos &&
        first.find("\"address\":\"0x800909A8\"") != std::string::npos &&
        first.find("\"contract_failure\":0") != std::string::npos &&
        first.find("\"gameplay_shown\":\"BLOCKED\"") != std::string::npos &&
        first.find("unbound GAMELOAD") != std::string::npos &&
        first.find("synthetic") != std::string::npos);
  CHECK(first.find("\"hash_algorithm\":\"FNV-1a-64\"") !=
            std::string::npos &&
        first.find("\"hash_seed\":\"0xcbf29ce484222325\"") !=
            std::string::npos &&
        first.find("le32 pc,address,logical_address,value; le64 operation; u8 width,known_mask,transfer_mask,kind") !=
            std::string::npos &&
        first.find("170 bytes: gpr0..gpr31,hi,lo; each le32 word then u8 known_mask") !=
            std::string::npos);
  CHECK(first.find("\"operations\":2048,\"access_events\":2048") !=
            std::string::npos &&
        first.find("\"instruction_events\":2329") != std::string::npos &&
        first.find("\"main_result\":-5") != std::string::npos &&
        first.find("\"main_stopped_pc\":2147650408") !=
            std::string::npos &&
        first.find("\"main_stopped_target\":2149454864") !=
            std::string::npos &&
        first.find("\"copy_result\":1,\"copy_completed\":1") !=
            std::string::npos);
  CHECK(first.find("\"source_hash\":\"bba78b2e14826543\"") !=
            std::string::npos &&
        first.find("\"destination_before_hash\":\"f445382fdf747325\"") !=
            std::string::npos &&
        first.find("\"destination_after_hash\":\"bba78b2e14826543\"") !=
            std::string::npos &&
        first.find("\"input_cpu_hash\":\"3549d5ad34747f25\"") !=
            std::string::npos &&
        first.find("\"output_cpu_hash\":\"683dbb60493dd505\"") !=
            std::string::npos &&
        first.find("\"access_hash\":\"9c54c2e996a836ad\"") !=
            std::string::npos &&
        first.find("\"pc_hash\":\"366e5049102a5de6\"") !=
            std::string::npos);
  CHECK(first.find("\"copy_machine_input\":{\"gpr\":[") !=
            std::string::npos &&
        first.find("\"copy_machine_output\":{\"gpr\":[") !=
            std::string::npos &&
        first.find("\"hi\":{\"word\":305419896,\"known_mask\":5}") !=
            std::string::npos &&
        first.find("\"lo\":{\"word\":2596069104,\"known_mask\":10}") !=
            std::string::npos &&
        occurrences(first, "\"known_mask\":") == 68 &&
        first.find("frame_") == std::string::npos);
  CHECK(first.find("startup writes 0x80015098=1") != std::string::npos &&
        first.find("two allocation callbacks set V0=0x80130000") !=
            std::string::npos &&
        first.find("clock callback sets V0=0") != std::string::npos &&
        first.find("loader callback sets V0=0x80140000") !=
            std::string::npos &&
        first.find("size callback sets V0=4096") != std::string::npos &&
        first.find("every other FEONLY service preserves the full CPU and guest RAM") !=
            std::string::npos);
}
}  // namespace

int main() {
  naturalMainReachesHonestUnboundTransfer();
  ownerFailureRemainsVisibleAtNaturalSite();
  captureReceiptIsDeterministicAndHonest();
  std::printf("frontend_memory_copy_integration_tests: PASS (%u checks)\n",
              checks);
  return 0;
}
