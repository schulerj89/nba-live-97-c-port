#include "recovered/game_period_presentation_finish.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "period-presentation finish check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Source = 0x8001ede8u;
constexpr std::uint32_t Presentation = 0x800eb680u;
constexpr std::uint32_t Gate = 0x800fdb78u;
constexpr std::uint32_t Active = 0x80109afcu;
constexpr std::uint32_t Published = 0x80109ae4u;
constexpr std::uint32_t EntrySp = 0x800ff800u;
constexpr std::uint32_t Return = 0x81234568u;
constexpr std::uint32_t FinalV0 = 0xcafebabeu;

bool same(Nba97GamePeriodPresentationFinishWord a,
          Nba97GamePeriodPresentationFinishWord b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}

struct Call {
  Nba97GamePeriodPresentationFinishEvent event{};
  Nba97GamePeriodPresentationFinishMachine machine{};
};

struct Fixture {
  std::vector<std::uint8_t> bytes =
      std::vector<std::uint8_t>(0x120000u, 0xa5);
  std::vector<std::uint8_t> known =
      std::vector<std::uint8_t>(0x120000u, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GamePeriodPresentationFinishAccess, 16> journal{};
  Nba97GamePeriodPresentationFinishContext context{};
  Nba97GamePeriodPresentationFinishProgress progress{};
  std::vector<Call> calls;
  unsigned refuse = 0;
  unsigned invalidate_gpr = 0;
  unsigned invalidate_hi = 0;
  unsigned invalidate_lo = 0;
  bool rewrite_gate_first = false;
  std::uint8_t rewritten_gate = 0;
  std::uint8_t rewritten_gate_known = 1;
  bool relocate_second = false;
  std::uint32_t relocated_sp = EntrySp - 0x18u + 0x100u;
  std::uint32_t relocated_ra = 0x82468ac0u;
  std::uint8_t relocated_ra_mask = 15;

  explicit Fixture(std::uint8_t gate = 0, std::uint32_t source = 0x81234560u,
                   std::uint8_t source_mask = 15) {
    context.memory = {&region, 1};
    context.operation_budget = 16;
    context.io = io;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
      context.machine.registers.gpr[i].word =
          0x21000000u + i * 0x01010101u;
      context.machine.registers.gpr[i].known_mask =
          static_cast<std::uint8_t>((i * 7u) & 15u);
    }
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {Return, 15};
    context.machine.hi = {0x12345678u, 5};
    context.machine.lo = {0x9abcdef0u, 10};
    put(Source, source, 4, source_mask);
    put(Gate, gate, 1);
    put(Presentation, 0x7b, 1);
    put(Active, 0x55667788u, 4);
    put(Published, 0x11223344u, 4);
  }

  void put(std::uint32_t address, std::uint32_t value, unsigned width,
           std::uint8_t mask = 15) {
    const auto offset = static_cast<std::size_t>(address - Ram);
    for (unsigned i = 0; i < width; ++i) {
      bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8u));
      known[offset + i] = static_cast<std::uint8_t>((mask >> i) & 1u);
    }
  }

  std::uint32_t get(std::uint32_t address, unsigned width) const {
    const auto offset = static_cast<std::size_t>(address - Ram);
    std::uint32_t value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= std::uint32_t(bytes[offset + i]) << (i * 8u);
    return value;
  }

  std::uint8_t getKnown(std::uint32_t address, unsigned width) const {
    const auto offset = static_cast<std::size_t>(address - Ram);
    std::uint8_t mask = 0;
    for (unsigned i = 0; i < width; ++i)
      if (known[offset + i])
        mask = static_cast<std::uint8_t>(mask | (1u << i));
    return mask;
  }

  static int io(void *user, const Nba97GameTextMemory *,
                const Nba97GamePeriodPresentationFinishEvent *event,
                Nba97GamePeriodPresentationFinishMachine *machine) {
    auto &f = *static_cast<Fixture *>(user);
    f.calls.push_back({*event, *machine});
    const unsigned call = static_cast<unsigned>(f.calls.size());
    if (event->entry == 0x80044550u && f.rewrite_gate_first)
      f.put(Gate, f.rewritten_gate, 1, f.rewritten_gate_known);
    if (event->entry == 0x80046c2cu) {
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {FinalV0, 6};
      if (f.relocate_second) {
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
            f.relocated_sp, 15};
        machine->hi = {0xaabbccddu, 9};
        machine->lo = {0x55667788u, 6};
        f.put(f.relocated_sp + 0x10u, f.relocated_ra, 4,
              f.relocated_ra_mask);
      }
    }
    if (f.invalidate_gpr == call)
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_T9].known_mask = 16;
    if (f.invalidate_hi == call)
      machine->hi.known_mask = 16;
    if (f.invalidate_lo == call)
      machine->lo.known_mask = 16;
    return f.refuse == call ? 0 : 1;
  }

  int run() {
    return nba97_game_period_presentation_finish(&context, &progress);
  }
};

