#include "recovered/gameload_main.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

namespace {
using U = std::uint32_t;
unsigned checks;

void checkAt(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "gameload main check %u failed at line %u\n", checks,
                 line);
    std::exit(1);
  }
}
#define CHECK(value) checkAt((value), __LINE__)

constexpr U Base = 0x80000000u;
constexpr U Size = 0x200000u;
constexpr U EntrySp = 0x801ff000u;
constexpr U Frame = EntrySp - 24u;
constexpr U EntryRa = 0x801e14b4u;
constexpr U SizeWord = 0x80015004u;
constexpr U TargetWord = 0x80015000u;
constexpr U Gameonly = 0x8002a000u;

constexpr std::array<U, 9> CallPcs{{
    0x801e1374u, 0x801e137cu, 0x801e1384u, 0x801e1394u,
    0x801e13b0u, 0x801e13c4u, 0x801e13ccu, 0x801e13e0u,
    0x801e13f4u}};
constexpr std::array<U, 9> Targets{{
    0x801e14b8u, 0x801e000cu, 0x801e059cu, 0x801e0938u,
    0x801e1344u, 0x801e1300u, 0x801e1670u, 0x801e1344u,
    Gameonly}};
constexpr std::array<std::uint8_t, 9> Argc{{0, 0, 0, 2, 3, 2, 0, 3, 0}};
constexpr std::array<U, 6> AccessPcs{{
    0x801e1370u, 0x801e1378u, 0x801e13a0u,
    0x801e13ecu, 0x801e13fcu, 0x801e1400u}};
constexpr std::array<U, 6> AccessAddresses{{
    Frame + 20u, Frame + 16u, SizeWord, TargetWord,
    Frame + 20u, Frame + 16u}};
constexpr std::array<std::size_t, 6> AccessOperations{{1, 2, 7, 12, 14,
                                                       15}};

bool same(const Nba97GameloadMainWord &a,
          const Nba97GameloadMainWord &b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}

bool same(const Nba97GameloadMainMachine &a,
          const Nba97GameloadMainMachine &b) {
  for (unsigned i = 0; i < 32; ++i)
    if (!same(a.registers.gpr[i], b.registers.gpr[i]))
      return false;
  return same(a.hi, b.hi) && same(a.lo, b.lo);
}

