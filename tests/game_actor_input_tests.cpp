#include "recovered/game_actor_input.h"

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
    std::fprintf(stderr, "actor input check %u failed at line %u\n", checks,
                 line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t EntrySp = 0x800ff000u;
constexpr std::uint32_t FrameSp = EntrySp - 0x48u;
constexpr std::uint32_t ActorTable = 0x80020becu;
constexpr std::uint32_t Actor0 = 0x80030000u;
constexpr std::uint32_t Controller = 0x80040000u;
constexpr std::uint32_t ActionTable = 0x800275c4u;
constexpr std::array<std::uint32_t, 21> ActionEntries{
    {0x800670a8u, 0x8006afb0u, 0x8006c518u, 0x8006b064u, 0x8006c720u,
     0x8006b168u, 0x8006cae0u, 0x800597ecu, 0x8005853cu, 0x8006ce60u,
     0x8006ac0cu, 0x8006bd88u, 0x8006b170u, 0x8005c5e0u, 0x80059f44u,
     0x8005b028u, 0x8005c438u, 0x80059968u, 0x8005d070u, 0x8005cf5cu,
     0x8005d9f0u}};
constexpr std::array<std::uint32_t, 21> ActionPcs{
    {0x80068a4cu, 0x80068a5cu, 0x80068a6cu, 0x80068a7cu, 0x80068a8cu,
     0x80068a9cu, 0x80068aacu, 0x80068abcu, 0x80068accu, 0x80068adcu,
     0x80068aecu, 0x80068afcu, 0x80068b0cu, 0x80068b1cu, 0x80068b2cu,
     0x80068b3cu, 0x80068b4cu, 0x80068b5cu, 0x80068b6cu, 0x80068b7cu,
     0x80068b8cu}};

struct Fixture {
  enum Mode {
    Ordinary,
    RefuseFirst,
    InvalidMachine,
    MutateLate,
    MutateTableAtInput
  } mode = Ordinary;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x110000u, 0);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x110000u, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameActorInputAccess, 2048> journal{};
  Nba97GameActorInputContext context{};
  Nba97GameActorInputProgress progress{};
  Nba97GameActorInputMachine incoming{};
  std::vector<Nba97GameActorInputEvent> events;
  std::vector<Nba97GameActorInputMachine> prefixes;
  unsigned action_state{};
  bool map47{};

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      incoming.registers.gpr[i] = {0x11000000u + i * 0x01010101u,
                                   static_cast<std::uint8_t>((i % 15u) + 1u)};
    incoming.registers.gpr[0] = {0, 0x0f};
    incoming.registers.gpr[29] = {EntrySp, 0x0f};
    incoming.registers.gpr[31] = {0x81234568u, 0x0f};
    incoming.hi = {0x13572468u, 5};
    incoming.lo = {0x89abcdefu, 0x0a};
    context.memory = {&region, 1};
    context.operation_budget = 10000;
    context.machine = incoming;
    context.io = io;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();

    put(0x80021d82u, 0, 1);
    put(0x800fdb8au, 0, 2);
    put(0x800fdb6eu, 3, 2);
    put(0x800fdb94u, 0x1234, 2);
    put(0x8001edf4u + 0x14u, 0x1234, 2);
    put(0x8001eeb8u + 0x14u, 0x1234, 2);
    put(0x800fdc50u, Controller, 4);
    put(Controller, 0xabcdef00u, 4);
    put(Controller + 0x28u, 0, 2);
    put(Controller + 0x2au, 6, 2);
    for (unsigned i = 0; i < 21; ++i)
      put(ActionTable + i * 4u, ActionPcs[i], 4);
    for (unsigned i = 0; i < 10; ++i) {
      const auto actor = Actor0 + i * 0x1000u;
      put(ActorTable + i * 4u, actor, 4);
      put(actor + 4u, 0xffffu, 2);
      put(actor + 0x1au, 0, 1);
      put(actor + 0x46u, 0x2bu, 2);
    }
  }

  std::size_t offset(std::uint32_t address) const {
    return static_cast<std::size_t>(address - Ram);
  }
  void put(std::uint32_t address, std::uint32_t value, unsigned width,
           std::uint8_t mask = 0x0f) {
    auto at = offset(address);
    for (unsigned i = 0; i < width; ++i) {
      bytes[at + i] = static_cast<std::uint8_t>(value >> (8u * i));
      known[at + i] = static_cast<std::uint8_t>((mask >> i) & 1u);
    }
  }
  std::uint32_t get(std::uint32_t address, unsigned width) const {
    auto at = offset(address);
    std::uint32_t value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= std::uint32_t(bytes[at + i]) << (8u * i);
    return value;
  }
  void enableActor(unsigned index, unsigned state) {
    action_state = state;
    auto actor = Actor0 + index * 0x1000u;
    put(actor + 4u, 0, 2);
    put(actor + 0x1au, state, 1);
  }
  static int io(void *opaque, const Nba97GameTextMemory *,
                const Nba97GameActorInputEvent *event,
                Nba97GameActorInputMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    f.events.push_back(*event);
    f.prefixes.push_back(*machine);
    if (f.mode == RefuseFirst) {
      for (unsigned i = 1; i < 32; ++i)
        machine->registers.gpr[i] = {0x60000000u + i * 0x00010101u,
                                     static_cast<std::uint8_t>(i & 0x0fu)};
      machine->hi = {0x24681357u, 6};
      machine->lo = {0xfedcba98u, 9};
      return 0;
    }
    if (f.mode == InvalidMachine) {
      machine->lo.known_mask = 0x10;
      return 1;
    }
    if (event->entry == 0x8008f224u)
      machine->registers.gpr[2] = {0x55667788u, 0x0f};
    else if (event->entry == 0x8002d2dcu)
      machine->registers.gpr[2] = {0x11223344u, 0x0f};
    else if (event->entry == 0x800700e4u)
      machine->registers.gpr[2] = {0x99aabbccu, 0x0f};
    if (f.mode == MutateTableAtInput && event->pc == 0x80068814u)
      f.put(ActionTable + f.action_state * 4u, ActionPcs[20], 4);
    if (f.mode == MutateLate && event->pc >= 0x80068a4cu) {
      machine->registers.gpr[20] = {9, 0x0f};
      machine->registers.gpr[22] = {0x80021000u, 0x0f};
      machine->registers.gpr[29] = {0x800fee00u, 0x0f};
      f.put(0x800fee44u, 0x82345678u, 4);
      f.put(0x800fdc4cu, 0x80041000u, 4);
      f.put(0x80041028u, 0, 2);
    }
    return 1;
  }
  int run() {
    context.machine = incoming;
    return nba97_game_actor_input(&context, &progress);
  }
};

