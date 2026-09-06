#include "recovered/game_opponent_contact.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "opponent contact check %u failed at %u\n", checks,
                 line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t First = 0x80030000u;
constexpr std::uint32_t Second = 0x80040000u;
constexpr std::uint32_t EntrySp = 0x800ff000u;
constexpr std::uint32_t FrameSp = EntrySp - 0x18u;

struct Fixture {
  enum Mode {
    Ordinary,
    Refuse,
    InvalidMachine,
    MoveStack,
    UnknownMovedRa
  } mode = Ordinary;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x110000u, 0);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x110000u, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameOpponentContactAccess, 32> journal{};
  Nba97GameOpponentContactContext context{};
  Nba97GameOpponentContactProgress progress{};
  Nba97GameOpponentContactMachine incoming{};
  Nba97GameOpponentContactMachine child_machine{};
  Nba97GameOpponentContactEvent event{};
  unsigned calls{};
  Nba97GameOpponentContactWord child_result{0x12345600u, 0x0f};

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      incoming.registers.gpr[i] = {0x21000000u + i * 0x01010101u,
                                   static_cast<std::uint8_t>((i % 15u) + 1u)};
    incoming.registers.gpr[0] = {0, 0x0f};
    incoming.registers.gpr[4] = {First, 0x0f};
    incoming.registers.gpr[5] = {Second, 0x0f};
    incoming.registers.gpr[29] = {EntrySp, 0x0f};
    incoming.registers.gpr[31] = {0x81234568u, 0x0f};
    incoming.hi = {0x13572468u, 5};
    incoming.lo = {0x89abcdefu, 0x0a};
    context.memory = {&region, 1};
    context.operation_budget = 64;
    context.machine = incoming;
    context.io = io;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    put(First, 7, 4);
    put(First + 0xc2u, 0, 2);
    put(First + 0xdau, 0, 1);
    put(Second + 0xc2u, 0, 2);
    put(Second + 0xdau, 0, 1);
    put(0x80021d8au, 0, 1);
    put(0x800fdb90u, 0, 2);
    put(0x800fdbccu, 7, 2);
  }
  std::size_t offset(std::uint32_t address) const {
    return static_cast<std::size_t>(address - Ram);
  }
  void put(std::uint32_t address, std::uint32_t value, unsigned width,
           std::uint8_t mask = 0x0f) {
    auto at = offset(address);
    for (unsigned i = 0; i < width; ++i) {
      bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8u));
      known[at + i] = static_cast<std::uint8_t>((mask >> i) & 1u);
    }
  }
  std::uint32_t get(std::uint32_t address, unsigned width) const {
    auto at = offset(address);
    std::uint32_t result = 0;
    for (unsigned i = 0; i < width; ++i)
      result |= std::uint32_t(bytes[at + i]) << (i * 8u);
    return result;
  }
  static int io(void *opaque, const Nba97GameTextMemory *,
                const Nba97GameOpponentContactEvent *event,
                Nba97GameOpponentContactMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    ++f.calls;
    f.event = *event;
    f.child_machine = *machine;
    if (f.mode == Refuse) {
      for (unsigned i = 1; i < 32; ++i)
        machine->registers.gpr[i] = {0x60000000u + i * 0x00010101u,
                                     static_cast<std::uint8_t>(i & 0x0fu)};
      machine->hi = {0x24681357u, 6};
      machine->lo = {0xfedcba98u, 9};
      return 0;
    }
    if (f.mode == InvalidMachine) {
      machine->hi.known_mask = 0x10;
      return 1;
    }
    if (f.mode == MoveStack || f.mode == UnknownMovedRa) {
      machine->registers.gpr[29] = {0x800fee00u, 0x0f};
      machine->registers.gpr[6] = {0xdeadbeefu, 3};
      machine->hi = {0xabcdef01u, 6};
      machine->lo = {0x10203040u, 9};
      f.put(0x800fee10u, 0x82345678u, 4, f.mode == UnknownMovedRa ? 7 : 15);
    }
    machine->registers.gpr[2] = f.child_result;
    return 1;
  }
  int run() {
    context.machine = incoming;
    return nba97_game_opponent_contact(&context, &progress);
  }
};

