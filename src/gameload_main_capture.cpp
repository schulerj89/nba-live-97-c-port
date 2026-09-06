#include "gameload_main_capture.h"

#include "recovered/gameload_main.h"

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace nba97 {
namespace {
using U = std::uint32_t;
constexpr U Base = 0x80000000u;
constexpr U Size = 0x200000u;
constexpr U Sp = 0x801ff000u;
constexpr U Stage = 0x801b0000u;
constexpr U TargetWord = 0x80015000u;
constexpr U Workspace = 0x80015008u;
constexpr U CopySize = 4096u;
constexpr U Gameonly = 0x80020000u;
constexpr std::uint64_t FnvOffset = UINT64_C(14695981039346656037);
constexpr std::uint64_t FnvPrime = UINT64_C(1099511628211);

struct Call {
  Nba97GameloadMainEvent event{};
  Nba97GameloadMainMachine machine{};
};

void append8(std::vector<std::uint8_t> &bytes, std::uint8_t value) {
  bytes.push_back(value);
}

void append32(std::vector<std::uint8_t> &bytes, U value) {
  for (unsigned i = 0; i < 4; ++i)
    append8(bytes, static_cast<std::uint8_t>(value >> (i * 8u)));
}

void append64(std::vector<std::uint8_t> &bytes, std::uint64_t value) {
  for (unsigned i = 0; i < 8; ++i)
    append8(bytes, static_cast<std::uint8_t>(value >> (i * 8u)));
}

std::uint64_t hash(const std::vector<std::uint8_t> &bytes) {
  std::uint64_t value = FnvOffset;
  for (const auto byte : bytes) {
    value ^= byte;
    value *= FnvPrime;
  }
  return value;
}

void serializeMachine(std::vector<std::uint8_t> &bytes,
                      const Nba97GameloadMainMachine &machine) {
  for (const auto &word : machine.registers.gpr) {
    append32(bytes, word.word);
    append8(bytes, word.known_mask);
  }
  append32(bytes, machine.hi.word);
  append8(bytes, machine.hi.known_mask);
  append32(bytes, machine.lo.word);
  append8(bytes, machine.lo.known_mask);
}

std::string hex64(std::uint64_t value) {
  std::ostringstream stream;
  stream << std::hex << std::setfill('0') << std::setw(16) << value;
  return stream.str();
}

void machineJson(std::ostringstream &out,
                 const Nba97GameloadMainMachine &machine) {
  out << "{\"gpr\":[";
  for (unsigned i = 0; i < 32; ++i) {
    if (i)
      out << ',';
    const auto &word = machine.registers.gpr[i];
    out << '[' << word.word << ',' << unsigned(word.known_mask) << ']';
  }
  out << "],\"hi\":[" << machine.hi.word << ','
      << unsigned(machine.hi.known_mask) << "],\"lo\":[" << machine.lo.word
      << ',' << unsigned(machine.lo.known_mask) << "]}";
}

struct Capture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97GameloadMainMachine input{};
  Nba97GameloadMainContext context{};
  Nba97GameloadMainProgress progress{};
  std::array<Nba97GameloadMainAccess, 16> accesses{};
  std::array<U, 64> pcs{};
  std::array<Call, 9> calls{};
  std::size_t call_count = 0;
  bool call_overflow = false;
  bool contract_failure = false;
  U copies = 0;
  U staged_checksum = 0;
  U loaded_checksum = 0;
  U restored_checksum = 0;

  Capture() {
    for (unsigned i = 0; i < 32; ++i)
      input.registers.gpr[i] = {0x33000000u + i * 0x101u,
                                 static_cast<std::uint8_t>(i & 15u)};
    input.registers.gpr[0] = {0, 15};
    input.registers.gpr[NBA97_GAMELOAD_MAIN_SP] = {Sp, 15};
    input.registers.gpr[NBA97_GAMELOAD_MAIN_RA] = {0x801e14b4u, 15};
    input.hi = {0x12345678u, 5};
    input.lo = {0x9abcdef0u, 10};
    for (U i = 0; i < CopySize; ++i)
      putByte(Workspace + i, static_cast<std::uint8_t>(i * 37u + 11u));
    putWord(0x80015004u, CopySize);
    context.memory = {&region, 1};
    context.operation_budget = 100;
    context.machine = input;
    context.io = io;
    context.user = this;
    context.access_journal = accesses.data();
    context.access_journal_capacity = accesses.size();
    context.instruction_journal = pcs.data();
    context.instruction_journal_capacity = pcs.size();
  }

  bool extent(U address, U count) {
    if (address < Base || count > Size || address - Base > Size - count) {
      contract_failure = true;
      return false;
    }
    return true;
  }

  void putByte(U address, std::uint8_t value) {
    if (extent(address, 1))
      bytes[address - Base] = value;
  }

  void putWord(U address, U value) {
    if (!extent(address, 4))
      return;
    for (unsigned i = 0; i < 4; ++i)
      bytes[address - Base + i] = static_cast<std::uint8_t>(value >> (i * 8u));
  }

  U checksum(U address, U count) {
    if (!extent(address, count))
      return 0;
    U value = 2166136261u;
    for (U i = 0; i < count; ++i) {
      value ^= bytes[address - Base + i];
      value *= 16777619u;
    }
    return value;
  }

  bool copy(U destination, U source, U count) {
    if (!extent(destination, count) || !extent(source, count))
      return false;
    for (U i = 0; i < count; ++i) {
      bytes[destination - Base + i] = bytes[source - Base + i];
      known[destination - Base + i] = known[source - Base + i];
    }
    ++copies;
    return true;
  }

  static int io(void *opaque, const Nba97GameTextMemory *,
                const Nba97GameloadMainEvent *event,
                Nba97GameloadMainMachine *machine,
                Nba97GameloadMainCalleeOutcome *outcome) {
    auto &capture = *static_cast<Capture *>(opaque);
    static constexpr std::array<U, 9> callPcs{{
        0x801e1374u, 0x801e137cu, 0x801e1384u, 0x801e1394u,
        0x801e13b0u, 0x801e13c4u, 0x801e13ccu, 0x801e13e0u,
        0x801e13f4u}};
    static constexpr std::array<U, 8> targets{{
        0x801e14b8u, 0x801e000cu, 0x801e059cu, 0x801e0938u,
        0x801e1344u, 0x801e1300u, 0x801e1670u, 0x801e1344u}};
    static constexpr std::array<std::uint8_t, 9> argc{{0, 0, 0, 2, 3, 2, 0,
                                                       3, 0}};
    if (!event || !machine || !outcome || event->site == 0 ||
        event->site >= NBA97_GAMELOAD_MAIN_SITE_COUNT) {
      capture.contract_failure = true;
      return 0;
    }
    const unsigned index = event->site - 1u;
    const bool dynamic = index == 8;
    if (event->pc != callPcs[index] || event->delay_slot_pc != event->pc + 4u ||
        event->argument_count != argc[index] ||
        event->target_program !=
            (dynamic ? NBA97_GAMELOAD_MAIN_PROGRAM_GAMEONLY
                     : NBA97_GAMELOAD_MAIN_PROGRAM_GAMELOAD) ||
        (!dynamic && event->entry != targets[index]) ||
        (dynamic && event->entry != Gameonly) ||
        machine->registers.gpr[NBA97_GAMELOAD_MAIN_RA].word != event->pc + 8u ||
        machine->registers.gpr[NBA97_GAMELOAD_MAIN_RA].known_mask != 15) {
      capture.contract_failure = true;
      return 0;
    }
    if (capture.call_count >= capture.calls.size()) {
      capture.call_overflow = true;
      capture.contract_failure = true;
      return 0;
    }
    capture.calls[capture.call_count++] = {*event, *machine};
    if (dynamic)
      return 0; // The fixture has no GAMEONLY runtime and must stop here.
    if (event->site == NBA97_GAMELOAD_MAIN_SITE_801E1374) {
      capture.putWord(0x80015098u, 1);
    } else if (event->site == NBA97_GAMELOAD_MAIN_SITE_801E13B0 ||
               event->site == NBA97_GAMELOAD_MAIN_SITE_801E13E0) {
      const U destination = machine->registers.gpr[NBA97_GAMELOAD_MAIN_A0].word;
      const U source = machine->registers.gpr[NBA97_GAMELOAD_MAIN_A1].word;
      const U count = machine->registers.gpr[NBA97_GAMELOAD_MAIN_A2].word;
      if (count != CopySize || !capture.copy(destination, source, count))
        return 0;
      if (event->site == NBA97_GAMELOAD_MAIN_SITE_801E13B0)
        capture.staged_checksum = capture.checksum(Stage, CopySize);
    } else if (event->site == NBA97_GAMELOAD_MAIN_SITE_801E13C4) {
      if (machine->registers.gpr[NBA97_GAMELOAD_MAIN_A0].word != 0x801e0060u ||
          machine->registers.gpr[NBA97_GAMELOAD_MAIN_A1].word != TargetWord)
        return 0;
      for (U i = 0; i < CopySize; ++i)
        capture.putByte(TargetWord + i,
                        static_cast<std::uint8_t>(i * 19u + 3u));
      capture.putWord(TargetWord, Gameonly);
      capture.putWord(0x80015004u, CopySize);
      capture.loaded_checksum = capture.checksum(TargetWord, CopySize);
    }
    return capture.contract_failure ? 0 : 1;
  }

  std::string run() {
    const int result = nba97_gameload_main(&context, &progress);
    restored_checksum = checksum(Workspace, CopySize);
    std::size_t callAttempts = 0;
    std::size_t callCompletions = 0;
    for (unsigned site = 1; site < NBA97_GAMELOAD_MAIN_SITE_COUNT; ++site) {
      callAttempts += progress.call_attempts[site];
      callCompletions += progress.call_count[site];
    }

    std::vector<std::uint8_t> machineInput;
    std::vector<std::uint8_t> machineOutput;
    std::vector<std::uint8_t> accessBytes;
    std::vector<std::uint8_t> pcBytes;
    std::vector<std::uint8_t> callBytes;
    serializeMachine(machineInput, input);
    serializeMachine(machineOutput, progress.machine);
    const std::size_t accessCount =
        progress.access_events < accesses.size() ? progress.access_events
                                                 : accesses.size();
    for (std::size_t i = 0; i < accessCount; ++i) {
      const auto &access = accesses[i];
      append32(accessBytes, access.pc);
      append32(accessBytes, access.address);
      append32(accessBytes, access.value);
      append64(accessBytes, access.operation);
      append8(accessBytes, access.width);
      append8(accessBytes, access.known_mask);
      append8(accessBytes, access.kind);
    }
    const std::size_t pcCount =
        progress.instruction_events < pcs.size() ? progress.instruction_events
                                                 : pcs.size();
    for (std::size_t i = 0; i < pcCount; ++i)
      append32(pcBytes, pcs[i]);
    for (std::size_t i = 0; i < call_count; ++i) {
      const auto &call = calls[i];
      append32(callBytes, call.event.pc);
      append32(callBytes, call.event.delay_slot_pc);
      append32(callBytes, call.event.entry);
      append64(callBytes, call.event.operation);
      append64(callBytes, call.event.invocation);
      append8(callBytes, call.event.site);
      append8(callBytes, call.event.argument_count);
      append8(callBytes, call.event.target_program);
      serializeMachine(callBytes, call.machine);
    }

    std::ostringstream out;
    out << "{\"routine\":\"GAMELOAD:801E136C-801E140F\",";
    out << "\"source_bytes\":164,\"source_instructions\":41,";
    out << "\"source_sha256\":\"a2d2a4b742c47b1c72d89e7c8b2ddbada0fee604cef947e11914515653e82398\",";
    out << "\"fixture_contract\":{\"memory\":\"synthetic standalone 2 MiB "
           "main memory with byte-knownness\",\"machine\":\"synthetic full "
           "34-word machine\",\"startup_80015098\":1,";
    out << "\"copy_size\":" << CopySize
        << ",\"loader_entry\":" << Gameonly
        << ",\"direct_services\":\"startup writes; two bounded byte copies; "
           "synthetic GAMEONLY loader; other direct services preserve full "
           "CPU and RAM\",\"gameonly_service\":\"refused/unbound\"},";
    out << "\"result\":" << result << ",\"completed\":"
        << unsigned(progress.completed) << ",\"transferred\":"
        << unsigned(progress.transferred) << ",\"gameplay_shown\":\"BLOCKED\",";
    out << "\"stopped_pc\":" << progress.stopped_pc
        << ",\"stopped_target\":" << progress.stopped_target
        << ",\"operations\":" << progress.operations
        << ",\"accesses\":" << progress.accesses
        << ",\"callbacks_completed\":" << progress.callbacks_completed
        << ",\"call_attempts\":" << callAttempts
        << ",\"call_completions\":" << callCompletions << ',';
    out << "\"instruction_count\":" << progress.instruction_count
        << ",\"instruction_events\":" << progress.instruction_events
        << ",\"access_events\":" << progress.access_events
        << ",\"copies\":" << copies << ",\"staged_checksum\":"
        << staged_checksum << ",\"loaded_checksum\":" << loaded_checksum
        << ",\"restored_checksum\":" << restored_checksum
        << ",\"call_overflow\":" << unsigned(call_overflow)
        << ",\"contract_failure\":" << unsigned(contract_failure) << ',';
    out << "\"canonical_fingerprint\":{\"algorithm\":\"FNV-1a-64\",";
    out << "\"seed\":\"cbf29ce484222325\",\"endianness\":\"little\",";
    out << "\"machine_layout\":\"34*(u32 word,u8 known)=170 bytes\",";
    out << "\"access_layout\":\"u32 pc,address,value;u64 operation;u8 "
           "width,known,kind\",\"pc_layout\":\"u32 pc\",";
    out << "\"call_layout\":\"u32 pc,delay,target;u64 operation,invocation;u8 "
           "site,argc,program;170-byte machine\",\"input_machine\":\""
        << hex64(hash(machineInput)) << "\",\"output_machine\":\""
        << hex64(hash(machineOutput)) << "\",\"accesses\":\""
        << hex64(hash(accessBytes)) << "\",\"pcs\":\""
        << hex64(hash(pcBytes)) << "\",\"calls\":\""
        << hex64(hash(callBytes)) << "\"},\"call_sequence\":[";
    for (std::size_t i = 0; i < call_count; ++i) {
      if (i)
        out << ',';
      const auto &call = calls[i];
      out << "{\"pc\":" << call.event.pc << ",\"delay\":"
          << call.event.delay_slot_pc << ",\"target\":" << call.event.entry
          << ",\"operation\":" << call.event.operation
          << ",\"invocation\":" << call.event.invocation
          << ",\"site\":" << unsigned(call.event.site)
          << ",\"argc\":" << unsigned(call.event.argument_count)
          << ",\"program\":" << unsigned(call.event.target_program)
          << ",\"machine\":";
      machineJson(out, call.machine);
      out << '}';
    }
    out << "],\"next_unbound_boundary\":{\"first_production\":\"801E1374->801E14B8 startup\",\"remaining_source_children\":[\"801E137C->801E000C GP setup\",\"801E1384->801E059C hardware setup\",\"801E1394->801E0938 registration\",\"801E13B0->801E1344 stage copy\",\"801E13C4->801E1300 GAMEONLY loader\",\"801E13CC->801E1670 interrupt shutdown\",\"801E13E0->801E1344 restore copy\"],\"fixture_stop\":\"801E13F4->80020000 refused synthetic GAMEONLY\"},\"input_machine\":";
    machineJson(out, input);
    out << ",\"output_machine\":";
    machineJson(out, progress.machine);
    out << '}';
    return out.str();
  }
};
} // namespace

std::string captureGameloadMain() {
  Capture capture;
  return capture.run();
}
} // namespace nba97
