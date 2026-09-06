#include "recovered/game_text_submission.h"

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
    throw std::runtime_error("text-submission check failed at " +
                             std::to_string(line));
}
#define check(value) checkAt((value), __LINE__)

struct Call {
  Nba97GameTextSubmissionEvent event{};
  Nba97GameTextSubmissionMachine machine{};
};

struct Fixture {
  static constexpr U Base = 0x80000000u;
  static constexpr U Size = 0x200000u;
  static constexpr U Font = 0x80110000u;
  static constexpr U Records = 0x80120000u;
  static constexpr U Glyphs = 0x80130000u;
  static constexpr U Characters = 0x80140000u;
  static constexpr U Packets = 0x80150000u;
  static constexpr U Heads = 0x80160000u;
  static constexpr U String = 0x800249fcu;
  static constexpr U Sp = 0x801ff000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97GameTextSubmissionMachine entry{};
  Nba97GameTextSubmissionProgress progress{};
  std::array<Nba97GameTextSubmissionAccess, 1024> journal{};
  std::vector<Call> calls;
  std::size_t budget = 2048;
  U refusePc = 0;
  bool nullAllocation = false;
  std::uint16_t packetCount = 0;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      entry.registers.gpr[i] = {0x11000000u + i * 0x101u, 15};
    entry.registers.gpr[0] = {0, 15};
    entry.registers.gpr[4] = {201, 15};
    entry.registers.gpr[5] = {String, 15};
    entry.registers.gpr[6] = {0x1ec, 15};
    entry.registers.gpr[7] = {0x14, 15};
    entry.registers.gpr[29] = {Sp, 15};
    entry.registers.gpr[31] = {0x81234568u, 15};
    entry.hi = {0x12345678u, 5};
    entry.lo = {0x89abcdefu, 10};
    put(Sp + 0x10u, 0, 4);
    put(0x800b2048u, Font, 4);
    put(Font + 8u, Glyphs, 4);
    put(Font + 0x0cu, Characters, 4);
    put(Font + 0x10u, Records, 4);
    put(Font + 0x14u, Heads, 4);
    put(Font + 0x22u, 1, 2);
    put(Font + 0x26u, 0, 2);
    put(Font + 0x28u, 0x33, 2);
    put(Font + 0x2au, 0, 2);
    put(Font + 0x2cu, 0xffffu, 2);
    put(Font + 0x2eu, 0xffffu, 2);
    put(Font + 0x30u, 0xffffu, 2);
    put(Font + 0x32u, 0xffffu, 2);
    put(Font + 0x34u, 0xffffu, 2);
    put(Font + 0x36u, 0xffffu, 2);
    put(Font + 0x38u, 0xffffu, 2);
    put(Font + 0x3au, 0xffffu, 2);
    put(Font + 0x3cu, 0xffffu, 2);
    put(Font + 0x3eu, 0xffffu, 2);
    put(Font + 0x40u, 0, 2);
    put(Font + 0x42u, 4, 1);
    put(Font + 0x4au, 1, 1);
    put(Font + 0x52u, 10, 1);
    put(Records + 0x12u, 0xffffu, 2);
    put(Heads + 201u * 2u, 0xffffu, 2);
    put(String, 0, 1);
    put(Characters + 65u * 2u, 0, 2);
    put(Characters + 0x7eu, 0, 2);
    for (unsigned i = 0; i < 20; ++i)
      put(Glyphs + i, 0x20u + i, 1);
  }

  std::size_t at(U address, unsigned width = 1) const {
    if (address < Base || std::uint64_t(address) + width > Base + Size)
      throw std::out_of_range("unmapped");
    return address - Base;
  }
  void put(U address, U value, unsigned width, std::uint8_t mask = 15) {
    const auto offset = at(address, width);
    for (unsigned i = 0; i < width; ++i) {
      bytes[offset + i] = std::uint8_t(value >> (i * 8u));
      known[offset + i] = std::uint8_t((mask >> i) & 1u);
    }
  }
  U get(U address, unsigned width) const {
    U value = 0;
    const auto offset = at(address, width);
    for (unsigned i = 0; i < width; ++i)
      value |= U(bytes[offset + i]) << (i * 8u);
    return value;
  }

  static int io(void *opaque, const Nba97GameTextMemory *,
                const Nba97GameTextSubmissionEvent *event,
                Nba97GameTextSubmissionMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    f.calls.push_back({*event, *machine});
    if (event->pc == f.refusePc)
      return 0;
    if (event->entry == 0x8002eb50u) {
      const U sp = machine->registers.gpr[29].word;
      f.put(sp + 0x10u, 8, 2);
      f.put(sp + 0x12u, 4, 2);
      f.put(machine->registers.gpr[6].word, f.packetCount, 2);
    } else if (event->entry == 0x8002ef88u) {
      machine->registers.gpr[2] = {f.nullAllocation ? 0u : Packets, 15};
    } else if (event->entry == 0x8002ecd4u) {
      machine->registers.gpr[2] = {2, 15};
    }
    return 1;
  }

  int run() {
    Nba97GameTextSubmissionContext context{};
    context.memory = {&region, 1};
    context.operation_budget = budget;
    context.machine = entry;
    context.io = io;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    return nba97_game_text_submission(&context, &progress);
  }
};

