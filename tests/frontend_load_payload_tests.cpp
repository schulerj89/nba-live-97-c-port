#include "recovered/frontend_load_payload.h"

#include <array>
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
    throw std::runtime_error("frontend-load-payload failed at " +
                             std::to_string(line));
}
#define CHECK(value) checkAt((value), __LINE__)

constexpr U Base = 0x80000000u;
constexpr U Size = 0x200000u;
constexpr U Sp = 0x801f0000u;
constexpr U Ra = 0x8007b14cu;
constexpr U Descriptor = 0x80170000u;
constexpr U Payload = 0x89abcdefu;
constexpr U Relocated = 0x801ed000u;
constexpr std::array<U, 13> Pcs{{
    0x8007b15cu, 0x8007b160u, 0x8007b164u, 0x8007b168u,
    0x8007b16cu, 0x8007b170u, 0x8007b174u, 0x8007b178u,
    0x8007b17cu, 0x8007b180u, 0x8007b184u, 0x8007b188u,
    0x8007b18cu}};

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendLoadPayloadContext context{};
  Nba97FrontendLoadPayloadProgress progress{};
  std::array<Nba97FrontendLoadPayloadAccess, 8> access{};
  std::array<U, 20> instructions{};
  Nba97FrontendLoadPayloadWord child_return{Descriptor, 15};
  Nba97FrontendLoadPayloadEvent event{};
  unsigned callbacks = 0;
  bool refuse = false;
  bool malformed_machine = false;
  bool relocate = false;
  bool mutate_machine = false;
  bool override_sp = false;
  bool malformed_saved_plane = false;
  Nba97FrontendLoadPayloadWord callback_sp{};
  U mutate_saved_ra = 0;
  std::uint8_t mutate_saved_ra_mask = 15;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {
          0x42000000u + i * 0x101u, std::uint8_t((i % 15u) + 1u)};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[4] = {0x80024854u, 13};
    context.machine.registers.gpr[5] = {0x11223344u, 6};
    context.machine.registers.gpr[6] = {1, 15};
    context.machine.registers.gpr[29] = {Sp, 15};
    context.machine.registers.gpr[31] = {Ra, 15};
    context.machine.hi = {0x12345678u, 5};
    context.machine.lo = {0x90abcdefu, 10};
    put(Descriptor, Payload);
    context.memory = {&region, 1};
    context.operation_budget = 4;
    context.io = callback;
    context.user = this;
    context.access_journal = access.data();
    context.access_journal_capacity = access.size();
    context.instruction_journal = instructions.data();
    context.instruction_journal_capacity = instructions.size();
  }

  bool extent(U address, U width = 4) const {
    return address >= Base && width <= Size && address - Base <= Size - width;
  }
  void put(U address, U value, std::uint8_t mask = 15) {
    if (!extent(address)) throw std::runtime_error("fixture write outside RAM");
    for (unsigned i = 0; i < 4; ++i) {
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
      if (region.known)
        known[address - Base + i] = std::uint8_t((mask >> i) & 1u);
    }
  }
  U get(U address) const {
    if (!extent(address)) throw std::runtime_error("fixture read outside RAM");
    U value = 0;
    for (unsigned i = 0; i < 4; ++i)
      value |= U(bytes[address - Base + i]) << (i * 8u);
    return value;
  }
  int run() { return nba97_frontend_load_payload(&context, &progress); }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97FrontendLoadPayloadEvent *event,
                      Nba97FrontendLoadPayloadMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    ++f.callbacks;
    if (!event || !machine || event->pc != 0x8007b164u ||
        event->delay_slot_pc != 0x8007b168u ||
        event->entry != 0x8007b1d0u || event->operation != 2 ||
        event->invocation != 1 ||
        event->site != NBA97_FRONTEND_LOAD_PAYLOAD_SITE_8007B164 ||
        event->argument_count != 3 ||
        event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
        machine->registers.gpr[4].word != 0x80024854u ||
        machine->registers.gpr[4].known_mask != 13 ||
        machine->registers.gpr[5].word != 0x11223344u ||
        machine->registers.gpr[5].known_mask != 6 ||
        machine->registers.gpr[6].word != 1 ||
        machine->registers.gpr[6].known_mask != 15 ||
        machine->registers.gpr[31].word != 0x8007b16cu ||
        machine->registers.gpr[31].known_mask != 15)
      return 0;
    f.event = *event;
    if (f.relocate) {
      const U old_frame = machine->registers.gpr[29].word;
      if (!f.extent(old_frame, 24) || !f.extent(Relocated, 24)) return 0;
      for (U i = 0; i < 24; ++i) {
        f.bytes[Relocated - Base + i] = f.bytes[old_frame - Base + i];
        if (f.region.known)
          f.known[Relocated - Base + i] = f.known[old_frame - Base + i];
      }
      machine->registers.gpr[29] = {Relocated, 15};
    }
    if (f.override_sp)
      machine->registers.gpr[29] = f.callback_sp;
    if (f.mutate_saved_ra)
      f.put(machine->registers.gpr[29].word + 16u, f.mutate_saved_ra,
            f.mutate_saved_ra_mask);
    if (f.malformed_saved_plane) {
      const U address = machine->registers.gpr[29].word + 16u;
      if (!f.extent(address)) return 0;
      f.known[address - Base + 3u] = 2;
    }
    if (f.mutate_machine) {
      machine->registers.gpr[8] = {0x55667788u, 3};
      machine->hi = {0xaabbccddu, 7};
      machine->lo = {0x10293847u, 11};
    }
    machine->registers.gpr[2] = f.child_return;
    if (f.malformed_machine) machine->registers.gpr[9].known_mask = 16;
    return f.refuse ? 0 : 1;
  }
};

