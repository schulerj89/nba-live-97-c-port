#include "frontend_resource_lookup_adapter.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using U = std::uint32_t;
unsigned checks;
std::array<bool, 80> covered{};
void check(bool value, int line) {
  ++checks;
  if (!value)
    throw std::runtime_error("frontend-resource-lookup line " +
                             std::to_string(line));
}
#define CHECK(value) check((value), __LINE__)
constexpr U Base = 0x80000000u;
constexpr U Size = 0x200000u;
constexpr U Sp = 0x801f0000u;
constexpr U Name = 0x80024854u;
constexpr U Descriptor = 0x80110000u;
constexpr U Allocation = 0x80130000u;

struct Seen {
  Nba97FrontendResourceLookupEvent event{};
  Nba97FrontendResourceLookupMachine machine{};
};

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  std::array<std::uint8_t, 4> zeroBytes{};
  std::array<std::uint8_t, 4> zeroKnown{{1, 1, 1, 1}};
  std::array<Nba97GameTextRegion, 2> regions{};
  Nba97FrontendResourceLookupContext context{};
  Nba97FrontendResourceLookupProgress progress{};
  std::array<Nba97FrontendResourceLookupAccess, 128> access{};
  std::array<U, 256> pcs{};
  std::vector<Seen> calls;
  U initial = Descriptor;
  U allocation = Allocation;
  U chainHit = 0;
  std::uint8_t initialMask = 15;
  unsigned chainHitAt = 1;
  unsigned refuseSite = 0;
  bool malformed = false;
  bool mutateLiveSaved = false;
  U relocateFrame = 0;

  Fixture() {
    regions[0] = {Base, bytes.data(), known.data(), bytes.size()};
    regions[1] = {0, zeroBytes.data(), zeroKnown.data(), zeroBytes.size()};
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {0x51000000u + i * 0x101u, 15};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[4] = {Name, 15};
    context.machine.registers.gpr[29] = {Sp, 15};
    context.machine.registers.gpr[31] = {0x8007b1f8u, 15};
    context.machine.hi = {0x12345678u, 5};
    context.machine.lo = {0x9abcdef0u, 10};
    context.memory = {regions.data(), 1};
    context.operation_budget = 128;
    context.io = callback;
    context.user = this;
    context.access_journal = access.data();
    context.access_journal_capacity = access.size();
    context.instruction_journal = pcs.data();
    context.instruction_journal_capacity = pcs.size();
    put(Descriptor, 0x80120000u);
    put(Descriptor + 20, 8);
    put(Descriptor + 24, 8);
    put(Allocation, 0x80140000u);
    put(0x800c7374u, 0x80150000u);
    put(0x80150010u, 0x100u);
    put(0x800f8db0u, 0x200u);
    put(0x800f8dc8u, 0);
  }

  void put(U address, U value, unsigned mask = 15) {
    if (address < Base || address - Base > bytes.size() - 4)
      throw std::runtime_error("fixture write outside RAM");
    for (unsigned i = 0; i < 4; ++i) {
      bytes[address - Base + i] = std::uint8_t(value >> (8 * i));
      if (regions[0].known)
        known[address - Base + i] = std::uint8_t((mask >> i) & 1);
    }
  }

  U get(U address) const {
    if (address < Base || address - Base > bytes.size() - 4)
      throw std::runtime_error("fixture read outside RAM");
    U value = 0;
    for (unsigned i = 0; i < 4; ++i)
      value |= U(bytes[address - Base + i]) << (8 * i);
    return value;
  }

  int run() {
    int result = nba97_frontend_resource_lookup(&context, &progress);
    std::size_t count = std::min(progress.instruction_events, pcs.size());
    for (std::size_t i = 0; i < count; ++i)
      if (pcs[i] >= 0x8008a2c8u && pcs[i] <= 0x8008a404u &&
          ((pcs[i] - 0x8008a2c8u) & 3u) == 0)
        covered[(pcs[i] - 0x8008a2c8u) / 4] = true;
    return result;
  }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97FrontendResourceLookupEvent *event,
                      Nba97FrontendResourceLookupMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    if (!event || !machine)
      return 0;
    fixture.calls.push_back({*event, *machine});
    if (event->site == NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A2E0)
      machine->registers.gpr[2] = {fixture.initial, fixture.initialMask};
    if (event->site == NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A314 ||
        event->site == NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A3C4) {
      machine->registers.gpr[2] = {fixture.allocation, 15};
      if (fixture.relocateFrame) {
        U source = machine->registers.gpr[29].word;
        U destination = fixture.relocateFrame;
        if (source < Base || source - Base > fixture.bytes.size() - 40 ||
            destination < Base ||
            destination - Base > fixture.bytes.size() - 40)
          return 0;
        for (unsigned i = 0; i < 40; ++i) {
          fixture.bytes[destination - Base + i] =
              fixture.bytes[source - Base + i];
          if (fixture.regions[0].known)
            fixture.known[destination - Base + i] =
                fixture.known[source - Base + i];
        }
        machine->registers.gpr[29] = {destination, 15};
      }
    }
    if (event->site == NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A36C) {
      std::size_t attempt = fixture.progress.call_attempts[event->site];
      machine->registers.gpr[2] = {
          attempt == fixture.chainHitAt ? fixture.chainHit : 0, 15};
    }
    if (fixture.mutateLiveSaved &&
        event->site == NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A2E0)
      machine->registers.gpr[18] = {0x80026000u, 15};
    if (fixture.mutateLiveSaved &&
        event->site == NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A33C)
      machine->registers.gpr[17] = {0x80160000u, 15};
    if (fixture.malformed)
      machine->lo.known_mask = 16;
    return event->site != fixture.refuseSite;
  }
};