void emptyAndNullAllocation() {
  Fixture f;
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.progress.return_v0.word == Fixture::Records && f.calls.size() == 4);
  check(f.calls[0].event.pc == 0x80030e14u &&
        f.calls[1].event.pc == 0x80030e20u &&
        f.calls[2].event.pc == 0x800310a8u &&
        f.calls[3].event.pc == 0x800310b4u);
  check(f.get(Fixture::Records + 8u, 4) == Fixture::Packets);
  check(f.get(Fixture::Records + 0x14u, 2) == 201);
  check(f.get(Fixture::Heads + 201u * 2u, 2) == 0);
  check(f.get(Fixture::Font + 0x2eu, 2) == 0xffffu);
  check(f.progress.machine.registers.gpr[29].word == Fixture::Sp &&
        f.progress.machine.registers.gpr[31].word == 0x81234568u &&
        f.progress.machine.hi.word == f.entry.hi.word &&
        f.progress.machine.lo.word == f.entry.lo.word);

  Fixture null;
  null.nullAllocation = true;
  check(null.run() == NBA97_TEXT_COMPLETE && null.progress.completed &&
        null.progress.return_v0.word == 0 && null.calls.size() == 2 &&
        null.get(Fixture::Records + 8u, 4) == 0);
}

void allocationSearchPaths() {
  Fixture later;
  later.put(Fixture::Font + 0x22u, 3, 2);
  later.put(Fixture::Records + 0x12u, 0, 2);
  later.put(Fixture::Records + 64u + 0x12u, 0xffffu, 2);
  check(later.run() == NBA97_TEXT_COMPLETE &&
        later.progress.return_v0.word == Fixture::Records + 64u &&
        later.progress.allocation_iterations == 2);

  Fixture wrapped;
  wrapped.put(Fixture::Font + 0x22u, 3, 2);
  wrapped.put(Fixture::Font + 0x40u, 2, 2);
  wrapped.put(Fixture::Records + 2u * 64u + 0x12u, 0, 2);
  wrapped.put(Fixture::Records + 0x12u, 0xffffu, 2);
  check(wrapped.run() == NBA97_TEXT_COMPLETE &&
        wrapped.progress.return_v0.word == Fixture::Records &&
        wrapped.progress.allocation_iterations == 2);

  Fixture exhausted;
  exhausted.put(Fixture::Font + 0x22u, 3, 2);
  for (unsigned index = 0; index < 3; ++index)
    exhausted.put(Fixture::Records + index * 64u + 0x12u, 0, 2);
  check(exhausted.run() == NBA97_TEXT_COMPLETE &&
        exhausted.progress.return_v0.word == Fixture::Records &&
        exhausted.progress.allocation_iterations == 3);
}