void both_paths_and_all_flag_bytes() {
  for (unsigned flag = 0; flag < 256; ++flag) {
    Fixture f(static_cast<std::uint8_t>(flag));
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.calls.size() == (flag == 0 ? 2u : 1u) &&
          f.progress.optional_child_called == (flag == 0));
    check(f.get(Presentation, 1) == 0 && f.get(Active, 4) == 0 &&
          f.get(Published, 4) == 0x81234560u &&
          f.progress.gate_flag.word == flag &&
          f.progress.gate_flag.known_mask == 15);
    check(f.progress.returned_value.word ==
              (flag == 0 ? FinalV0 : flag) &&
          f.progress.returned_value.known_mask ==
              (flag == 0 ? 6 : 15));
  }
}

void source_order_calls_and_untouched_machine() {
  Fixture f;
  const auto entry = f.context.machine;
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.operations == 10 &&
        f.progress.accesses == 8 && f.progress.reads == 3 &&
        f.progress.stores == 5 && f.progress.callbacks_completed == 2 &&
        f.progress.call_count[
            NBA97_GAME_PERIOD_PRESENTATION_FINISH_CHILD_80044550] == 1 &&
        f.progress.call_count[
            NBA97_GAME_PERIOD_PRESENTATION_FINISH_CHILD_80046C2C] == 1);
  check(f.calls[0].event.pc == 0x8002ddf8u &&
        f.calls[0].event.delay_slot_pc == 0x8002ddfcu &&
        f.calls[0].event.entry == 0x80044550u &&
        f.calls[0].event.operation == 6 &&
        f.calls[0].event.invocation == 1 &&
        f.calls[0].event.argument_count == 0 &&
        f.calls[0].machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x8002de00u &&
        f.calls[0].machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            0x81234560u &&
        f.calls[0].machine.registers.gpr[NBA97_MATCH_INITIALIZE_V1].word == 1);
  check(f.get(Presentation, 1) == 0 && f.get(Active, 4) == 0 &&
        f.get(Published, 4) == 0x81234560u &&
        f.get(EntrySp - 8u, 4) == Return);
  check(f.calls[1].event.pc == 0x8002de14u &&
        f.calls[1].event.delay_slot_pc == 0x8002de18u &&
        f.calls[1].event.entry == 0x80046c2cu &&
        f.calls[1].event.operation == 8 &&
        f.calls[1].event.invocation == 1 &&
        f.calls[1].event.argument_count == 0 &&
        f.calls[1].machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x8002de1cu);
  check(f.progress.frame_stack_pointer == EntrySp - 0x18u &&
        same(f.progress.saved_return_address,
             entry.registers.gpr[NBA97_MATCH_INITIALIZE_RA]) &&
        same(f.progress.restored_return_address,
             entry.registers.gpr[NBA97_MATCH_INITIALIZE_RA]) &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp &&
        same(f.progress.machine.hi, entry.hi) &&
        same(f.progress.machine.lo, entry.lo));
  for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
    if (i == NBA97_MATCH_INITIALIZE_AT || i == NBA97_MATCH_INITIALIZE_V0 ||
        i == NBA97_MATCH_INITIALIZE_V1)
      continue;
    check(same(f.progress.machine.registers.gpr[i],
               entry.registers.gpr[i]));
  }

  const std::array<std::uint32_t, 8> pcs{
      0x8002ddd0u, 0x8002dddcu, 0x8002dde4u, 0x8002ddecu,
      0x8002ddf4u, 0x8002de04u, 0x8002de20u, 0x8002de24u};
  const std::array<std::uint32_t, 8> addresses{
      Source, EntrySp - 8u, Presentation, Active, Published, Gate, Active,
      EntrySp - 8u};
  const std::array<std::size_t, 8> operations{1, 2, 3, 4, 5, 7, 9, 10};
  for (unsigned i = 0; i < pcs.size(); ++i)
    check(f.journal[i].pc == pcs[i] &&
          f.journal[i].address == addresses[i] &&
          f.journal[i].operation == operations[i] &&
          f.journal[i].kind ==
              ((i == 0 || i == 5 || i == 7)
                   ? NBA97_GAME_PERIOD_PRESENTATION_FINISH_READ
                   : NBA97_GAME_PERIOD_PRESENTATION_FINISH_STORE));
}