void contractsAndInitialPaths() {
  const U pcs[] = {0,           0x8008a2e0u, 0x8008a314u, 0x8008a33cu,
                   0x8008a344u, 0x8008a36cu, 0x8008a3c4u, 0x8008a3dcu};
  const U targets[] = {0,           0x8008a0a8u, 0x800771f0u, 0x800909a8u,
                       0x80077638u, 0x80089ffcu, 0x800771f0u, 0x800909a8u};
  const unsigned argc[] = {0, 1, 4, 3, 1, 2, 4, 3};
  for (unsigned site = 1; site < 8; ++site) {
    Nba97FrontendResourceLookupSiteContract contract{};
    CHECK(nba97_frontend_resource_lookup_site_contract(
        static_cast<std::uint8_t>(site), &contract));
    CHECK(contract.pc == pcs[site] && contract.delay_slot_pc == pcs[site] + 4 &&
          contract.target == targets[site] &&
          contract.argument_count == argc[site] &&
          contract.target_program == NBA97_FRONTEND_MAIN_PROGRAM_FEONLY);
  }
  Nba97FrontendResourceLookupSiteContract unused{};
  CHECK(!nba97_frontend_resource_lookup_site_contract(0, &unused));
  CHECK(!nba97_frontend_resource_lookup_site_contract(8, &unused));
  CHECK(!nba97_frontend_resource_lookup_site_contract(1, nullptr));

  Fixture oldBit;
  oldBit.put(Descriptor + 24, 0x18);
  CHECK(oldBit.run() == NBA97_TEXT_COMPLETE && oldBit.progress.completed);
  CHECK(oldBit.calls.size() == 1 && oldBit.progress.instruction_count == 27 &&
        oldBit.progress.machine.registers.gpr[2].word == Descriptor);
  CHECK(oldBit.get(Descriptor + 24) == 0x10 &&
        oldBit.access[5].pc == 0x8008a2f4u &&
        oldBit.access[6].pc == 0x8008a308u);

  Fixture allocated;
  CHECK(allocated.run() == NBA97_TEXT_COMPLETE && allocated.progress.completed);
  CHECK(allocated.calls.size() == 4 && allocated.progress.copied &&
        allocated.progress.freed &&
        allocated.progress.machine.registers.gpr[2].word == Allocation);
  CHECK(allocated.calls[1].event.site == 2 &&
        allocated.calls[1].machine.registers.gpr[4].word == Name &&
        allocated.calls[1].machine.registers.gpr[5].word == 8 &&
        allocated.calls[1].machine.registers.gpr[6].word == 0 &&
        allocated.calls[1].machine.registers.gpr[7].word == 0);
  CHECK(allocated.calls[2].event.site == 3 &&
        allocated.calls[2].machine.registers.gpr[4].word == 0x80120000u &&
        allocated.calls[2].machine.registers.gpr[5].word == 0x80140000u &&
        allocated.calls[2].machine.registers.gpr[6].word == 8);
  CHECK(allocated.calls[3].event.site == 4 &&
        allocated.calls[3].machine.registers.gpr[4].word == Descriptor);
  CHECK(allocated.progress.saved_return_address.word == 0x8007b1f8u &&
        allocated.progress.machine.registers.gpr[31].word == 0x8007b1f8u &&
        allocated.progress.machine.registers.gpr[29].word == Sp &&
        allocated.progress.machine.hi.word == 0x12345678u &&
        allocated.progress.machine.lo.word == 0x9abcdef0u);

  Fixture nullAllocation;
  nullAllocation.allocation = 0;
  CHECK(nullAllocation.run() == NBA97_TEXT_COMPLETE &&
        nullAllocation.progress.machine.registers.gpr[2].word == Descriptor &&
        nullAllocation.calls.size() == 2 && !nullAllocation.progress.copied);
  Fixture relocated;
  relocated.relocateFrame = 0x801ed000u;
  CHECK(relocated.run() == NBA97_TEXT_COMPLETE &&
        relocated.progress.completed &&
        relocated.progress.machine.registers.gpr[29].word == 0x801ed028u &&
        relocated.progress.machine.registers.gpr[31].word == 0x8007b1f8u &&
        relocated.progress.machine.registers.gpr[16].word == 0x51001010u &&
        relocated.progress.machine.registers.gpr[19].word == 0x51001313u);
  Fixture liveSaved;
  liveSaved.mutateLiveSaved = true;
  CHECK(liveSaved.run() == NBA97_TEXT_COMPLETE && liveSaved.calls.size() == 4 &&
        liveSaved.calls[1].machine.registers.gpr[4].word == 0x80026000u &&
        liveSaved.calls[3].machine.registers.gpr[4].word == 0x80160000u &&
        liveSaved.progress.machine.registers.gpr[17].word == 0x51001111u &&
        liveSaved.progress.machine.registers.gpr[18].word == 0x51001212u);
}

