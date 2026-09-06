#include "frontend_resource_lookup_capture.h"

#include "frontend_resource_lookup_adapter.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <vector>

namespace nba97 {
namespace {
using U = std::uint32_t;
constexpr U Base = 0x80000000u;
constexpr U Size = 0x200000u;
constexpr U Sp = 0x801f0000u;

struct Call {
  Nba97FrontendResourceLookupEvent event{};
  Nba97FrontendResourceLookupMachine machine{};
};

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendResourceLookupContext context{};
  Nba97FrontendResourceLookupProgress progress{};
  std::array<Nba97FrontendResourceLookupAccess, 64> accesses{};
  std::array<U, 128> pcs{};
  std::vector<Call> calls;
  bool bad = false;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {0x71000000u + i * 0x101u, 15};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[4] = {0x80024854u, 15};
    context.machine.registers.gpr[29] = {Sp, 15};
    context.machine.registers.gpr[31] = {0x8007b1f8u, 15};
    context.machine.hi = {0x12345678u, 5};
    context.machine.lo = {0x9abcdef0u, 10};
    put(0x80110000u, 0x80120000u);
    put(0x80110014u, 8);
    put(0x80110018u, 8);
    put(0x80130000u, 0x80140000u, 9);
    context.memory = {&region, 1};
    context.operation_budget = 64;
    context.io = callback;
    context.user = this;
    context.access_journal = accesses.data();
    context.access_journal_capacity = accesses.size();
    context.instruction_journal = pcs.data();
    context.instruction_journal_capacity = pcs.size();
  }

  void put(U address, U value, unsigned mask = 15) {
    if (address < Base || address - Base > bytes.size() - 4) {
      bad = true;
      return;
    }
    for (unsigned i = 0; i < 4; ++i) {
      bytes[address - Base + i] = std::uint8_t(value >> (8 * i));
      if (region.known)
        known[address - Base + i] = std::uint8_t((mask >> i) & 1u);
    }
  }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97FrontendResourceLookupEvent *event,
                      Nba97FrontendResourceLookupMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    Nba97FrontendResourceLookupSiteContract contract{};
    if (!event || !machine ||
        !nba97_frontend_resource_lookup_site_contract(event->site, &contract) ||
        event->pc != contract.pc ||
        event->delay_slot_pc != contract.delay_slot_pc ||
        event->entry != contract.target ||
        event->argument_count != contract.argument_count ||
        event->target_program != contract.target_program ||
        event->invocation != 1 || fixture.calls.size() >= 4) {
      fixture.bad = true;
      return 0;
    }
    fixture.calls.push_back({*event, *machine});
    if (event->site == NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A2E0)
      machine->registers.gpr[2] = {0x80110000u, 15};
    if (event->site == NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A314)
      machine->registers.gpr[2] = {0x80130000u, 15};
    return 1;
  }
};

std::string hex(U value) {
  std::ostringstream out;
  out << "\"0x" << std::hex << std::setw(8) << std::setfill('0') << value
      << '"';
  return out.str();
}

void word(std::ostringstream &out,
          const Nba97FrontendResourceLookupWord &value) {
  out << "{\"word\":" << hex(value.word)
      << ",\"known_mask\":" << unsigned(value.known_mask) << '}';
}

void machine(std::ostringstream &out,
             const Nba97FrontendResourceLookupMachine &value) {
  out << "{\"gpr\":[";
  for (unsigned i = 0; i < 32; ++i) {
    if (i)
      out << ',';
    word(out, value.registers.gpr[i]);
  }
  out << "],\"hi\":";
  word(out, value.hi);
  out << ",\"lo\":";
  word(out, value.lo);
  out << '}';
}
} // namespace