void c2_option_and_phase_gates() {
  Fixture both_zero;
  both_zero.put(0x80021d8au, 0x55, 1, 0);
  both_zero.put(0x800fdb90u, 0x1234, 2, 0);
  check(both_zero.run() == NBA97_TEXT_COMPLETE && both_zero.calls == 1 &&
        both_zero.progress.option.known_mask == 0 &&
        both_zero.progress.phase.known_mask == 0);
  check(both_zero.progress.accesses == 5 &&
        both_zero.journal[1].address == First + 0xc2u &&
        both_zero.journal[2].address == Second + 0xc2u &&
        both_zero.journal[3].address == Second + 0xdau);

  for (auto first_c2 : {1u, 0xffffu, 0x8000u}) {
    Fixture f;
    f.put(First + 0xc2u, first_c2, 2);
    f.put(Second + 0xc2u, 0x3344, 2, 0);
    check(f.run() == NBA97_TEXT_COMPLETE && f.calls == 1);
    bool read_second = false;
    for (unsigned i = 0; i < f.progress.access_events; ++i)
      read_second |= f.journal[i].address == Second + 0xc2u;
    check(!read_second);
  }

  for (auto option : {1u, 255u}) {
    Fixture f;
    f.put(First + 0xc2u, 1, 2);
    f.put(0x80021d8au, option, 1);
    check(f.run() == NBA97_TEXT_COMPLETE && f.calls == 0 &&
          f.progress.returned_value.word == 0 &&
          f.progress.returned_value.known_mask == 0x0f);
  }
  for (auto phase : {128u, 129u, 0xffffu}) {
    Fixture f;
    f.put(Second + 0xc2u, 1, 2);
    f.put(0x800fdb90u, phase, 2);
    check(f.run() == NBA97_TEXT_COMPLETE);
    check(f.calls == (phase == 129 ? 0u : 1u));
  }
}

void all_contact_orders_and_owner_width() {
  struct Case {
    unsigned second_da;
    unsigned first_da;
    std::uint32_t owner;
    std::uint32_t id;
    std::uint32_t a0;
    std::uint32_t a1;
  };
  constexpr std::array<Case, 6> cases{{
      {0, 0, 7, 7, First, Second},
      {0, 1, 7, 7, First, Second},
      {1, 0, 7, 7, Second, First},
      {1, 1, 7, 7, First, Second},
      {1, 1, 8, 7, Second, First},
      {1, 1, 0xffffu, 0xffffffffu, First, Second},
  }};
  for (const auto &test : cases) {
    Fixture f;
    f.put(Second + 0xdau, test.second_da, 1);
    f.put(First + 0xdau, test.first_da, 1);
    f.put(0x800fdbccu, test.owner, 2);
    f.put(First, test.id, 4);
    check(f.run() == NBA97_TEXT_COMPLETE && f.calls == 1);
    check(f.event.pc == 0x8005f92cu && f.event.delay_slot_pc == 0x8005f930u &&
          f.event.entry == 0x8005f3bcu && f.event.argument_count == 2 &&
          f.child_machine.registers.gpr[31].word == 0x8005f934u);
    check(f.child_machine.registers.gpr[4].word == test.a0 &&
          f.child_machine.registers.gpr[5].word == test.a1 &&
          f.child_machine.registers.gpr[6].word == First);
  }

  Fixture width_mismatch;
  width_mismatch.put(Second + 0xdau, 1, 1);
  width_mismatch.put(First + 0xdau, 1, 1);
  width_mismatch.put(0x800fdbccu, 0xffffu, 2);
  width_mismatch.put(First, 0x0000ffffu, 4);
  check(width_mismatch.run() == NBA97_TEXT_COMPLETE &&
        width_mismatch.child_machine.registers.gpr[4].word == Second &&
        width_mismatch.child_machine.registers.gpr[5].word == First);
  check(width_mismatch.journal[5].pc == 0x8005f90cu &&
        width_mismatch.journal[5].address == 0x800fdbccu &&
        width_mismatch.journal[6].pc == 0x8005f910u &&
        width_mismatch.journal[6].address == First);
}

void return_values_and_knownness() {
  constexpr std::array<std::uint32_t, 4> words{{0, 1, 0x100u, 0xffffffffu}};
  for (auto word : words) {
    for (unsigned mask = 0; mask < 16; ++mask) {
      Fixture f;
      f.child_result = {word, static_cast<std::uint8_t>(mask)};
      check(f.run() == NBA97_TEXT_COMPLETE);
      check(f.progress.returned_value.word == (word & 0xffu) &&
            f.progress.returned_value.known_mask ==
                static_cast<std::uint8_t>((mask & 1u) | 0x0eu));
    }
  }
}

