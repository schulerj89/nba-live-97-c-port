#include "recovered/frontend_dispatch_entry.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "frontend dispatch entry check %u failed at line %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define CHECK(value) checkAt((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::size_t RamSize = 0x000d0000u;
constexpr std::uint32_t Stack = 0x801ff000u;
constexpr std::uint32_t EntrySp = 0x801fff00u;
constexpr std::uint32_t Frame = EntrySp - 24u;
constexpr std::uint32_t Flag = 0x80021ee4u;
constexpr std::uint32_t Scalar = 0x800c6e68u;
constexpr std::uint32_t CallerRa = 0x80028aa8u;

bool same(const Nba97FrontendDispatchEntryWord &a,
          const Nba97FrontendDispatchEntryWord &b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}

struct Fixture {
  enum Mode {
    Accept,
    Refuse,
    InvalidZero,
    InvalidGpr,
    InvalidHi,
    InvalidLo,
    RelocateSp,
    PartialSavedRa,
    RefuseMutated,
    LiveSpUnknown,
    LiveSpUnaligned,
    LiveSpUnmapped,
    MalformedRestore
  } mode = Accept;
  std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(RamSize, 0xcd);
  std::vector<std::uint8_t> ram_known =
      std::vector<std::uint8_t>(RamSize, 1);
  std::array<std::uint8_t, 0x1000> stack{};
  std::array<std::uint8_t, 0x1000> stack_known{};
  std::array<Nba97GameTextRegion, 2> regions{{
      {Ram, ram.data(), ram_known.data(), ram.size()},
      {Stack, stack.data(), stack_known.data(), stack.size()},
  }};
  std::array<Nba97FrontendDispatchEntryAccess, 8> journal{};
  Nba97FrontendDispatchEntryContext context{};
  Nba97FrontendDispatchEntryProgress progress{};
  Nba97FrontendDispatchEntryMachine initial{};
  Nba97FrontendDispatchEntryMachine at_call{};
  Nba97FrontendDispatchEntryEvent event{};
  unsigned calls = 0;
  std::uint32_t relocated_sp = Stack + 0x300u;
  std::uint32_t relocated_ra = 0x13579bdfu;

  Fixture() {
    stack.fill(0xcd);
    stack_known.fill(1);
    for (unsigned i = 0; i < NBA97_FRONTEND_DISPATCH_ENTRY_REGISTER_COUNT;
         ++i)
      initial.registers.gpr[i] = {0x41000000u + i * 0x01010101u,
                                  static_cast<std::uint8_t>(i & 0x0fu)};
    initial.registers.gpr[0] = {0, 0x0f};
    initial.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_SP] = {EntrySp, 0x0f};
    initial.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_RA] = {CallerRa, 0x0f};
    initial.hi = {0x10203040u, 5};
    initial.lo = {0x50607080u, 10};
    context.memory = {regions.data(), regions.size()};
    context.operation_budget = 5;
    context.machine = initial;
    context.io = callback;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
  }

  std::uint8_t *byte(std::uint32_t address) {
    for (auto &region : regions) {
      if (address < region.base)
        continue;
      const std::uint64_t offset = std::uint64_t(address) - region.base;
      if (offset < region.size)
        return region.data + offset;
    }
    return nullptr;
  }
  std::uint8_t *known(std::uint32_t address) {
    for (auto &region : regions) {
      if (address < region.base)
        continue;
      const std::uint64_t offset = std::uint64_t(address) - region.base;
      if (offset < region.size)
        return region.known ? region.known + offset : nullptr;
    }
    return nullptr;
  }
  void put(std::uint32_t address, std::uint32_t value,
           std::uint8_t mask = 0x0f) {
    for (unsigned i = 0; i < 4; ++i) {
      CHECK(byte(address + i));
      *byte(address + i) = static_cast<std::uint8_t>(value >> (i * 8u));
      if (known(address + i))
        *known(address + i) = static_cast<std::uint8_t>((mask >> i) & 1u);
    }
  }
  std::uint32_t get(std::uint32_t address) {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) {
      CHECK(byte(address + i));
      value |= std::uint32_t(*byte(address + i)) << (i * 8u);
    }
    return value;
  }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97FrontendDispatchEntryEvent *event,
                      Nba97FrontendDispatchEntryMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    ++f.calls;
    f.event = *event;
    f.at_call = *machine;
    if (f.mode == Refuse)
      return 0;
    if (f.mode == InvalidZero)
      machine->registers.gpr[0] = {1, 0x0f};
    else if (f.mode == InvalidGpr)
      machine->registers.gpr[7].known_mask = 0x10;
    else if (f.mode == InvalidHi)
      machine->hi.known_mask = 0x10;
    else if (f.mode == InvalidLo)
      machine->lo.known_mask = 0xff;
    else if (f.mode == RelocateSp) {
      machine->registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_SP] =
          {f.relocated_sp, 0x0f};
      f.put(f.relocated_sp + 16u, f.relocated_ra);
      machine->registers.gpr[9] = {0xdecafbad, 3};
      machine->hi = {0xaabbccddu, 6};
      machine->lo = {0x11223344u, 9};
    } else if (f.mode == PartialSavedRa) {
      f.put(Frame + 16u, f.relocated_ra, 7);
    } else if (f.mode == RefuseMutated) {
      machine->registers.gpr[9] = {0xdecafbad, 3};
      machine->registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_SP] =
          {f.relocated_sp, 0x0f};
      machine->hi = {0xaabbccddu, 6};
      machine->lo = {0x11223344u, 9};
      return 0;
    } else if (f.mode == LiveSpUnknown) {
      machine->registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_SP] =
          {f.relocated_sp, 0x0e};
    } else if (f.mode == LiveSpUnaligned) {
      machine->registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_SP] =
          {f.relocated_sp + 1u, 0x0f};
    } else if (f.mode == LiveSpUnmapped) {
      machine->registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_SP] =
          {0x90000000u, 0x0f};
    } else if (f.mode == MalformedRestore) {
      *f.known(Frame + 19u) = 2;
    }
    return 1;
  }

  int run() { return nba97_frontend_dispatch_entry(&context, &progress); }
};

