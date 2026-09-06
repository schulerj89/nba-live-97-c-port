#include "recovered/game_image_record_upload.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
#define check(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      std::cerr << "check failed: " #condition " at line " << __LINE__ << '\n'; \
      std::abort();                                                             \
    }                                                                           \
  } while (0)

struct Call {
  Nba97GameImageRecordUploadEvent event{};
  Nba97GameImageRecordUploadMachine machine{};
};

struct Fixture {
  static constexpr std::uint32_t Base = 0x80000000u;
  static constexpr std::uint32_t Sp = 0x801ff000u;
  static constexpr std::uint32_t Record = 0x80010000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97GameImageRecordUploadMachine machine{};
  Nba97GameImageRecordUploadProgress progress{};
  std::array<Nba97GameImageRecordUploadAccess, 128> journal{};
  std::vector<Call> calls;
  std::size_t budget = 128;
  int factor = 3;
  int refuseKind = 0;
  bool invalidMachine = false;
  bool relocate = false;
  std::uint32_t poisonAddress = 0;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i) {
      machine.registers.gpr[i].word = 0x11000000u + i * 0x01010101u;
      machine.registers.gpr[i].known_mask = 15;
    }
    machine.registers.gpr[0] = {0, 15};
    machine.registers.gpr[4] = {Record, 15};
    machine.registers.gpr[5] = {0x340, 15};
    machine.registers.gpr[6] = {0xf0, 15};
    machine.registers.gpr[7] = {0x10, 15};
    machine.registers.gpr[29] = {Sp, 15};
    machine.registers.gpr[31] = {0x80012340u, 15};
    machine.hi = {0xabcdef01u, 15};
    machine.lo = {0x12345678u, 15};
    put(Sp + 0x10u, 1, 4);
    put(Record, 0x23, 1);
    put(Record + 4, 0x20, 2);
  }

  void put(std::uint32_t address, std::uint32_t value, unsigned width) {
    for (unsigned i = 0; i < width; ++i)
      bytes[address - Base + i] = std::uint8_t(value >> (8u * i));
  }
  std::uint32_t get(std::uint32_t address, unsigned width) const {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= std::uint32_t(bytes[address - Base + i]) << (8u * i);
    return value;
  }
  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameImageRecordUploadEvent *event,
                      Nba97GameImageRecordUploadMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    f.calls.push_back({*event, *machine});
    if (f.refuseKind == event->kind)
      return 0;
    if (event->kind == NBA97_GAME_IMAGE_RECORD_UPLOAD_CHILD_800A3BF8)
      machine->registers.gpr[2] = {std::uint32_t(f.factor), 15};
    if (f.relocate) {
      machine->registers.gpr[16] = {Record + 0x100u, 15};
      machine->registers.gpr[29] = {Sp - 0x100u, 15};
    }
    if (f.invalidMachine)
      machine->hi.known_mask = 16;
    if (f.poisonAddress &&
        event->kind == NBA97_GAME_IMAGE_RECORD_UPLOAD_CHILD_800944F4)
      f.known[f.poisonAddress - Base] = 2;
    return 1;
  }
  int run() {
    Nba97GameImageRecordUploadContext context{};
    context.memory = {&region, 1};
    context.operation_budget = budget;
    context.machine = machine;
    context.io = callback;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    return nba97_game_image_record_upload(&context, &progress);
  }
};

bool sameWord(const Nba97GameImageRecordUploadWord &left,
              const Nba97GameImageRecordUploadWord &right) {
  return left.word == right.word && left.known_mask == right.known_mask;
}

bool sameMachine(const Nba97GameImageRecordUploadMachine &left,
                 const Nba97GameImageRecordUploadMachine &right) {
  for (unsigned i = 0; i < 32; ++i)
    if (!sameWord(left.registers.gpr[i], right.registers.gpr[i]))
      return false;
  return sameWord(left.hi, right.hi) && sameWord(left.lo, right.lo);
}

