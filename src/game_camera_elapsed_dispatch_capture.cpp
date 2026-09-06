#include "game_camera_elapsed_dispatch_capture.h"
#include "game_camera_state_lookup_adapter.h"
#include "game_camera_remainder_gate_adapter.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>
namespace nba97 {
namespace {
std::uint32_t read(const Nba97GameTextMemory &m, std::uint32_t address) {
  for (std::size_t i = 0; i < m.count; ++i) {
    const auto &r = m.region[i];
    if (address < r.base || std::uint64_t(address - r.base) + 4 > r.size)
      continue;
    std::uint32_t value = 0;
    for (unsigned j = 0; j < 4; ++j) {
      const auto at = address - r.base + j;
      if (r.known && r.known[at] != 1)
        throw std::runtime_error("elapsed capture unknown state");
      value |= std::uint32_t(r.data[at]) << (8 * j);
    }
    return value;
  }
  throw std::runtime_error("elapsed capture unmapped state");
}
struct Fixture {
  unsigned calls = 0;
  Nba97GameCameraStateLookupBinding lookup{};
  Fixture() { nba97_game_camera_state_lookup_binding_init(&lookup, 4); }
  static int child(void *u, const Nba97GameTextMemory *memory,
                   const Nba97GameCameraElapsedDispatchEvent *e,
                   Nba97GameCameraElapsedDispatchMachine *m) {
    auto &f = *static_cast<Fixture *>(u);
    ++f.calls;
    if (e->pc != 0x8007999c || e->delay_slot_pc != 0x800799a0 ||
        e->entry != 0x8007a410 || e->argument_count != 0 ||
        m->registers.gpr[31].word != 0x800799a4 ||
        m->registers.gpr[31].known_mask != 15)
      return 0;
    return nba97_game_camera_state_lookup_from_elapsed_dispatch(
        &f.lookup, memory, e, m);
  }
};

std::string captureRemainderGate(const Nba97GameTextMemory &memory,
                               const Nba97GameCameraElapsedDispatchMachine &machine) {
  Nba97GameCameraElapsedDispatchContext context{};
  context.memory = memory;
  context.machine = machine;
  // An explicitly requested second diagnostic dispatch uses the actual first
  // dispatch's full return and retained cache. This is not a match-loop tick.
  context.machine.registers.gpr[4] = {UINT32_MAX, 15};
  context.operation_budget = 100;
  Nba97GameCameraElapsedDispatchProgress parent{};
  Nba97GameCameraRemainderGateBinding gate{};
  Nba97GameCameraStateLookupBinding lookup{};
  nba97_game_camera_remainder_gate_binding_init(&gate, 2);
  nba97_game_camera_state_lookup_binding_init(&lookup, 4);
  const int result = nba97_game_camera_elapsed_dispatch_with_remainder_gate(
      &context, &gate, &lookup, &parent);
  const auto &p = gate.progress;
  const bool keep = p.source_value.word == 0;
  if (result != NBA97_TEXT_COMPLETE || !parent.completed || !p.completed ||
      gate.completions != 1 || gate.event.pc != 0x80079978 ||
      p.operations != 1 || p.reads != 1 || p.returned_value.known_mask != 15 ||
      p.returned_value.word != unsigned(keep) ||
      p.source_value.word != (keep ? 0u : 0xffffff00u) ||
      lookup.completions != unsigned(!keep) || read(memory, 0x800d8eec) != 42 ||
      read(memory, 0x800bc1f4) != 42 || read(memory, 0x80106074) != 0)
    throw std::runtime_error("camera remainder native composition drifted");
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x8007A468\","
       "\"inclusive_end\":\"0x8007A497\",\"bytes\":48,\"instructions\":12,"
       "\"classification\":\"no direct visual effect\",\"completed\":true,"
       "\"parent_completed\":true,\"same_parent_memory\":true,"
       "\"scope\":\"explicit second elapsed dispatch using first full return; no advancing match\","
       "\"call_pc\":" << gate.event.pc << ",\"source\":" << p.source_value.word
    << ",\"remainder\":" << p.remainder_value.word
    << ",\"returned_value\":" << p.returned_value.word
    << ",\"instruction_count\":" << p.instruction_count
    << ",\"operations\":" << p.operations << ",\"reads\":" << p.reads
    << ",\"lookup_completions\":" << lookup.completions
    << ",\"cache_after\":42,\"publication_after\":42,\"elapsed_after\":0,"
       "\"sp\":" << p.machine.registers.gpr[29].word
    << ",\"ra\":" << p.machine.registers.gpr[31].word << "}";
  return o.str();
}
} // namespace
int GameCameraElapsedDispatchCapture::dispatch(
    const Nba97GameTextMemory *memory, const Nba97GameCameraSelectEvent *event,
    Nba97GameCameraSelectRegisters *registers) {
  if (!memory || !event || !registers || !receipt.empty())
    return 0;
  const auto before = read(*memory, 0x80106074);
  const auto cacheBefore = read(*memory, 0x800bc1f4);
  const auto publicationBefore = read(*memory, 0x800d8eec);
  Fixture f;
  Nba97GameCameraElapsedDispatchBinding binding{};
  nba97_game_camera_elapsed_dispatch_binding_init(&binding, 64);
  binding.io = Fixture::child;
  binding.user = &f;
  const int result = nba97_game_camera_elapsed_dispatch_from_camera_select(
      &binding, memory, event, registers);
  const auto &p = binding.progress;
  const auto &k = f.lookup.progress;
  if (result != 1 || !p.completed || binding.invocations != 1 || f.calls != 1 ||
      p.operations != 14 || p.reads != 8 || p.stores != 5 ||
      p.instruction_count != 48 || read(*memory, 0x80106074) != 0 ||
      read(*memory, 0x800bc1f4) != 42 || read(*memory, 0x800d8eec) != 42 ||
      p.machine.hi.known_mask || p.machine.lo.known_mask || !k.completed ||
      f.lookup.completions != 1 || k.operations != 2 || k.reads != 2 ||
      k.returned_value.word != 42 || k.returned_value.known_mask != 15)
    throw std::runtime_error("camera elapsed native composition drifted: caller=" +
        std::to_string(event->pc) + " result=" + std::to_string(result) +
        " lookup_result=" + std::to_string(f.lookup.result) +
        " source=" + std::to_string(k.source_value.word) +
        " address=" + std::to_string(k.lookup_address.word) +
        " raw_return=" + std::to_string(k.returned_value.word));
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x800798B4\",\"inclusive_end\":"
       "\"0x800799CB\",\"bytes\":280,\"instructions\":70,\"classification\":"
       "\"no direct visual effect\",\"scope\":\"actual camera-selector caller "
       "and elapsed and lookup owners on same retained memory; explicit generated "
       "thresholds and signed tables; no advancing "
       "match\",\"completed\":true,\"call_pc\":"
    << event->pc << ",\"delay_pc\":" << event->delay_slot_pc
    << ",\"requested_delta\":" << p.requested_delta.word
    << ",\"elapsed_before\":" << before
    << ",\"elapsed_after\":0,\"cache_before\":" << cacheBefore
    << ",\"cache_after\":42,\"publication_before\":" << publicationBefore
    << ",\"publication_after\":42,\"operations\":" << p.operations
    << ",\"reads\":" << p.reads << ",\"stores\":" << p.stores
    << ",\"callbacks\":" << p.callbacks_completed
    << ",\"instruction_count\":" << p.instruction_count
    << ",\"return_address\":" << p.restored_return_address.word
    << ",\"sp\":" << p.machine.registers.gpr[29].word
    << ",\"hilo_known_masks\":[0,0],\"child_pc\":2147981724,\"state_lookup\":{"
       "\"program\":\"GAMEONLY\",\"address\":\"0x8007A410\",\"inclusive_end\":\"0x8007A467\","
       "\"bytes\":88,\"instructions\":22,\"classification\":\"no direct visual effect\","
       "\"completed\":true,\"same_parent_memory\":true,\"call_pc\":" << f.lookup.event.pc
    << ",\"source\":" << k.source_value.word
    << ",\"signed_index\":" << k.signed_index.word
    << ",\"negative_table\":" << unsigned(k.negative_table)
    << ",\"lookup_address\":" << k.lookup_address.word
    << ",\"raw_return\":" << k.returned_value.word
    << ",\"instruction_count\":" << k.instruction_count
    << ",\"operations\":" << k.operations << ",\"reads\":" << k.reads
    << ",\"sp\":" << k.machine.registers.gpr[29].word
    << ",\"ra\":" << k.machine.registers.gpr[31].word << "}}";
  receipt = o.str();
  receipt.pop_back();
  receipt += ",\"remainder_gate\":" + captureRemainderGate(*memory, p.machine) + "}";
  return result;
}
} // namespace nba97