void exactNormalPath() {
  Fixture f;
  const auto initial = f.initial;
  CHECK(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
  CHECK(f.progress.instruction_count == 14 && f.progress.operations == 5 &&
        f.progress.accesses == 4 && f.progress.reads == 1 &&
        f.progress.stores == 3 && f.progress.access_events == 4 &&
        f.progress.callback_attempts == 1 &&
        f.progress.callbacks_completed == 1 && f.calls == 1);
  CHECK(f.get(Flag) == 1 && f.get(Scalar) == 32 &&
        f.get(Frame + 16u) == CallerRa);
  CHECK(f.event.pc == 0x800360f4u && f.event.delay_slot_pc == 0x800360f8u &&
        f.event.entry == 0x8003f7c8u && f.event.operation == 4 &&
        f.event.invocation == 1 && f.event.argument_count == 0);
  CHECK(f.at_call.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_V0].word == 32 &&
        f.at_call.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_AT].word ==
            0x800c0000u &&
        f.at_call.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_RA].word ==
            0x800360fcu &&
        f.at_call.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_SP].word ==
            Frame);
  CHECK(f.progress.frame_stack_pointer == Frame &&
        same(f.progress.saved_return_address,
             initial.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_RA]) &&
        same(f.progress.restored_return_address,
             initial.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_RA]));
  for (unsigned i = 0; i < NBA97_FRONTEND_DISPATCH_ENTRY_REGISTER_COUNT; ++i) {
    auto expected = initial.registers.gpr[i];
    if (i == NBA97_FRONTEND_DISPATCH_ENTRY_AT)
      expected = {0x800c0000u, 0x0f};
    else if (i == NBA97_FRONTEND_DISPATCH_ENTRY_V0)
      expected = {32, 0x0f};
    CHECK(same(f.progress.machine.registers.gpr[i], expected));
  }
  CHECK(same(f.progress.machine.hi, initial.hi) &&
        same(f.progress.machine.lo, initial.lo));

  const std::array<std::uint32_t, 4> pc{{0x800360e0u, 0x800360e8u,
                                         0x800360f0u, 0x800360fcu}};
  const std::array<std::uint32_t, 4> address{{Flag, Frame + 16u, Scalar,
                                              Frame + 16u}};
  const std::array<std::uint32_t, 4> value{{1, CallerRa, 32, CallerRa}};
  const std::array<std::size_t, 4> operation{{1, 2, 3, 5}};
  for (unsigned i = 0; i < 4; ++i) {
    CHECK(f.journal[i].pc == pc[i] && f.journal[i].address == address[i] &&
          f.journal[i].value == value[i] &&
          f.journal[i].operation == operation[i] && f.journal[i].width == 4 &&
          f.journal[i].known_mask == 0x0f &&
          f.journal[i].kind == (i == 3 ? NBA97_FRONTEND_DISPATCH_ENTRY_READ
                                       : NBA97_FRONTEND_DISPATCH_ENTRY_STORE));
  }
}