void normalPathsAndAllPcs() {
  std::array<bool, 13> seen{};
  auto mark = [&](const Fixture &f) {
    for (std::size_t i = 0; i < f.progress.instruction_events; ++i)
      for (unsigned pc = 0; pc < Pcs.size(); ++pc)
        if (f.instructions[i] == Pcs[pc]) seen[pc] = true;
  };

  Fixture nonnull;
  nonnull.mutate_machine = true;
  CHECK(nonnull.run() == NBA97_TEXT_COMPLETE && nonnull.progress.completed &&
        nonnull.progress.operations == 4 && nonnull.progress.accesses == 3 &&
        nonnull.progress.reads == 2 && nonnull.progress.stores == 1 &&
        nonnull.progress.callbacks_completed == 1 &&
        nonnull.progress.instruction_count == 11 && nonnull.callbacks == 1 &&
        nonnull.progress.payload_result.word == Payload &&
        nonnull.progress.payload_result.known_mask == 15 &&
        nonnull.progress.machine.registers.gpr[2].word == Payload &&
        nonnull.progress.machine.registers.gpr[8].word == 0x55667788u &&
        nonnull.progress.machine.hi.word == 0xaabbccddu &&
        nonnull.progress.machine.lo.word == 0x10293847u &&
        nonnull.progress.machine.registers.gpr[29].word == Sp &&
        nonnull.progress.machine.registers.gpr[31].word == Ra);
  CHECK(nonnull.progress.forwarded_a0.word == 0x80024854u &&
        nonnull.progress.forwarded_a0.known_mask == 13 &&
        nonnull.progress.forwarded_a1.word == 0x11223344u &&
        nonnull.progress.forwarded_a1.known_mask == 6 &&
        nonnull.progress.forwarded_a2.word == 1 &&
        nonnull.progress.saved_return_address.word == Ra &&
        nonnull.progress.child_return.word == Descriptor);
  CHECK(nonnull.access[0].pc == 0x8007b160u &&
        nonnull.access[0].address == Sp - 8u &&
        nonnull.access[0].kind == NBA97_FRONTEND_LOAD_PAYLOAD_STORE &&
        nonnull.access[1].pc == 0x8007b17cu &&
        nonnull.access[1].address == Descriptor &&
        nonnull.access[1].value == Payload &&
        nonnull.access[2].pc == 0x8007b180u &&
        nonnull.access[2].address == Sp - 8u);
  mark(nonnull);

  Fixture null_result;
  null_result.child_return = {0, 15};
  CHECK(null_result.run() == NBA97_TEXT_COMPLETE &&
        null_result.progress.completed && null_result.progress.operations == 3 &&
        null_result.progress.accesses == 2 && null_result.progress.reads == 1 &&
        null_result.progress.stores == 1 &&
        null_result.progress.instruction_count == 12 &&
        null_result.progress.child_return.word == 0 &&
        null_result.progress.payload_result.word == 0 &&
        null_result.progress.payload_result.known_mask == 15 &&
        null_result.progress.machine.registers.gpr[2].word == 0);
  mark(null_result);
  for (bool value : seen) CHECK(value);
}

