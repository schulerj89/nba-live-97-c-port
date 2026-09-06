#include "recovered/frontend_main.h"

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
    std::fprintf(stderr, "frontend main check %u failed at line %u\n", checks,
                 line);
    std::exit(1);
  }
}
#define CHECK(value) checkAt((value), __LINE__)

constexpr U Base = 0x80000000u;
constexpr U Size = 0x200000u;
constexpr U EntrySp = 0x801ff000u;
constexpr U Frame = EntrySp - 40u;
constexpr U Flag = 0x80021ee4u;
constexpr U IntroFlag = 0x8001edecu;
constexpr U Selector = 0x80021568u;
constexpr U DynamicWord = 0x801e0000u;
constexpr U EntryRa = 0x8007b840u;
constexpr U Handle = 0x80140000u;
constexpr U LoadSize = 0x1234u;

constexpr std::array<U, 50> CallPcs{{
    0x80028810, 0x80028818, 0x80028834, 0x80028858, 0x80028880,
    0x80028898, 0x800288a8, 0x800288b8, 0x800288c0, 0x800288c8,
    0x800288d0, 0x800288d8, 0x800288ec, 0x800288f4, 0x800288fc,
    0x80028904, 0x8002890c, 0x80028934, 0x8002893c, 0x8002894c,
    0x80028954, 0x8002895c, 0x80028974, 0x800289f4, 0x800289fc,
    0x80028a04, 0x80028a0c, 0x80028a14, 0x80028a48, 0x80028a50,
    0x80028a58, 0x80028a60, 0x80028a7c, 0x80028a90, 0x80028aa0,
    0x80028aa8, 0x80028ab0, 0x80028acc, 0x80028ad8, 0x80028af0,
    0x80028af8, 0x80028b00, 0x80028b08, 0x80028b1c, 0x80028b24,
    0x80028b2c, 0x80028b34, 0x80028b44, 0x80028b54, 0x80028b68}};
constexpr std::array<U, 50> Targets{{
    0x8007b844, 0x8008b368, 0x800769e0, 0x80061674, 0x8008bfb0,
    0x80078b7c, 0x8008a4f8, 0x80079bf0, 0x8007f5a8, 0x8007f5d0,
    0x80076148, 0x8008004c, 0x8007844c, 0x8008b104, 0x800802b8,
    0x80028b8c, 0x80028ed0, 0x800807d8, 0x800804e8, 0x800807d8,
    0x800804e8, 0x8008044c, 0x8008bfb0, 0x80035d80, 0x800517bc,
    0x800673a0, 0x8008da98, 0x8008acb0, 0x80036008, 0x80035984,
    0x8008e5a0, 0x80064c90, 0x8008da5c, 0x80029b20, 0x800360d4,
    0x8002f084, 0x80028e08, 0x8007b11c, 0x80077cd4, 0x80084c44,
    0x80084c84, 0x80084c9c, 0x80028b8c, 0x8008b1f0, 0x800785f0,
    0x80076110, 0x80051b44, 0x8008a944, 0x800909a8, 0}};
constexpr std::array<std::uint8_t, 50> Argc{{
    0, 0, 3, 1, 2, 0, 1, 2, 1, 0, 1, 1, 1, 0, 1, 0, 1,
    3, 1, 3, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 2, 1, 2, 1, 1, 0, 0, 0, 0, 0, 2, 3, 0}};

bool same(const Nba97FrontendMainWord &a, const Nba97FrontendMainWord &b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}