std::string captureFrontendResourceLookup() {
  try {
    Fixture fixture;
    int result =
        nba97_frontend_resource_lookup(&fixture.context, &fixture.progress);
    if (result != NBA97_TEXT_COMPLETE || !fixture.progress.completed ||
        fixture.calls.size() != 4 ||
        fixture.progress.instruction_events > fixture.pcs.size() ||
        fixture.progress.access_events > fixture.accesses.size())
      fixture.bad = true;
    std::ostringstream out;
    out << "{\"program\":\"FEONLY\",\"address\":" << hex(0x8008a2c8u)
        << ",\"inclusive_end\":" << hex(0x8008a407u)
        << ",\"bytes\":320,\"instructions\":80,\"source_sha256\":"
           "\"2bc268004a25001f37dc4a8df569c9a94b5dea9e5253ab3533dbc18e08df00d1"
           "\","
           "\"result\":"
        << result << ",\"completed\":" << unsigned(fixture.progress.completed)
        << ",\"contract_failure\":" << fixture.bad
        << ",\"classification\":\"no direct visual effect\","
           "\"gameplay_shown\":\"BLOCKED\",\"fixture_contract\":"
           "\"Synthetic standalone full-entry CPU probe. Lookup returns "
           "V0=0x80110000/mask15; allocation returns "
           "V0=0x80130000/mask15 whose descriptor destination word is "
           "0x80140000/mask9. Source descriptor supplies pointer "
           "0x80120000/mask15, length 8/mask15, flags 8/mask15. Copy and "
           "free fixtures preserve V0 and every other CPU word/mask and all "
           "RAM. The seven children are typed dependencies; only this probe's "
           "implementations are synthetic.\",\"owner\":{\"operations\":"
        << fixture.progress.operations
        << ",\"accesses\":" << fixture.progress.accesses
        << ",\"reads\":" << fixture.progress.reads
        << ",\"stores\":" << fixture.progress.stores
        << ",\"instruction_trace\":[";
    std::size_t pcCount =
        std::min(fixture.progress.instruction_events, fixture.pcs.size());
    for (std::size_t i = 0; i < pcCount; ++i) {
      if (i)
        out << ',';
      out << hex(fixture.pcs[i]);
    }
    out << "],\"access_journal\":[";
    std::size_t accessCount =
        std::min(fixture.progress.access_events, fixture.accesses.size());
    for (std::size_t i = 0; i < accessCount; ++i) {
      if (i)
        out << ',';
      const auto &event = fixture.accesses[i];
      out << "{\"pc\":" << hex(event.pc)
          << ",\"address\":" << hex(event.address)
          << ",\"value\":" << hex(event.value)
          << ",\"operation\":" << event.operation
          << ",\"width\":" << unsigned(event.width)
          << ",\"known_mask\":" << unsigned(event.known_mask)
          << ",\"kind\":" << unsigned(event.kind) << '}';
    }
    out << "],\"calls\":[";
    for (std::size_t i = 0; i < fixture.calls.size(); ++i) {
      if (i)
        out << ',';
      const auto &call = fixture.calls[i];
      out << "{\"pc\":" << hex(call.event.pc)
          << ",\"delay\":" << hex(call.event.delay_slot_pc)
          << ",\"target\":" << hex(call.event.entry)
          << ",\"operation\":" << call.event.operation
          << ",\"invocation\":" << call.event.invocation
          << ",\"site\":" << unsigned(call.event.site)
          << ",\"program\":" << unsigned(call.event.target_program)
          << ",\"argument_count\":" << unsigned(call.event.argument_count)
          << ",\"machine\":";
      machine(out, call.machine);
      out << '}';
    }
    out << "]},\"final_machine\":";
    machine(out, fixture.progress.machine);
    out << ",\"next_unbound_boundary\":\"FEONLY 0x8008A2E0 -> "
           "0x8008A0A8 lookup service; heap, free, and chain lookup services "
           "remain typed dependencies. The separate integration test composes "
           "the committed DF caller and committed DC copy owner at both copy "
           "sites.\"}";
    return out.str();
  } catch (...) {
    return "{\"program\":\"FEONLY\",\"address\":\"0x8008a2c8\","
           "\"contract_failure\":true,\"gameplay_shown\":\"BLOCKED\"}";
  }
}
} // namespace nba97