void callback_mutable_gate() {
  Fixture becomes_zero(0xff);
  becomes_zero.rewrite_gate_first = true;
  becomes_zero.rewritten_gate = 0;
  check(becomes_zero.run() == NBA97_TEXT_COMPLETE &&
        becomes_zero.calls.size() == 2 &&
        becomes_zero.progress.optional_child_called &&
        becomes_zero.progress.gate_flag.word == 0);

  Fixture becomes_nonzero(0);
  becomes_nonzero.rewrite_gate_first = true;
  becomes_nonzero.rewritten_gate = 0x80;
  check(becomes_nonzero.run() == NBA97_TEXT_COMPLETE &&
        becomes_nonzero.calls.size() == 1 &&
        !becomes_nonzero.progress.optional_child_called &&
        becomes_nonzero.progress.gate_flag.word == 0x80 &&
        becomes_nonzero.progress.returned_value.word == 0x80);

  Fixture becomes_unknown(0);
  becomes_unknown.rewrite_gate_first = true;
  becomes_unknown.rewritten_gate = 0x7f;
  becomes_unknown.rewritten_gate_known = 0;
  check(becomes_unknown.run() == NBA97_TEXT_UNKNOWN &&
        becomes_unknown.progress.stopped_pc == 0x8002de0cu &&
        becomes_unknown.calls.size() == 1 &&
        becomes_unknown.progress.gate_flag.word == 0x7f &&
        becomes_unknown.progress.gate_flag.known_mask == 14 &&
        becomes_unknown.get(Active, 4) == 1);
}

void every_budget_and_callback_failure() {
  const std::array<std::uint32_t, 10> stopped{
      0x8002ddd0u, 0x8002dddcu, 0x8002dde4u, 0x8002ddecu,
      0x8002ddf4u, 0x8002ddf8u, 0x8002de04u, 0x8002de14u,
      0x8002de20u, 0x8002de24u};
  for (unsigned budget = 0; budget <= 10; ++budget) {
    Fixture f;
    f.context.operation_budget = budget;
    const int result = f.run();
    check((budget == 10 && result == NBA97_TEXT_COMPLETE &&
           f.progress.completed) ||
          (budget < 10 && result == NBA97_TEXT_LIMIT &&
           f.progress.stopped_pc == stopped[budget] &&
           !f.progress.completed));
    check(f.progress.operations == budget);
  }
  const std::array<std::uint32_t, 9> nonzero_stopped{
      0x8002ddd0u, 0x8002dddcu, 0x8002dde4u, 0x8002ddecu,
      0x8002ddf4u, 0x8002ddf8u, 0x8002de04u, 0x8002de20u,
      0x8002de24u};
  for (unsigned budget = 0; budget <= 9; ++budget) {
    Fixture f(1);
    f.context.operation_budget = budget;
    const int result = f.run();
    check((budget == 9 && result == NBA97_TEXT_COMPLETE &&
           f.progress.completed) ||
          (budget < 9 && result == NBA97_TEXT_LIMIT &&
           f.progress.stopped_pc == nonzero_stopped[budget] &&
           !f.progress.completed));
    check(f.progress.operations == budget);
  }

  Fixture first;
  first.refuse = 1;
  check(first.run() == NBA97_TEXT_IO_REFUSED &&
        first.progress.stopped_pc == 0x8002ddf8u &&
        first.progress.stopped_entry == 0x80044550u &&
        first.progress.callbacks_completed == 0 &&
        first.get(Presentation, 1) == 0 && first.get(Active, 4) == 1 &&
        first.get(Published, 4) == 0x81234560u);
  Fixture second;
  second.refuse = 2;
  check(second.run() == NBA97_TEXT_IO_REFUSED &&
        second.progress.stopped_pc == 0x8002de14u &&
        second.progress.stopped_entry == 0x80046c2cu &&
        second.progress.callbacks_completed == 1 &&
        second.get(Active, 4) == 1);

  for (unsigned kind = 0; kind < 3; ++kind) {
    for (unsigned call = 1; call <= 2; ++call) {
      Fixture f;
      if (kind == 0)
        f.invalidate_gpr = call;
      else if (kind == 1)
        f.invalidate_hi = call;
      else
        f.invalidate_lo = call;
      check(f.run() == NBA97_TEXT_ARGUMENT &&
            f.progress.stopped_pc ==
                (call == 1 ? 0x8002ddf8u : 0x8002de14u) &&
            f.progress.callbacks_completed == call - 1);
    }
  }
}

