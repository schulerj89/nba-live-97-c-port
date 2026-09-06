#include "frontend_resource_info_adapter.h"
#include "frontend_resource_info_capture.h"

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
    throw std::runtime_error("frontend-resource-info integration line " +
                             std::to_string(line));
}
#define CHECK(value) check((value), __LINE__)

constexpr U Base = 0x80000000u;
constexpr U Size = 0x200000u;
constexpr U Sp = 0x801f0000u;
constexpr U Filename = 0x80024854u;
constexpr U Allocation = 0x80170000u;

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  unsigned parent_calls = 0;
  unsigned child_calls = 0;
  unsigned child_refuse_site = 0;
  bool bad_contract = false;

  Fixture() { put(0x800d9b50u, 0); }

  void put(U address, U value, unsigned mask = 15) {
    if (address < Base || address - Base > Size - 4)
      throw std::runtime_error("integration fixture write outside RAM");
    for (unsigned i = 0; i < 4; ++i) {
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
      if (region.known)
        known[address - Base + i] = std::uint8_t((mask >> i) & 1u);
    }
  }

  U get(U address) const {
    if (address < Base || address - Base > Size - 4)
      throw std::runtime_error("integration fixture read outside RAM");
    U value = 0;
    for (unsigned i = 0; i < 4; ++i)
      value |= U(bytes[address - Base + i]) << (i * 8u);
    return value;
  }

  static int parentIo(void *opaque, const Nba97GameTextMemory *,
                      const Nba97FrontendResourceLoadEvent *event,
                      Nba97FrontendResourceLoadMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    if (!event || !machine)
      return 0;
    ++fixture.parent_calls;
    switch (event->site) {
    case NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B1F0:
      machine->registers.gpr[2] = {0, 15};
      break;
    case NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B230:
      machine->registers.gpr[2] = {Allocation, 15};
      fixture.put(Allocation, 0x55667788u, 9);
      break;
    case NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B250:
    case NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B268:
      break;
    default:
      fixture.bad_contract = true;
      return 0;
    }
    return 1;
  }

  static int childIo(void *opaque, const Nba97GameTextMemory *,
                     const Nba97FrontendResourceInfoEvent *event,
                     Nba97FrontendResourceInfoMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    Nba97FrontendResourceInfoSiteContract contract{};
    if (!event || !machine ||
        !nba97_frontend_resource_info_site_contract(event->site, &contract) ||
        event->pc != contract.pc ||
        event->delay_slot_pc != contract.delay_slot_pc ||
        event->entry != contract.target ||
        event->argument_count != contract.argument_count ||
        event->target_program != contract.target_program) {
      fixture.bad_contract = true;
      return 0;
    }
    ++fixture.child_calls;
    switch (event->site) {
    case NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A5E8:
      machine->registers.gpr[2] = {1, 15};
      break;
    case NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A648:
      machine->registers.gpr[2] = {0x44u, 15};
      break;
    case NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A658:
      machine->registers.gpr[2] = {0x1200u, 15};
      break;
    case NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A66C:
      machine->registers.gpr[2] = {0, 15};
      break;
    default:
      break;
    }
    return event->site != fixture.child_refuse_site;
  }
};

int chain(Fixture &fixture, Nba97FrontendResourceLoadProgress &parent,
          Nba97FrontendResourceInfoBinding &binding,
          Nba97FrontendResourceInfoAdapterProgress &nested,
          std::array<Nba97FrontendResourceInfoAccess, 40> &accesses,
          std::array<U, 96> &pcs) {
  Nba97FrontendResourceLoadContext context{};
  for (unsigned i = 0; i < 32; ++i)
    context.machine.registers.gpr[i] = {0x41000000u + i * 0x101u, 15};
  context.machine.registers.gpr[0] = {0, 15};
  context.machine.registers.gpr[4] = {Filename, 15};
  context.machine.registers.gpr[5] = {0x13579bdfu, 15};
  context.machine.registers.gpr[6] = {7, 15};
  context.machine.registers.gpr[29] = {Sp, 15};
  context.machine.registers.gpr[31] = {0x8007b16cu, 15};
  context.machine.hi = {0x12345678u, 15};
  context.machine.lo = {0x9abcdef0u, 15};
  context.memory = {&fixture.region, 1};
  context.operation_budget = 25;
  context.io = Fixture::parentIo;
  context.user = &fixture;

  binding.operation_budget = 30;
  binding.io = Fixture::childIo;
  binding.user = &fixture;
  binding.access_journal = accesses.data();
  binding.access_journal_capacity = accesses.size();
  binding.instruction_journal = pcs.data();
  binding.instruction_journal_capacity = pcs.size();
  return nba97_frontend_resource_load_with_recovered_info(
      &context, &binding, &parent, &nested);
}

