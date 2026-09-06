#include "recovered/frontend_resource_info.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using U = std::uint32_t;
unsigned checks;

void check(bool value, int line) {
  ++checks;
  if (!value)
    throw std::runtime_error("frontend-resource-info line " +
                             std::to_string(line));
}
#define CHECK(value) check((value), __LINE__)

constexpr U Base = 0x80000000u;
constexpr U Size = 0x200000u;
constexpr U Sp = 0x801f0000u;
constexpr U Filename = 0x80024854u;
constexpr U HandleOut = 0x801e0000u;
constexpr U OtherOut = 0x801e0004u;
constexpr U SizeOut = 0x801e0008u;
constexpr U Fifth = 0x2468ace0u;

struct Seen {
  Nba97FrontendResourceInfoEvent event{};
  Nba97FrontendResourceInfoMachine machine{};
};

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendResourceInfoContext context{};
  Nba97FrontendResourceInfoProgress progress{};
  std::array<Nba97FrontendResourceInfoAccess, 160> access{};
  std::array<U, 400> pcs{};
  std::vector<Seen> calls;
  U prefix = 1;
  std::uint8_t prefix_mask = 15;
  std::vector<U> opens{0x44u};
  std::vector<std::uint8_t> open_masks{15};
  U info = 0x1200u;
  std::uint8_t info_mask = 15;
  U seek = 0;
  std::uint8_t seek_mask = 15;
  unsigned refuse_site = 0;
  unsigned succeed_after = 1;
  U relocate_frame = 0;
  bool malformed_machine = false;
  bool open_sets_stale_success = false;
  bool keep_retry_counter_nonzero = false;
  bool mutate_live_machine = false;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {
          0x31000000u + i * 0x101u,
          static_cast<std::uint8_t>((i % 15u) + 1u)};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[4] = {Filename, 5};
    context.machine.registers.gpr[5] = {HandleOut, 15};
    context.machine.registers.gpr[6] = {OtherOut, 15};
    context.machine.registers.gpr[7] = {SizeOut, 15};
    context.machine.registers.gpr[29] = {Sp, 15};
    context.machine.registers.gpr[31] = {0x8007b21cu, 15};
    context.machine.hi = {0x12345678u, 5};
    context.machine.lo = {0x9abcdef0u, 10};
    put(Sp + 16, Fifth, 9);
    context.memory = {&region, 1};
    context.operation_budget = 160;
    context.io = callback;
    context.user = this;
    context.access_journal = access.data();
    context.access_journal_capacity = access.size();
    context.instruction_journal = pcs.data();
    context.instruction_journal_capacity = pcs.size();
  }

  void put(U address, U value, std::uint8_t mask = 15) {
    if (address < Base || address - Base > Size - 4)
      throw std::runtime_error("fixture write outside retained RAM");
    for (unsigned i = 0; i < 4; ++i) {
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
      if (region.known)
        known[address - Base + i] = std::uint8_t((mask >> i) & 1u);
    }
  }

  U get(U address) const {
    if (address < Base || address - Base > Size - 4)
      throw std::runtime_error("fixture read outside retained RAM");
    U value = 0;
    for (unsigned i = 0; i < 4; ++i)
      value |= U(bytes[address - Base + i]) << (i * 8u);
    return value;
  }

  int run() { return nba97_frontend_resource_info(&context, &progress); }

  void moveFrame(Nba97FrontendResourceInfoMachine &machine) {
    U old_frame = machine.registers.gpr[29].word;
    if (!relocate_frame)
      return;
    if (old_frame < Base || old_frame - Base > Size - 384 ||
        relocate_frame < Base || relocate_frame - Base > Size - 384)
      throw std::runtime_error("fixture frame move outside retained RAM");
    for (unsigned i = 0; i < 384; ++i) {
      bytes[relocate_frame - Base + i] = bytes[old_frame - Base + i];
      if (region.known)
        known[relocate_frame - Base + i] = known[old_frame - Base + i];
    }
    machine.registers.gpr[29] = {relocate_frame, 15};
    relocate_frame = 0;
  }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97FrontendResourceInfoEvent *event,
                      Nba97FrontendResourceInfoMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    if (!event || !machine)
      return 0;
    fixture.calls.push_back({*event, *machine});
    if (event->site == NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A5E8)
      machine->registers.gpr[2] = {fixture.prefix, fixture.prefix_mask};
    if (event->site == NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A63C) {
      fixture.moveFrame(*machine);
      if (fixture.mutate_live_machine) {
        machine->registers.gpr[8] = {0xa1b2c3d4u, 6};
        machine->hi = {0x01020304u, 3};
        machine->lo = {0xf0e0d0c0u, 12};
      }
    }
    if (event->site == NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A648) {
      std::size_t index = event->invocation - 1;
      if (index >= fixture.opens.size())
        index = fixture.opens.size() - 1;
      std::size_t mask_index = event->invocation - 1;
      if (mask_index >= fixture.open_masks.size())
        mask_index = fixture.open_masks.size() - 1;
      machine->registers.gpr[2] = {fixture.opens[index],
                                   fixture.open_masks[mask_index]};
      if (fixture.open_sets_stale_success) {
        machine->registers.gpr[17] = {5, 15};
        machine->registers.gpr[19] = {0, 15};
      }
      if (fixture.keep_retry_counter_nonzero)
        machine->registers.gpr[18] = {2, 15};
    }
    if (event->site == NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A658) {
      U value = event->invocation >= fixture.succeed_after ? fixture.info : 0;
      machine->registers.gpr[2] = {value, fixture.info_mask};
    }
    if (event->site == NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A66C)
      machine->registers.gpr[2] = {fixture.seek, fixture.seek_mask};
    if (fixture.malformed_machine)
      machine->hi.known_mask = 16;
    return event->site != fixture.refuse_site;
  }
};