struct Seen {
  Nba97FrontendMainEvent event{};
  Nba97FrontendMainMachine machine{};
};

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size, 0xcd);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97FrontendMainAccess, 128> accesses{};
  std::array<U, 1024> instructions{};
  Nba97FrontendMainContext context{};
  Nba97FrontendMainProgress progress{};
  Nba97FrontendMainMachine initial{};
  std::vector<Seen> calls;
  U initial_flag = 1;
  U menu_flag = 1;
  U intro_flag = 0;
  U selector = 0;
  U intro_count = 0;
  U dynamic_entry = 0x801e1410u;
  std::uint8_t dynamic_mask = 15;
  Nba97FrontendMainCalleeOutcome dynamic_outcome =
      NBA97_FRONTEND_MAIN_CALLEE_RETURNED;
  unsigned refuse_site = 0;
  unsigned invalid_mode = 0;
  bool mutate_flag = false;
  bool replenish_intro = false;
  bool mutate_dynamic_machine = false;
  bool relocate_dynamic_frame = false;
  bool terminate_negative_intro = false;
  bool wrap_wait = false;
  unsigned return_ra_mask = 0x100u;
  U malformed_known_address = 0;
  unsigned dynamic_sp_mode = 0;
  bool malformed_return_byte = false;
  U relocated_frame = Frame - 0x200u;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      initial.registers.gpr[i] = {0x21000000u + i * 0x01010101u,
                                  static_cast<std::uint8_t>(i & 15u)};
    initial.registers.gpr[0] = {0, 15};
    initial.registers.gpr[NBA97_FRONTEND_MAIN_SP] = {EntrySp, 15};
    initial.registers.gpr[NBA97_FRONTEND_MAIN_RA] = {EntryRa, 15};
    initial.registers.gpr[NBA97_FRONTEND_MAIN_S0].known_mask = 7;
    initial.registers.gpr[NBA97_FRONTEND_MAIN_S1].known_mask = 11;
    initial.registers.gpr[NBA97_FRONTEND_MAIN_S2].known_mask = 13;
    initial.hi = {0x10203040u, 5};
    initial.lo = {0x50607080u, 10};
    context.memory = {&region, 1};
    context.operation_budget = 1000;
    context.machine = initial;
    context.io = callback;
    context.user = this;
    context.access_journal = accesses.data();
    context.access_journal_capacity = accesses.size();
    context.instruction_journal = instructions.data();
    context.instruction_journal_capacity = instructions.size();
    resetMemory();
  }

  void resetMemory() {
    put(Flag, initial_flag);
    put(IntroFlag, intro_flag, 2);
    put(Selector, selector, 2);
    put(DynamicWord, dynamic_entry, 4, dynamic_mask);
  }
  void put(U address, U value, unsigned width = 4, std::uint8_t mask = 15) {
    CHECK(address >= Base && std::uint64_t(address - Base) + width <= Size);
    for (unsigned i = 0; i < width; ++i) {
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
      if (region.known)
        known[address - Base + i] = std::uint8_t((mask >> i) & 1u);
    }
  }
  U get(U address, unsigned width = 4) const {
    U value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= U(bytes[address - Base + i]) << (i * 8u);
    return value;
  }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97FrontendMainEvent *event,
                      Nba97FrontendMainMachine *machine,
                      Nba97FrontendMainCalleeOutcome *outcome) {
    auto &f = *static_cast<Fixture *>(opaque);
    if (!event || !machine || !outcome || event->site == 0 ||
        event->site >= NBA97_FRONTEND_MAIN_SITE_COUNT)
      return 0;
    const unsigned index = event->site - 1u;
    if (event->pc != CallPcs[index] || event->delay_slot_pc != event->pc + 4u ||
        event->argument_count != Argc[index] ||
        event->target_program !=
            (event->site == NBA97_FRONTEND_MAIN_SITE_80028B68
                 ? NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD
                 : NBA97_FRONTEND_MAIN_PROGRAM_FEONLY) ||
        (event->site != NBA97_FRONTEND_MAIN_SITE_80028B68 &&
         event->entry != Targets[index]) ||
        (event->site == NBA97_FRONTEND_MAIN_SITE_80028B68 &&
         event->entry != f.dynamic_entry) ||
        machine->registers.gpr[NBA97_FRONTEND_MAIN_RA].known_mask != 15 ||
        machine->registers.gpr[NBA97_FRONTEND_MAIN_RA].word != event->pc + 8u)
      return 0;
    f.calls.push_back({*event, *machine});
    if (f.refuse_site == event->site)
      return 0;
    if (event->site == NBA97_FRONTEND_MAIN_SITE_80028A7C)
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {f.intro_count, 15};
    if (event->site == NBA97_FRONTEND_MAIN_SITE_80028A90 &&
        f.terminate_negative_intro)
      machine->registers.gpr[NBA97_FRONTEND_MAIN_S0] = {0, 15};
    if (event->site == NBA97_FRONTEND_MAIN_SITE_80028A14) {
      if (f.mutate_flag)
        f.put(Flag, f.menu_flag);
      if (f.replenish_intro)
        f.put(IntroFlag, f.intro_flag, 2);
    }
    if (event->site == NBA97_FRONTEND_MAIN_SITE_80028ACC)
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {Handle, 15};
    if (event->site == NBA97_FRONTEND_MAIN_SITE_80028AD8)
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {LoadSize, 15};
    if (event->site == NBA97_FRONTEND_MAIN_SITE_80028B08 && f.wrap_wait) {
      if (event->invocation == 1)
        machine->registers.gpr[NBA97_FRONTEND_MAIN_S0] = {UINT32_MAX, 15};
      else
        machine->registers.gpr[NBA97_FRONTEND_MAIN_S0] = {20, 15};
    }
    if (event->site == NBA97_FRONTEND_MAIN_SITE_80028B54)
      f.put(DynamicWord, f.dynamic_entry, 4, f.dynamic_mask);
    if (event->site == NBA97_FRONTEND_MAIN_SITE_80028B68) {
      *outcome = f.dynamic_outcome;
      if (f.relocate_dynamic_frame) {
        for (unsigned i = 0; i < 40; ++i) {
          f.bytes[f.relocated_frame - Base + i] = f.bytes[Frame - Base + i];
          if (f.region.known)
            f.known[f.relocated_frame - Base + i] = f.known[Frame - Base + i];
        }
        machine->registers.gpr[NBA97_FRONTEND_MAIN_SP] = {f.relocated_frame,
                                                           15};
        f.put(f.relocated_frame + 36u, 0x13579bdcu);
        f.put(f.relocated_frame + 32u, 0x22222222u, 4, 3);
        f.put(f.relocated_frame + 28u, 0x11111111u, 4, 5);
        f.put(f.relocated_frame + 24u, 0x00000010u, 4, 9);
      }
      if (f.mutate_dynamic_machine) {
        for (unsigned i = 1; i < 32; ++i)
          machine->registers.gpr[i] = {0xa0000000u + i,
                                       static_cast<std::uint8_t>(i & 15u)};
        machine->registers.gpr[NBA97_FRONTEND_MAIN_SP] =
            {f.relocate_dynamic_frame ? f.relocated_frame : Frame, 15};
        machine->hi = {0xaabbccddu, 6};
        machine->lo = {0x11223344u, 9};
      }
      if (f.dynamic_sp_mode == 1)
        machine->registers.gpr[NBA97_FRONTEND_MAIN_SP] = {Frame + 2u, 15};
      else if (f.dynamic_sp_mode == 2)
        machine->registers.gpr[NBA97_FRONTEND_MAIN_SP] = {0x90000000u, 15};
      if (f.return_ra_mask <= 15u && f.region.known) {
        const U live_sp = machine->registers.gpr[NBA97_FRONTEND_MAIN_SP].word;
        for (unsigned i = 0; i < 4; ++i)
          f.known[live_sp + 36u - Base + i] =
              std::uint8_t((f.return_ra_mask >> i) & 1u);
      }
      if (f.malformed_return_byte && f.region.known) {
        const U live_sp = machine->registers.gpr[NBA97_FRONTEND_MAIN_SP].word;
        f.known[live_sp + 39u - Base] = 2;
      }
    }
    if (f.invalid_mode == 1)
      machine->registers.gpr[0].word = 1;
    else if (f.invalid_mode == 2)
      machine->registers.gpr[8].known_mask = 16;
    else if (f.invalid_mode == 3)
      machine->hi.known_mask = 16;
    else if (f.invalid_mode == 4)
      machine->lo.known_mask = 16;
    else if (f.invalid_mode == 5)
      *outcome = static_cast<Nba97FrontendMainCalleeOutcome>(99);
    return 1;
  }

  int run() {
    resetMemory();
    if (malformed_known_address)
      known[malformed_known_address - Base] = 2;
    return nba97_frontend_main(&context, &progress);
  }
};

