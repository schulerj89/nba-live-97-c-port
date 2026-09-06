#include "recovered/game_display_environment.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
using U32 = std::uint32_t;
unsigned checks;
void checkAt(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "display-environment check %u failed at line %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) checkAt((value), __LINE__)

struct Seen {
  Nba97GameDisplayEnvironmentEvent event{};
  Nba97GameDisplayEnvironmentMachine machine{};
};

struct Fixture {
  static constexpr U32 Base = 0x80000000u;
  static constexpr std::size_t Size = 0x120000u;
  static constexpr U32 Env = 0x80022000u;
  static constexpr U32 Stack = 0x8010ff00u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameDisplayEnvironmentAccess, 96> journal{};
  std::vector<Seen> seen;
  Nba97GameDisplayEnvironmentContext context{};
  Nba97GameDisplayEnvironmentProgress progress{};
  U32 video = 0;
  U32 origin = 0x456;
  bool reject = false;
  bool malformed = false;
  bool relocateOnCopy = false;
  bool unknownSavedRa = false;
  bool partialVideo = false;
  U32 videoS0 = 0;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {0x11000000u + i * 0x101u, 15};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {Env, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Stack, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x81234568u,
                                                                15};
    context.machine.hi = {0x89abcdefu, 15};
    context.machine.lo = {0x76543210u, 15};
    context.memory = {&region, 1};
    context.operation_budget = 200;
    context.io = io;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    seed();
  }
  std::size_t at(U32 address) const { return address - Base; }
  void put(U32 address, U32 value, unsigned width = 4) {
    for (unsigned i = 0; i < width; ++i) {
      bytes[at(address) + i] = static_cast<std::uint8_t>(value >> (8u * i));
      known[at(address) + i] = 1;
    }
  }
  U32 get(U32 address, unsigned width = 4) const {
    U32 value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= U32(bytes[at(address) + i]) << (8u * i);
    return value;
  }
  void seed() {
    put(0x800c55c2u, 0, 1);
    put(0x800c55c0u, 0, 1);
    put(0x800c55c3u, 0, 1);
    put(0x800c55bcu, 0x8009cb2cu);
    put(0x800c55b8u, 0x800c5578u);
    put(0x800c5588u, 0x8009a97cu);
    put(Env + 0, 10, 2);
    put(Env + 2, 20, 2);
    put(Env + 4, 320, 2);
    put(Env + 6, 240, 2);
    put(Env + 8, 0, 2);
    put(Env + 10, 0, 2);
    put(Env + 12, 256, 2);
    put(Env + 14, 240, 2);
    put(Env + 16, 0);
    copy(Env, 0x800c562cu, 20);
  }
  void copy(U32 source, U32 destination, unsigned size) {
    std::memmove(bytes.data() + at(destination), bytes.data() + at(source),
                 size);
    std::memmove(known.data() + at(destination), known.data() + at(source),
                 size);
  }
  static int io(void *opaque, const Nba97GameTextMemory *,
                const Nba97GameDisplayEnvironmentEvent *event,
                Nba97GameDisplayEnvironmentMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    f.seen.push_back({*event, *machine});
    if (f.reject)
      return 0;
    if (event->kind == NBA97_GAME_DISPLAY_ENVIRONMENT_ORIGIN_HELPER)
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {f.origin, 15};
    if (event->kind == NBA97_GAME_DISPLAY_ENVIRONMENT_VIDEO_MODE)
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {f.video, 15};
    if (event->kind == NBA97_GAME_DISPLAY_ENVIRONMENT_VIDEO_MODE &&
        f.partialVideo)
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask = 14;
    if (event->kind == NBA97_GAME_DISPLAY_ENVIRONMENT_VIDEO_MODE && f.videoS0)
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {f.videoS0, 15};
    if (event->kind == NBA97_GAME_DISPLAY_ENVIRONMENT_COPY) {
      f.copy(machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].word,
             machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word,
             machine->registers.gpr[NBA97_MATCH_INITIALIZE_A2].word);
      if (f.relocateOnCopy) {
        const U32 alternate = 0x8010fe00u;
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {alternate, 15};
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {0x80023000u, 15};
        machine->registers.gpr[12] = {0xdecafbadu, 15};
        machine->hi = {0x10203040u, 15};
        machine->lo = {0x50607080u, 15};
        f.put(alternate + 32, 0x90000004u);
        f.put(alternate + 28, 0x33333333u);
        f.put(alternate + 24, 0x22222222u);
        f.put(alternate + 20, 0x11111111u);
        f.put(alternate + 16, 0x00000000u);
      }
      if (f.unknownSavedRa)
        f.known[f.at(machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word +
                     32u)] = 0;
    }
    if (f.malformed)
      machine->registers.gpr[0].known_mask = 0;
    return 1;
  }
  int run() { return nba97_game_display_environment(&context, &progress); }
};