void glyphCallsBudgetsAndReturns() {
  Fixture glyph;
  glyph.put(Fixture::String, 'A', 1);
  glyph.put(Fixture::String + 1u, 0, 1);
  glyph.packetCount = 1;
  check(glyph.run() == NBA97_TEXT_COMPLETE &&
        glyph.progress.glyph_iterations == 1 && glyph.calls.size() == 7 &&
        glyph.calls[4].event.pc == 0x80031470u &&
        glyph.calls[5].event.pc == 0x800314b8u &&
        glyph.calls[6].event.pc == 0x800314c4u);
  check(glyph.calls[4].machine.registers.gpr[4].word == Fixture::Packets &&
        glyph.calls[4].machine.registers.gpr[5].word ==
            Fixture::Packets + 0x28u &&
        glyph.calls[4].machine.registers.gpr[6].word == 0x28u);

  Fixture baseline;
  check(baseline.run() == NBA97_TEXT_COMPLETE);
  for (std::size_t budget = 0; budget < baseline.progress.operations;
       ++budget) {
    Fixture limited;
    limited.budget = budget;
    check(limited.run() == NBA97_TEXT_LIMIT &&
          limited.progress.operations == budget && !limited.progress.completed);
  }
  Fixture glyphBaseline;
  glyphBaseline.put(Fixture::String, 'A', 1);
  glyphBaseline.put(Fixture::String + 1u, 0, 1);
  glyphBaseline.packetCount = 1;
  check(glyphBaseline.run() == NBA97_TEXT_COMPLETE);
  for (std::size_t budget = 0; budget < glyphBaseline.progress.operations;
       ++budget) {
    Fixture limited;
    limited.put(Fixture::String, 'A', 1);
    limited.put(Fixture::String + 1u, 0, 1);
    limited.packetCount = 1;
    limited.budget = budget;
    check(limited.run() == NBA97_TEXT_LIMIT &&
          limited.progress.operations == budget && !limited.progress.completed);
  }
  for (U pc : {0x80030e14u, 0x80030e20u, 0x800310a8u, 0x800310b4u}) {
    Fixture refused;
    refused.refusePc = pc;
    check(refused.run() == NBA97_TEXT_IO_REFUSED &&
          refused.progress.stopped_pc == pc &&
          refused.progress.stopped_entry == refused.calls.back().event.entry);
  }
  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture ra;
    ra.entry.registers.gpr[31].known_mask = std::uint8_t(mask);
    const int result = ra.run();
    check(result == (mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN));
    if (mask != 15)
      check(ra.progress.stopped_pc == 0x8003151cu);
  }
}