void all_runtime_action_cases_and_arguments() {
  for (unsigned state = 0; state < 21; ++state) {
    Fixture f;
    f.enableActor(0, state);
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.events.size() == 15 && f.events[5].pc == ActionPcs[state] &&
          f.events[5].entry == ActionEntries[state] &&
          f.events[5].delay_slot_pc == ActionPcs[state] + 4u &&
          f.events[5].argument_count == ((state == 0 || state == 5) ? 0 : 1));
    check(f.events[0].pc == 0x800687b0u && f.events[0].argument_count == 1 &&
          f.prefixes[0].registers.gpr[4].word == 0);
    check(f.events[1].pc == 0x800687e4u && f.events[1].argument_count == 2 &&
          f.prefixes[1].registers.gpr[4].word == 0x55667788u &&
          f.prefixes[1].registers.gpr[5].word == 0x8004003fu);
    check(f.events[2].pc == 0x800687f4u && f.events[2].argument_count == 2 &&
          f.prefixes[2].registers.gpr[4].word == 0x80040000u &&
          f.prefixes[2].registers.gpr[5].word == 0x11223344u &&
          f.prefixes[2].registers.gpr[16].word == 0x11223344u);
    check(f.events[3].pc == 0x80068808u && f.events[3].argument_count == 4 &&
          f.prefixes[3].registers.gpr[4].word == Actor0 &&
          f.prefixes[3].registers.gpr[5].word == 0x80040000u &&
          f.prefixes[3].registers.gpr[6].word == 0x99aabbccu &&
          f.prefixes[3].registers.gpr[7].word == 0x11223344u);
    check(f.events[4].pc == 0x80068814u && f.events[4].argument_count == 1 &&
          f.prefixes[4].registers.gpr[4].word == Actor0);
    check(f.get(Controller + 0x28u, 2) == 1 &&
          f.get(0x800fdc3cu, 4) == Actor0 + 9u * 0x1000u &&
          f.get(0x800fdc40u, 4) == 0x8001eeb8u);
    bool read_runtime_word = false;
    for (unsigned i = 0; i < f.progress.access_events; ++i)
      read_runtime_word |= f.journal[i].pc == 0x80068a3cu &&
                           f.journal[i].address == ActionTable + state * 4u;
    check(read_runtime_word);
  }
}