void type23AndFullWordGates() {
  Fixture f;
  f.machine.registers.gpr[7] = {0x10000u, 15};
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.progress.stopped_pc == 0 && f.progress.stopped_address == 0 &&
        f.progress.stopped_entry == 0);
  check(f.calls.size() == 1 &&
        f.calls[0].event.kind == NBA97_GAME_IMAGE_RECORD_UPLOAD_CHILD_800944F4 &&
        f.calls[0].event.pc == 0x8009464cu &&
        f.calls[0].event.delay_slot_pc == 0x80094650u &&
        f.calls[0].event.entry == 0x800944f4u &&
        f.calls[0].event.invocation == 1 &&
        f.calls[0].event.argument_count == 2);
  check(f.get(Fixture::Record, 1) == 0x2bu);
  check(f.get(Fixture::Record + 0x0c, 2) == 0 &&
        f.get(Fixture::Record + 0x0e, 2) == 1);
  const auto frame = Fixture::Sp - 0x30u;
  check(f.get(frame + 0x10, 2) == 0 && f.get(frame + 0x12, 2) == 1 &&
        f.get(frame + 0x14, 2) == 0x20 && f.get(frame + 0x16, 2) == 1);
  check(f.progress.machine.registers.gpr[29].word == Fixture::Sp &&
        f.progress.machine.registers.gpr[31].word == 0x80012340u);
  const std::array<std::uint32_t, 24> pcs{
      0x80094544u, 0x80094548u, 0x8009454cu, 0x80094554u, 0x8009455cu,
      0x80094564u, 0x80094570u, 0x80094574u, 0x8009460cu, 0x80094610u,
      0x80094614u, 0x8009461cu, 0x80094620u, 0x80094624u, 0x80094628u,
      0x80094630u, 0x80094638u, 0x80094654u, 0x80094680u,
      0x80094684u, 0x80094688u, 0x8009468cu, 0x80094690u, 0x80094694u};
  check(f.progress.access_events == 24);
  for (std::size_t i = 0; i < f.progress.access_events; ++i)
    check(f.journal[i].pc == pcs[i]);

  Fixture zero;
  zero.machine.registers.gpr[7] = {0, 15};
  zero.put(Fixture::Sp + 0x10u, 0, 4);
  check(zero.run() == NBA97_TEXT_COMPLETE && zero.calls.empty());

  Fixture lowZero;
  lowZero.machine.registers.gpr[7] = {0, 15};
  lowZero.put(Fixture::Sp + 0x10u, 0x10000u, 4);
  check(lowZero.run() == NBA97_TEXT_COMPLETE && lowZero.calls.size() == 1);
}

void type40AndBoundaries() {
  Fixture f;
  f.put(Fixture::Record, 0x40, 1);
  f.put(Fixture::Record + 4, 0xfff5u, 2);
  f.put(Fixture::Record + 6, 7, 2);
  f.put(Fixture::Record + 0x0c, 0xc123, 2);
  check(f.run() == NBA97_TEXT_COMPLETE && f.calls.size() == 2);
  check(f.calls[0].event.kind == NBA97_GAME_IMAGE_RECORD_UPLOAD_CHILD_800A3BF8 &&
        f.calls[0].event.pc == 0x800945c8u &&
        f.calls[0].event.delay_slot_pc == 0x800945ccu &&
        f.calls[0].machine.registers.gpr[31].word == 0x800945d0u);
  check(f.calls[1].event.kind == NBA97_GAME_IMAGE_RECORD_UPLOAD_CHILD_800944F4);
  check(f.get(Fixture::Record, 1) == 0x48 &&
        f.get(Fixture::Record + 0x0c, 2) == 0xc340 &&
        f.get(Fixture::Record + 0x0e, 2) == 0xf0);
  const auto frame = Fixture::Sp - 0x30u;
  check(f.get(frame + 0x14, 2) == 0xffffu &&
        f.get(frame + 0x16, 2) == 7);
  check(f.progress.machine.lo.word == std::uint32_t(std::int32_t(-11) * 3) &&
        f.progress.machine.hi.word == 0xffffffffu);

  for (const auto header : {0x22u, 0x24u, 0x3fu, 0x44u}) {
    Fixture skipped;
    skipped.put(Fixture::Record, header, 1);
    check(skipped.run() == NBA97_TEXT_COMPLETE && skipped.calls.empty() &&
          skipped.get(Fixture::Record, 1) == header);
  }
  Fixture masked;
  masked.put(Fixture::Record, 0x4b, 1);
  check(masked.run() == NBA97_TEXT_COMPLETE && masked.calls.size() == 2 &&
        masked.get(Fixture::Record, 1) == 0x4b);
}

void linksCallbacksAndPrefixes() {
  Fixture linked;
  linked.put(Fixture::Record, 0x10023u, 4);
  linked.put(Fixture::Record + 0x100u, 0x23u, 4);
  linked.machine.registers.gpr[7] = {0, 15};
  linked.put(Fixture::Sp + 0x10u, 0, 4);
  check(linked.run() == NBA97_TEXT_COMPLETE && linked.progress.records_visited == 2);

  Fixture refused;
  refused.refuseKind = NBA97_GAME_IMAGE_RECORD_UPLOAD_CHILD_800944F4;
  check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.progress.stopped_pc == 0x8009464cu &&
        refused.progress.stopped_entry == 0x800944f4u);

  Fixture invalid;
  invalid.invalidMachine = true;
  check(invalid.run() == NBA97_TEXT_ARGUMENT &&
        invalid.progress.callbacks_completed == 0);

  Fixture relocated;
  relocated.put(Fixture::Record, 0x40, 1);
  relocated.put(Fixture::Record + 4, 1, 2);
  relocated.put(Fixture::Record + 6, 1, 2);
  relocated.put(Fixture::Record + 0x100u, 0, 4);
  const auto newFrame = Fixture::Sp - 0x100u;
  relocated.put(newFrame + 0x2c, 0x80056780u, 4);
  relocated.put(newFrame + 0x28, 0x44u, 4);
  relocated.put(newFrame + 0x24, 0x33u, 4);
  relocated.put(newFrame + 0x20, 0x22u, 4);
  relocated.put(newFrame + 0x1c, 0x11u, 4);
  relocated.put(newFrame + 0x18, 0, 4);
  relocated.relocate = true;
  check(relocated.run() == NBA97_TEXT_COMPLETE &&
        relocated.progress.machine.registers.gpr[29].word == Fixture::Sp - 0xd0u &&
        relocated.progress.machine.registers.gpr[31].word == 0x80056780u);
}