void normalReturnedAndTransferredPaths() {
  Fixture returned;
  CHECK(returned.run() == NBA97_TEXT_COMPLETE && returned.progress.completed &&
        !returned.progress.transferred && returned.progress.wait_iterations == 20 &&
        returned.progress.intro_iterations == 0);
  CHECK(returned.progress.gameload_handle.word == Handle &&
        returned.progress.gameload_size.word == LoadSize &&
        returned.progress.dynamic_entry.word == returned.dynamic_entry &&
        returned.calls.back().event.target_program ==
            NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD &&
        returned.calls.back().event.entry == returned.dynamic_entry);
  CHECK(returned.progress.machine.registers.gpr[NBA97_FRONTEND_MAIN_SP].word ==
            EntrySp &&
        same(returned.progress.restored_return_address,
             returned.initial.registers.gpr[NBA97_FRONTEND_MAIN_RA]) &&
        same(returned.progress.restored_s0,
             returned.initial.registers.gpr[NBA97_FRONTEND_MAIN_S0]) &&
        same(returned.progress.restored_s1,
             returned.initial.registers.gpr[NBA97_FRONTEND_MAIN_S1]) &&
        same(returned.progress.restored_s2,
             returned.initial.registers.gpr[NBA97_FRONTEND_MAIN_S2]));

  Fixture transferred;
  transferred.dynamic_outcome = NBA97_FRONTEND_MAIN_CALLEE_TRANSFERRED;
  CHECK(transferred.run() == NBA97_TEXT_COMPLETE &&
        transferred.progress.completed && transferred.progress.transferred &&
        transferred.progress.machine.registers.gpr[NBA97_FRONTEND_MAIN_SP].word ==
            Frame &&
        transferred.progress.machine.registers.gpr[NBA97_FRONTEND_MAIN_RA].word ==
            0x80028b70u);
}