struct Seen {
  Nba97GameloadMainEvent event{};
  Nba97GameloadMainMachine machine{};
};

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size, 0xcd);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameloadMainAccess, 16> accesses{};
  std::array<U, 64> instructions{};
  Nba97GameloadMainContext context{};
  Nba97GameloadMainProgress progress{};
  Nba97GameloadMainMachine initial{};
  std::vector<Seen> calls;
  U copy_size = 0x10203040u;
  std::uint8_t copy_mask = 15;
  U gameonly = Gameonly;
  std::uint8_t gameonly_mask = 15;
  unsigned refuse_site = 0;
  unsigned transfer_site = 0;
  unsigned invalid_mode = 0;
  unsigned relocated_mode = 0;
  unsigned return_ra_mask = 15;
  U return_ra_word = 0x80001000u;
  bool mutate_second_length = false;
  bool mutate_loaded_target = false;
  bool mutate_dynamic_machine = false;
  bool malformed_restore_byte = false;
  U malformed_known_address = 0;
  U second_length = 0x55667788u;
  std::uint8_t second_length_mask = 5;
  U relocated_frame = Frame - 0x200u;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      initial.registers.gpr[i] = {
          0x21000000u + i * 0x01010101u,
          static_cast<std::uint8_t>(i & 15u)};
    initial.registers.gpr[0] = {0, 15};
    initial.registers.gpr[NBA97_GAMELOAD_MAIN_SP] = {EntrySp, 15};
    initial.registers.gpr[NBA97_GAMELOAD_MAIN_RA] = {EntryRa, 15};
    initial.registers.gpr[NBA97_GAMELOAD_MAIN_S0].known_mask = 9;
    initial.hi = {0x10203040u, 5};
    initial.lo = {0x50607080u, 10};
    context.memory = {&region, 1};
    context.operation_budget = 100;
    context.machine = initial;
    context.io = callback;
    context.user = this;
    context.access_journal = accesses.data();
    context.access_journal_capacity = accesses.size();
    context.instruction_journal = instructions.data();
    context.instruction_journal_capacity = instructions.size();
    resetMemory();
  }

  void put(U address, U value, std::uint8_t mask = 15) {
    CHECK(address >= Base && std::uint64_t(address - Base) + 4u <= Size);
    for (unsigned i = 0; i < 4; ++i) {
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
      if (region.known)
        known[address - Base + i] = std::uint8_t((mask >> i) & 1u);
    }
  }

  U get(U address) const {
    CHECK(address >= Base && address - Base <= Size-4);
    U value = 0;
    for (unsigned i = 0; i < 4; ++i)
      value |= U(bytes[address - Base + i]) << (i * 8u);
    return value;
  }

  void copyFrame(U destination) {
    CHECK(destination >= Base && destination-Base <= Size-24 && Frame-Base <= Size-24);
    for (unsigned i = 0; i < 24; ++i) {
      bytes[destination - Base + i] = bytes[Frame - Base + i];
      if (region.known)
        known[destination - Base + i] = known[Frame - Base + i];
    }
  }

  void resetMemory() {
    put(SizeWord, copy_size, copy_mask);
    put(TargetWord, gameonly, gameonly_mask);
  }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameloadMainEvent *event,
                      Nba97GameloadMainMachine *machine,
                      Nba97GameloadMainCalleeOutcome *outcome) {
    auto &f = *static_cast<Fixture *>(opaque);
    if (!event || !machine || !outcome || event->site == 0 ||
        event->site >= NBA97_GAMELOAD_MAIN_SITE_COUNT)
      return 0;
    const unsigned index = event->site - 1u;
    if (event->pc != CallPcs[index] ||
        event->delay_slot_pc != event->pc + 4u ||
        event->argument_count != Argc[index] ||
        event->entry != (index == 8 ? f.gameonly : Targets[index]) ||
        event->target_program !=
            (index == 8 ? NBA97_GAMELOAD_MAIN_PROGRAM_GAMEONLY
                        : NBA97_GAMELOAD_MAIN_PROGRAM_GAMELOAD) ||
        event->operation != index + 3u + (index >= 4 ? 1u : 0u) +
                                (index >= 8 ? 1u : 0u) ||
        event->invocation != 1 ||
        machine->registers.gpr[NBA97_GAMELOAD_MAIN_RA].known_mask != 15 ||
        machine->registers.gpr[NBA97_GAMELOAD_MAIN_RA].word != event->pc + 8u)
      return 0;
    f.calls.push_back({*event, *machine});
    if (f.refuse_site == event->site)
      return 0;
    if (event->site == NBA97_GAMELOAD_MAIN_SITE_801E13CC &&
        f.mutate_second_length) {
      machine->registers.gpr[NBA97_GAMELOAD_MAIN_S0] = {
          f.second_length, f.second_length_mask};
    }
    if (event->site == NBA97_GAMELOAD_MAIN_SITE_801E13E0 &&
        f.mutate_loaded_target)
      f.put(TargetWord, f.gameonly, f.gameonly_mask);
    if (event->site == NBA97_GAMELOAD_MAIN_SITE_801E13F4) {
      if (f.mutate_loaded_target)
        f.put(TargetWord, 0x80030000u, 15);
      if (f.relocated_mode == 1) {
        f.copyFrame(f.relocated_frame);
        machine->registers.gpr[NBA97_GAMELOAD_MAIN_SP] = {f.relocated_frame,
                                                          15};
        f.put(f.relocated_frame + 20u, f.return_ra_word,
              static_cast<std::uint8_t>(f.return_ra_mask));
        f.put(f.relocated_frame + 16u, 0x22334455u, 3);
      } else if (f.relocated_mode == 2) {
        machine->registers.gpr[NBA97_GAMELOAD_MAIN_SP] = {Frame + 2u, 15};
      } else if (f.relocated_mode == 3) {
        machine->registers.gpr[NBA97_GAMELOAD_MAIN_SP] = {0x90000000u, 15};
      }
      if (f.mutate_dynamic_machine) {
        for (unsigned i = 1; i < 32; ++i)
          machine->registers.gpr[i] = {
              0xa0000000u + i, static_cast<std::uint8_t>(i & 15u)};
        machine->registers.gpr[NBA97_GAMELOAD_MAIN_SP] = {
            f.relocated_mode == 1 ? f.relocated_frame : Frame, 15};
        machine->hi = {0xaabbccddu, 6};
        machine->lo = {0x11223344u, 9};
      }
      if (f.malformed_restore_byte && f.region.known) {
        U live_sp = machine->registers.gpr[NBA97_GAMELOAD_MAIN_SP].word;
        f.known[live_sp + 20u - Base] = 2;
      }
    }
    if (f.transfer_site == event->site)
      *outcome = NBA97_GAMELOAD_MAIN_CALLEE_TRANSFERRED;
    if (f.invalid_mode == 1)
      machine->registers.gpr[0].word = 1;
    else if (f.invalid_mode == 2)
      machine->registers.gpr[8].known_mask = 16;
    else if (f.invalid_mode == 3)
      machine->hi.known_mask = 16;
    else if (f.invalid_mode == 4)
      machine->lo.known_mask = 16;
    else if (f.invalid_mode == 5)
      *outcome = static_cast<Nba97GameloadMainCalleeOutcome>(99);
    return 1;
  }

  int run() {
    resetMemory();
    if (malformed_known_address)
      known[malformed_known_address - Base] = 2;
    return nba97_gameload_main(&context, &progress);
  }
};

