#include "frontend_resource_info_capture.h"

#include "frontend_resource_info_adapter.h"

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
  Nba97FrontendResourceInfoEvent event{};
  Nba97FrontendResourceInfoMachine machine{};
};

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendResourceInfoContext context{};
  Nba97FrontendResourceInfoProgress progress{};
  std::array<Nba97FrontendResourceInfoAccess, 32> accesses{};
  std::array<U, 96> pcs{};
  std::vector<Call> calls;
  bool contract_failure = false;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {
          0x57000000u + i * 0x101u,
          static_cast<std::uint8_t>((i % 15u) + 1u)};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[4] = {0x80024854u, 5};
    context.machine.registers.gpr[5] = {0x801e0000u, 15};
    context.machine.registers.gpr[6] = {0x801e0004u, 15};
    context.machine.registers.gpr[7] = {0x801e0008u, 15};
    context.machine.registers.gpr[29] = {Sp, 15};
    context.machine.registers.gpr[31] = {0x8007b21cu, 15};
    context.machine.hi = {0x12345678u, 5};
    context.machine.lo = {0x9abcdef0u, 10};
    put(Sp + 16, 0x2468ace0u, 9);
    context.memory = {&region, 1};
    context.operation_budget = 30;
    context.io = callback;
    context.user = this;
    context.access_journal = accesses.data();
    context.access_journal_capacity = accesses.size();
    context.instruction_journal = pcs.data();
    context.instruction_journal_capacity = pcs.size();
  }

  void put(U address, U value, unsigned mask = 15) {
    if (address < Base || address - Base > Size - 4) {
      contract_failure = true;
      return;
    }
    for (unsigned i = 0; i < 4; ++i) {
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
      if (region.known)
        known[address - Base + i] = std::uint8_t((mask >> i) & 1u);
    }
  }

  static int callback(void *opaque, const Nba97GameTextMemory *,
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
      fixture.contract_failure = true;
      return 0;
    }
    if (fixture.calls.size() >= 5) {
      fixture.contract_failure = true;
      return 0;
    }
    fixture.calls.push_back({*event, *machine});
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
    return 1;
  }
};

std::string hex(U value) {
  std::ostringstream out;
  out << "\"0x" << std::hex << std::setw(8) << std::setfill('0') << value
      << '\"';
  return out.str();
}

void writeWord(std::ostringstream &out,
               const Nba97FrontendResourceInfoWord &word) {
  out << "{\"word\":" << hex(word.word)
      << ",\"known_mask\":" << unsigned(word.known_mask) << '}';
}

void writeMachine(std::ostringstream &out,
                  const Nba97FrontendResourceInfoMachine &machine) {
  out << "{\"gpr\":[";
  for (unsigned i = 0; i < 32; ++i) {
    if (i)
      out << ',';
    writeWord(out, machine.registers.gpr[i]);
  }
  out << "],\"hi\":";
  writeWord(out, machine.hi);
  out << ",\"lo\":";
  writeWord(out, machine.lo);
  out << '}';
}
} // namespace

std::string captureFrontendResourceInfo() {
  try {
    Fixture fixture;
    int result = nba97_frontend_resource_info(&fixture.context,
                                               &fixture.progress);
    if (result != NBA97_TEXT_COMPLETE || !fixture.progress.completed ||
        fixture.progress.instruction_count != 67 ||
        fixture.progress.operations != 30 ||
        fixture.progress.accesses != 25 || fixture.calls.size() != 5 ||
        fixture.progress.instruction_events > fixture.pcs.size() ||
        fixture.progress.access_events > fixture.accesses.size())
      fixture.contract_failure = true;

    std::ostringstream out;
    out << "{\"program\":\"FEONLY\",\"address\":" << hex(0x8008a594u)
        << ",\"inclusive_end\":" << hex(0x8008a6ebu)
        << ",\"bytes\":344,\"instructions\":86,\"source_sha256\":"
           "\"494529aeb56f769fbc5f40e3792f83492ad9368f40e6672ce2f4359a6d0a887a\","
           "\"result\":"
        << result << ",\"completed\":" << unsigned(fixture.progress.completed)
        << ",\"contract_failure\":" << fixture.contract_failure
        << ",\"classification\":\"no direct visual effect\","
           "\"gameplay_shown\":\"BLOCKED\",\"fixture_contract\":"
           "\"Synthetic standalone full-machine entry. Prefix comparison "
           "returns V0=1/mask15. Formatter preserves CPU and RAM. Open returns "
           "V0=0x44/mask15. Info returns V0=0x1200/mask15. Seek returns "
           "V0=0/mask15. Every other child preserves all 32 GPRs, HI/LO and "
           "retained bytes; no child CPU ABI or production service is claimed.\","
           "\"owner\":{\"operations\":"
        << fixture.progress.operations
        << ",\"accesses\":" << fixture.progress.accesses
        << ",\"reads\":" << fixture.progress.reads
        << ",\"stores\":" << fixture.progress.stores
        << ",\"callbacks\":" << fixture.progress.callbacks_completed
        << ",\"attempts\":" << fixture.progress.attempts_started
        << ",\"instruction_trace\":[";
    for (std::size_t i = 0;
         i < std::min(fixture.progress.instruction_events, fixture.pcs.size());
         ++i) {
      if (i)
        out << ',';
      out << hex(fixture.pcs[i]);
    }
    out << "],\"access_journal\":[";
    for (std::size_t i = 0;
         i < std::min(fixture.progress.access_events, fixture.accesses.size());
         ++i) {
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
          << ",\"program\":" << unsigned(call.event.target_program)
          << ",\"argument_count\":" << unsigned(call.event.argument_count)
          << ",\"machine\":";
      writeMachine(out, call.machine);
      out << '}';
    }
    out << "]},\"final_machine\":";
    writeMachine(out, fixture.progress.machine);
    out << ",\"visual\":{\"native_input_steps\":\"manager-owned verifier\","
           "\"expected_pixel_effect\":\"none\",\"frame_hashes\":\"independently verified "
           "by native verifier\"},\"next_unbound_boundaries\":"
           "\"FEONLY services 0x80084910, 0x80074184, 0x80083B70, "
           "0x8007F588, 0x8008A408, 0x8007F318 and 0x8008A7B0 remain "
           "explicit fixtures; the real frontend lifecycle, loader handoff, "
           "and advancing match loop are also unbound\"}";
    return out.str();
  } catch (...) {
    return "{\"program\":\"FEONLY\",\"address\":\"0x8008a594\","
           "\"contract_failure\":true,\"gameplay_shown\":\"BLOCKED\"}";
  }
}
} // namespace nba97