void unknown_branches_and_delay_publication() {
  Fixture first_c2;
  first_c2.put(First + 0xc2u, 0, 2, 0);
  check(first_c2.run() == NBA97_TEXT_UNKNOWN &&
        first_c2.progress.stopped_pc == 0x8005f89cu &&
        first_c2.progress.instruction_count == 7);

  Fixture option;
  option.put(First + 0xc2u, 1, 2);
  option.put(0x80021d8au, 0, 1, 0);
  check(option.run() == NBA97_TEXT_UNKNOWN &&
        option.progress.stopped_pc == 0x8005f8c0u &&
        option.progress.returned_value.word == 0 &&
        option.progress.returned_value.known_mask == 0x0f);

  Fixture phase;
  phase.put(First + 0xc2u, 1, 2);
  phase.put(0x800fdb90u, 128, 2, 1);
  check(phase.run() == NBA97_TEXT_UNKNOWN &&
        phase.progress.stopped_pc == 0x8005f8d8u &&
        phase.progress.last_predicate.known_mask == 0x0e &&
        phase.progress.returned_value.known_mask == 0x0f);

  Fixture second_da;
  second_da.put(Second + 0xdau, 0, 1, 0);
  check(second_da.run() == NBA97_TEXT_UNKNOWN &&
        second_da.progress.stopped_pc == 0x8005f8ecu &&
        second_da.progress.machine.registers.gpr[4].word == First);
  Fixture first_da;
  first_da.put(Second + 0xdau, 1, 1);
  first_da.put(First + 0xdau, 0, 1, 0);
  check(first_da.run() == NBA97_TEXT_UNKNOWN &&
        first_da.progress.stopped_pc == 0x8005f900u &&
        first_da.progress.machine.registers.gpr[4].word == Second &&
        first_da.progress.machine.registers.gpr[5].word == Second);
}

void budgets_callbacks_and_live_stack() {
  Fixture complete;
  complete.put(Second + 0xdau, 1, 1);
  complete.put(First + 0xdau, 1, 1);
  check(complete.run() == NBA97_TEXT_COMPLETE);
  const auto operations = complete.progress.operations;
  for (std::size_t budget = 0; budget < operations; ++budget) {
    Fixture prefix;
    prefix.put(Second + 0xdau, 1, 1);
    prefix.put(First + 0xdau, 1, 1);
    prefix.context.operation_budget = budget;
    check(prefix.run() == NBA97_TEXT_LIMIT &&
          prefix.progress.operations == budget);
  }

  Fixture refused;
  refused.mode = Fixture::Refuse;
  check(refused.run() == NBA97_TEXT_IO_REFUSED && refused.calls == 1 &&
        refused.progress.stopped_pc == 0x8005f92cu &&
        refused.progress.stopped_entry == 0x8005f3bcu &&
        refused.progress.machine.hi.word == 0x24681357u &&
        refused.progress.machine.lo.known_mask == 9);
  for (unsigned i = 1; i < 32; ++i)
    check(refused.progress.machine.registers.gpr[i].word ==
              0x60000000u + i * 0x00010101u &&
          refused.progress.machine.registers.gpr[i].known_mask == (i & 15u));

  Fixture invalid;
  invalid.mode = Fixture::InvalidMachine;
  check(invalid.run() == NBA97_TEXT_ARGUMENT &&
        invalid.progress.machine.hi.known_mask == 0x10);
  Fixture moved;
  moved.mode = Fixture::MoveStack;
  check(moved.run() == NBA97_TEXT_COMPLETE &&
        moved.progress.frame_stack_pointer == FrameSp &&
        moved.progress.restored_return_address.word == 0x82345678u &&
        moved.progress.machine.registers.gpr[29].word == 0x800fee18u &&
        moved.progress.machine.registers.gpr[6].word == 0xdeadbeefu &&
        moved.progress.machine.registers.gpr[6].known_mask == 3 &&
        moved.progress.machine.hi.word == 0xabcdef01u &&
        moved.progress.machine.lo.known_mask == 9);
  Fixture unknown_ra;
  unknown_ra.mode = Fixture::UnknownMovedRa;
  check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.stopped_pc == 0x8005f940u &&
        unknown_ra.progress.machine.registers.gpr[29].word == 0x800fee18u);
}