void callbackLiveMachineAndFailures() {
  Fixture relocated;
  relocated.mode = Fixture::RelocateSp;
  CHECK(relocated.run() == NBA97_TEXT_COMPLETE && relocated.progress.completed &&
        relocated.progress.machine.registers.gpr[
            NBA97_FRONTEND_DISPATCH_ENTRY_SP].word ==
            relocated.relocated_sp + 24u &&
        relocated.progress.restored_return_address.word ==
            relocated.relocated_ra &&
        relocated.progress.machine.registers.gpr[
            NBA97_FRONTEND_DISPATCH_ENTRY_RA].word == relocated.relocated_ra &&
        relocated.progress.machine.registers.gpr[9].known_mask == 3 &&
        relocated.progress.machine.hi.word == 0xaabbccddu &&
        relocated.progress.machine.hi.known_mask == 6 &&
        relocated.progress.machine.lo.word == 0x11223344u &&
        relocated.progress.machine.lo.known_mask == 9 &&
        relocated.journal[3].address == relocated.relocated_sp + 16u);

  Fixture refused;
  refused.mode = Fixture::Refuse;
  CHECK(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.progress.operations == 4 &&
        refused.progress.instruction_count == 10 &&
        refused.progress.callback_attempts == 1 &&
        refused.progress.callbacks_completed == 0 && refused.calls == 1 &&
        refused.progress.machine.registers.gpr[
            NBA97_FRONTEND_DISPATCH_ENTRY_RA].word == 0x800360fcu);

  Fixture refused_mutated;
  refused_mutated.mode = Fixture::RefuseMutated;
  CHECK(refused_mutated.run() == NBA97_TEXT_IO_REFUSED &&
        refused_mutated.progress.callbacks_completed == 0 &&
        refused_mutated.progress.machine.registers.gpr[9].word ==
            0xdecafbadu &&
        refused_mutated.progress.machine.registers.gpr[9].known_mask == 3 &&
        refused_mutated.progress.machine.registers.gpr[
            NBA97_FRONTEND_DISPATCH_ENTRY_SP].word ==
            refused_mutated.relocated_sp &&
        refused_mutated.progress.machine.hi.word == 0xaabbccddu &&
        refused_mutated.progress.machine.lo.word == 0x11223344u);

  for (auto mode : {Fixture::InvalidZero, Fixture::InvalidGpr,
                    Fixture::InvalidHi, Fixture::InvalidLo}) {
    Fixture malformed;
    malformed.mode = mode;
    CHECK(malformed.run() == NBA97_TEXT_ARGUMENT &&
          malformed.progress.operations == 4 &&
          malformed.progress.instruction_count == 10 &&
          malformed.progress.callback_attempts == 1 &&
          malformed.progress.callbacks_completed == 0 &&
          malformed.calls == 1);
  }

  Fixture partial;
  partial.mode = Fixture::PartialSavedRa;
  CHECK(partial.run() == NBA97_TEXT_UNKNOWN && !partial.progress.completed &&
        partial.progress.callbacks_completed == 1 &&
        partial.progress.operations == 5 &&
        partial.progress.reads == 1 &&
        partial.progress.instruction_count == 14 &&
        partial.progress.stopped_pc == 0x80036104u &&
        partial.progress.restored_return_address.word == partial.relocated_ra &&
        partial.progress.restored_return_address.known_mask == 7 &&
        partial.progress.machine.registers.gpr[
            NBA97_FRONTEND_DISPATCH_ENTRY_SP].word == EntrySp);

  for (auto mode : {Fixture::LiveSpUnknown, Fixture::LiveSpUnaligned,
                    Fixture::LiveSpUnmapped}) {
    Fixture live_sp;
    live_sp.mode = mode;
    const int expected = mode == Fixture::LiveSpUnknown
                             ? NBA97_TEXT_UNKNOWN
                             : mode == Fixture::LiveSpUnaligned
                                   ? NBA97_TEXT_ALIGNMENT_TRAP
                                   : NBA97_TEXT_RESOURCE;
    CHECK(live_sp.run() == expected &&
          live_sp.progress.callbacks_completed == 1 &&
          live_sp.progress.instruction_count == 11 &&
          live_sp.progress.stopped_pc == 0x800360fcu &&
          live_sp.progress.operations ==
              (mode == Fixture::LiveSpUnknown ? 4u : 5u));
  }

  Fixture malformed_restore;
  malformed_restore.mode = Fixture::MalformedRestore;
  CHECK(malformed_restore.run() == NBA97_TEXT_ARGUMENT &&
        malformed_restore.progress.callbacks_completed == 1 &&
        malformed_restore.progress.operations == 5 &&
        malformed_restore.progress.instruction_count == 11 &&
        malformed_restore.progress.reads == 0 &&
        malformed_restore.progress.machine.registers.gpr[
            NBA97_FRONTEND_DISPATCH_ENTRY_RA].word == 0x800360fcu);
}