void naturalComposition(bool with_knownness) {
  Fixture fixture;
  if (!with_knownness)
    fixture.region.known = nullptr;
  Nba97FrontendResourceLoadProgress parent{};
  Nba97FrontendResourceInfoBinding binding{};
  Nba97FrontendResourceInfoAdapterProgress nested{};
  std::array<Nba97FrontendResourceInfoAccess, 40> accesses{};
  std::array<U, 96> pcs{};
  CHECK(chain(fixture, parent, binding, nested, accesses, pcs) ==
        NBA97_TEXT_COMPLETE);
  CHECK(parent.completed && binding.progress.completed &&
        nested.progress.completed && binding.invocations == 1 &&
        binding.completions == 1 && nested.invocations == 1 &&
        nested.completions == 1 && !fixture.bad_contract);
  CHECK(binding.parent_event.pc == 0x8007b214u &&
        binding.parent_event.delay_slot_pc == 0x8007b218u &&
        binding.parent_event.entry == 0x8008a594u &&
        binding.parent_event.argument_count == 5 &&
        binding.parent_machine.registers.gpr[31].word == 0x8007b21cu);
  CHECK(binding.progress.input_fifth_argument.word == 7 &&
        binding.progress.input_fifth_argument.known_mask == 15 &&
        binding.progress.input_handle_pointer.word == Sp - 40 &&
        binding.progress.input_other_pointer.word == Sp - 36 &&
        binding.progress.input_size_pointer.word == Sp - 32);
  CHECK(binding.progress.instruction_count == 67 &&
        binding.progress.operations == 30 && binding.progress.accesses == 25 &&
        fixture.parent_calls == 4 && fixture.child_calls == 5);
  CHECK(fixture.get(Sp - 40) == 0x44u && fixture.get(Sp - 36) == 0 &&
        fixture.get(Sp - 32) == 0x1200u &&
        fixture.get(0x800d9ae8u) == 0x1200u &&
        fixture.get(Allocation) == 0x55667788u);
  // Reconstruct the complete parent return from the actual child return and
  // the evidenced caller suffix, including the absent-plane descriptor mask.
  auto expected = binding.progress.machine;
  for (unsigned reg = 16; reg <= 19; ++reg)
    expected.registers.gpr[reg] = {0x41000000u + reg * 0x101u, 15};
  expected.registers.gpr[1] = {0x800e0000u, 15};
  expected.registers.gpr[2] = {Allocation, 15};
  expected.registers.gpr[4] = {Allocation, 15};
  expected.registers.gpr[5] = {0x55667788u,
      static_cast<std::uint8_t>(with_knownness ? 9 : 15)};
  expected.registers.gpr[6] = {0x1200u, 15};
  expected.registers.gpr[7] = {7, 15};
  expected.registers.gpr[29] = {Sp, 15};
  expected.registers.gpr[31] = {0x8007b16cu, 15};
  for (unsigned reg = 0; reg < 32; ++reg)
    CHECK(parent.machine.registers.gpr[reg].word == expected.registers.gpr[reg].word &&
          parent.machine.registers.gpr[reg].known_mask == expected.registers.gpr[reg].known_mask);
  CHECK(parent.machine.hi.word == expected.hi.word &&
        parent.machine.hi.known_mask == expected.hi.known_mask &&
        parent.machine.lo.word == expected.lo.word &&
        parent.machine.lo.known_mask == expected.lo.known_mask);
  CHECK(parent.machine.registers.gpr[2].word == Allocation &&
        parent.machine.registers.gpr[29].word == Sp &&
        parent.machine.registers.gpr[31].word == 0x8007b16cu &&
        parent.machine.hi.word == 0x12345678u &&
        parent.machine.lo.word == 0x9abcdef0u);
}

void nestedFailure() {
  Fixture fixture;
  fixture.child_refuse_site = NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A63C;
  Nba97FrontendResourceLoadProgress parent{};
  Nba97FrontendResourceInfoBinding binding{};
  Nba97FrontendResourceInfoAdapterProgress nested{};
  std::array<Nba97FrontendResourceInfoAccess, 40> accesses{};
  std::array<U, 96> pcs{};
  CHECK(chain(fixture, parent, binding, nested, accesses, pcs) ==
        NBA97_TEXT_IO_REFUSED);
  CHECK(binding.result == NBA97_TEXT_IO_REFUSED && binding.invocations == 1 &&
        binding.completions == 0 &&
        binding.progress.stopped_pc == 0x8008a63cu &&
        binding.progress.stopped_target == 0x80083b70u &&
        nested.result == NBA97_TEXT_IO_REFUSED &&
        parent.stopped_pc == 0x8007b214u && !parent.completed);

  Fixture limited;
  /* Exercise a direct binding limit so no caller callback contract is
   * invented around a child that did not complete. */
  Nba97FrontendResourceLoadEvent event{
      0x8007b214u, 0x8007b218u, 0x8008a594u, 1, 1,
      NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B214, 5,
      NBA97_FRONTEND_MAIN_PROGRAM_FEONLY};
  Nba97FrontendResourceLoadMachine machine{};
  for (auto &word : machine.registers.gpr)
    word = {0, 15};
  machine.registers.gpr[4] = {Filename, 15};
  machine.registers.gpr[5] = {0x801e0000u, 15};
  machine.registers.gpr[6] = {0x801e0004u, 15};
  machine.registers.gpr[7] = {0x801e0008u, 15};
  machine.registers.gpr[29] = {Sp, 15};
  machine.registers.gpr[31] = {0x8007b21cu, 15};
  limited.put(Sp + 16, 7);
  Nba97GameTextMemory memory{&limited.region, 1};
  Nba97FrontendResourceInfoBinding direct{};
  direct.operation_budget = 1;
  direct.io = Fixture::childIo;
  direct.user = &limited;
  CHECK(!nba97_frontend_resource_info_from_frontend_resource_load(
            &direct, &memory, &event, &machine) &&
        direct.result == NBA97_TEXT_LIMIT && direct.progress.operations == 1 &&
        direct.progress.stopped_pc == 0x8008a5a0u);
}