void chainPathsAndKnownness() {
  Fixture miss;
  miss.initial = 0;
  miss.chainHitAt = 99;
  CHECK(miss.run() == NBA97_TEXT_COMPLETE && miss.progress.completed);
  CHECK(miss.progress.chain_attempts == 2 && miss.calls.size() == 3 &&
        miss.progress.machine.registers.gpr[2].word == 0 &&
        miss.progress.instruction_count == 57);
  CHECK(miss.calls[1].machine.registers.gpr[5].word == 0x100 &&
        miss.calls[2].machine.registers.gpr[5].word == 0x200);

  Fixture secondary;
  secondary.initial = 0;
  secondary.chainHit = Descriptor;
  CHECK(secondary.run() == NBA97_TEXT_COMPLETE &&
        secondary.progress.secondary_path && secondary.progress.copied &&
        !secondary.progress.freed &&
        secondary.progress.machine.registers.gpr[2].word == Allocation);
  CHECK(secondary.calls.size() == 4 && secondary.calls[2].event.site == 6 &&
        secondary.calls[3].event.site == 7);
  CHECK(secondary.get(Descriptor + 24) == 0 &&
        secondary.access[secondary.progress.access_events - 10].pc != 0);

  Fixture secondHit;
  secondHit.initial = 0;
  secondHit.chainHit = Descriptor;
  secondHit.chainHitAt = 2;
  CHECK(secondHit.run() == NBA97_TEXT_COMPLETE &&
        secondHit.progress.chain_attempts == 2 &&
        secondHit.progress.secondary_path && secondHit.calls.size() == 5);

  Fixture runaway;
  runaway.initial = 0;
  runaway.chainHitAt = 999;
  runaway.put(0x800f8db0u, 0x100u);
  runaway.context.operation_budget = 20;
  CHECK(runaway.run() == NBA97_TEXT_LIMIT &&
        runaway.progress.operations == 20 &&
        runaway.progress.chain_attempts >= 4 && !runaway.progress.completed);

  Fixture nullSecondary;
  nullSecondary.initial = 0;
  nullSecondary.chainHit = Descriptor;
  nullSecondary.allocation = 0;
  CHECK(nullSecondary.run() == NBA97_TEXT_RESOURCE &&
        nullSecondary.progress.stopped_pc == 0x8008a3d4u &&
        nullSecondary.progress.stopped_address == 0 &&
        nullSecondary.progress.call_count[6] == 1);
  nullSecondary.context.memory.count = 2;
  nullSecondary.zeroBytes = {{0x00, 0x40, 0x18, 0x80}};
  CHECK(nullSecondary.run() == NBA97_TEXT_COMPLETE &&
        nullSecondary.progress.copied &&
        nullSecondary.calls.back().machine.registers.gpr[5].word ==
            0x80184000u);

  Fixture unknownFlags;
  unknownFlags.put(Descriptor + 24, 8, 14);
  CHECK(unknownFlags.run() == NBA97_TEXT_UNKNOWN &&
        unknownFlags.progress.stopped_pc == 0x8008a304u &&
        unknownFlags.get(Descriptor + 24) == 0);
  Fixture unknownLookup;
  unknownLookup.initial = 0;
  unknownLookup.initialMask = 14;
  CHECK(unknownLookup.run() == NBA97_TEXT_UNKNOWN &&
        unknownLookup.progress.stopped_pc == 0x8008a2ecu);
  Fixture masks;
  masks.put(Descriptor, 0x80120000u, 5);
  masks.put(Descriptor + 20, 8, 6);
  masks.put(Allocation, 0x80140000u, 9);
  CHECK(masks.run() == NBA97_TEXT_COMPLETE && masks.calls.size() == 4 &&
        masks.calls[2].machine.registers.gpr[4].known_mask == 5 &&
        masks.calls[2].machine.registers.gpr[5].known_mask == 9 &&
        masks.calls[2].machine.registers.gpr[6].known_mask == 6);
  Fixture unknownChainAddress;
  unknownChainAddress.initial = 0;
  unknownChainAddress.put(0x80150010u, 0x100u, 13);
  CHECK(unknownChainAddress.run() == NBA97_TEXT_UNKNOWN &&
        unknownChainAddress.progress.stopped_pc == 0x8008a398u);
  Fixture absentPlane;
  absentPlane.regions[0].known = nullptr;
  CHECK(absentPlane.run() == NBA97_TEXT_COMPLETE &&
        absentPlane.progress.copied);
  Fixture badPlane;
  badPlane.known[Descriptor + 24 - Base] = 2;
  CHECK(badPlane.run() == NBA97_TEXT_ARGUMENT &&
        badPlane.progress.stopped_pc == 0x8008a2f4u);
  Fixture unknownSp;
  unknownSp.context.machine.registers.gpr[29].known_mask = 7;
  CHECK(unknownSp.run() == NBA97_TEXT_UNKNOWN &&
        unknownSp.progress.stopped_pc == 0x8008a2ccu);
  Fixture misaligned;
  misaligned.initial = Descriptor + 2;
  CHECK(misaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misaligned.progress.stopped_pc == 0x8008a2f4u);
  Fixture missing;
  missing.initial = 0x80200000u;
  CHECK(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc == 0x8008a2f4u);
  Fixture wrappedStack;
  wrappedStack.context.machine.registers.gpr[29] = {16, 15};
  CHECK(wrappedStack.run() == NBA97_TEXT_RESOURCE &&
        wrappedStack.progress.stopped_pc == 0x8008a2ccu &&
        wrappedStack.progress.stopped_address == 0);
}