void unchangedAndOriginPacking() {
  Fixture f;
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
  check(f.seen.size() == 2 && f.seen[0].event.pc == 0x80099d6cu &&
        f.seen[1].event.pc == 0x8009a128u);
  check(f.progress.origin_command.word == 0x0500500au);
  check(!f.progress.screen_rectangle_changed && !f.progress.mode_changed);
  check(f.progress.return_v0.word == Fixture::Env &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            Fixture::Env);
  check(f.progress.machine.hi.word == 0x89abcdefu &&
        f.progress.machine.lo.word == 0x76543210u);
  check(f.seen[0].event.entry == 0x8009a97cu &&
        f.seen[0].event.delay_slot_pc == 0x80099d70u &&
        f.seen[0].machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x80099d74u);

  for (U32 type : {1u, 2u}) {
    Fixture helper;
    helper.put(0x800c55c0u, type, 1);
    check(helper.run() == NBA97_TEXT_COMPLETE && helper.seen.size() == 3);
    check(
        helper.seen[0].event.pc == 0x80099d14u &&
        helper.seen[0].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            Fixture::Env);
    check(helper.progress.origin_command.word == 0x05014456u);
  }
  for (U32 type : {3u, 255u}) {
    Fixture packed;
    packed.put(0x800c55c0u, type, 1);
    check(packed.run() == NBA97_TEXT_COMPLETE && packed.seen.size() == 2 &&
          packed.progress.origin_command.word == 0x0500500au);
  }
}

void debugGateAndAllChanged() {
  for (U32 debug : {0u, 1u, 2u, 255u}) {
    Fixture f;
    f.put(0x800c55c2u, debug, 1);
    check(f.run() == NBA97_TEXT_COMPLETE);
    check((!f.seen.empty() &&
           f.seen[0].event.kind == NBA97_GAME_DISPLAY_ENVIRONMENT_DEBUG) ==
          (debug >= 2));
    if (debug >= 2)
      check(f.seen[0].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
                0x800283a0u &&
            f.seen[0].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
                Fixture::Env);
  }

  Fixture changed;
  changed.video = 1;
  changed.put(0x800c5634u, 0xffffu, 2);
  changed.put(0x800c563cu, 0xffffffffu);
  changed.put(Fixture::Env + 8, 50, 2);
  changed.put(Fixture::Env + 10, 300, 2);
  changed.put(Fixture::Env + 12, 0, 2);
  changed.put(Fixture::Env + 14, 0, 2);
  check(changed.run() == NBA97_TEXT_COMPLETE && changed.seen.size() == 7);
  const U32 pcs[] = {0x80099d6cu, 0x80099de8u, 0x80099f78u, 0x80099fa4u,
                     0x8009a034u, 0x8009a114u, 0x8009a128u};
  for (unsigned i = 0; i < 7; ++i)
    check(changed.seen[i].event.pc == pcs[i]);
  check(changed.progress.screen_rectangle_changed &&
        changed.progress.mode_changed);
  check(changed.progress.horizontal_command.word ==
        ((1108u & 0xfffu) | ((3290u & 0xfffu) << 12u) | 0x06000000u));
  check(changed.progress.vertical_command.word ==
        ((310u & 0x3ffu) | ((312u & 0x3ffu) << 10u) | 0x07000000u));
}