void checkRestores(const Fixture &fixture, U final_sp = Sp) {
  CHECK(fixture.progress.machine.registers.gpr[29].word == final_sp);
  CHECK(fixture.progress.machine.registers.gpr[31].word == 0x8007b21cu);
  for (unsigned reg = 16; reg <= 23; ++reg) {
    CHECK(fixture.progress.machine.registers.gpr[reg].word ==
          0x31000000u + reg * 0x101u);
    CHECK(fixture.progress.machine.registers.gpr[reg].known_mask ==
          static_cast<std::uint8_t>((reg % 15u) + 1u));
  }
  CHECK(fixture.progress.machine.hi.word == 0x12345678u &&
        fixture.progress.machine.hi.known_mask == 5 &&
        fixture.progress.machine.lo.word == 0x9abcdef0u &&
        fixture.progress.machine.lo.known_mask == 10);
}

void delegatedPath() {
  Fixture fixture;
  fixture.prefix = 0;
  CHECK(fixture.run() == NBA97_TEXT_COMPLETE && fixture.progress.completed &&
        fixture.progress.delegated_path && fixture.calls.size() == 2);
  CHECK(fixture.progress.instruction_count == 47 &&
        fixture.progress.operations == 23 && fixture.progress.accesses == 21 &&
        fixture.progress.reads == 10 && fixture.progress.stores == 11);
  CHECK(fixture.calls[0].event.pc == 0x8008a5e8u &&
        fixture.calls[0].event.entry == 0x80084910u &&
        fixture.calls[0].event.argument_count == 3 &&
        fixture.calls[0].machine.registers.gpr[4].word == 0x800d96a8u &&
        fixture.calls[0].machine.registers.gpr[5].word == 0x800d9a58u &&
        fixture.calls[0].machine.registers.gpr[6].word == 6);
  const auto &delegated = fixture.calls[1];
  CHECK(delegated.event.pc == 0x8008a610u &&
        delegated.event.delay_slot_pc == 0x8008a614u &&
        delegated.event.entry == 0x80074184u &&
        delegated.event.argument_count == 6 && delegated.event.invocation == 1);
  CHECK(delegated.machine.registers.gpr[4].word == Filename &&
        delegated.machine.registers.gpr[5].word == HandleOut &&
        delegated.machine.registers.gpr[6].word == OtherOut &&
        delegated.machine.registers.gpr[7].word == SizeOut &&
        fixture.get(Sp - 352 + 16) == Sp - 48 &&
        fixture.get(Sp - 352 + 20) == Fifth);
  checkRestores(fixture);
}

