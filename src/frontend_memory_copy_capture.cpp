#include "frontend_memory_copy_capture.h"

#include "frontend_memory_copy_adapter.h"

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <vector>

namespace nba97 {
namespace {
class CanonicalHash {
 public:
  void byte(std::uint8_t value) {
    value_ ^= value;
    value_ *= 1099511628211ull;
  }

  void le32(std::uint32_t value) {
    for (unsigned byte_index = 0; byte_index < 4; ++byte_index)
      byte(std::uint8_t(value >> (byte_index * 8u)));
  }

  void le64(std::uint64_t value) {
    for (unsigned byte_index = 0; byte_index < 8; ++byte_index)
      byte(std::uint8_t(value >> (byte_index * 8u)));
  }

  std::uint64_t value() const { return value_; }

 private:
  std::uint64_t value_ = 14695981039346656037ull;
};

std::uint64_t hashBytes(const std::uint8_t *data, std::size_t size) {
  CanonicalHash hash;
  for (std::size_t i = 0; i < size; ++i)
    hash.byte(data[i]);
  return hash.value();
}

std::uint64_t hashMachine(const Nba97FrontendMemoryCopyMachine &machine) {
  CanonicalHash hash;
  auto append = [&](Nba97FrontendMemoryCopyWord word) {
    hash.le32(word.word);
    hash.byte(word.known_mask);
  };
  for (const auto word : machine.registers.gpr)
    append(word);
  append(machine.hi);
  append(machine.lo);
  return hash.value();
}

std::uint64_t hashAccesses(const Nba97FrontendMemoryCopyAccess *accesses,
                           std::size_t count) {
  CanonicalHash hash;
  for (std::size_t i = 0; i < count; ++i) {
    const auto &access = accesses[i];
    hash.le32(access.pc);
    hash.le32(access.address);
    hash.le32(access.logical_address);
    hash.le32(access.value);
    hash.le64(static_cast<std::uint64_t>(access.operation));
    hash.byte(access.width);
    hash.byte(access.known_mask);
    hash.byte(access.transfer_mask);
    hash.byte(access.kind);
  }
  return hash.value();
}

std::uint64_t hashPcs(const std::uint32_t *pcs, std::size_t count) {
  CanonicalHash hash;
  for (std::size_t i = 0; i < count; ++i)
    hash.le32(pcs[i]);
  return hash.value();
}

bool spanValid(std::uint32_t base, std::size_t size, std::uint32_t address,
               std::size_t width) {
  const std::uint64_t offset = std::uint64_t(address) - base;
  return address >= base && offset <= size && width <= size - std::size_t(offset);
}

std::string hex(std::uint64_t value) {
  std::ostringstream out;
  out << std::hex << std::setw(16) << std::setfill('0') << value;
  return out.str();
}

void appendMachineJson(std::ostringstream &out,
                       const Nba97FrontendMemoryCopyMachine &machine) {
  out << "{\"gpr\":[";
  for (unsigned i = 0; i < 32; ++i) {
    if (i)
      out << ',';
    out << "{\"word\":" << machine.registers.gpr[i].word
        << ",\"known_mask\":"
        << unsigned(machine.registers.gpr[i].known_mask) << '}';
  }
  out << "],\"hi\":{\"word\":" << machine.hi.word
      << ",\"known_mask\":" << unsigned(machine.hi.known_mask)
      << "},\"lo\":{\"word\":" << machine.lo.word
      << ",\"known_mask\":" << unsigned(machine.lo.known_mask) << "}}";
}

struct SyntheticServices {
  static constexpr std::uint32_t Base = 0x80000000u;
  static constexpr std::uint32_t Handle = 0x80140000u;
  static constexpr std::uint32_t Length = 4096u;
  std::vector<std::uint8_t> *bytes;
  bool startup = false;
  std::size_t allocations = 0;
  bool clock = false;
  bool loader = false;
  bool size = false;
  bool unbound = false;
  std::size_t calls = 0;

  bool put(std::uint32_t address, std::uint32_t value, unsigned width = 4) {
    if (!bytes || !spanValid(Base, bytes->size(), address, width))
      return false;
    for (unsigned i = 0; i < width; ++i)
      (*bytes)[address - Base + i] = std::uint8_t(value >> (i * 8u));
    return true;
  }