void adapterGuardsAndContracts() {
  const U pcs[] = {0,          0x8008a5e8u, 0x8008a610u, 0x8008a63cu,
                   0x8008a648u, 0x8008a658u, 0x8008a66cu, 0x8008a698u};
  const U targets[] = {0,          0x80084910u, 0x80074184u, 0x80083b70u,
                       0x8007f588u, 0x8008a408u, 0x8007f318u, 0x8008a7b0u};
  const unsigned argc[] = {0, 3, 6, 4, 2, 1, 3, 1};
  Nba97FrontendResourceInfoSiteContract contract{};
  CHECK(!nba97_frontend_resource_info_site_contract(0, &contract));
  CHECK(!nba97_frontend_resource_info_site_contract(
      NBA97_FRONTEND_RESOURCE_INFO_SITE_COUNT, &contract));
  for (unsigned site = 1; site < NBA97_FRONTEND_RESOURCE_INFO_SITE_COUNT;
       ++site) {
    CHECK(nba97_frontend_resource_info_site_contract(
              static_cast<std::uint8_t>(site), &contract) &&
          contract.pc == pcs[site] && contract.delay_slot_pc == pcs[site] + 4 &&
          contract.target == targets[site] &&
          contract.argument_count == argc[site] &&
          contract.target_program == NBA97_FRONTEND_MAIN_PROGRAM_FEONLY);
  }

  Fixture fixture;
  Nba97GameTextMemory memory{&fixture.region, 1};
  Nba97FrontendResourceLoadEvent event{
      0x8007b214u, 0x8007b218u, 0x8008a594u, 1, 1,
      NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B214, 5,
      NBA97_FRONTEND_MAIN_PROGRAM_FEONLY};
  Nba97FrontendResourceLoadMachine machine{};
  for (auto &word : machine.registers.gpr)
    word = {0, 15};
  machine.registers.gpr[29] = {Sp, 15};
  machine.registers.gpr[31] = {0x8007b21cu, 15};
  fixture.put(Sp + 16, 7);

  auto rejected = [&](Nba97FrontendResourceLoadEvent bad_event,
                      Nba97FrontendResourceLoadMachine bad_machine) {
    Nba97FrontendResourceInfoBinding binding{};
    CHECK(!nba97_frontend_resource_info_from_frontend_resource_load(
              &binding, &memory, &bad_event, &bad_machine) &&
          binding.result == NBA97_TEXT_ARGUMENT && binding.invocations == 0);
  };
  auto bad = event;
  bad.pc ^= 4u;
  rejected(bad, machine);
  bad = event;
  bad.delay_slot_pc ^= 4u;
  rejected(bad, machine);
  bad = event;
  bad.entry ^= 4u;
  rejected(bad, machine);
  bad = event;
  bad.invocation = 2;
  rejected(bad, machine);
  bad = event;
  bad.argument_count = 4;
  rejected(bad, machine);
  bad = event;
  bad.site = NBA97_FRONTEND_RESOURCE_LOAD_SITE_NONE;
  rejected(bad, machine);
  bad = event;
  bad.target_program = NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD;
  rejected(bad, machine);
  auto bad_machine = machine;
  bad_machine.registers.gpr[31].word ^= 4u;
  rejected(event, bad_machine);
  bad_machine = machine;
  bad_machine.registers.gpr[31].known_mask = 7;
  rejected(event, bad_machine);
}

void captureReceipt() {
  std::string receipt = nba97::captureFrontendResourceInfo();
  CHECK(receipt.find("\"contract_failure\":0") != std::string::npos &&
        receipt.find("\"address\":\"0x8008a594\"") != std::string::npos &&
        receipt.find("\"argument_count\":3") != std::string::npos &&
        receipt.find("\"argument_count\":4") != std::string::npos &&
        receipt.find("\"argument_count\":2") != std::string::npos &&
        receipt.find("\"gameplay_shown\":\"BLOCKED\"") != std::string::npos &&
        receipt.find("all 32 GPRs, HI/LO") != std::string::npos);
}
} // namespace

int main() {
  try {
    naturalComposition(true);
    naturalComposition(false);
    nestedFailure();
    adapterGuardsAndContracts();
    captureReceipt();
    std::printf("frontend_resource_info_integration_tests passed %u checks\n",
                checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