void pointerAndPayloadKnownness() {
  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture f;
    f.child_return.known_mask = std::uint8_t(mask);
    const bool definite_nonzero = (mask & 12u) != 0;
    const int expected = mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN;
    CHECK(f.run() == expected);
    if (mask != 15) {
      CHECK(f.progress.operations == 2 && f.progress.accesses == 1 &&
            f.progress.callbacks_completed == 1);
      if (definite_nonzero)
        CHECK(f.progress.stopped_pc == 0x8007b17cu &&
              f.progress.stopped_address == Descriptor);
      else
        CHECK(f.progress.stopped_pc == 0x8007b16cu &&
              f.progress.stopped_address == 0);
    }
  }
  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture f;
    f.put(Descriptor, Payload, std::uint8_t(mask));
    CHECK(f.run() == NBA97_TEXT_COMPLETE &&
          f.progress.payload_result.word == Payload &&
          f.progress.payload_result.known_mask == mask &&
          f.progress.machine.registers.gpr[2].known_mask == mask);
  }
  for (unsigned mask = 0; mask < 15; ++mask) {
    Fixture f;
    f.child_return = {0, std::uint8_t(mask)};
    CHECK(f.run() == NBA97_TEXT_UNKNOWN &&
          f.progress.stopped_pc == 0x8007b16cu &&
          f.progress.operations == 2);
  }
}

void aliasesRelocationAndReturnState() {
  Fixture alias;
  alias.child_return = {Sp - 8u, 15};
  CHECK(alias.run() == NBA97_TEXT_COMPLETE &&
        alias.progress.payload_result.word == Ra &&
        alias.access[0].address == Sp - 8u &&
        alias.access[1].address == Sp - 8u &&
        alias.access[2].address == Sp - 8u);

  Fixture relocated;
  relocated.relocate = true;
  relocated.child_return = {Relocated + 16u, 15};
  CHECK(relocated.run() == NBA97_TEXT_COMPLETE &&
        relocated.progress.payload_result.word == Ra &&
        relocated.progress.restored_return_address.word == Ra &&
        relocated.progress.machine.registers.gpr[29].word == Relocated + 24u &&
        relocated.access[1].address == Relocated + 16u &&
        relocated.access[2].address == Relocated + 16u);

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture f;
    f.context.machine.registers.gpr[31].known_mask = std::uint8_t(mask);
    f.child_return = {0, 15};
    const int expected = mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN;
    CHECK(f.run() == expected && f.progress.operations == 3 &&
          f.progress.machine.registers.gpr[2].word == 0 &&
          f.progress.machine.registers.gpr[31].known_mask == mask);
    if (mask != 15)
      CHECK(f.progress.stopped_pc == 0x8007b188u &&
            f.progress.stopped_target == Ra);
  }
  Fixture bad_ra;
  bad_ra.context.machine.registers.gpr[31] = {Ra + 1u, 15};
  bad_ra.child_return = {0, 15};
  CHECK(bad_ra.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        bad_ra.progress.operations == 3 &&
        bad_ra.progress.stopped_pc == 0x8007b188u &&
        bad_ra.progress.stopped_target == Ra + 1u);
}