void directSuccess() {
  Fixture fixture;
  CHECK(fixture.run() == NBA97_TEXT_COMPLETE && fixture.progress.completed &&
        fixture.progress.direct_path_success && fixture.calls.size() == 5);
  CHECK(fixture.progress.instruction_count == 67 &&
        fixture.progress.operations == 30 && fixture.progress.accesses == 25 &&
        fixture.progress.reads == 11 && fixture.progress.stores == 14 &&
        fixture.progress.callbacks_completed == 5);
  const U call_pcs[] = {0x8008a5e8u, 0x8008a63cu, 0x8008a648u,
                        0x8008a658u, 0x8008a66cu};
  const U targets[] = {0x80084910u, 0x80083b70u, 0x8007f588u,
                       0x8008a408u, 0x8007f318u};
  const unsigned argc[] = {3, 4, 2, 1, 3};
  for (unsigned i = 0; i < 5; ++i)
    CHECK(fixture.calls[i].event.pc == call_pcs[i] &&
          fixture.calls[i].event.entry == targets[i] &&
          fixture.calls[i].event.argument_count == argc[i]);
  CHECK(fixture.calls[1].machine.registers.gpr[4].word == Sp - 328 &&
        fixture.calls[1].machine.registers.gpr[5].word == 0x800d9a60u &&
        fixture.calls[1].machine.registers.gpr[6].word == 0x800d96a8u &&
        fixture.calls[1].machine.registers.gpr[7].word == Filename);
  CHECK(fixture.calls[2].machine.registers.gpr[4].word == Sp - 328 &&
        fixture.calls[2].machine.registers.gpr[5].word == 1 &&
        fixture.calls[3].machine.registers.gpr[4].word == 0x44u &&
        fixture.calls[4].machine.registers.gpr[4].word == 0x44u &&
        fixture.calls[4].machine.registers.gpr[5].word == 0 &&
        fixture.calls[4].machine.registers.gpr[6].word == 0);
  CHECK(fixture.get(HandleOut) == 0x44u && fixture.get(OtherOut) == 0 &&
        fixture.get(SizeOut) == 0x1200u &&
        fixture.progress.input_fifth_argument.word == Fifth &&
        fixture.progress.input_fifth_argument.known_mask == 9);
  const U access_pcs[] = {
      0x8008a598u, 0x8008a5a0u, 0x8008a5a8u, 0x8008a5b0u,
      0x8008a5b8u, 0x8008a5c0u, 0x8008a5c8u, 0x8008a5ccu,
      0x8008a5e4u, 0x8008a5ecu, 0x8008a634u, 0x8008a638u,
      0x8008a640u, 0x8008a654u, 0x8008a660u, 0x8008a6b8u,
      0x8008a6bcu, 0x8008a6c0u, 0x8008a6c4u, 0x8008a6c8u,
      0x8008a6ccu, 0x8008a6d0u, 0x8008a6d4u, 0x8008a6d8u,
      0x8008a6dcu};
  const U addresses[] = {
      Sp - 20, Sp - 40, Sp - 16, Sp - 24, Sp - 36, Sp - 32, Sp - 12,
      Sp + 16, Sp - 8,  Sp - 28, HandleOut, OtherOut, SizeOut, HandleOut,
      HandleOut, SizeOut, Sp - 8, Sp - 12, Sp - 16, Sp - 20, Sp - 24,
      Sp - 28, Sp - 32, Sp - 36, Sp - 40};
  const unsigned operations[] = {1,  2,  3,  4,  5,  6,  7,  8,  9,
                                 10, 12, 13, 14, 17, 19, 21, 22, 23,
                                 24, 25, 26, 27, 28, 29, 30};
  for (unsigned i = 0; i < 25; ++i) {
    CHECK(fixture.access[i].pc == access_pcs[i] &&
          fixture.access[i].address == addresses[i] &&
          fixture.access[i].operation == operations[i]);
    CHECK(fixture.access[i].kind ==
          ((i == 7 || i == 14 || i >= 16)
               ? NBA97_FRONTEND_RESOURCE_INFO_READ
               : NBA97_FRONTEND_RESOURCE_INFO_STORE));
  }
  checkRestores(fixture);
}