void teams_mapping_dedup_and_live_globals() {
  Fixture f;
  f.enableActor(0, 6);
  f.enableActor(1, 7);
  check(f.run() == NBA97_TEXT_COMPLETE);
  unsigned input_calls = 0, action_calls = 0;
  for (const auto &event : f.events) {
    input_calls += event.pc >= 0x800687b0u && event.pc <= 0x80068814u;
    action_calls += event.pc >= 0x80068a4cu && event.pc <= 0x80068b8cu;
  }
  check(input_calls == 5 && action_calls == 10 &&
        f.get(Controller + 0x28u, 2) == 1);
  bool first_team = false, second_team = false;
  for (unsigned i = 0; i < f.progress.access_events; ++i) {
    const auto &a = f.journal[i];
    if (a.pc == 0x80068760u && a.value == 0x8001edf4u)
      first_team = true;
    if (a.pc == 0x8006876cu && a.value == 0x8001eeb8u)
      second_team = true;
  }
  check(first_team && second_team);

  Fixture mapping;
  mapping.enableActor(0, 0);
  mapping.put(0x8001edf4u + 0x14u, 0x4321, 2);
  check(mapping.run() == NBA97_TEXT_COMPLETE &&
        mapping.prefixes[1].registers.gpr[5].word == 0x80040047u);
}

void countdown_state_edges_and_b6() {
  for (auto value : {0u, 1u, 0x8000u, 0xffffu}) {
    Fixture f;
    f.put(0x80021d82u, 1, 1);
    f.put(0x800fdb8au, value, 2);
    check(f.run() == NBA97_TEXT_COMPLETE);
    check(f.get(0x800fdb8au, 2) == (value ? ((value - 1u) & 0xffffu) : 0));
  }
  Fixture disabled;
  disabled.put(0x80021d82u, 0, 1);
  disabled.put(0x800fdb8au, 7, 2);
  check(disabled.run() == NBA97_TEXT_COMPLETE &&
        disabled.get(0x800fdb8au, 2) == 7);

  for (auto state : {6u, 7u, 20u}) {
    Fixture f;
    f.enableActor(0, state);
    check(f.run() == NBA97_TEXT_COMPLETE &&
          f.events[5].entry == ActionEntries[state]);
  }
  for (auto state : {21u, 255u}) {
    Fixture f;
    f.enableActor(0, state);
    check(f.run() == NBA97_TEXT_COMPLETE && f.events.size() == 14);
  }

  for (auto b6 : {0u, 1u, 0x7fffu, 0x8000u, 0xffffu}) {
    Fixture f;
    f.enableActor(0, 0);
    f.put(Actor0 + 0x46u, 0, 2);
    f.put(Actor0 + 0x64u, 1, 2);
    f.put(Actor0 + 0xb6u, b6, 2);
    check(f.run() == NBA97_TEXT_COMPLETE);
    check(f.get(Actor0 + 0xb6u, 2) ==
          ((b6 > 0 && b6 < 0x8000u) ? ((b6 - 3u) & 0xffffu) : b6));
  }
}