void source_knownness_and_load_atomicity() {
  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture f(0, 0x89abcdefu, static_cast<std::uint8_t>(mask));
    check(f.run() == NBA97_TEXT_COMPLETE &&
          f.progress.source_word.word == 0x89abcdefu &&
          f.progress.source_word.known_mask == mask &&
          f.calls[0].machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                  .known_mask == mask &&
          f.get(Published, 4) == 0x89abcdefu &&
          f.getKnown(Published, 4) == mask);
  }

  Fixture malformed_source;
  malformed_source.known[Source - Ram + 2] = 2;
  check(malformed_source.run() == NBA97_TEXT_ARGUMENT &&
        malformed_source.progress.stopped_pc == 0x8002ddd0u &&
        malformed_source.progress.reads == 0 &&
        malformed_source.progress.machine
                .registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .word == 0x80020000u &&
        malformed_source.progress.machine
                .registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                .word == EntrySp);
  Fixture malformed_gate;
  malformed_gate.known[Gate - Ram] = 2;
  check(malformed_gate.run() == NBA97_TEXT_ARGUMENT &&
        malformed_gate.progress.stopped_pc == 0x8002de04u &&
        malformed_gate.progress.reads == 1 &&
        malformed_gate.progress.machine
                .registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .word == 0x80100000u &&
        malformed_gate.get(Active, 4) == 1);

  Fixture raw;
  raw.region.known = nullptr;
  check(raw.run() == NBA97_TEXT_COMPLETE && raw.progress.completed);
  Fixture raw_unknown_store;
  raw_unknown_store.region.known = nullptr;
  raw_unknown_store.context.machine
      .registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .known_mask = 7;
  check(raw_unknown_store.run() == NBA97_TEXT_ARGUMENT &&
        raw_unknown_store.progress.stopped_pc == 0x8002dddcu &&
        raw_unknown_store.progress.stores == 0);
}

void callback_machine_stack_aliases_and_wrap() {
  Fixture relocated;
  relocated.relocate_second = true;
  check(relocated.run() == NBA97_TEXT_COMPLETE && relocated.progress.completed &&
        relocated.progress.restored_return_address.word ==
            relocated.relocated_ra &&
        relocated.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                .word == relocated.relocated_sp + 0x18u &&
        relocated.progress.machine.hi.word == 0xaabbccddu &&
        relocated.progress.machine.hi.known_mask == 9 &&
        relocated.progress.machine.lo.word == 0x55667788u &&
        relocated.progress.machine.lo.known_mask == 6);

  Fixture source_alias(0, 0x81234560u);
  source_alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
      Source + 8u, 15};
  check(source_alias.run() == NBA97_TEXT_COMPLETE &&
        source_alias.progress.source_word.word == 0x81234560u &&
        source_alias.get(Source, 4) == Return &&
        source_alias.get(Published, 4) == 0x81234560u);
  Fixture presentation_alias;
  presentation_alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
      Presentation + 8u, 15};
  check(presentation_alias.run() == NBA97_TEXT_COMPLETE &&
        presentation_alias.progress.restored_return_address.word ==
            (Return & 0xffffff00u) &&
        presentation_alias.get(Presentation, 1) == 0);
  Fixture active_alias;
  active_alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
      Active + 8u, 15};
  check(active_alias.run() == NBA97_TEXT_COMPLETE &&
        active_alias.progress.restored_return_address.word == 0 &&
        active_alias.get(Active, 4) == 0);
  Fixture published_alias(0, 0x81234560u);
  published_alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
      Published + 8u, 15};
  check(published_alias.run() == NBA97_TEXT_COMPLETE &&
        published_alias.progress.restored_return_address.word ==
            0x81234560u);
  Fixture gate_alias;
  gate_alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
      Gate + 8u, 15};
  check(gate_alias.run() == NBA97_TEXT_COMPLETE &&
        gate_alias.calls.size() == 1 &&
        gate_alias.progress.gate_flag.word == (Return & 0xffu) &&
        gate_alias.progress.restored_return_address.word == Return);

  Fixture wrapping;
  std::array<std::uint8_t, 12> low{};
  std::array<std::uint8_t, 12> low_known{};
  low_known.fill(1);
  Nba97GameTextRegion regions[2]{
      wrapping.region, {0, low.data(), low_known.data(), low.size()}};
  wrapping.context.memory = {regions, 2};
  wrapping.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
      0x10u, 15};
  check(wrapping.run() == NBA97_TEXT_COMPLETE &&
        wrapping.progress.frame_stack_pointer == 0xfffffff8u &&
        wrapping.journal[1].address == 8 && wrapping.journal[7].address == 8 &&
        wrapping.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                .word == 0x10u);
}