void normalPathsAndExactTrace() {
  Fixture f;
  f.mutate_second_length = true;
  f.mutate_loaded_target = true;
  CHECK(f.run() == NBA97_TEXT_COMPLETE);
  CHECK(f.progress.completed && !f.progress.transferred &&
        f.progress.instruction_count == 41 &&
        f.progress.instruction_events == 41 && f.progress.operations == 15 &&
        f.progress.accesses == 6 && f.progress.reads == 4 &&
        f.progress.stores == 2 && f.progress.callbacks_completed == 9 &&
        f.progress.access_events == 6 && f.calls.size() == 9);
  for (unsigned i = 0; i < 41; ++i)
    CHECK(f.instructions[i] == 0x801e136cu + i * 4u);
  for (unsigned i = 0; i < 9; ++i) {
    const auto &e = f.calls[i].event;
    CHECK(e.site == i + 1 && e.pc == CallPcs[i] &&
          e.delay_slot_pc == CallPcs[i] + 4u && e.argument_count == Argc[i]);
    CHECK(f.progress.call_attempts[i + 1] == 1 &&
          f.progress.call_count[i + 1] == 1);
  }
  for (unsigned i = 0; i < 6; ++i) {
    CHECK(f.accesses[i].pc == AccessPcs[i] &&
          f.accesses[i].address == AccessAddresses[i] &&
          f.accesses[i].width == 4 &&
          f.accesses[i].operation == AccessOperations[i]);
    CHECK(f.accesses[i].kind ==
          (i < 2 ? NBA97_GAMELOAD_MAIN_STORE : NBA97_GAMELOAD_MAIN_READ));
  }
  CHECK(f.progress.frame_stack_pointer == Frame &&
        f.get(Frame + 20u) == EntryRa &&
        f.get(Frame + 16u) == f.initial.registers.gpr[NBA97_GAMELOAD_MAIN_S0].word);
  CHECK(same(f.progress.saved_return_address,
             f.initial.registers.gpr[NBA97_GAMELOAD_MAIN_RA]) &&
        same(f.progress.saved_s0,
             f.initial.registers.gpr[NBA97_GAMELOAD_MAIN_S0]) &&
        f.progress.loaded_copy_size.word == f.copy_size &&
        f.progress.first_copy_length.word == f.copy_size &&
        f.progress.first_copy_length.known_mask == f.copy_mask &&
        f.progress.second_copy_length.word == f.second_length &&
        f.progress.second_copy_length.known_mask == f.second_length_mask &&
        f.calls[4].machine.registers.gpr[NBA97_GAMELOAD_MAIN_A2].word ==
            f.copy_size &&
        f.calls[7].machine.registers.gpr[NBA97_GAMELOAD_MAIN_A2].word ==
            f.second_length);
  CHECK(f.progress.loaded_gameonly_entry.word == f.gameonly &&
        f.calls.back().event.entry == f.gameonly &&
        f.get(TargetWord) == 0x80030000u);
  CHECK(f.progress.machine.registers.gpr[NBA97_GAMELOAD_MAIN_SP].word ==
            EntrySp &&
        same(f.progress.restored_return_address,
             f.initial.registers.gpr[NBA97_GAMELOAD_MAIN_RA]) &&
        same(f.progress.restored_s0,
             f.initial.registers.gpr[NBA97_GAMELOAD_MAIN_S0]) &&
        f.progress.stopped_pc == 0 && f.progress.stopped_target == 0);

  Fixture transferred;
  transferred.transfer_site = NBA97_GAMELOAD_MAIN_SITE_801E13F4;
  transferred.mutate_dynamic_machine = true;
  CHECK(transferred.run() == NBA97_TEXT_COMPLETE &&
        transferred.progress.completed && transferred.progress.transferred &&
        transferred.progress.instruction_count == 36 &&
        transferred.progress.operations == 13 &&
        transferred.progress.access_events == 4);
  CHECK(transferred.progress.machine.hi.word == 0xaabbccddu &&
        transferred.progress.machine.lo.word == 0x11223344u);
  for (unsigned i = 1; i < 32; ++i)
    CHECK(same(transferred.progress.machine.registers.gpr[i],
               Nba97GameloadMainWord{0xa0000000u + i,
                                     static_cast<std::uint8_t>(i & 15u)}) ||
          (i == NBA97_GAMELOAD_MAIN_SP &&
           transferred.progress.machine.registers.gpr[i].word == Frame));
}

