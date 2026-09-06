#include "frontend_resource_lookup_adapter.h"
#include "frontend_resource_lookup_capture.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using U = std::uint32_t;
unsigned checks;
void check(bool value, int line) {
  ++checks;
  if (!value)
    throw std::runtime_error("frontend-resource-lookup integration line " +
                             std::to_string(line));
}
#define CHECK(value) check((value), __LINE__)
constexpr U Base = 0x80000000u;
constexpr U Size = 0x200000u;
constexpr U Sp = 0x801f0000u;
constexpr U Name = 0x80024854u;
constexpr U Descriptor = 0x80110000u;
constexpr U Allocation = 0x80130000u;
constexpr U Source = 0x80120000u;
constexpr U Destination = 0x80140000u;

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  bool secondary = false;
  bool refuseLookup = false;
  bool relocate = false;
  unsigned dfCalls = 0;
  unsigned diCalls = 0;

  Fixture() {
    put(Descriptor, Source);
    put(Descriptor + 20, 8);
    put(Descriptor + 24, 8);
    put(Allocation, Destination);
    put(0x800c7374u, 0x80150000u);
    put(0x80150010u, 0x100u);
    put(0x800f8db0u, 0);
    for (unsigned i = 0; i < 8; ++i) {
      bytes[Source - Base + i] = std::uint8_t(0xa0u + i);
      known[Source - Base + i] = std::uint8_t(i & 1u);
    }
    put(0x800d9b50u, 0);
  }

  void put(U address, U value, unsigned mask = 15) {
    if (address < Base || address - Base > bytes.size() - 4)
      throw std::runtime_error("integration fixture write outside RAM");
    for (unsigned i = 0; i < 4; ++i) {
      bytes[address - Base + i] = std::uint8_t(value >> (8 * i));
      if (region.known)
        known[address - Base + i] = std::uint8_t((mask >> i) & 1u);
    }
  }

  static int dfIo(void *opaque, const Nba97GameTextMemory *,
                  const Nba97FrontendResourceLoadEvent *event,
                  Nba97FrontendResourceLoadMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    ++fixture.dfCalls;
    if (event->site == NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B214) {
      U sp = machine->registers.gpr[29].word;
      fixture.put(sp + 24, 0x44);
      fixture.put(sp + 32, 0);
    }
    return 1;
  }

  static int diIo(void *opaque, const Nba97GameTextMemory *,
                  const Nba97FrontendResourceLookupEvent *event,
                  Nba97FrontendResourceLookupMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    ++fixture.diCalls;
    if (fixture.refuseLookup)
      return 0;
    if (event->site == NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A2E0)
      machine->registers.gpr[2] = {fixture.secondary ? 0u : Descriptor, 15};
    if (event->site == NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A2E0 &&
        fixture.relocate) {
      U source = machine->registers.gpr[29].word;
      U destination = 0x801ed000u;
      if (source < Base || source - Base > fixture.bytes.size() - 104 ||
          destination - Base > fixture.bytes.size() - 104)
        return 0;
      for (unsigned i = 0; i < 104; ++i) {
        fixture.bytes[destination - Base + i] =
            fixture.bytes[source - Base + i];
        if (fixture.region.known)
          fixture.known[destination - Base + i] =
              fixture.known[source - Base + i];
      }
      machine->registers.gpr[29] = {destination, 15};
    }
    if (event->site == NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A36C)
      machine->registers.gpr[2] = {Descriptor, 15};
    if (event->site == NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A314 ||
        event->site == NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A3C4)
      machine->registers.gpr[2] = {Allocation, 15};
    return 1;
  }
};

struct Run {
  Fixture fixture;
  Nba97FrontendResourceLoadContext context{};
  Nba97FrontendResourceLoadProgress progress{};
  Nba97FrontendResourceLookupBinding binding{};
  Nba97FrontendResourceLookupAdapterProgress adapter{};
  std::array<Nba97FrontendResourceLookupAccess, 128> diAccess{};
  std::array<U, 256> diPc{};
  std::array<Nba97FrontendMemoryCopyAccess, 64> copyAccess[2]{};
  std::array<U, 128> copyPc[2]{};