void budgetsAndFailurePrefixes() {
  for (unsigned budget = 0; budget < 4; ++budget) {
    Fixture f;
    f.context.operation_budget = budget;
    CHECK(f.run() == NBA97_TEXT_LIMIT && f.progress.operations == budget &&
          !f.progress.completed);
    const std::array<U, 4> stop_pc{{0x8007b160u, 0x8007b164u,
                                    0x8007b17cu, 0x8007b180u}};
    CHECK(f.progress.stopped_pc == stop_pc[budget]);
  }
  for (unsigned budget = 0; budget < 3; ++budget) {
    Fixture f;
    f.child_return = {0, 15};
    f.context.operation_budget = budget;
    CHECK(f.run() == NBA97_TEXT_LIMIT && f.progress.operations == budget);
    const std::array<U, 3> stop_pc{{0x8007b160u, 0x8007b164u,
                                    0x8007b180u}};
    CHECK(f.progress.stopped_pc == stop_pc[budget]);
  }

  Fixture refused;
  refused.refuse = true;
  refused.mutate_machine = true;
  CHECK(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.progress.operations == 2 && refused.progress.stores == 1 &&
        refused.progress.callbacks_completed == 0 &&
        refused.progress.machine.registers.gpr[8].word == 0x55667788u &&
        refused.progress.machine.registers.gpr[2].word == Descriptor &&
        refused.progress.stopped_pc == 0x8007b164u &&
        refused.progress.stopped_target == 0x8007b1d0u);
  Fixture malformed;
  malformed.malformed_machine = true;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.operations == 2 &&
        malformed.progress.callbacks_completed == 0 &&
        malformed.progress.machine.registers.gpr[9].known_mask == 16);
}

void memoryStackAndArguments() {
  Fixture malformed_store;
  malformed_store.known[Sp - 8u - Base + 3u] = 2;
  CHECK(malformed_store.run() == NBA97_TEXT_ARGUMENT &&
        malformed_store.progress.operations == 1 &&
        malformed_store.progress.stores == 0 &&
        malformed_store.progress.stopped_pc == 0x8007b160u);
  Fixture malformed_payload;
  malformed_payload.known[Descriptor - Base + 3u] = 2;
  CHECK(malformed_payload.run() == NBA97_TEXT_ARGUMENT &&
        malformed_payload.progress.operations == 3 &&
        malformed_payload.progress.reads == 0 &&
        malformed_payload.progress.stopped_pc == 0x8007b17cu);
  Fixture malformed_restore;
  malformed_restore.child_return = {0, 15};
  malformed_restore.mutate_saved_ra = Ra;
  malformed_restore.mutate_saved_ra_mask = 7;
  CHECK(malformed_restore.run() == NBA97_TEXT_UNKNOWN &&
        malformed_restore.progress.stopped_pc == 0x8007b188u);
  Fixture invalid_restore_plane;
  invalid_restore_plane.child_return = {0, 15};
  invalid_restore_plane.malformed_saved_plane = true;
  CHECK(invalid_restore_plane.run() == NBA97_TEXT_ARGUMENT &&
        invalid_restore_plane.progress.operations == 3 &&
        invalid_restore_plane.progress.reads == 0 &&
        invalid_restore_plane.progress.stopped_pc == 0x8007b180u);

  Fixture absent;
  absent.region.known = nullptr;
  CHECK(absent.run() == NBA97_TEXT_COMPLETE &&
        absent.progress.payload_result.known_mask == 15);
  Fixture absent_partial;
  absent_partial.region.known = nullptr;
  absent_partial.context.machine.registers.gpr[31].known_mask = 7;
  CHECK(absent_partial.run() == NBA97_TEXT_ARGUMENT &&
        absent_partial.progress.operations == 1 &&
        absent_partial.progress.stores == 0);

  Fixture misaligned_sp;
  misaligned_sp.context.machine.registers.gpr[29] = {Sp + 1u, 15};
  CHECK(misaligned_sp.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misaligned_sp.progress.stopped_pc == 0x8007b160u);
  Fixture unknown_sp;
  unknown_sp.context.machine.registers.gpr[29].known_mask = 14;
  CHECK(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.operations == 0 &&
        unknown_sp.progress.stopped_pc == 0x8007b160u);
  Fixture callback_unknown_sp;
  callback_unknown_sp.child_return = {0, 15};
  callback_unknown_sp.override_sp = true;
  callback_unknown_sp.callback_sp = {Sp - 24u, 14};
  CHECK(callback_unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        callback_unknown_sp.progress.operations == 2 &&
        callback_unknown_sp.progress.stopped_pc == 0x8007b180u);
  Fixture callback_misaligned_sp;
  callback_misaligned_sp.child_return = {0, 15};
  callback_misaligned_sp.override_sp = true;
  callback_misaligned_sp.callback_sp = {Sp - 23u, 15};
  CHECK(callback_misaligned_sp.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        callback_misaligned_sp.progress.operations == 3 &&
        callback_misaligned_sp.progress.stopped_pc == 0x8007b180u);
  Fixture callback_unmapped_sp;
  callback_unmapped_sp.child_return = {0, 15};
  callback_unmapped_sp.override_sp = true;
  callback_unmapped_sp.callback_sp = {0x80200000u, 15};
  CHECK(callback_unmapped_sp.run() == NBA97_TEXT_RESOURCE &&
        callback_unmapped_sp.progress.operations == 3 &&
        callback_unmapped_sp.progress.stopped_pc == 0x8007b180u &&
        callback_unmapped_sp.progress.stopped_address == 0x80200010u);
  Fixture missing_descriptor;
  missing_descriptor.child_return = {0x80200000u, 15};
  CHECK(missing_descriptor.run() == NBA97_TEXT_RESOURCE &&
        missing_descriptor.progress.stopped_pc == 0x8007b17cu &&
        missing_descriptor.progress.stopped_address == 0x80200000u);
  Fixture misaligned_descriptor;
  misaligned_descriptor.child_return = {Descriptor + 1u, 15};
  CHECK(misaligned_descriptor.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misaligned_descriptor.progress.stopped_pc == 0x8007b17cu);

  Fixture overlap;
  std::array<std::uint8_t, 4> extra{};
  Nba97GameTextRegion regions[2]{{Base, overlap.bytes.data(),
                                  overlap.known.data(), overlap.bytes.size()},
                                 {Descriptor, extra.data(), nullptr,
                                  extra.size()}};
  overlap.context.memory = {regions, 2};
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT);
  Fixture wrapping;
  wrapping.region.base = UINT32_MAX;
  wrapping.region.size = 4;
  CHECK(wrapping.run() == NBA97_TEXT_ARGUMENT);

  Fixture no_io;
  no_io.context.io = nullptr;
  CHECK(no_io.run() == NBA97_TEXT_IO_REFUSED && no_io.callbacks == 0 &&
        no_io.progress.operations == 2 && no_io.progress.stores == 1);
}