void listModesSlotsAndControls() {
  const std::array<U, 4> headOffset{0x30u, 0x34u, 0x38u, 0x3cu};
  for (unsigned mode = 0; mode < 4; ++mode) {
    Fixture empty;
    empty.put(Fixture::Font + 0x2au, mode, 2);
    check(empty.run() == NBA97_TEXT_COMPLETE &&
          empty.get(Fixture::Font + headOffset[mode], 2) == 0 &&
          empty.get(Fixture::Font + headOffset[mode] + 2u, 2) == 0);

    Fixture append;
    append.put(Fixture::Font + 0x2au, mode, 2);
    append.put(Fixture::Font + headOffset[mode], 1, 2);
    append.put(Fixture::Font + headOffset[mode] + 2u, 1, 2);
    append.put(Fixture::Records + 64u + 0x12u, 0, 2);
    append.put(Fixture::Records + 64u + 0x1cu, 0xffffu, 2);
    check(append.run() == NBA97_TEXT_COMPLETE &&
          append.get(Fixture::Records + 0x1au, 2) == 1 &&
          append.get(Fixture::Records + 0x1cu, 2) == 0xffffu &&
          append.get(Fixture::Records + 64u + 0x1cu, 2) == 0);
  }
  Fixture defaultMode;
  defaultMode.put(Fixture::Font + 0x2au, 7, 2);
  check(defaultMode.run() == NBA97_TEXT_COMPLETE &&
        defaultMode.get(Fixture::Font + 0x30u, 2) == 0);

  for (U slot : {0xffffffffu, 0u, 99u, 100u, 199u, 200u}) {
    Fixture f;
    f.entry.registers.gpr[4].word = slot;
    if ((slot & 0xffffu) < 0x8000u)
      f.put(Fixture::Heads + (slot & 0xffffu) * 2u, 0xffffu, 2);
    check(f.run() == NBA97_TEXT_COMPLETE);
    if (slot == 99)
      check(f.get(Fixture::Font + 0x2cu, 2) == 99);
    if (slot == 100 || slot == 199)
      check(f.get(Fixture::Font + 0x2eu, 2) == slot);
  }

  for (unsigned mode = 0; mode < 5; ++mode) {
    Fixture f;
    f.put(Fixture::Sp + 0x10u, mode, 4);
    if (mode == 3 || mode == 4)
      check(f.run() == NBA97_TEXT_COMPLETE && f.calls.size() == 5 &&
            f.calls[4].event.pc == 0x80031138u);
    else
      check(f.run() == NBA97_TEXT_COMPLETE && f.calls.size() == 4);
  }

  const std::array<std::vector<std::uint8_t>, 5> controls{
      {{0x20, 0}, {0x1f, 5, 0}, {0x1e, 2, 0}, {0x1d, 1, 0}, {0x1c, 0xfe, 0}}};
  for (const auto &text : controls) {
    Fixture f;
    for (std::size_t i = 0; i < text.size(); ++i)
      f.put(Fixture::String + U(i), text[i], 1);
    f.put(Fixture::Characters + U(text[0]) * 2u, 0xffffu, 2);
    f.put(0x800b2054u, 0x11223344u, 4);
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.glyph_iterations == 1);
  }

  Fixture newline;
  newline.put(Fixture::String, 0x0a, 1);
  newline.put(Fixture::String + 1u, 0, 1);
  newline.put(Fixture::Characters + 0x14u, 0xffffu, 2);
  newline.put(0x80024940u, 0x800311ecu, 4);
  check(newline.run() == NBA97_TEXT_COMPLETE && newline.calls.size() == 5 &&
        newline.calls[4].event.pc == 0x800311b4u);

  Fixture fallback;
  fallback.put(Fixture::String, '#', 1);
  fallback.put(Fixture::String + 1u, 0, 1);
  fallback.put(Fixture::Characters + U('#') * 2u, 0xffffu, 2);
  fallback.packetCount = 1;
  check(fallback.run() == NBA97_TEXT_COMPLETE &&
        fallback.calls[4].event.pc == 0x80031470u);

  const std::array<U, 5> newlineTargets{0x800311ecu, 0x800311f4u, 0x80031208u,
                                        0x80031214u, 0x80031220u};
  for (U target : newlineTargets) {
    Fixture dispatched;
    dispatched.put(Fixture::Sp + 0x10u, 2, 4);
    dispatched.put(Fixture::String, 0x0a, 1);
    dispatched.put(Fixture::String + 1u, 0, 1);
    dispatched.put(Fixture::Characters + 0x14u, 0xffffu, 2);
    dispatched.put(0x80024948u, target, 4);
    check(dispatched.run() == NBA97_TEXT_COMPLETE &&
          dispatched.calls.size() ==
              ((target == 0x80031214u || target == 0x80031220u) ? 6u : 5u));
  }

  Fixture repeated;
  repeated.put(Fixture::String, 'A', 1);
  repeated.put(Fixture::String + 1u, 'A', 1);
  repeated.put(Fixture::String + 2u, 0, 1);
  repeated.packetCount = 2;
  check(repeated.run() == NBA97_TEXT_COMPLETE &&
        repeated.progress.glyph_iterations == 2 &&
        repeated.progress.packet_link_iterations == 2 &&
        repeated.calls.size() == 10 &&
        repeated.calls[4].event.pc == 0x80031470u &&
        repeated.calls[5].event.pc == 0x80031470u &&
        repeated.calls[6].event.pc == 0x800314b8u &&
        repeated.calls[7].event.pc == 0x800314c4u &&
        repeated.calls[8].event.pc == 0x800314b8u &&
        repeated.calls[9].event.pc == 0x800314c4u);
}