void retriesAndQuirks() {
  Fixture ten;
  ten.opens = {0};
  CHECK(ten.run() == NBA97_TEXT_COMPLETE && ten.progress.completed &&
        !ten.progress.direct_path_success && ten.progress.attempts_started == 10 &&
        ten.progress.failed_attempts == 10);
  CHECK(ten.progress.instruction_count == 288 &&
        ten.progress.operations == 101 && ten.progress.accesses == 80 &&
        ten.progress.callbacks_completed == 21 && ten.get(SizeOut) == 0);
  CHECK(ten.progress.call_count[NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A63C] ==
            10 &&
        ten.progress.call_count[NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A648] ==
            10 &&
        ten.progress.call_count[NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A698] ==
            0);

  Fixture close;
  close.succeed_after = 3;
  CHECK(close.run() == NBA97_TEXT_COMPLETE);
  CHECK(close.progress.completed);
  CHECK(close.progress.attempts_started == 3);
  CHECK(close.progress.failed_attempts == 2);
  CHECK(close.progress.call_count[NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A698] ==
        2);
  CHECK(close.progress.direct_path_success);
  CHECK(close.get(SizeOut) == 0x1200u);

  Fixture stale;
  stale.opens = {0xffffffffu};
  stale.open_sets_stale_success = true;
  CHECK(stale.run() == NBA97_TEXT_COMPLETE && stale.progress.completed &&
        stale.progress.attempts_started == 1 && stale.progress.failed_attempts == 0 &&
        stale.progress.direct_path_success && stale.get(HandleOut) == 0xffffffffu &&
        stale.get(SizeOut) == 5);

  Fixture alias;
  alias.context.machine.registers.gpr[7] = {Sp - 36, 15};
  CHECK(alias.run() == NBA97_TEXT_COMPLETE && alias.progress.completed &&
        alias.progress.machine.registers.gpr[17].word == 0x1200u &&
        alias.progress.machine.registers.gpr[17].known_mask == 15);

  Fixture mutated;
  mutated.mutate_live_machine = true;
  CHECK(mutated.run() == NBA97_TEXT_COMPLETE && mutated.progress.completed &&
        mutated.progress.machine.registers.gpr[8].word == 0xa1b2c3d4u &&
        mutated.progress.machine.registers.gpr[8].known_mask == 6 &&
        mutated.progress.machine.hi.word == 0x01020304u &&
        mutated.progress.machine.hi.known_mask == 3 &&
        mutated.progress.machine.lo.word == 0xf0e0d0c0u &&
        mutated.progress.machine.lo.known_mask == 12);
}

void callbacksAndCoverage() {
  std::set<U> covered;
  auto gather = [&](const Fixture &fixture) {
    for (std::size_t i = 0; i < fixture.progress.instruction_events; ++i)
      covered.insert(fixture.pcs[i]);
  };
  Fixture delegated;
  delegated.prefix = 0;
  CHECK(delegated.run() == NBA97_TEXT_COMPLETE);
  gather(delegated);
  Fixture success;
  CHECK(success.run() == NBA97_TEXT_COMPLETE);
  gather(success);
  Fixture close;
  close.info = 0;
  close.succeed_after = 2;
  CHECK(close.run() == NBA97_TEXT_COMPLETE);
  gather(close);
  for (U pc = 0x8008a594u; pc <= 0x8008a6e8u; pc += 4)
    CHECK(covered.count(pc) == 1);

  for (unsigned site = 1; site < NBA97_FRONTEND_RESOURCE_INFO_SITE_COUNT;
       ++site) {
    Fixture refused;
    refused.refuse_site = site;
    if (site == NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A610)
      refused.prefix = 0;
    if (site == NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A698)
      refused.info = 0;
    CHECK(refused.run() == NBA97_TEXT_IO_REFUSED &&
          refused.progress.call_attempts[site] == 1 &&
          refused.progress.call_count[site] == 0 &&
          refused.progress.stopped_target != 0);
  }
  Fixture no_io;
  no_io.context.io = nullptr;
  CHECK(no_io.run() == NBA97_TEXT_IO_REFUSED &&
        no_io.progress.stopped_pc == 0x8008a5e8u);
  Fixture malformed;
  malformed.malformed_machine = true;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.callbacks_completed == 0);
}