void allPcsSitesAndDelays() {
  std::array<bool, 227> seen{};
  for (U selector : {0u, 1u}) {
    Fixture f;
    f.initial_flag = 0;
    f.menu_flag = 0;
    f.mutate_flag = true;
    f.intro_flag = 1;
    f.replenish_intro = true;
    f.intro_count = 1;
    f.selector = selector;
    CHECK(f.run() == NBA97_TEXT_COMPLETE && f.progress.intro_iterations == 1);
    std::array<bool, NBA97_FRONTEND_MAIN_SITE_COUNT> sites{};
    for (const auto &call : f.calls) {
      sites[call.event.site] = true;
      const unsigned index = call.event.site - 1u;
      CHECK(call.event.pc == CallPcs[index] &&
            call.event.delay_slot_pc == CallPcs[index] + 4u &&
            call.event.argument_count == Argc[index]);
    }
    for (unsigned site = 1; site < NBA97_FRONTEND_MAIN_SITE_COUNT; ++site)
      CHECK(sites[site]);
    for (std::size_t i = 0; i < f.progress.instruction_events; ++i) {
      const U pc = f.instructions[i];
      CHECK(pc >= 0x80028800u && pc <= 0x80028b88u && !(pc & 3u));
      seen[(pc - 0x80028800u) / 4u] = true;
    }
  }
  for (bool value : seen)
    CHECK(value);
}

void independentFlagsCountsAndMutableLoops() {
  for (U count : {0u, 1u, 99u, 100u, 65535u}) {
    Fixture f;
    f.intro_flag = count;
    f.initial_flag = 1;
    f.menu_flag = 1;
    CHECK(f.run() == NBA97_TEXT_COMPLETE);
    const bool found = f.progress.call_count[
                           NBA97_FRONTEND_MAIN_SITE_80028A48] == 1;
    CHECK(found == (count > 0 && count < 100));
  }

  Fixture cold_then_warm;
  cold_then_warm.initial_flag = 0;
  cold_then_warm.menu_flag = 1;
  cold_then_warm.mutate_flag = true;
  CHECK(cold_then_warm.run() == NBA97_TEXT_COMPLETE &&
        cold_then_warm.progress.loaded_initial_frontend_flag.word == 0 &&
        cold_then_warm.progress.loaded_menu_frontend_flag.word == 1 &&
        cold_then_warm.progress.call_count[
            NBA97_FRONTEND_MAIN_SITE_80028858] == 1 &&
        cold_then_warm.progress.call_count[
            NBA97_FRONTEND_MAIN_SITE_80028A7C] == 0);

  Fixture warm_then_cold;
  warm_then_cold.initial_flag = 1;
  warm_then_cold.menu_flag = 0;
  warm_then_cold.mutate_flag = true;
  warm_then_cold.intro_count = 2;
  CHECK(warm_then_cold.run() == NBA97_TEXT_COMPLETE &&
        warm_then_cold.progress.loaded_initial_frontend_flag.word == 1 &&
        warm_then_cold.progress.loaded_menu_frontend_flag.word == 0 &&
        warm_then_cold.progress.intro_iterations == 2);

  Fixture negative;
  negative.initial_flag = 0;
  negative.menu_flag = 0;
  negative.mutate_flag = true;
  negative.intro_count = UINT32_MAX;
  negative.terminate_negative_intro = true;
  CHECK(negative.run() == NBA97_TEXT_COMPLETE &&
        negative.progress.intro_iterations == 1);

  Fixture wait_wrap;
  wait_wrap.wrap_wait = true;
  CHECK(wait_wrap.run() == NBA97_TEXT_COMPLETE &&
        wait_wrap.progress.wait_iterations == 2 &&
        wait_wrap.progress.call_count[NBA97_FRONTEND_MAIN_SITE_80028B08] == 2);
}