  Run() {
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {0x62000000u + i * 0x101u, 15};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[4] = {Name, 15};
    context.machine.registers.gpr[5] = {0, 15};
    context.machine.registers.gpr[6] = {1, 15};
    context.machine.registers.gpr[29] = {Sp, 15};
    context.machine.registers.gpr[31] = {0x8007b16cu, 15};
    context.machine.hi = {0x13579bdfu, 5};
    context.machine.lo = {0x2468ace0u, 10};
    context.memory = {&fixture.region, 1};
    context.operation_budget = 64;
    context.io = Fixture::dfIo;
    context.user = &fixture;
    binding.operation_budget = 128;
    binding.io = Fixture::diIo;
    binding.user = &fixture;
    binding.access_journal = diAccess.data();
    binding.access_journal_capacity = diAccess.size();
    binding.instruction_journal = diPc.data();
    binding.instruction_journal_capacity = diPc.size();
    for (unsigned i = 0; i < 2; ++i) {
      binding.copy[i].operation_budget = 128;
      binding.copy[i].access_journal = copyAccess[i].data();
      binding.copy[i].access_journal_capacity = copyAccess[i].size();
      binding.copy[i].instruction_journal = copyPc[i].data();
      binding.copy[i].instruction_journal_capacity = copyPc[i].size();
    }
  }

  int run() {
    return nba97_frontend_resource_load_with_recovered_lookup(
        &context, &binding, &progress, &adapter);
  }
};

void checkFullReturn(const Run &run) {
  auto expected = run.binding.progress.machine;
  const U childReturnSp = expected.registers.gpr[29].word;
  expected.registers.gpr[2] = {0,15};
  expected.registers.gpr[4] = {0,15};
  expected.registers.gpr[5] = {0,15};
  expected.registers.gpr[6] = {childReturnSp+28,15};
  expected.registers.gpr[7] = {childReturnSp+32,15};
  for (unsigned i=16;i<=19;++i) expected.registers.gpr[i]=run.context.machine.registers.gpr[i];
  expected.registers.gpr[29]={childReturnSp+64,15};
  expected.registers.gpr[31]=run.context.machine.registers.gpr[31];
  for (unsigned i=0;i<32;++i) {
    CHECK(run.progress.machine.registers.gpr[i].word == expected.registers.gpr[i].word);
    CHECK(run.progress.machine.registers.gpr[i].known_mask == expected.registers.gpr[i].known_mask);
  }
  CHECK(run.progress.machine.hi.word == expected.hi.word && run.progress.machine.hi.known_mask == expected.hi.known_mask);
  CHECK(run.progress.machine.lo.word == expected.lo.word && run.progress.machine.lo.known_mask == expected.lo.known_mask);
}

void composedCopies() {
  Run initial;
  CHECK(initial.run() == NBA97_TEXT_COMPLETE && initial.progress.completed &&
        initial.binding.progress.completed && initial.adapter.completions == 1);
  CHECK(initial.binding.parent_event.pc == 0x8007b1f0u &&
        initial.binding.parent_event.delay_slot_pc == 0x8007b1f4u &&
        initial.binding.parent_event.entry == 0x8008a2c8u &&
        initial.binding.parent_machine.registers.gpr[31].word == 0x8007b1f8u);
  CHECK(initial.binding.copy[0].invocations == 1 &&
        initial.binding.copy[0].completions == 1 &&
        initial.binding.copy[0].result == NBA97_TEXT_COMPLETE &&
        initial.binding.copy[0].parent_event.pc == 0x8008a33cu &&
        initial.binding.copy[1].invocations == 0);
  for (unsigned i = 0; i < 8; ++i)
    CHECK(initial.fixture.bytes[Destination - Base + i] ==
              std::uint8_t(0xa0u + i) &&
          initial.fixture.known[Destination - Base + i] ==
              std::uint8_t(i & 1u));
  CHECK(initial.progress.machine.registers.gpr[29].word == Sp &&
        initial.progress.machine.registers.gpr[31].word == 0x8007b16cu &&
        initial.progress.machine.hi.word == 0x13579bdfu &&
        initial.progress.machine.hi.known_mask == 5 &&
        initial.progress.machine.lo.word == 0x2468ace0u &&
        initial.progress.machine.lo.known_mask == 10);

  Run secondary;
  secondary.fixture.secondary = true;
  CHECK(secondary.run() == NBA97_TEXT_COMPLETE &&
        secondary.binding.progress.secondary_path &&
        secondary.binding.copy[1].invocations == 1 &&
        secondary.binding.copy[1].completions == 1 &&
        secondary.binding.copy[0].invocations == 0);
  for (unsigned i = 0; i < 8; ++i)
    CHECK(secondary.fixture.bytes[Destination - Base + i] ==
              std::uint8_t(0xa0u + i) &&
          secondary.fixture.known[Destination - Base + i] ==
              std::uint8_t(i & 1u));

  Run absent;
  absent.fixture.region.known = nullptr;
  CHECK(absent.run() == NBA97_TEXT_COMPLETE &&
        absent.binding.copy[0].completions == 1);
  Run relocated;
  relocated.fixture.relocate = true;
  CHECK(relocated.run() == NBA97_TEXT_COMPLETE &&
        relocated.binding.progress.completed && relocated.progress.completed &&
        relocated.progress.machine.registers.gpr[29].word == 0x801ed068u &&
        relocated.progress.machine.registers.gpr[31].word == 0x8007b16cu &&
        relocated.progress.machine.registers.gpr[16].word == 0x62001010u &&
        relocated.progress.machine.registers.gpr[19].word == 0x62001313u);
  checkFullReturn(initial);
  checkFullReturn(secondary);
  checkFullReturn(absent);
  checkFullReturn(relocated);
  Run limited;
  limited.binding.operation_budget = 5;
  CHECK(limited.run() == NBA97_TEXT_IO_REFUSED &&
        limited.binding.result == NBA97_TEXT_LIMIT &&
        limited.binding.progress.operations == 5 &&
        limited.progress.stopped_pc == 0x8007b1f0u && limited.binding.progress.stopped_pc == 0x8008a2e0u &&
        !limited.progress.completed);
  Run refused;
  refused.fixture.refuseLookup = true;
  CHECK(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.binding.result == NBA97_TEXT_IO_REFUSED &&
        refused.binding.progress.stopped_pc == 0x8008a2e0u);
  Run copyLimited;
  copyLimited.binding.copy[0].operation_budget = 0;
  CHECK(copyLimited.run() == NBA97_TEXT_IO_REFUSED &&
        copyLimited.binding.copy[0].result == NBA97_TEXT_LIMIT &&
        copyLimited.binding.progress.stopped_pc == 0x8008a33cu &&
        copyLimited.progress.stopped_pc == 0x8007b1f0u);
}