void wrappingSpAndRepeatability() {
  for (U entry_sp : {0u, 0x10u}) {
    Fixture f;
    std::array<std::uint8_t, 4> stack{};
    std::array<std::uint8_t, 4> stack_known{{1, 1, 1, 1}};
    const U save = entry_sp - 8u;
    Nba97GameTextRegion stack_region{save, stack.data(), stack_known.data(), 4};
    f.context.memory = {&stack_region, 1};
    f.context.machine.registers.gpr[29] = {entry_sp, 15};
    f.child_return = {0, 15};
    CHECK(f.run() == NBA97_TEXT_COMPLETE &&
          f.progress.frame_stack_pointer == entry_sp - 24u &&
          f.progress.machine.registers.gpr[29].word == entry_sp &&
          f.progress.machine.registers.gpr[31].word == Ra);
  }
  Fixture max_sp;
  max_sp.context.machine.registers.gpr[29] = {UINT32_MAX, 15};
  CHECK(max_sp.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        max_sp.progress.stopped_address == UINT32_MAX - 8u);

  Fixture repeated;
  CHECK(repeated.run() == NBA97_TEXT_COMPLETE &&
        repeated.progress.machine.registers.gpr[2].word == Payload);
  repeated.put(Descriptor, 0x10203040u, 5);
  CHECK(repeated.run() == NBA97_TEXT_COMPLETE &&
        repeated.progress.machine.registers.gpr[2].word == 0x10203040u &&
        repeated.progress.machine.registers.gpr[2].known_mask == 5);
}
} // namespace

int main() {
  try {
    normalPathsAndAllPcs();
    pointerAndPayloadKnownness();
    aliasesRelocationAndReturnState();
    budgetsAndFailurePrefixes();
    memoryStackAndArguments();
    wrappingSpAndRepeatability();
    std::printf("frontend_load_payload_tests: PASS (%u checks)\n", checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