void saved_ra_and_hilo_knownness() {
  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture f;
    f.incoming.registers.gpr[31] = {0x81234568u,
                                    static_cast<std::uint8_t>(mask)};
    f.incoming.hi = {0x11223344u, static_cast<std::uint8_t>(mask)};
    f.incoming.lo = {0x55667788u, static_cast<std::uint8_t>(15u - mask)};
    check(f.run() == (mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN));
    check(f.progress.machine.registers.gpr[31].word == 0x81234568u &&
          f.progress.machine.registers.gpr[31].known_mask == mask);
    check(f.progress.machine.hi.word == 0x11223344u &&
          f.progress.machine.hi.known_mask == mask &&
          f.progress.machine.lo.word == 0x55667788u &&
          f.progress.machine.lo.known_mask == 15u - mask);
  }
}

void mapping_metadata_and_wrapping_stack() {
  Fixture unknown_sp;
  unknown_sp.incoming.registers.gpr[29].known_mask = 7;
  check(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stopped_pc == 0x8005f890u);
  Fixture unaligned;
  unaligned.incoming.registers.gpr[29].word += 1;
  check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x8005f890u);
  Fixture missing;
  missing.region.size = 0x100u;
  check(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc == 0x8005f890u);
  Fixture malformed;
  malformed.incoming.hi.known_mask = 0x10;
  check(malformed.run() == NBA97_TEXT_ARGUMENT);
  Fixture invalid_byte;
  invalid_byte.known[invalid_byte.offset(First + 0xc2u)] = 2;
  check(invalid_byte.run() == NBA97_TEXT_ARGUMENT &&
        invalid_byte.progress.stopped_pc == 0x8005f894u);

  Fixture no_known;
  no_known.region.known = nullptr;
  no_known.incoming.registers.gpr[31].known_mask = 7;
  check(no_known.run() == NBA97_TEXT_ARGUMENT &&
        no_known.progress.stopped_pc == 0x8005f890u);
  check(no_known.get(FrameSp + 0x10u, 4) == 0u &&
        no_known.progress.stores == 0u);
  Fixture id_alias;
  id_alias.incoming.registers.gpr[29].word = First + 8u;
  id_alias.incoming.registers.gpr[31].word = 7u;
  id_alias.put(First, 100u, 4);
  id_alias.put(First + 0xdau, 1u, 1);
  id_alias.put(Second + 0xdau, 1u, 1);
  check(id_alias.run() == NBA97_TEXT_COMPLETE &&
        id_alias.get(First, 4) == 7u &&
        id_alias.child_machine.registers.gpr[4].word == First &&
        id_alias.child_machine.registers.gpr[5].word == Second &&
        id_alias.progress.restored_return_address.word == 7u);
  Fixture overlap;
  std::array<Nba97GameTextRegion, 2> overlapping{{
      overlap.region,
      {Ram + 4u, overlap.bytes.data() + 4u, overlap.known.data() + 4u, 16},
  }};
  overlap.context.memory = {overlapping.data(), overlapping.size()};
  check(overlap.run() == NBA97_TEXT_ARGUMENT);
  check(nba97_game_opponent_contact(nullptr, &overlap.progress) ==
        NBA97_TEXT_ARGUMENT);
  check(nba97_game_opponent_contact(&overlap.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);

  Fixture wrap;
  wrap.incoming.registers.gpr[29] = {8, 0x0f};
  std::array<std::uint8_t, 16> high{};
  std::array<std::uint8_t, 16> high_known{};
  std::array<std::uint8_t, 8> low{};
  std::array<std::uint8_t, 8> low_known{};
  high_known.fill(1);
  low_known.fill(1);
  std::array<Nba97GameTextRegion, 3> regions{{
      wrap.region,
      {0xfffffff0u, high.data(), high_known.data(), high.size()},
      {0, low.data(), low_known.data(), low.size()},
  }};
  wrap.context.memory = {regions.data(), regions.size()};
  check(wrap.run() == NBA97_TEXT_COMPLETE &&
        wrap.progress.frame_stack_pointer == 0xfffffff0u &&
        wrap.progress.machine.registers.gpr[29].word == 8 &&
        wrap.progress.restored_return_address.word == 0x81234568u);
}
} // namespace

int main() {
  c2_option_and_phase_gates();
  all_contact_orders_and_owner_width();
  return_values_and_knownness();
  unknown_branches_and_delay_publication();
  budgets_callbacks_and_live_stack();
  saved_ra_and_hilo_knownness();
  mapping_metadata_and_wrapping_stack();
  std::printf("game opponent contact: %u checks\n", checks);
}