void validationAndRepeatability() {
  Nba97GameTextSubmissionProgress progress{};
  check(nba97_game_text_submission(nullptr, &progress) == NBA97_TEXT_ARGUMENT);
  Fixture invalid;
  invalid.entry.registers.gpr[0].word = 1;
  check(invalid.run() == NBA97_TEXT_ARGUMENT);
  Fixture unaligned;
  unaligned.entry.registers.gpr[29].word |= 1;
  check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x80030d1cu);
  Fixture malformed;
  const U originalA2 = malformed.entry.registers.gpr[6].word;
  malformed.known[malformed.at(Fixture::Font + 0x23u)] = 2;
  check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.stopped_pc == 0x80030d64u &&
        malformed.progress.machine.registers.gpr[6].word == originalA2 &&
        malformed.get(Fixture::Sp - 0x80u + 0x7cu, 4) == 0x81234568u);
  Fixture unknownAddress;
  unknownAddress.entry.registers.gpr[29].known_mask = 14;
  check(unknownAddress.run() == NBA97_TEXT_UNKNOWN &&
        unknownAddress.progress.stopped_pc == 0x80030d1cu);
  Fixture noKnownStore;
  noKnownStore.region.known = nullptr;
  noKnownStore.entry.registers.gpr[7].known_mask = 14;
  check(noKnownStore.run() == NBA97_TEXT_ARGUMENT &&
        noKnownStore.progress.stopped_pc == 0x80030d5cu);
  Fixture huge;
  huge.region.size = SIZE_MAX;
  check(huge.run() == NBA97_TEXT_ARGUMENT);
  Fixture wrapPrefix;
  wrapPrefix.put(Fixture::Font + 0x40u, 2, 2);
  wrapPrefix.put(Fixture::Font + 0x22u, 1, 2);
  wrapPrefix.budget = 20;
  check(wrapPrefix.run() == NBA97_TEXT_LIMIT &&
        wrapPrefix.progress.stopped_pc == 0x80030ddcu &&
        wrapPrefix.progress.machine.registers.gpr[2].word == 2 &&
        wrapPrefix.progress.machine.registers.gpr[17].word == 0);
  struct DispatchFailure {
    U target;
    std::uint8_t mask;
    int result;
  };
  const std::array<DispatchFailure, 3> failures{{
      {0x800311ecu, 14, NBA97_TEXT_UNKNOWN},
      {0x800311efu, 15, NBA97_TEXT_ALIGNMENT_TRAP},
      {0x80032000u, 15, NBA97_TEXT_IO_REFUSED},
  }};
  for (const auto &failure : failures) {
    Fixture dispatch;
    dispatch.put(Fixture::Sp + 0x10u, 2, 4);
    dispatch.put(Fixture::String, 0x0a, 1);
    dispatch.put(Fixture::String + 1u, 0, 1);
    dispatch.put(Fixture::Characters + 0x14u, 0xffffu, 2);
    dispatch.put(0x80024948u, failure.target, 4, failure.mask);
    check(dispatch.run() == failure.result &&
          dispatch.progress.stopped_pc == 0x800311e4u);
  }
  Fixture badReturn;
  badReturn.entry.registers.gpr[31] = {0x81234569u, 15};
  check(badReturn.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        badReturn.progress.stopped_pc == 0x8003151cu &&
        badReturn.progress.machine.registers.gpr[29].word == Fixture::Sp);
  Fixture a;
  Fixture b;
  check(a.run() == NBA97_TEXT_COMPLETE && b.run() == NBA97_TEXT_COMPLETE &&
        a.bytes == b.bytes && a.known == b.known &&
        a.progress.return_v0.word == b.progress.return_v0.word);
  for (unsigned i = 0; i < 32; ++i)
    check(a.progress.machine.registers.gpr[i].word ==
              b.progress.machine.registers.gpr[i].word &&
          a.progress.machine.registers.gpr[i].known_mask ==
              b.progress.machine.registers.gpr[i].known_mask);
  check(a.progress.machine.hi.word == b.progress.machine.hi.word &&
        a.progress.machine.hi.known_mask == b.progress.machine.hi.known_mask &&
        a.progress.machine.lo.word == b.progress.machine.lo.word &&
        a.progress.machine.lo.known_mask == b.progress.machine.lo.known_mask &&
        a.progress.instruction_count == b.progress.instruction_count &&
        a.progress.operations == b.progress.operations &&
        a.progress.access_events == b.progress.access_events);
  for (std::size_t i = 0; i < a.progress.access_events; ++i)
    check(a.journal[i].kind == b.journal[i].kind &&
          a.journal[i].width == b.journal[i].width &&
          a.journal[i].pc == b.journal[i].pc &&
          a.journal[i].address == b.journal[i].address &&
          a.journal[i].value == b.journal[i].value &&
          a.journal[i].known_mask == b.journal[i].known_mask);
}
} // namespace

int main() {
  try {
    emptyAndNullAllocation();
    allocationSearchPaths();
    glyphCallsBudgetsAndReturns();
    listModesSlotsAndControls();
    validationAndRepeatability();
    std::printf("game_text_submission_tests: PASS (%u checks)\n", checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