void liveReturnAndTargetFaults() {
  Fixture moved;
  moved.relocated_mode = 1;
  moved.mutate_dynamic_machine = true;
  CHECK(moved.run() == NBA97_TEXT_COMPLETE &&
        moved.progress.machine.registers.gpr[NBA97_GAMELOAD_MAIN_SP].word ==
            moved.relocated_frame + 24u &&
        moved.progress.restored_return_address.word == 0x80001000u &&
        moved.progress.restored_s0.word == 0x22334455u &&
        moved.progress.restored_s0.known_mask == 3 &&
        moved.progress.machine.hi.word == 0xaabbccddu);
  for (unsigned i = 1; i < 32; ++i)
    if (i != NBA97_GAMELOAD_MAIN_SP && i != NBA97_GAMELOAD_MAIN_RA &&
        i != NBA97_GAMELOAD_MAIN_S0)
      CHECK(moved.progress.machine.registers.gpr[i].word == 0xa0000000u + i);

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture ra;
    ra.relocated_mode = 1;
    ra.return_ra_mask = mask;
    int expected = mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN;
    CHECK(ra.run() == expected &&
          ra.progress.restored_return_address.known_mask == mask &&
          ra.progress.instruction_events == 41);
  }
  Fixture unaligned_ra;
  unaligned_ra.relocated_mode = 1;
  unaligned_ra.return_ra_word = 0x80001002u;
  CHECK(unaligned_ra.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_ra.progress.trapped &&
        unaligned_ra.progress.stopped_pc == 0x801e1408u &&
        unaligned_ra.progress.stopped_target == 0x80001002u);

  Fixture unknown_target;
  unknown_target.gameonly_mask = 7;
  CHECK(unknown_target.run() == NBA97_TEXT_UNKNOWN &&
        unknown_target.progress.stopped_pc == 0x801e13f4u &&
        unknown_target.progress.stopped_target == Gameonly &&
        unknown_target.progress.instruction_count == 36 &&
        unknown_target.progress.call_attempts[NBA97_GAMELOAD_MAIN_SITE_801E13F4] ==
            0);
  Fixture unaligned_target;
  unaligned_target.gameonly = Gameonly + 2u;
  CHECK(unaligned_target.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_target.progress.trapped &&
        unaligned_target.progress.stopped_pc == 0x801e13f4u);
  Fixture unaligned_sp;
  unaligned_sp.relocated_mode = 2;
  CHECK(unaligned_sp.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_sp.progress.stopped_pc == 0x801e13fcu);
  Fixture unmapped_sp;
  unmapped_sp.relocated_mode = 3;
  CHECK(unmapped_sp.run() == NBA97_TEXT_RESOURCE &&
        unmapped_sp.progress.stopped_pc == 0x801e13fcu &&
        unmapped_sp.progress.stopped_address == 0x90000014u);
  Fixture malformed;
  malformed.malformed_restore_byte = true;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.stopped_pc == 0x801e13fcu);
}