void modeThresholdsAndBits() {
  struct Case {
    std::int16_t width;
    U32 bits;
  };
  const Case cases[] = {{280, 0},  {281, 1}, {352, 1}, {353, 64},
                        {400, 64}, {401, 2}, {560, 2}, {561, 3}};
  for (const auto &item : cases) {
    Fixture f;
    f.video = 1;
    f.put(0x800c563cu, 0xffffffffu);
    f.put(Fixture::Env + 4, static_cast<std::uint16_t>(item.width), 2);
    f.put(Fixture::Env + 6, 289, 2);
    f.put(Fixture::Env + 16, 0x00000101u);
    f.put(0x800c55c3u, 1, 1);
    check(f.run() == NBA97_TEXT_COMPLETE);
    check(f.progress.mode_command.word ==
          (0x08000000u | 8u | 16u | 32u | 128u | item.bits | 36u));
  }
  for (U32 video : {0u, 1u, 2u}) {
    for (U32 height : {256u, 257u, 288u, 289u}) {
      Fixture f;
      f.video = video;
      f.put(0x800c563cu, 0xffffffffu);
      f.put(Fixture::Env + 6, height, 2);
      check(f.run() == NBA97_TEXT_COMPLETE);
      const bool heightBit = height >= (video ? 289u : 257u);
      check((f.progress.mode_command.word & 36u) == (heightBit ? 36u : 0u));
    }
  }
}

void liveMutationFailuresAndBudgets() {
  Fixture moved;
  moved.relocateOnCopy = true;
  check(moved.run() == NBA97_TEXT_COMPLETE && moved.progress.completed);
  check(moved.progress.return_v0.word == 0x80023000u &&
        moved.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            0x80023000u &&
        moved.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            0x8010fe28u &&
        moved.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x90000004u &&
        moved.progress.machine.registers.gpr[12].word == 0xdecafbadu &&
        moved.progress.machine.hi.word == 0x10203040u);

  Fixture complete;
  check(complete.run() == NBA97_TEXT_COMPLETE);
  const std::size_t operations = complete.progress.operations;
  for (std::size_t budget = 0; budget < operations; ++budget) {
    Fixture f;
    f.context.operation_budget = budget;
    check(f.run() == NBA97_TEXT_LIMIT && f.progress.operations == budget &&
          !f.progress.completed);
  }
  Fixture refused;
  refused.reject = true;
  check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.progress.stopped_pc == 0x80099d6cu);
  Fixture malformed;
  malformed.malformed = true;
  check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.stopped_pc == 0x80099d6cu &&
        malformed.progress.machine.registers.gpr[0].known_mask == 0);
}