void failurePrefixes() {
  Fixture complete;
  CHECK(complete.run() == NBA97_TEXT_COMPLETE);
  std::size_t operations = complete.progress.operations;
  for (std::size_t budget = 0; budget < operations; ++budget) {
    Fixture limited;
    limited.context.operation_budget = budget;
    CHECK(limited.run() == NBA97_TEXT_LIMIT &&
          limited.progress.operations == budget &&
          !limited.progress.completed &&
          limited.progress.access_events <= limited.access.size() &&
          limited.progress.instruction_events <= limited.pcs.size());
  }
  for (unsigned site = 1; site <= 4; ++site) {
    Fixture refused;
    refused.refuseSite = site;
    CHECK(refused.run() == NBA97_TEXT_IO_REFUSED &&
          refused.progress.call_attempts[site] == 1 &&
          refused.progress.call_count[site] == 0);
  }
  Fixture chainRefused;
  chainRefused.initial = 0;
  chainRefused.refuseSite = 5;
  CHECK(chainRefused.run() == NBA97_TEXT_IO_REFUSED &&
        chainRefused.progress.stopped_pc == 0x8008a36cu);
  Fixture secondaryRefused;
  secondaryRefused.initial = 0;
  secondaryRefused.chainHit = Descriptor;
  secondaryRefused.refuseSite = 6;
  CHECK(secondaryRefused.run() == NBA97_TEXT_IO_REFUSED &&
        secondaryRefused.progress.stopped_pc == 0x8008a3c4u);
  Fixture copyRefused;
  copyRefused.initial = 0;
  copyRefused.chainHit = Descriptor;
  copyRefused.refuseSite = 7;
  CHECK(copyRefused.run() == NBA97_TEXT_IO_REFUSED &&
        copyRefused.progress.stopped_pc == 0x8008a3dcu &&
        copyRefused.progress.call_attempts[7] == 1 &&
        copyRefused.progress.call_count[7] == 0);
  Fixture malformed;
  malformed.malformed = true;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.callbacks_completed == 0);
  Fixture noIo;
  noIo.context.io = nullptr;
  CHECK(noIo.run() == NBA97_TEXT_IO_REFUSED &&
        noIo.progress.stopped_pc == 0x8008a2e0u);
  Fixture badRa;
  badRa.context.machine.registers.gpr[31] = {0x8007b1f9u, 15};
  CHECK(badRa.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        badRa.progress.stopped_pc == 0x8008a400u);
}
} // namespace

int main() {
  try {
    contractsAndInitialPaths();
    chainPathsAndKnownness();
    failurePrefixes();
    for (bool pc : covered)
      CHECK(pc);
    std::printf("frontend_resource_lookup_tests passed %u checks\n", checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