void dynamicEdgesAndLiveReturnFrame() {
  Fixture zero;
  zero.dynamic_entry = 0;
  CHECK(zero.run() == NBA97_TEXT_COMPLETE &&
        zero.calls.back().event.entry == 0 &&
        zero.calls.back().event.target_program ==
            NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD);

  Fixture unknown;
  unknown.dynamic_mask = 7;
  CHECK(unknown.run() == NBA97_TEXT_UNKNOWN &&
        unknown.progress.stopped_pc == 0x80028b68u &&
        unknown.progress.dynamic_entry.known_mask == 7 &&
        unknown.progress.machine.registers.gpr[NBA97_FRONTEND_MAIN_RA].word ==
            0x80028b70u &&
        unknown.progress.call_attempts[NBA97_FRONTEND_MAIN_SITE_80028B68] == 0);

  Fixture unaligned;
  unaligned.dynamic_entry = 0x801e1411u;
  CHECK(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x80028b68u &&
        unaligned.progress.stopped_target == 0x801e1411u);

  Fixture refused;
  refused.refuse_site = NBA97_FRONTEND_MAIN_SITE_80028B68;
  CHECK(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.progress.stopped_pc == 0x80028b68u &&
        refused.progress.call_attempts[NBA97_FRONTEND_MAIN_SITE_80028B68] == 1 &&
        refused.progress.call_count[NBA97_FRONTEND_MAIN_SITE_80028B68] == 0);

  Fixture moved;
  moved.relocate_dynamic_frame = true;
  moved.mutate_dynamic_machine = true;
  CHECK(moved.run() == NBA97_TEXT_COMPLETE &&
        moved.progress.machine.registers.gpr[NBA97_FRONTEND_MAIN_SP].word ==
            moved.relocated_frame + 40u &&
        moved.progress.restored_return_address.word == 0x13579bdcu &&
        moved.progress.restored_s2.word == 0x22222222u &&
        moved.progress.restored_s2.known_mask == 3 &&
        moved.progress.restored_s1.word == 0x11111111u &&
        moved.progress.restored_s1.known_mask == 5 &&
        moved.progress.restored_s0.word == 0x10u &&
        moved.progress.restored_s0.known_mask == 9 &&
        moved.progress.machine.hi.word == 0xaabbccddu &&
        moved.progress.machine.lo.word == 0x11223344u);
  for (unsigned i = 1; i < 32; ++i)
    if (i != NBA97_FRONTEND_MAIN_SP && i != NBA97_FRONTEND_MAIN_RA &&
        i != NBA97_FRONTEND_MAIN_S0 && i != NBA97_FRONTEND_MAIN_S1 &&
        i != NBA97_FRONTEND_MAIN_S2)
      CHECK(moved.progress.machine.registers.gpr[i].word == 0xa0000000u + i);

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture ra;
    ra.return_ra_mask = mask;
    const int expected = mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN;
    CHECK(ra.run() == expected &&
          ra.progress.restored_return_address.known_mask == mask &&
          ra.progress.instruction_count == ra.progress.instruction_events);
  }

  Fixture restore_unaligned;
  restore_unaligned.dynamic_sp_mode = 1;
  CHECK(restore_unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        restore_unaligned.progress.stopped_pc == 0x80028b70u);
  Fixture restore_unmapped;
  restore_unmapped.dynamic_sp_mode = 2;
  CHECK(restore_unmapped.run() == NBA97_TEXT_RESOURCE &&
        restore_unmapped.progress.stopped_pc == 0x80028b70u &&
        restore_unmapped.progress.stopped_address == 0x90000024u);
  Fixture malformed_restore;
  malformed_restore.malformed_return_byte = true;
  CHECK(malformed_restore.run() == NBA97_TEXT_ARGUMENT &&
        malformed_restore.progress.stopped_pc == 0x80028b70u &&
        malformed_restore.progress.restored_return_address.known_mask == 0);
}