void budgets() {
  Fixture full;
  CHECK(full.run() == NBA97_TEXT_COMPLETE);
  const std::size_t direct_operations = full.progress.operations;
  for (std::size_t budget = 0; budget < direct_operations; ++budget) {
    Fixture limited;
    limited.context.operation_budget = budget;
    CHECK(limited.run() == NBA97_TEXT_LIMIT &&
          limited.progress.operations == budget && !limited.progress.completed);
  }

  Fixture delegated_full;
  delegated_full.prefix = 0;
  CHECK(delegated_full.run() == NBA97_TEXT_COMPLETE);
  for (std::size_t budget = 0; budget < delegated_full.progress.operations;
       ++budget) {
    Fixture limited;
    limited.prefix = 0;
    limited.context.operation_budget = budget;
    CHECK(limited.run() == NBA97_TEXT_LIMIT &&
          limited.progress.operations == budget && !limited.progress.completed);
  }

  Fixture retry_full;
  retry_full.opens = {0};
  CHECK(retry_full.run() == NBA97_TEXT_COMPLETE);
  for (std::size_t budget = 0; budget < retry_full.progress.operations;
       ++budget) {
    Fixture limited;
    limited.opens = {0};
    limited.context.operation_budget = budget;
    CHECK(limited.run() == NBA97_TEXT_LIMIT &&
          limited.progress.operations == budget && !limited.progress.completed);
  }

  Fixture bounded_runaway;
  bounded_runaway.opens = {0};
  bounded_runaway.keep_retry_counter_nonzero = true;
  bounded_runaway.context.operation_budget = 40;
  CHECK(bounded_runaway.run() == NBA97_TEXT_LIMIT &&
        bounded_runaway.progress.operations == 40 &&
        bounded_runaway.progress.attempts_started > 3 &&
        !bounded_runaway.progress.completed);
}

void knownnessAndFailures() {
  for (unsigned mask = 0; mask < 15; ++mask) {
    Fixture prefix;
    prefix.prefix = 0;
    prefix.prefix_mask = static_cast<std::uint8_t>(mask);
    CHECK(prefix.run() == NBA97_TEXT_UNKNOWN &&
          prefix.progress.stopped_pc == 0x8008a5f0u &&
          prefix.get(HandleOut) == 0);
  }

  Fixture unknown_open;
  unknown_open.opens = {0};
  unknown_open.open_masks = {8};
  CHECK(unknown_open.run() == NBA97_TEXT_UNKNOWN &&
        unknown_open.progress.stopped_pc == 0x8008a650u &&
        unknown_open.get(HandleOut) == 0);

  Fixture negative_partial;
  negative_partial.opens = {0x80000000u};
  negative_partial.open_masks = {8};
  CHECK(negative_partial.run() == NBA97_TEXT_COMPLETE &&
        negative_partial.progress.attempts_started == 10 &&
        negative_partial.progress.call_count
                [NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A698] == 0);

  Fixture absent;
  absent.region.known = nullptr;
  for (auto &word : absent.context.machine.registers.gpr)
    word.known_mask = 15;
  absent.context.machine.hi.known_mask = 15;
  absent.context.machine.lo.known_mask = 15;
  CHECK(absent.run() == NBA97_TEXT_COMPLETE && absent.progress.completed);

  Fixture absent_partial;
  absent_partial.region.known = nullptr;
  absent_partial.context.machine.registers.gpr[21].known_mask = 7;
  CHECK(absent_partial.run() == NBA97_TEXT_ARGUMENT &&
        absent_partial.progress.stopped_pc == 0x8008a598u);

  Fixture malformed_byte;
  malformed_byte.known[Sp - 20u - Base] = 2;
  CHECK(malformed_byte.run() == NBA97_TEXT_ARGUMENT &&
        malformed_byte.progress.stopped_pc == 0x8008a598u);

  Fixture misaligned;
  misaligned.context.machine.registers.gpr[5] = {HandleOut + 1, 15};
  CHECK(misaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misaligned.progress.stopped_pc == 0x8008a634u);

  Fixture unmapped;
  unmapped.context.machine.registers.gpr[6] = {0x90000000u, 15};
  CHECK(unmapped.run() == NBA97_TEXT_RESOURCE &&
        unmapped.progress.stopped_pc == 0x8008a638u);

  Fixture overlap;
  Nba97GameTextRegion regions[2] = {overlap.region, overlap.region};
  overlap.context.memory = {regions, 2};
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT);

  Fixture partial_ra;
  partial_ra.context.machine.registers.gpr[31].known_mask = 7;
  CHECK(partial_ra.run() == NBA97_TEXT_UNKNOWN &&
        partial_ra.progress.stopped_pc == 0x8008a6e4u);

  Fixture bad_ra;
  bad_ra.context.machine.registers.gpr[31].word = 0x8007b21eu;
  CHECK(bad_ra.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        bad_ra.progress.stopped_target == 0x8007b21eu);
}