unsigned calls_to(const Fixture &fixture, std::uint32_t entry) {
  unsigned result = 0;
  for (const auto &event : fixture.events)
    result += event.entry == entry;
  return result;
}

void phase_motion_and_special_gates() {
  Fixture phase_low;
  phase_low.enableActor(0, 0);
  phase_low.put(Actor0 + 0x46u, 0, 2);
  check(phase_low.run() == NBA97_TEXT_COMPLETE &&
        calls_to(phase_low, 0x8006fac4u) == 1);
  check(phase_low.events[5].pc == 0x8006891cu &&
        phase_low.events[5].argument_count == 2 &&
        phase_low.prefixes[5].registers.gpr[4].word == Actor0 &&
        phase_low.prefixes[5].registers.gpr[5].word == 6);

  Fixture phase_side;
  phase_side.enableActor(0, 0);
  phase_side.put(Actor0 + 0x46u, 0, 2);
  phase_side.put(0x800fdb90u, 0x80, 2);
  phase_side.put(Actor0 + 0xc2u, 9, 2);
  check(phase_side.run() == NBA97_TEXT_COMPLETE &&
        calls_to(phase_side, 0x8006fac4u) == 1 &&
        phase_side.prefixes[5].registers.gpr[5].word == 8);

  Fixture motion;
  motion.enableActor(0, 0);
  check(motion.run() == NBA97_TEXT_COMPLETE &&
        calls_to(motion, 0x8006fac4u) == 0);
  Fixture repeat;
  repeat.enableActor(0, 0);
  repeat.put(Actor0 + 0x46u, 0, 2);
  repeat.put(Actor0 + 0xe6u, 1, 2);
  check(repeat.run() == NBA97_TEXT_COMPLETE &&
        calls_to(repeat, 0x8006fac4u) == 0);
  Fixture flag;
  flag.enableActor(0, 0);
  flag.put(Actor0 + 0x46u, 0, 2);
  flag.put(Actor0 + 0xdau, 4, 1);
  check(flag.run() == NBA97_TEXT_COMPLETE && calls_to(flag, 0x8006fac4u) == 0);

  Fixture special;
  special.enableActor(0, 0);
  special.put(0x800fe8ccu, 10, 2);
  special.put(0x800fe8cau, 0, 2);
  special.put(Actor0 + 0x46u, 0, 2);
  special.put(Actor0 + 0x64u, 1, 2);
  special.put(Actor0 + 0xb6u, 7, 2);
  check(special.run() == NBA97_TEXT_COMPLETE &&
        special.get(Actor0 + 0xb6u, 2) == 7);

  Fixture live_table;
  live_table.enableActor(0, 3);
  live_table.mode = Fixture::MutateTableAtInput;
  check(live_table.run() == NBA97_TEXT_COMPLETE &&
        live_table.events[5].pc == ActionPcs[20] &&
        live_table.events[5].entry == ActionEntries[20]);
}

void indirect_failures_unknown_delay_and_prefixes() {
  Fixture unknown;
  unknown.enableActor(0, 0);
  unknown.put(ActionTable, ActionPcs[0], 4, 7);
  check(unknown.run() == NBA97_TEXT_UNKNOWN &&
        unknown.progress.stopped_pc == 0x80068a44u &&
        unknown.progress.instruction_count > 0);

  Fixture unaligned;
  unaligned.enableActor(0, 0);
  unaligned.put(ActionTable, 0x800670a9u, 4);
  check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x80068a44u &&
        unaligned.progress.stopped_address == 0x800670a9u);

  Fixture unsupported;
  unsupported.enableActor(0, 0);
  unsupported.put(ActionTable, 0x80060000u, 4);
  check(unsupported.run() == NBA97_TEXT_RESOURCE &&
        unsupported.progress.stopped_address == 0x80060000u);

  Fixture compare;
  compare.enableActor(0, 0);
  compare.put(Actor0 + 0x1au, 0, 1, 0);
  const auto compare_result = compare.run();
  check(compare_result == NBA97_TEXT_UNKNOWN &&
        compare.progress.stopped_pc == 0x800689a8u &&
        compare.progress.machine.registers.gpr[2].known_mask == 0x0e &&
        compare.progress.instruction_count == 102);

  Fixture complete;
  complete.enableActor(0, 0);
  check(complete.run() == NBA97_TEXT_COMPLETE);
  const auto operations = complete.progress.operations;
  for (std::size_t budget = 0; budget < operations; ++budget) {
    Fixture prefix;
    prefix.enableActor(0, 0);
    prefix.context.operation_budget = budget;
    check(prefix.run() == NBA97_TEXT_LIMIT &&
          prefix.progress.operations == budget);
  }
  Fixture exact;
  exact.enableActor(0, 0);
  exact.context.operation_budget = operations;
  check(exact.run() == NBA97_TEXT_COMPLETE &&
        exact.progress.operations == operations);
}