void unknownAndInvalidMemory() {
  Fixture branch;
  branch.known[branch.at(0x800c55c2u)] = 0;
  check(branch.run() == NBA97_TEXT_UNKNOWN &&
        branch.progress.stopped_pc == 0x80099cd4u &&
        branch.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 3]
                .word == 0x08000000u);

  Fixture target;
  target.known[target.at(0x800c5588u)] = 0;
  check(target.run() == NBA97_TEXT_UNKNOWN &&
        target.progress.stopped_pc == 0x80099d6cu &&
        target.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x80099d74u);

  Fixture clamp;
  clamp.put(0x800c5634u, 0x7fffu, 2);
  clamp.known[clamp.at(Fixture::Env + 8)] = 0;
  check(clamp.run() == NBA97_TEXT_UNKNOWN &&
        clamp.progress.stopped_pc == 0x80099e58u &&
        clamp.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .known_mask == 14 &&
        clamp.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            500u &&
        clamp.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
                .known_mask == 15);

  Fixture mode;
  mode.put(0x800c563cu, 0xffffffffu);
  mode.put(0x800c5630u, 0x7fffu, 2);
  mode.known[mode.at(Fixture::Env + 4)] = 0;
  check(mode.run() == NBA97_TEXT_UNKNOWN &&
        mode.progress.stopped_pc == 0x8009a0a4u &&
        mode.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .known_mask == 14);

  Fixture ordered;
  ordered.put(0x800c5634u, 0x7fffu, 2);
  ordered.videoS0 = 0x800c5622u;
  ordered.partialVideo = true;
  check(ordered.run() == NBA97_TEXT_UNKNOWN &&
        ordered.progress.stopped_pc == 0x80099e10u);
  std::size_t readIndex = ordered.progress.access_events;
  std::size_t storeIndex = ordered.progress.access_events;
  for (std::size_t i = 0; i < ordered.progress.access_events; ++i) {
    if (ordered.journal[i].pc == 0x80099df0u)
      readIndex = i;
    if (ordered.journal[i].pc == 0x80099df4u)
      storeIndex = i;
  }
  check(readIndex < storeIndex &&
        ordered.journal[readIndex].kind ==
            NBA97_GAME_DISPLAY_ENVIRONMENT_READ &&
        ordered.journal[storeIndex].kind ==
            NBA97_GAME_DISPLAY_ENVIRONMENT_STORE &&
        ordered.journal[storeIndex].address == 0x800c5634u &&
        ordered.known[ordered.at(0x800c5634u)] == 0);

  Fixture invalid;
  invalid.region.size = 0;
  check(invalid.run() == NBA97_TEXT_ARGUMENT);
  Fixture overlap;
  std::array<Nba97GameTextRegion, 2> regions{{overlap.region, overlap.region}};
  overlap.context.memory = {regions.data(), regions.size()};
  check(overlap.run() == NBA97_TEXT_ARGUMENT);
  Fixture misaligned;
  misaligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word++;
  check(misaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misaligned.progress.stopped_pc == 0x80099ca8u);

  Fixture lateByte;
  lateByte.put(0x800c55c2u, 2, 1);
  lateByte.known[lateByte.at(0x800c55bcu) + 3] = 2;
  check(
      lateByte.run() == NBA97_TEXT_ARGUMENT &&
      lateByte.progress.stopped_pc == 0x80099ce8u &&
      lateByte.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
          0x800c0000u &&
      lateByte.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
              .known_mask == 15);

  Fixture unknownStore;
  unknownStore.put(0x800c5634u, 0x7fffu, 2);
  unknownStore.partialVideo = true;
  const U32 preservedByte = unknownStore.get(Fixture::Env + 18, 1);
  unknownStore.region.known = nullptr;
  check(unknownStore.run() == NBA97_TEXT_ARGUMENT &&
        unknownStore.progress.stopped_pc == 0x80099df4u &&
        unknownStore.get(Fixture::Env + 18, 1) == preservedByte);

  Fixture unknownRa;
  unknownRa.unknownSavedRa = true;
  check(unknownRa.run() == NBA97_TEXT_UNKNOWN &&
        unknownRa.progress.stopped_pc == 0x8009a14cu &&
        unknownRa.progress.restored_return_address.known_mask == 14 &&
        unknownRa.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
                .known_mask == 14);

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture partial;
    partial.context.machine.registers.gpr[13].known_mask =
        static_cast<std::uint8_t>(mask);
    partial.context.machine.hi.known_mask = static_cast<std::uint8_t>(mask);
    partial.context.machine.lo.known_mask =
        static_cast<std::uint8_t>(15 - mask);
    check(partial.run() == NBA97_TEXT_COMPLETE && partial.progress.completed &&
          partial.progress.machine.registers.gpr[13].known_mask == mask &&
          partial.progress.machine.hi.known_mask == mask &&
          partial.progress.machine.lo.known_mask == 15 - mask);
  }

  Fixture deterministicA;
  Fixture deterministicB;
  check(deterministicA.run() == deterministicB.run() &&
        deterministicA.progress.origin_command.word ==
            deterministicB.progress.origin_command.word &&
        deterministicA.bytes == deterministicB.bytes);
}
} // namespace

int main() {
  unchangedAndOriginPacking();
  debugGateAndAllChanged();
  modeThresholdsAndBits();
  liveMutationFailuresAndBudgets();
  unknownAndInvalidMemory();
  std::printf("game display environment tests passed (%u checks)\n", checks);
  return 0;
}