void adapterGuards() {
  Fixture fixture;
  Nba97FrontendResourceLoadEvent event{
      0x8007b1f0u, 0x8007b1f4u,
      0x8008a2c8u, 1,
      1,           NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B1F0,
      1,           NBA97_FRONTEND_MAIN_PROGRAM_FEONLY};
  Nba97FrontendResourceLoadMachine machine{};
  for (auto &word : machine.registers.gpr)
    word = {0, 15};
  machine.registers.gpr[31] = {0x8007b1f8u, 15};
  Nba97GameTextMemory memory{&fixture.region, 1};
  auto rejects = [&](Nba97FrontendResourceLoadEvent badEvent,
                     Nba97FrontendResourceLoadMachine badMachine) {
    Nba97FrontendResourceLookupBinding binding{};
    CHECK(!nba97_frontend_resource_lookup_from_resource_load(
              &binding, &memory, &badEvent, &badMachine) &&
          binding.result == NBA97_TEXT_ARGUMENT && binding.invocations == 0);
  };
  auto bad = event;
  bad.pc ^= 4u;
  rejects(bad, machine);
  bad = event;
  bad.delay_slot_pc ^= 4u;
  rejects(bad, machine);
  bad = event;
  bad.entry ^= 4u;
  rejects(bad, machine);
  bad = event;
  bad.invocation = 2;
  rejects(bad, machine);
  bad = event;
  bad.argument_count = 2;
  rejects(bad, machine);
  bad = event;
  bad.site = NBA97_FRONTEND_RESOURCE_LOAD_SITE_NONE;
  rejects(bad, machine);
  bad = event;
  bad.target_program = NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD;
  rejects(bad, machine);
  auto badMachine = machine;
  badMachine.registers.gpr[31].word ^= 4u;
  rejects(event, badMachine);
  badMachine = machine;
  badMachine.registers.gpr[31].known_mask = 7;
  rejects(event, badMachine);
  badMachine = machine;
  badMachine.registers.gpr[0].word = 1;
  rejects(event, badMachine);

  Nba97FrontendResourceLookupBinding badJournal{};
  badJournal.access_journal_capacity = 1;
  CHECK(!nba97_frontend_resource_lookup_from_resource_load(&badJournal, &memory,
                                                           &event, &machine) &&
        badJournal.result == NBA97_TEXT_ARGUMENT);
}
} // namespace

int main() {
  try {
    composedCopies();
    adapterGuards();
    std::string capture = nba97::captureFrontendResourceLookup();
    CHECK(capture.find("\"contract_failure\":0") != std::string::npos &&
          capture.find("\"instructions\":80") != std::string::npos &&
          capture.find("\"gameplay_shown\":\"BLOCKED\"") != std::string::npos);
    std::printf("frontend_resource_lookup_integration_tests passed %u checks\n",
                checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