void budgetsRefusalsMalformedAndMemoryFailures() {
  Fixture baseline;
  baseline.initial_flag = 0;
  baseline.menu_flag = 0;
  baseline.mutate_flag = true;
  baseline.intro_flag = 1;
  baseline.replenish_intro = true;
  baseline.intro_count = 1;
  CHECK(baseline.run() == NBA97_TEXT_COMPLETE);
  const std::size_t operations = baseline.progress.operations;
  for (std::size_t budget = 0; budget < operations; ++budget) {
    Fixture f;
    f.initial_flag = 0;
    f.menu_flag = 0;
    f.mutate_flag = true;
    f.intro_flag = 1;
    f.replenish_intro = true;
    f.intro_count = 1;
    f.context.operation_budget = budget;
    CHECK(f.run() == NBA97_TEXT_LIMIT && f.progress.operations == budget);
  }

  for (unsigned site = 1; site < NBA97_FRONTEND_MAIN_SITE_COUNT; ++site) {
    Fixture f;
    f.initial_flag = 0;
    f.menu_flag = 0;
    f.mutate_flag = true;
    f.intro_flag = 1;
    f.replenish_intro = true;
    f.intro_count = 1;
    f.refuse_site = site;
    CHECK(f.run() == NBA97_TEXT_IO_REFUSED &&
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
  unknown_sp.context.machine.registers.gpr[NBA97_FRONTEND_MAIN_SP].known_mask = 14;
  CHECK(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stopped_pc == 0x80028804u &&
        unknown_sp.progress.operations == 0);
  Fixture unaligned_sp;
  unaligned_sp.context.machine.registers.gpr[NBA97_FRONTEND_MAIN_SP].word += 2;
  CHECK(unaligned_sp.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_sp.progress.stopped_pc == 0x80028804u);

  Fixture malformed;
  malformed.malformed_known_address = Flag + 3u;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.stopped_pc == 0x80028840u);

  Fixture overlap;
  Nba97GameTextRegion duplicate[2] = {overlap.region, overlap.region};
  overlap.context.memory = {duplicate, 2};
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT && overlap.progress.operations == 0);
  Fixture huge;
  std::uint8_t one = 0;
  Nba97GameTextRegion huge_region{1, &one, nullptr,
                                  std::numeric_limits<std::size_t>::max()};
  huge.context.memory = {&huge_region, 1};
  CHECK(huge.run() == NBA97_TEXT_ARGUMENT && huge.progress.operations == 0);

  Fixture no_plane;
  no_plane.region.known = nullptr;
  no_plane.initial.registers.gpr[NBA97_FRONTEND_MAIN_S0].known_mask = 15;
  no_plane.initial.registers.gpr[NBA97_FRONTEND_MAIN_S1].known_mask = 15;
  no_plane.initial.registers.gpr[NBA97_FRONTEND_MAIN_S2].known_mask = 15;
  no_plane.context.machine = no_plane.initial;
  CHECK(no_plane.run() == NBA97_TEXT_COMPLETE && no_plane.progress.completed);

  Fixture alias;
  alias.context.machine.registers.gpr[NBA97_FRONTEND_MAIN_SP] = {Flag + 4u, 15};
  CHECK(alias.run() == NBA97_TEXT_COMPLETE &&
        alias.progress.frame_stack_pointer == Flag - 36u &&
        alias.progress.loaded_initial_frontend_flag.word == EntryRa &&
        alias.progress.restored_return_address.word == EntryRa);

  Fixture wrapped;
  std::array<std::uint8_t, 24> low{};
  std::array<std::uint8_t, 24> low_known{};
  low_known.fill(1);
  Nba97GameTextRegion wrap_regions[2] = {
      wrapped.region, {8, low.data(), low_known.data(), low.size()}};
  wrapped.context.memory = {wrap_regions, 2};
  wrapped.context.machine.registers.gpr[NBA97_FRONTEND_MAIN_SP] = {0x20u, 15};
  CHECK(wrapped.run() == NBA97_TEXT_COMPLETE &&
        wrapped.progress.frame_stack_pointer == 0xfffffff8u &&
        wrapped.progress.machine.registers.gpr[NBA97_FRONTEND_MAIN_SP].word ==
            0x20u);

  Nba97FrontendMainProgress progress{};
  CHECK(nba97_frontend_main(nullptr, &progress) == NBA97_TEXT_ARGUMENT);
  Fixture null_out;
  CHECK(nba97_frontend_main(&null_out.context, nullptr) == NBA97_TEXT_ARGUMENT);
}
} // namespace

int main() {
  normalReturnedAndTransferredPaths();
  allPcsSitesAndDelays();
  independentFlagsCountsAndMutableLoops();
  dynamicEdgesAndLiveReturnFrame();
  budgetsRefusalsMalformedAndMemoryFailures();
  std::printf("frontend_main_tests: PASS (%u checks)\n", checks);
}