void budgetsAndPrefixes() {
  const std::array<std::uint32_t, 5> stopped{{0x800360e0u, 0x800360e8u,
                                              0x800360f0u, 0x800360f4u,
                                              0x800360fcu}};
  const std::array<std::uint32_t, 5> instructions{{4, 6, 8, 10, 11}};
  for (unsigned budget = 0; budget < 5; ++budget) {
    Fixture f;
    f.context.operation_budget = budget;
    CHECK(f.run() == NBA97_TEXT_LIMIT &&
          f.progress.operations == budget &&
          f.progress.stopped_pc == stopped[budget] &&
          f.progress.instruction_count == instructions[budget] &&
          f.progress.stores == (budget < 3 ? budget : 3) &&
          f.progress.callbacks_completed == (budget == 4 ? 1u : 0u));
    CHECK(f.get(Flag) == (budget ? 1u : 0xcdcdcdcdu));
    CHECK(f.get(Scalar) == (budget >= 3 ? 32u : 0xcdcdcdcdu));
  }
  Fixture complete;
  complete.context.operation_budget = 5;
  CHECK(complete.run() == NBA97_TEXT_COMPLETE);
}

void knownnessMappingWrappingAndAtomicity() {
  Fixture unknown_sp;
  unknown_sp.context.machine.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_SP]
      .known_mask = 0x0e;
  CHECK(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stores == 1 &&
        unknown_sp.progress.operations == 1 &&
        unknown_sp.progress.instruction_count == 6 &&
        unknown_sp.progress.stopped_pc == 0x800360e8u &&
        unknown_sp.get(Flag) == 1);

  Fixture unaligned;
  unaligned.context.machine.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_SP]
      .word += 2u;
  CHECK(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.operations == 2 && unaligned.progress.stores == 1 &&
        unaligned.progress.stopped_pc == 0x800360e8u);

  Fixture missing_stack;
  missing_stack.context.machine.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_SP] =
      {0x90000018u, 0x0f};
  CHECK(missing_stack.run() == NBA97_TEXT_RESOURCE &&
        missing_stack.progress.stopped_address == 0x90000010u &&
        missing_stack.progress.stores == 1);

  Fixture malformed_late;
  *malformed_late.known(Flag + 3u) = 2;
  const auto before = malformed_late.get(Flag);
  CHECK(malformed_late.run() == NBA97_TEXT_ARGUMENT &&
        malformed_late.progress.operations == 1 &&
        malformed_late.progress.stores == 0 &&
        malformed_late.get(Flag) == before);

  Fixture malformed_saved_ra;
  *malformed_saved_ra.known(Frame + 19u) = 2;
  const auto saved_before = malformed_saved_ra.get(Frame + 16u);
  CHECK(malformed_saved_ra.run() == NBA97_TEXT_ARGUMENT &&
        malformed_saved_ra.progress.operations == 2 &&
        malformed_saved_ra.progress.stores == 1 &&
        malformed_saved_ra.get(Frame + 16u) == saved_before &&
        malformed_saved_ra.get(Flag) == 1);

  Fixture malformed_scalar;
  *malformed_scalar.known(Scalar + 3u) = 2;
  const auto scalar_before = malformed_scalar.get(Scalar);
  CHECK(malformed_scalar.run() == NBA97_TEXT_ARGUMENT &&
        malformed_scalar.progress.operations == 3 &&
        malformed_scalar.progress.stores == 2 &&
        malformed_scalar.get(Scalar) == scalar_before &&
        malformed_scalar.get(Flag) == 1 &&
        malformed_scalar.get(Frame + 16u) == CallerRa);

  Fixture unknown_destinations;
  for (unsigned i = 0; i < 4; ++i) {
    *unknown_destinations.known(Flag + i) = 0;
    *unknown_destinations.known(Scalar + i) = 0;
  }
  CHECK(unknown_destinations.run() == NBA97_TEXT_COMPLETE);
  for (unsigned i = 0; i < 4; ++i)
    CHECK(*unknown_destinations.known(Flag + i) == 1 &&
          *unknown_destinations.known(Scalar + i) == 1);

  Fixture no_known;
  no_known.regions[0].known = nullptr;
  no_known.regions[1].known = nullptr;
  CHECK(no_known.run() == NBA97_TEXT_COMPLETE && no_known.get(Flag) == 1 &&
        no_known.get(Scalar) == 32);

  Fixture partial_ra_no_plane;
  partial_ra_no_plane.regions[1].known = nullptr;
  partial_ra_no_plane.context.machine.registers.gpr[
      NBA97_FRONTEND_DISPATCH_ENTRY_RA].known_mask = 7;
  CHECK(partial_ra_no_plane.run() == NBA97_TEXT_ARGUMENT &&
        partial_ra_no_plane.progress.operations == 2 &&
        partial_ra_no_plane.progress.stores == 1 &&
        partial_ra_no_plane.get(Flag) == 1);

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture known_plane;
    known_plane.context.machine.registers.gpr[
        NBA97_FRONTEND_DISPATCH_ENTRY_RA].known_mask =
        static_cast<std::uint8_t>(mask);
    const int expected = mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN;
    CHECK(known_plane.run() == expected &&
          known_plane.progress.saved_return_address.known_mask == mask &&
          known_plane.journal[1].known_mask == mask &&
          known_plane.progress.restored_return_address.known_mask == mask &&
          known_plane.progress.instruction_count == 14);

    Fixture absent_plane;
    absent_plane.regions[1].known = nullptr;
    absent_plane.context.machine.registers.gpr[
        NBA97_FRONTEND_DISPATCH_ENTRY_RA].known_mask =
        static_cast<std::uint8_t>(mask);
    CHECK(absent_plane.run() ==
              (mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_ARGUMENT) &&
          absent_plane.progress.stores == (mask == 15 ? 3u : 1u));
  }

  Fixture split;
  std::array<Nba97GameTextRegion, 4> split_regions{{
      {Flag, split.byte(Flag), split.known(Flag), 4},
      {Scalar, split.byte(Scalar), split.known(Scalar), 4},
      {Frame + 16u, split.byte(Frame + 16u), split.known(Frame + 16u), 4},
      {Stack + 0x300u, split.byte(Stack + 0x300u),
       split.known(Stack + 0x300u), 0x100},
  }};
  split.context.memory = {split_regions.data(), split_regions.size()};
  CHECK(split.run() == NBA97_TEXT_COMPLETE);

  std::array<std::uint8_t, 4> zero_data{};
  std::array<std::uint8_t, 4> zero_known{{1, 1, 1, 1}};
  Fixture wrapped;
  std::array<Nba97GameTextRegion, 4> wrap_regions{{
      {Flag, wrapped.byte(Flag), wrapped.known(Flag), 4},
      {Scalar, wrapped.byte(Scalar), wrapped.known(Scalar), 4},
      {0, zero_data.data(), zero_known.data(), zero_data.size()},
      {0xfffffff0u, wrapped.stack.data(), wrapped.stack_known.data(), 16},
  }};
  wrapped.context.memory = {wrap_regions.data(), wrap_regions.size()};
  wrapped.context.machine.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_SP] =
      {8, 0x0f};
  CHECK(wrapped.run() == NBA97_TEXT_COMPLETE &&
        wrapped.progress.frame_stack_pointer == 0xfffffff0u &&
        wrapped.progress.machine.registers.gpr[
            NBA97_FRONTEND_DISPATCH_ENTRY_SP].word == 8 &&
        wrapped.progress.saved_return_address.word == CallerRa);

  Fixture zero_sp;
  std::array<std::uint8_t, 24> top_data{};
  std::array<std::uint8_t, 24> top_known{};
  top_known.fill(1);
  std::array<Nba97GameTextRegion, 3> zero_regions{{
      {Flag, zero_sp.byte(Flag), zero_sp.known(Flag), 4},
      {Scalar, zero_sp.byte(Scalar), zero_sp.known(Scalar), 4},
      {0xffffffe8u, top_data.data(), top_known.data(), top_data.size()},
  }};
  zero_sp.context.memory = {zero_regions.data(), zero_regions.size()};
  zero_sp.context.machine.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_SP] =
      {0, 0x0f};
  CHECK(zero_sp.run() == NBA97_TEXT_COMPLETE &&
        zero_sp.progress.frame_stack_pointer == 0xffffffe8u &&
        zero_sp.progress.machine.registers.gpr[
            NBA97_FRONTEND_DISPATCH_ENTRY_SP].word == 0);

  Fixture max_address;
  max_address.context.machine.registers.gpr[
      NBA97_FRONTEND_DISPATCH_ENTRY_SP] = {7, 0x0f};
  CHECK(max_address.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        max_address.progress.stopped_pc == 0x800360e8u &&
        max_address.progress.stopped_address == UINT32_MAX);

  Fixture alias_flag;
  alias_flag.context.machine.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_SP] =
      {Flag + 8u, 0x0f};
  CHECK(alias_flag.run() == NBA97_TEXT_COMPLETE &&
        alias_flag.get(Flag) == CallerRa &&
        alias_flag.progress.restored_return_address.word == CallerRa);

  Fixture alias_scalar;
  alias_scalar.context.machine.registers.gpr[
      NBA97_FRONTEND_DISPATCH_ENTRY_SP] = {Scalar + 8u, 0x0f};
  CHECK(alias_scalar.run() == NBA97_TEXT_COMPLETE &&
        alias_scalar.get(Scalar) == 32 &&
        alias_scalar.progress.restored_return_address.word == 32 &&
        alias_scalar.progress.machine.registers.gpr[
            NBA97_FRONTEND_DISPATCH_ENTRY_RA].word == 32);
}