void stack_and_epilogue_failures() {
  Fixture unknown_sp;
  unknown_sp.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
      .known_mask = 7;
  check(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stopped_pc == 0x8002dddcu &&
        unknown_sp.progress.operations == 1);
  Fixture unaligned_sp;
  unaligned_sp.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word =
      EntrySp + 1u;
  check(unaligned_sp.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_sp.progress.stopped_pc == 0x8002dddcu);
  Fixture unmapped_sp;
  unmapped_sp.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word =
      0x90000008u;
  check(unmapped_sp.run() == NBA97_TEXT_RESOURCE &&
        unmapped_sp.progress.stopped_pc == 0x8002dddcu);

  Fixture unknown_ra;
  unknown_ra.relocate_second = true;
  unknown_ra.relocated_ra_mask = 7;
  check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.stopped_pc == 0x8002de2cu &&
        unknown_ra.progress.stores == 5 &&
        unknown_ra.get(Active, 4) == 0 &&
        unknown_ra.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                .word == unknown_ra.relocated_sp + 0x18u);
  Fixture unaligned_ra;
  unaligned_ra.relocate_second = true;
  unaligned_ra.relocated_ra = 0x82468ac3u;
  check(unaligned_ra.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_ra.progress.stopped_pc == 0x8002de2cu &&
        unaligned_ra.get(Active, 4) == 0 &&
        unaligned_ra.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                .word == unaligned_ra.relocated_sp + 0x18u);
}

void metadata_and_determinism() {
  Nba97GamePeriodPresentationFinishProgress progress{};
  Fixture argument;
  check(nba97_game_period_presentation_finish(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
  check(nba97_game_period_presentation_finish(&argument.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
  argument.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T9]
      .known_mask = 16;
  check(argument.run() == NBA97_TEXT_ARGUMENT);
  Fixture null_regions;
  null_regions.context.memory.region = nullptr;
  check(null_regions.run() == NBA97_TEXT_ARGUMENT);
  Fixture null_data;
  null_data.region.data = nullptr;
  check(null_data.run() == NBA97_TEXT_ARGUMENT);
  Fixture zero_size;
  zero_size.region.size = 0;
  check(zero_size.run() == NBA97_TEXT_ARGUMENT);
  Fixture overflow;
  overflow.region.base = 0xfffffff0u;
  overflow.region.size = 32;
  check(overflow.run() == NBA97_TEXT_ARGUMENT);
  Fixture overlap;
  Nba97GameTextRegion regions[2]{overlap.region, overlap.region};
  overlap.context.memory = {regions, 2};
  check(overlap.run() == NBA97_TEXT_ARGUMENT);
  Fixture journal;
  journal.context.access_journal = nullptr;
  check(journal.run() == NBA97_TEXT_ARGUMENT);

  Fixture a;
  Fixture b;
  check(a.run() == NBA97_TEXT_COMPLETE && b.run() == NBA97_TEXT_COMPLETE);
  for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
    check(same(a.progress.machine.registers.gpr[i],
               b.progress.machine.registers.gpr[i]));
  check(same(a.progress.machine.hi, b.progress.machine.hi) &&
        same(a.progress.machine.lo, b.progress.machine.lo) &&
        a.bytes == b.bytes && a.known == b.known);
}
} // namespace

int main() {
  both_paths_and_all_flag_bytes();
  source_order_calls_and_untouched_machine();
  callback_mutable_gate();
  every_budget_and_callback_failure();
  source_knownness_and_load_atomicity();
  callback_machine_stack_aliases_and_wrap();
  stack_and_epilogue_failures();
  metadata_and_determinism();
  std::printf("game period-presentation finish: %u checks passed\n", checks);
}