void callback_machine_mutation_and_metadata() {
  Fixture refused;
  refused.enableActor(0, 0);
  refused.mode = Fixture::RefuseFirst;
  check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.progress.stopped_pc == 0x800687b0u &&
        refused.progress.stopped_entry == 0x8008f224u &&
        refused.progress.machine.hi.word == 0x24681357u &&
        refused.progress.machine.lo.known_mask == 9);
  for (unsigned i = 1; i < 32; ++i)
    check(refused.progress.machine.registers.gpr[i].word ==
              0x60000000u + i * 0x00010101u &&
          refused.progress.machine.registers.gpr[i].known_mask == (i & 0x0fu));

  Fixture invalid;
  invalid.enableActor(0, 0);
  invalid.mode = Fixture::InvalidMachine;
  check(invalid.run() == NBA97_TEXT_ARGUMENT &&
        invalid.progress.machine.lo.known_mask == 0x10);

  Fixture mutated;
  mutated.enableActor(0, 0);
  mutated.mode = Fixture::MutateLate;
  check(mutated.run() == NBA97_TEXT_COMPLETE && mutated.events.size() == 6 &&
        mutated.progress.frame_stack_pointer == FrameSp &&
        mutated.progress.machine.registers.gpr[29].word == 0x800fee48u &&
        mutated.progress.restored_return_address.word == 0x82345678u &&
        mutated.get(0x80041028u, 2) == 1);

  Fixture bad;
  bad.incoming.registers.gpr[0] = {1, 15};
  check(bad.run() == NBA97_TEXT_ARGUMENT);
  Fixture metadata;
  metadata.context.access_journal = nullptr;
  check(metadata.run() == NBA97_TEXT_ARGUMENT);
  Fixture unknown_sp;
  unknown_sp.incoming.registers.gpr[29].known_mask = 7;
  check(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stopped_pc == 0x800686c4u &&
        unknown_sp.progress.stores == 0);
  Fixture unaligned_sp;
  unaligned_sp.incoming.registers.gpr[29].word += 1;
  check(unaligned_sp.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_sp.progress.stopped_pc == 0x800686c4u);
  Fixture invalid_known;
  invalid_known.known[invalid_known.offset(0x80021d82u)] = 2;
  check(invalid_known.run() == NBA97_TEXT_ARGUMENT &&
        invalid_known.progress.stopped_pc == 0x800686bcu);
  check(nba97_game_actor_input(nullptr, &metadata.progress) ==
        NBA97_TEXT_ARGUMENT);
  check(nba97_game_actor_input(&metadata.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
}

void unknown_delay_publication() {
  Fixture option;
  option.put(0x80021d82u,0,1,0);
  check(option.run()==NBA97_TEXT_UNKNOWN && option.progress.stopped_pc==0x800686e8u
        && option.progress.stores==10 && option.progress.operations==11);
  check(option.journal[10].pc==0x800686ecu && option.journal[10].address==FrameSp+0x20u
        && option.journal[10].value==option.incoming.registers.gpr[16].word);
  Fixture claim;
  claim.put(Actor0+4u,0xffffu,2,1);
  check(claim.run()==NBA97_TEXT_UNKNOWN && claim.progress.stopped_pc==0x80068778u
        && claim.progress.machine.registers.gpr[2].word==0xfffffffcu
        && claim.progress.machine.registers.gpr[2].known_mask==1);
  Fixture raw;
  raw.region.known=nullptr;
  raw.incoming.registers.gpr[31].known_mask=7;
  check(raw.run()==NBA97_TEXT_ARGUMENT && raw.progress.stopped_pc==0x800686c4u
        && raw.progress.stores==0 && raw.get(FrameSp+0x44u,4)==0);
}

void saved_register_and_ra_knownness() {
  constexpr std::array<unsigned, 9> saved{{16, 17, 18, 19, 20, 21, 22, 23, 30}};
  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture f;
    for (auto reg : saved)
      f.incoming.registers.gpr[reg] = {0x71000000u + reg * 0x00010101u,
                                       static_cast<std::uint8_t>(mask)};
    f.incoming.hi = {0x10203040u, static_cast<std::uint8_t>(mask)};
    f.incoming.lo = {0x50607080u, static_cast<std::uint8_t>(15u - mask)};
    check(f.run() == NBA97_TEXT_COMPLETE);
    for (auto reg : saved)
      check(f.progress.machine.registers.gpr[reg].word ==
                f.incoming.registers.gpr[reg].word &&
            f.progress.machine.registers.gpr[reg].known_mask == mask);
    check(f.progress.machine.hi.known_mask == mask &&
          f.progress.machine.lo.known_mask == 15u - mask);

    Fixture ra;
    ra.incoming.registers.gpr[31] = {0x81234568u,
                                     static_cast<std::uint8_t>(mask)};
    check(ra.run() == (mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN));
    check(ra.progress.stopped_pc == (mask == 15 ? 0u : 0x80068bf0u) &&
          ra.progress.machine.registers.gpr[31].known_mask == mask);
  }
}