void validationAndRepeatability() {
  Fixture overlap;
  std::array<Nba97GameTextRegion, 2> duplicate{{overlap.regions[0],
                                                overlap.regions[0]}};
  overlap.context.memory = {duplicate.data(), duplicate.size()};
  const auto before = overlap.ram;
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT &&
        overlap.progress.operations == 0 && overlap.ram == before);

  Fixture huge;
  std::uint8_t byte = 0;
  Nba97GameTextRegion huge_region{1, &byte, nullptr,
                                  std::numeric_limits<std::size_t>::max()};
  huge.context.memory = {&huge_region, 1};
  CHECK(huge.run() == NBA97_TEXT_ARGUMENT && huge.progress.operations == 0);

  Fixture invalid_mask;
  invalid_mask.context.machine.registers.gpr[8].known_mask = 16;
  const auto invalid_before = invalid_mask.ram;
  CHECK(invalid_mask.run() == NBA97_TEXT_ARGUMENT &&
        invalid_mask.progress.operations == 0 &&
        invalid_mask.ram == invalid_before &&
        std::memcmp(&invalid_mask.context.machine, &invalid_mask.initial,
                    sizeof invalid_mask.initial) != 0);
  Nba97FrontendDispatchEntryProgress progress{};
  CHECK(nba97_frontend_dispatch_entry(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
  Fixture null_out;
  CHECK(nba97_frontend_dispatch_entry(&null_out.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);

  Fixture first;
  Fixture second;
  CHECK(first.run() == NBA97_TEXT_COMPLETE &&
        second.run() == NBA97_TEXT_COMPLETE);
  CHECK(first.progress.operations == second.progress.operations &&
        first.progress.access_events == second.progress.access_events &&
        first.progress.instruction_count == second.progress.instruction_count &&
        first.progress.completed == second.progress.completed);
  for (unsigned i = 0; i < NBA97_FRONTEND_DISPATCH_ENTRY_REGISTER_COUNT; ++i)
    CHECK(same(first.progress.machine.registers.gpr[i],
               second.progress.machine.registers.gpr[i]));
  CHECK(same(first.progress.machine.hi, second.progress.machine.hi) &&
        same(first.progress.machine.lo, second.progress.machine.lo));
  for (std::size_t i = 0; i < first.progress.access_events; ++i)
    CHECK(first.journal[i].pc == second.journal[i].pc &&
          first.journal[i].address == second.journal[i].address &&
          first.journal[i].value == second.journal[i].value &&
          first.journal[i].operation == second.journal[i].operation &&
          first.journal[i].width == second.journal[i].width &&
          first.journal[i].known_mask == second.journal[i].known_mask &&
          first.journal[i].kind == second.journal[i].kind);
}
} // namespace

int main() {
  exactNormalPath();
  callbackLiveMachineAndFailures();
  budgetsAndPrefixes();
  knownnessMappingWrappingAndAtomicity();
  validationAndRepeatability();
  std::printf("frontend_dispatch_entry_tests: PASS (%u checks)\n", checks);
  return 0;
}