void budgetsRefusalsAndMalformedInputs() {
  for (std::size_t budget = 0; budget < 15; ++budget) {
    Fixture f;
    f.context.operation_budget = budget;
    CHECK(f.run() == NBA97_TEXT_LIMIT && f.progress.operations == budget);
  }
  for (unsigned site = 1; site < NBA97_GAMELOAD_MAIN_SITE_COUNT; ++site) {
    Fixture f;
    f.refuse_site = site;
    CHECK(f.run() == NBA97_TEXT_IO_REFUSED &&
          f.progress.stopped_pc == CallPcs[site - 1u] &&
          f.progress.call_attempts[site] == 1 &&
          f.progress.call_count[site] == 0);
  }
  for (unsigned site = 1; site < NBA97_GAMELOAD_MAIN_SITE_801E13F4; ++site) {
    Fixture f;
    f.transfer_site = site;
    CHECK(f.run() == NBA97_TEXT_ARGUMENT &&
          f.progress.stopped_pc == CallPcs[site - 1u] &&
          f.progress.call_attempts[site] == 1 &&
          f.progress.call_count[site] == 0);
  }
  for (unsigned mode = 1; mode <= 5; ++mode) {
    Fixture f;
    f.invalid_mode = mode;
    CHECK(f.run() == NBA97_TEXT_ARGUMENT && f.progress.callbacks_completed == 0);
  }

  Fixture unknown_sp;
  unknown_sp.context.machine.registers.gpr[NBA97_GAMELOAD_MAIN_SP].known_mask = 14;
  CHECK(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stopped_pc == 0x801e1370u &&
        unknown_sp.progress.operations == 0);
  Fixture unaligned_entry_sp;
  unaligned_entry_sp.context.machine.registers.gpr[NBA97_GAMELOAD_MAIN_SP].word +=
      2u;
  CHECK(unaligned_entry_sp.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_entry_sp.progress.stopped_pc == 0x801e1370u);

  Fixture unknown_s0;
  unknown_s0.copy_mask = 6;
  CHECK(unknown_s0.run() == NBA97_TEXT_COMPLETE &&
        unknown_s0.progress.first_copy_length.known_mask == 6);
  Fixture no_plane;
  no_plane.region.known = nullptr;
  no_plane.context.machine.registers.gpr[NBA97_GAMELOAD_MAIN_RA].known_mask = 15;
  no_plane.context.machine.registers.gpr[NBA97_GAMELOAD_MAIN_S0].known_mask = 15;
  CHECK(no_plane.run() == NBA97_TEXT_COMPLETE);

  Fixture wrapped;
  std::array<std::uint8_t, 8> low{};
  std::array<std::uint8_t, 8> low_known{};
  low_known.fill(1);
  Nba97GameTextRegion wrap_regions[2] = {
      wrapped.region, {8u, low.data(), low_known.data(), low.size()}};
  wrapped.context.memory = {wrap_regions, 2};
  wrapped.context.machine.registers.gpr[NBA97_GAMELOAD_MAIN_SP] = {0x10u, 15};
  CHECK(wrapped.run() == NBA97_TEXT_COMPLETE &&
        wrapped.progress.frame_stack_pointer == 0xfffffff8u &&
        wrapped.progress.machine.registers.gpr[NBA97_GAMELOAD_MAIN_SP].word ==
            0x10u &&
        wrapped.progress.restored_return_address.word == EntryRa);

  Fixture zero_target;
  zero_target.gameonly = 0;
  CHECK(zero_target.run() == NBA97_TEXT_COMPLETE &&
        zero_target.calls.back().event.entry == 0);

  Fixture malformed_known;
  malformed_known.malformed_known_address = SizeWord + 1u;
  CHECK(malformed_known.run() == NBA97_TEXT_ARGUMENT &&
        malformed_known.progress.stopped_pc == 0x801e13a0u);
  Fixture overlap;
  Nba97GameTextRegion pair[2] = {overlap.region, overlap.region};
  overlap.context.memory = {pair, 2};
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT && overlap.progress.operations == 0);
  Fixture huge;
  std::uint8_t one = 0;
  Nba97GameTextRegion huge_region{1, &one, nullptr,
                                  std::numeric_limits<std::size_t>::max()};
  huge.context.memory = {&huge_region, 1};
  CHECK(huge.run() == NBA97_TEXT_ARGUMENT);
  Fixture bad_access_journal;
  bad_access_journal.context.access_journal = nullptr;
  CHECK(bad_access_journal.run() == NBA97_TEXT_ARGUMENT);
  Fixture bad_pc_journal;
  bad_pc_journal.context.instruction_journal = nullptr;
  CHECK(bad_pc_journal.run() == NBA97_TEXT_ARGUMENT);

  Nba97GameloadMainProgress progress{};
  CHECK(nba97_gameload_main(nullptr, &progress) == NBA97_TEXT_ARGUMENT);
  Fixture null_out;
  CHECK(nba97_gameload_main(&null_out.context, nullptr) == NBA97_TEXT_ARGUMENT);
}

void deterministicAndTruncatedJournals() {
  Fixture a;
  Fixture b;
  CHECK(a.run() == NBA97_TEXT_COMPLETE && b.run() == NBA97_TEXT_COMPLETE);
  CHECK(std::memcmp(&a.progress, &b.progress, sizeof a.progress) == 0);
  CHECK(std::memcmp(a.accesses.data(), b.accesses.data(), sizeof a.accesses) ==
        0);
  CHECK(std::memcmp(a.instructions.data(), b.instructions.data(),
                    sizeof a.instructions) == 0);
  for (unsigned cap = 0; cap < 6; ++cap) {
    Fixture f;
    f.context.access_journal_capacity = cap;
    f.context.instruction_journal_capacity = cap;
    CHECK(f.run() == NBA97_TEXT_COMPLETE && f.progress.access_events == 6 &&
          f.progress.instruction_events == 41);
  }
}
} // namespace

int main() {
  normalPathsAndExactTrace();
  liveReturnAndTargetFaults();
  budgetsRefusalsAndMalformedInputs();
  deterministicAndTruncatedJournals();
  std::printf("gameload_main_tests: PASS (%u checks)\n", checks);
}