void wrapping_stack_and_unmapped_table() {
  Fixture source;
  source.incoming.registers.gpr[29] = {0x10u, 0x0f};
  std::array<std::uint8_t, 0x38> high{};
  std::array<std::uint8_t, 0x38> high_known{};
  std::array<std::uint8_t, 0x10> low{};
  std::array<std::uint8_t, 0x10> low_known{};
  high_known.fill(1);
  low_known.fill(1);
  std::array<Nba97GameTextRegion, 3> regions{{
      source.region,
      {0xffffffc8u, high.data(), high_known.data(), high.size()},
      {0u, low.data(), low_known.data(), low.size()},
  }};
  source.context.memory = {regions.data(), regions.size()};
  check(source.run() == NBA97_TEXT_COMPLETE &&
        source.progress.frame_stack_pointer == 0xffffffc8u &&
        source.progress.machine.registers.gpr[29].word == 0x10u &&
        source.progress.restored_return_address.word == 0x81234568u);

  Fixture missing;
  missing.enableActor(0, 0);
  std::array<Nba97GameTextRegion, 2> split{{
      {Ram, missing.bytes.data(), missing.known.data(),
       static_cast<std::size_t>(ActionTable - Ram)},
      {ActionTable + 4u, missing.bytes.data() + (ActionTable + 4u - Ram),
       missing.known.data() + (ActionTable + 4u - Ram),
       missing.bytes.size() - (ActionTable + 4u - Ram)},
  }};
  missing.context.memory = {split.data(), split.size()};
  check(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc == 0x80068a3cu &&
        missing.progress.stopped_address == ActionTable);
}
} // namespace

int main() {
  all_runtime_action_cases_and_arguments();
  teams_mapping_dedup_and_live_globals();
  countdown_state_edges_and_b6();
  phase_motion_and_special_gates();
  indirect_failures_unknown_delay_and_prefixes();
  callback_machine_mutation_and_metadata();
  unknown_delay_publication();
  saved_register_and_ra_knownness();
  wrapping_stack_and_unmapped_table();
  std::printf("game actor input: %u checks\n", checks);
}