void relocationAndWrap() {
  Fixture relocated;
  relocated.relocate_frame = 0x801ed000u;
  CHECK(relocated.run() == NBA97_TEXT_COMPLETE && relocated.progress.completed &&
        relocated.progress.machine.registers.gpr[29].word == 0x801ed160u &&
        relocated.get(HandleOut) == 0x44u && relocated.get(SizeOut) == 0x1200u);
  checkRestores(relocated, 0x801ed160u);

  std::array<std::uint8_t, 512> low_bytes{};
  std::array<std::uint8_t, 512> low_known{};
  std::array<std::uint8_t, 4096> high_bytes{};
  std::array<std::uint8_t, 4096> high_known{};
  low_known.fill(1);
  high_known.fill(1);
  Nba97GameTextRegion low{0, low_bytes.data(), low_known.data(),
                          low_bytes.size()};
  Nba97GameTextRegion high{0xfffff000u, high_bytes.data(), high_known.data(),
                           high_bytes.size()};
  Nba97GameTextRegion regions[] = {low, high};
  Nba97FrontendResourceInfoContext context{};
  for (unsigned i = 0; i < 32; ++i)
    context.machine.registers.gpr[i] = {0x11000000u + i, 15};
  context.machine.registers.gpr[0] = {0, 15};
  context.machine.registers.gpr[4] = {0x180, 15};
  context.machine.registers.gpr[5] = {0x190, 15};
  context.machine.registers.gpr[6] = {0x194, 15};
  context.machine.registers.gpr[7] = {0x198, 15};
  context.machine.registers.gpr[29] = {0x100, 15};
  context.machine.registers.gpr[31] = {0x80001000, 15};
  for (unsigned i = 0; i < 4; ++i)
    low_bytes[0x110 + i] = std::uint8_t(Fifth >> (i * 8u));
  context.memory = {regions, 2};
  context.operation_budget = 40;
  context.io = [](void *, const Nba97GameTextMemory *,
                  const Nba97FrontendResourceInfoEvent *event,
                  Nba97FrontendResourceInfoMachine *machine) -> int {
    if (event->site == NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A5E8)
      machine->registers.gpr[2] = {0, 15};
    return 1;
  };
  std::array<Nba97FrontendResourceInfoAccess, 32> accesses{};
  context.access_journal = accesses.data();
  context.access_journal_capacity = accesses.size();
  Nba97FrontendResourceInfoProgress progress{};
  CHECK(nba97_frontend_resource_info(&context, &progress) ==
            NBA97_TEXT_COMPLETE &&
        progress.completed && progress.frame_stack_pointer == 0xffffffa0u &&
        progress.machine.registers.gpr[29].word == 0x100u &&
        progress.input_fifth_argument.word == Fifth);
}
} // namespace

int main() {
  try {
    delegatedPath();
    directSuccess();
    retriesAndQuirks();
    callbacksAndCoverage();
    budgets();
    knownnessAndFailures();
    relocationAndWrap();
    std::printf("frontend_resource_info_tests passed %u checks\n", checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