  static int invoke(void *opaque, const Nba97GameTextMemory *,
                    const Nba97FrontendMainEvent *event,
                    Nba97FrontendMainMachine *machine,
                    Nba97FrontendMainCalleeOutcome *) {
    auto &self = *static_cast<SyntheticServices *>(opaque);
    if (!event || !machine ||
        event->site == NBA97_FRONTEND_MAIN_SITE_80028B54)
      return 0;
    ++self.calls;
    if (event->site == NBA97_FRONTEND_MAIN_SITE_80028810) {
      if (!self.put(0x80015098u, 1))
        return 0;
      self.startup = true;
    } else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028880 ||
               event->site == NBA97_FRONTEND_MAIN_SITE_80028974) {
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {0x80130000u, 15};
      ++self.allocations;
    } else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028A7C) {
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {0, 15};
      self.clock = true;
    } else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028ACC) {
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {Handle, 15};
      self.loader = true;
    } else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028AD8) {
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {Length, 15};
      self.size = true;
    } else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028B68) {
      self.unbound = true;
      return 0;
    }
    return 1;
  }
};
}  // namespace

std::string captureFrontendMemoryCopy() {
  constexpr std::uint32_t base = SyntheticServices::Base;
  constexpr std::uint32_t source = SyntheticServices::Handle;
  constexpr std::uint32_t destination = 0x801e0000u;
  constexpr std::uint32_t length = SyntheticServices::Length;
  constexpr std::uint32_t game_entry = 0x801e1410u;
  std::vector<std::uint8_t> bytes(0x200000u);
  std::vector<std::uint8_t> known(bytes.size(), 1);
  auto put = [&](std::uint32_t address, std::uint32_t value,
                 unsigned width = 4) -> bool {
    if (!spanValid(base, bytes.size(), address, width))
      return false;
    for (unsigned i = 0; i < width; ++i)
      bytes[address - base + i] = std::uint8_t(value >> (i * 8u));
    return true;
  };
  bool fixture_valid = spanValid(base, bytes.size(), source, length) &&
                       spanValid(base, bytes.size(), destination, length);
  for (std::uint32_t i = 0; i < length; ++i)
    fixture_valid = put(source + i, i * 37u + (i >> 5u) + 11u, 1) &&
                    fixture_valid;
  fixture_valid = put(source, game_entry) && fixture_valid;
  if (fixture_valid)
    std::fill(bytes.begin() + (destination - base),
              bytes.begin() + (destination - base + length),
              std::uint8_t{0xa5u});
  fixture_valid = put(0x80021ee4u, 0) && fixture_valid;
  fixture_valid = put(0x8001edecu, 0, 2) && fixture_valid;
  fixture_valid = put(0x80021568u, 0, 2) && fixture_valid;
  fixture_valid = put(0x80015098u, 0) && fixture_valid;
  const auto source_hash =
      fixture_valid ? hashBytes(bytes.data() + source - base, length) : 0;
  const auto destination_before =
      fixture_valid ? hashBytes(bytes.data() + destination - base, length) : 0;

  Nba97GameTextRegion region{base, bytes.data(), known.data(), bytes.size()};
  std::vector<Nba97FrontendMainAccess> main_accesses(256);
  std::vector<std::uint32_t> main_instructions(2048);
  Nba97FrontendMainContext main_context{};
  main_context.memory = {&region, 1};
  main_context.operation_budget = 1000;
  for (unsigned i = 0; i < 32; ++i)
    main_context.machine.registers.gpr[i] = {0x44000000u + i * 0x101u, 15};
  main_context.machine.registers.gpr[0] = {0, 15};
  main_context.machine.registers.gpr[NBA97_FRONTEND_MAIN_SP] = {0x801ff000u,
                                                                15};
  main_context.machine.registers.gpr[NBA97_FRONTEND_MAIN_RA] = {0x8007b840u,
                                                                15};
  main_context.machine.hi = {0x12345678u, 5};
  main_context.machine.lo = {0x9abcdef0u, 10};
  SyntheticServices services{&bytes};
  main_context.io = SyntheticServices::invoke;
  main_context.user = &services;
  main_context.access_journal = main_accesses.data();
  main_context.access_journal_capacity = main_accesses.size();
  main_context.instruction_journal = main_instructions.data();
  main_context.instruction_journal_capacity = main_instructions.size();

  std::vector<Nba97FrontendMemoryCopyAccess> accesses(4096);
  std::vector<std::uint32_t> instructions(8192);
  Nba97FrontendMemoryCopyBinding binding{};
  binding.operation_budget = 10000;
  binding.access_journal = accesses.data();
  binding.access_journal_capacity = accesses.size();
  binding.instruction_journal = instructions.data();
  binding.instruction_journal_capacity = instructions.size();
  Nba97FrontendMainProgress main_progress{};
  const int main_result = nba97_frontend_main_with_recovered_memory_copy(
      &main_context, &binding, &main_progress);
  const bool destination_valid =
      spanValid(base, bytes.size(), destination, length);
  const auto destination_after = destination_valid
                                     ? hashBytes(bytes.data() + destination - base,
                                                 length)
                                     : 0;
  const bool access_journal_valid =
      binding.progress.access_events <= accesses.size();
  const bool pc_journal_valid =
      binding.progress.instruction_events <= instructions.size();
  const auto input_cpu_hash = hashMachine(binding.input_machine);
  const auto output_cpu_hash = hashMachine(binding.progress.machine);
  const auto access_hash =
      hashAccesses(accesses.data(),
                   access_journal_valid ? binding.progress.access_events : 0);
  const auto instruction_hash =
      hashPcs(instructions.data(),
              pc_journal_valid ? binding.progress.instruction_events : 0);
  const bool contract_failure =
      !fixture_valid || !destination_valid || !access_journal_valid ||
      !pc_journal_valid || main_result != NBA97_TEXT_IO_REFUSED ||
      main_progress.stopped_pc != 0x80028b68u ||
      main_progress.stopped_target != game_entry || !services.startup ||
      services.allocations != 2 || !services.clock || !services.loader ||
      !services.size || !services.unbound ||
      binding.result != NBA97_TEXT_COMPLETE || !binding.progress.completed ||
      binding.invocations != 1 || binding.completions != 1 ||
      source_hash != destination_after || destination_before == destination_after ||
      binding.progress.operations != 2048 ||
      binding.progress.access_events != 2048 ||
      binding.progress.instruction_events != 2329 ||
      binding.progress.bytes_read != length ||
      binding.progress.bytes_stored != length;

  std::ostringstream out;
  out << "{\"program\":\"FEONLY\",\"address\":\"0x800909A8\","
      << "\"range\":\"0x800909A8..0x80090CC7\",\"bytes\":800,"
      << "\"instructions\":200,\"evidence_sha256\":"
      << "\"589207dc7895ba0151f714f53c02c357959170daed411e652ca281ac7216ef4b\","
      << "\"fixture_contract\":\"synthetic standalone frontend-main machine and 2 MiB retained memory; startup writes 0x80015098=1; two allocation callbacks set V0=0x80130000; clock callback sets V0=0; loader callback sets V0=0x80140000; size callback sets V0=4096; every other FEONLY service preserves the full CPU and guest RAM; natural recovered main composes the copy owner; unbound GAMELOAD at copied entry 0x801E1410 is refused without mutation\","
      << "\"hash_algorithm\":\"FNV-1a-64\","
      << "\"hash_seed\":\"0xcbf29ce484222325\","
      << "\"access_hash_layout\":\"per event: le32 pc,address,logical_address,value; le64 operation; u8 width,known_mask,transfer_mask,kind\","
      << "\"pc_hash_layout\":\"executed order: le32 pc per event\","
      << "\"machine_hash_layout\":\"170 bytes: gpr0..gpr31,hi,lo; each le32 word then u8 known_mask\","
      << "\"source\":\"0x80140000\",\"destination\":\"0x801E0000\","
      << "\"length\":4096,\"operations\":" << binding.progress.operations
      << ",\"access_events\":" << binding.progress.access_events
      << ",\"reads\":" << binding.progress.reads << ",\"stores\":"
      << binding.progress.stores << ",\"instruction_events\":"
      << binding.progress.instruction_events << ",\"service_calls\":"
      << services.calls << ",\"main_result\":" << main_result
      << ",\"main_stopped_pc\":" << main_progress.stopped_pc
      << ",\"main_stopped_target\":" << main_progress.stopped_target
      << ",\"copy_result\":" << binding.result
      << ",\"copy_completed\":" << unsigned(binding.progress.completed)
      << ",\"source_hash\":\"" << hex(source_hash)
      << "\",\"destination_before_hash\":\"" << hex(destination_before)
      << "\",\"destination_after_hash\":\"" << hex(destination_after)
      << "\",\"input_cpu_hash\":\"" << hex(input_cpu_hash)
      << "\",\"output_cpu_hash\":\"" << hex(output_cpu_hash)
      << "\",\"access_hash\":\"" << hex(access_hash)
      << "\",\"pc_hash\":\"" << hex(instruction_hash)
      << "\",\"copy_machine_input\":";
  appendMachineJson(out, binding.input_machine);
  out << ",\"copy_machine_output\":";
  appendMachineJson(out, binding.progress.machine);
  out << ",\"gameplay_shown\":\"BLOCKED\",\"contract_failure\":"
      << (contract_failure ? 1 : 0) << '}';
  return out.str();
}
}  // namespace nba97