void unknownMappingAndBudgets() {
  Fixture partial;
  partial.known[Fixture::Record - Fixture::Base] = 0;
  check(partial.run() == NBA97_TEXT_UNKNOWN &&
        partial.progress.stopped_pc == 0x80094584u);

  Fixture unaligned;
  unaligned.machine.registers.gpr[29] = {Fixture::Sp + 2u, 15};
  check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x80094544u);

  Fixture complete;
  check(complete.run() == NBA97_TEXT_COMPLETE);
  const std::size_t required = complete.progress.operations;
  check(required > 0);
  for (std::size_t budget = 0; budget < required; ++budget) {
    Fixture bounded;
    bounded.budget = budget;
    check(bounded.run() == NBA97_TEXT_LIMIT &&
          bounded.progress.operations == budget && !bounded.progress.completed);
  }

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture ra;
    ra.machine.registers.gpr[31].known_mask = std::uint8_t(mask);
    const int result = ra.run();
    check((mask == 15 && result == NBA97_TEXT_COMPLETE) ||
          (mask != 15 && result == NBA97_TEXT_UNKNOWN &&
           ra.progress.stopped_pc == 0x8009469cu));
  }

  Fixture noKnown;
  noKnown.region.known = nullptr;
  noKnown.machine.registers.gpr[31].known_mask = 7;
  check(noKnown.run() == NBA97_TEXT_ARGUMENT &&
        noKnown.progress.stopped_pc == 0x80094570u);

  Fixture pointer;
  pointer.machine.registers.gpr[4] = {Fixture::Record + 1u, 1};
  check(pointer.run() == NBA97_TEXT_UNKNOWN &&
        pointer.progress.stopped_pc == 0x80094574u);

  Fixture late;
  late.poisonAddress = Fixture::Sp - 0x30u + 0x18u;
  const int lateResult = late.run();
  check(lateResult == NBA97_TEXT_ARGUMENT &&
        late.progress.stopped_pc == 0x80094694u &&
        late.progress.machine.registers.gpr[16].word == 0);

  Fixture cycle;
  cycle.machine.registers.gpr[7] = {0, 15};
  cycle.put(Fixture::Sp + 0x10u, 0, 4);
  cycle.put(Fixture::Record, 0x00010023u, 4);
  cycle.put(Fixture::Record + 0x100u, 0xffff0023u, 4);
  cycle.budget = 60;
  check(cycle.run() == NBA97_TEXT_LIMIT && cycle.progress.records_visited > 1);
}

void invalidMemoryRegions() {
  Fixture f;
  Nba97GameImageRecordUploadContext context{};
  Nba97GameImageRecordUploadProgress progress{};
  context.machine = f.machine;
  context.operation_budget = 1;
  std::uint8_t byte = 0;
  Nba97GameTextRegion bad{0xffffffffu, &byte, nullptr, 2};
  context.memory = {&bad, 1};
  check(nba97_game_image_record_upload(&context, &progress) == NBA97_TEXT_ARGUMENT);
  bad = {0, &byte, nullptr, SIZE_MAX};
  check(nba97_game_image_record_upload(&context, &progress) == NBA97_TEXT_ARGUMENT);
  std::array<std::uint8_t, 8> data{};
  std::array<Nba97GameTextRegion, 2> overlap{{
      {0x1000u, data.data(), nullptr, 8},
      {0x1004u, data.data(), nullptr, 4},
  }};
  context.memory = {overlap.data(), overlap.size()};
  check(nba97_game_image_record_upload(&context, &progress) == NBA97_TEXT_ARGUMENT);
}

void deterministicFullState() {
  Fixture left;
  Fixture right;
  check(left.run() == NBA97_TEXT_COMPLETE && right.run() == NBA97_TEXT_COMPLETE);
  check(left.bytes == right.bytes && left.known == right.known &&
        sameMachine(left.progress.machine, right.progress.machine));
  check(left.progress.access_events == right.progress.access_events);
  for (std::size_t i = 0; i < left.progress.access_events; ++i) {
    const auto &a = left.journal[i];
    const auto &b = right.journal[i];
    check(a.pc == b.pc && a.address == b.address && a.value == b.value &&
          a.operation == b.operation && a.width == b.width &&
          a.known_mask == b.known_mask && a.kind == b.kind);
  }
  for (const unsigned i : {8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 21u,
                           22u, 23u, 24u, 25u, 26u, 27u, 28u, 30u})
    check(sameWord(left.progress.machine.registers.gpr[i],
                   left.machine.registers.gpr[i]));
}
} // namespace

int main() {
  type23AndFullWordGates();
  type40AndBoundaries();
  linksCallbacksAndPrefixes();
  unknownMappingAndBudgets();
  invalidMemoryRegions();
  deterministicFullState();
  std::cout << "game_image_record_upload_tests: PASS\n";
}
