#include "game_actor_collision_response_adapter.h"

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
    std::fprintf(stderr, "actor collision response check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t First = 0x80010000u;
constexpr std::uint32_t Second = 0x80010200u;
constexpr std::uint32_t FirstDescriptor = 0x80010400u;
constexpr std::uint32_t SecondDescriptor = 0x80010500u;
constexpr std::uint32_t AlternateFirst = 0x80010800u;
constexpr std::uint32_t AlternateSecond = 0x80010a00u;
constexpr std::uint32_t EntrySp = 0x800ff000u;

struct Fixture {
  enum Mode {
    Ordinary,
    RefuseResolve,
    InvalidResolve,
    UnknownDistance
  } mode = Ordinary;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x100000u, 0);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x100000u, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameActorCollisionResponseAccess, 256> journal{};
  Nba97GameActorCollisionResponseContext context{};
  Nba97GameActorCollisionResponseProgress progress{};
  Nba97GameActorCollisionResponseGeometryBinding geometry{};
  std::array<Nba97GameActorCollisionResponseEvent, 16> events{};
  std::array<Nba97GameActorCollisionResponseMachine, 16> machines{};
  unsigned calls{};
  std::uint16_t angle_output{};
  std::uint32_t refuse_pc{};
  std::uint32_t geometry_result{256};
  std::uint8_t geometry_mask{15};
  bool zero_factors{};
  bool relocate_machine{};
  bool partial_multiply_stop{};

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {
          0x21000000u + i * 0x01010101u,
          static_cast<std::uint8_t>((i % 15u) + 1u)};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[4] = {First, 15};
    context.machine.registers.gpr[5] = {Second, 15};
    context.machine.registers.gpr[29] = {EntrySp, 15};
    context.machine.registers.gpr[31] = {0x81234568u, 15};
    context.machine.hi = {0x13572468u, 5};
    context.machine.lo = {0x89abcdefu, 10};
    context.memory = {&region, 1};
    context.operation_budget = 1000;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    nba97_game_actor_collision_response_geometry_binding_init(&geometry,
                                                              fallback, this);
    context.io = nba97_game_actor_collision_response_geometry_child;
    context.user = &geometry;
    put(First, 0x12345678u, 4);
    put(Second, 0xabcdef09u, 4);
    put(First + 8u, 0x1000, 4);
    put(Second + 8u, 0x0f00, 4);
    put(First + 0x0cu, 0x2000, 4);
    put(Second + 0x0cu, 0x2000, 4);
    put(First + 0x14u, 0, 2);
    put(Second + 0x14u, 64, 2);
    put(First + 0x16u, 0, 2);
    put(Second + 0x16u, 0, 2);
    put(First + 0x20u, FirstDescriptor, 4);
    put(Second + 0x20u, SecondDescriptor, 4);
    put(FirstDescriptor + 0x0au, 0, 1);
    put(SecondDescriptor + 0x0au, 0xf8, 1);
    put(0x800b8324u, 3, 2);
    put(0x800b8362u, 0xfffdu, 2);
    put(First + 0xdau, 1, 1);
    put(First + 0xe6u, 14, 2);
    put(Second + 0xe6u, 14, 2);
  }
  std::size_t offset(std::uint32_t address) const {
    return static_cast<std::size_t>(address - Ram);
  }
  void put(std::uint32_t address, std::uint32_t value, unsigned width,
           std::uint8_t mask = 15) {
    const auto at = offset(address);
    for (unsigned i = 0; i < width; ++i) {
      bytes[at + i] = static_cast<std::uint8_t>(value >> (8u * i));
      known[at + i] = static_cast<std::uint8_t>((mask >> i) & 1u);
    }
  }
  std::uint32_t get(std::uint32_t address, unsigned width) const {
    std::uint32_t value = 0;
    const auto at = offset(address);
    for (unsigned i = 0; i < width; ++i)
      value |= std::uint32_t(bytes[at + i]) << (8u * i);
    return value;
  }
  static int fallback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameActorCollisionResponseEvent *event,
                      Nba97GameActorCollisionResponseMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    const unsigned index = f.calls++;
    f.events[index] = *event;
    f.machines[index] = *machine;
    if (f.refuse_pc == event->pc)
      return 0;
    if (event->kind == NBA97_GAME_ACTOR_COLLISION_RESPONSE_RESOLVE_8005EA28) {
      if (f.mode == RefuseResolve)
        return 0;
      if (f.mode == InvalidResolve) {
        machine->hi.known_mask = 16;
        return 1;
      }
      const unsigned factor = f.zero_factors ? 0 : 1;
      f.put(machine->registers.gpr[4].word + 0xe2u, factor, 2);
      f.put(machine->registers.gpr[5].word + 0xe2u, factor, 2);
      if (f.relocate_machine) {
        machine->registers.gpr[16] = {AlternateFirst, 15};
        machine->registers.gpr[17] = {AlternateSecond, 15};
        machine->registers.gpr[29] = {0x800fee00u, 15};
        machine->hi = {0x10203040u, 6};
        machine->lo = {0xa0b0c0d0u, 9};
        f.put(AlternateFirst, 0x111111aau, 4);
        f.put(AlternateSecond, 0x222222bbu, 4);
        f.put(AlternateFirst + 0xe2u, 1, 2);
        f.put(AlternateSecond + 0xe2u, 1, 2);
        f.put(AlternateFirst + 0xe6u, 14, 2);
        f.put(AlternateSecond + 0xe6u, 14, 2);
        f.put(AlternateFirst + 0xdau, 1, 1);
        for (unsigned reg = 16; reg <= 23; ++reg)
          f.put(0x800fee00u + 0x30u + (reg - 16u) * 4u, 0x70000000u + reg, 4);
        f.put(0x800fee50u, 0x7000001eu, 4);
        f.put(0x800fee54u, 0x82345678u, 4);
      }
      if (f.partial_multiply_stop) {
        machine->registers.gpr[18] = {0x1234u, 1};
        f.put(machine->registers.gpr[5].word + 0xe2u, 1, 2, 2);
      }
    }
    if (event->kind == NBA97_GAME_ACTOR_COLLISION_RESPONSE_ANGLE_800706E4)
      f.put(machine->registers.gpr[6].word, f.angle_output, 2);
    return 1;
  }
  static int custom_geometry(void *opaque, const Nba97GameTextMemory *,
                             const Nba97GameActorCollisionResponseEvent *event,
                             Nba97GameActorCollisionResponseMachine *machine) {
    if (event->kind != NBA97_GAME_ACTOR_COLLISION_RESPONSE_GEOMETRY_8007066C)
      return 0;
    auto &f = *static_cast<Fixture *>(opaque);
    machine->registers.gpr[2] = {f.geometry_result, f.geometry_mask};
    return 1;
  }
  int run() { return nba97_game_actor_collision_response(&context, &progress); }
};

void positive_and_rejected_geometry() {
  Fixture success;
  check(success.run() == NBA97_TEXT_COMPLETE && success.progress.completed);
  check(success.progress.stopped_pc == 0 &&
        success.progress.instruction_count > 0);
  check(success.geometry.geometry_invocations == 1 && success.calls == 1);
  check(success.events[0].pc == 0x8005f598u &&
        success.events[0].delay_slot_pc == 0x8005f59cu &&
        success.events[0].entry == 0x8005ea28u &&
        success.events[0].argument_count == 8);
  check(success.machines[0].registers.gpr[4].word == First &&
        success.machines[0].registers.gpr[5].word == Second &&
        success.machines[0].registers.gpr[6].word == 3u &&
        success.machines[0].registers.gpr[7].word == 0xfffffffdu);
  const auto stack = success.machines[0].registers.gpr[29].word;
  check(success.get(stack + 0x10u, 4) == 0x100u &&
        success.get(stack + 0x14u, 4) == 0 &&
        success.get(stack + 0x18u, 4) == 64 &&
        success.get(stack + 0x1cu, 4) == 64);
  check(success.get(First + 0xdcu, 1) == 9u &&
        success.get(Second + 0xdcu, 1) == 0x78u &&
        success.get(0x800fdb88u, 2) == 1u &&
        success.progress.returned_value.word == 1u);
  check(success.progress.normal_x.word == 256u &&
        success.progress.normal_y.word == 0u &&
        success.progress.normal_velocity.word == 64u &&
        success.progress.tangent_velocity.word == 0u);

  Fixture zero;
  zero.put(First + 8u, 0x0f00, 4);
  check(zero.run() == NBA97_TEXT_COMPLETE && zero.calls == 0 &&
        zero.progress.returned_value.word == 0 &&
        zero.progress.stopped_pc == 0);
  Fixture negative;
  negative.context.io = Fixture::custom_geometry;
  negative.context.user = &negative;
  negative.geometry_result = UINT32_MAX;
  check(negative.run() == NBA97_TEXT_COMPLETE &&
        negative.progress.returned_value.word == 0 &&
        negative.progress.stopped_pc == 0 && negative.calls == 0);

  Fixture unknown;
  unknown.context.io = Fixture::custom_geometry;
  unknown.context.user = &unknown;
  unknown.geometry_mask = 14;
  check(unknown.run() == NBA97_TEXT_UNKNOWN &&
        unknown.progress.stopped_pc == 0x8005f4c0u &&
        unknown.progress.machine.lo.known_mask == 15);
}

void call_prefixes_and_failures() {
  Fixture refused;
  refused.mode = Fixture::RefuseResolve;
  check(refused.run() == NBA97_TEXT_IO_REFUSED && refused.calls == 1 &&
        refused.progress.stopped_pc == 0x8005f598u &&
        refused.progress.stopped_entry == 0x8005ea28u &&
        refused.progress.machine.registers.gpr[31].word == 0x8005f5a0u);
  Fixture invalid;
  invalid.mode = Fixture::InvalidResolve;
  check(invalid.run() == NBA97_TEXT_ARGUMENT &&
        invalid.progress.machine.hi.known_mask == 16u);

  Fixture metadata;
  metadata.context.io = nullptr;
  metadata.context.user = nullptr;
  check(metadata.run() == NBA97_TEXT_IO_REFUSED &&
        metadata.progress.stopped_pc == 0x8005f424u &&
        metadata.progress.stopped_entry == 0x8007066cu &&
        metadata.progress.machine.registers.gpr[31].word == 0x8005f42cu &&
        metadata.progress.machine.registers.gpr[19].word == 0u);
}

void all_twelve_calls_and_velocity_routes() {
  Fixture f;
  f.put(First + 0xdau, 0, 1);
  f.put(First + 0xe6u, 0, 2);
  f.put(Second + 0xe6u, 0, 2);
  for (auto actor : {First, Second}) {
    f.put(actor + 0x1au, 20, 1);
    f.put(actor + 0x46u, 0x50, 2);
    f.put(actor + 0x50u, 0, 2);
    f.put(actor + 0xa8u, 0, 2);
  }
  check(f.run() == NBA97_TEXT_COMPLETE &&
        f.geometry.geometry_invocations == 1 && f.calls == 11);
  constexpr std::array<std::uint32_t, 11> pcs{{
      0x8005f598u,
      0x8005f68cu,
      0x8005f6ccu,
      0x8005f6dcu,
      0x8005f6ecu,
      0x8005f6fcu,
      0x8005f7bcu,
      0x8005f7fcu,
      0x8005f80cu,
      0x8005f81cu,
      0x8005f82cu,
  }};
  constexpr std::array<unsigned, 11> args{{8, 3, 2, 3, 3, 3, 3, 2, 3, 3, 3}};
  for (unsigned i = 0; i < pcs.size(); ++i) {
    check(f.events[i].pc == pcs[i] &&
          f.events[i].delay_slot_pc == pcs[i] + 4u &&
          f.events[i].argument_count == args[i] &&
          f.machines[i].registers.gpr[31].word == pcs[i] + 8u);
  }
  check(f.get(Second + 0x14u, 2) != 64u && f.get(First + 0x14u, 2) != 0u);
  for (auto actor : {First, Second})
    check(f.get(actor + 0x48u, 2) == 0xffffu &&
          f.get(actor + 0x4cu, 2) == 0xffffu && f.get(actor + 0x60u, 2) == 0 &&
          f.get(actor + 0x64u, 2) == 0);

  struct ClampCase {
    std::int16_t relative_y;
    std::uint32_t tangent;
  };
  constexpr std::array<ClampCase, 6> clamps{{
      {64, 0xffffffc0u},
      {63, 0xffffffc0u},
      {0, 64u},
      {-63, 64u},
      {-64, 64u},
      {-65, 65u},
  }};
  for (const auto &test : clamps) {
    Fixture clamp;
    clamp.put(Second + 0x16u, static_cast<std::uint16_t>(test.relative_y), 2);
    check(clamp.run() == NBA97_TEXT_COMPLETE && clamp.calls == 1);
    const auto callback_sp = clamp.machines[0].registers.gpr[29].word;
    check(clamp.get(callback_sp + 0x1cu, 4) == test.tangent);
  }

  struct AngleCase {
    std::uint16_t output;
    unsigned window;
    unsigned calls;
  };
  constexpr std::array<AngleCase, 4> angles{{
      {256, 0, 6},
      {56, 200, 6},
      {55, 201, 6},
      {257, 0x3ff, 2},
  }};
  for (const auto &test : angles) {
    Fixture angle;
    angle.put(Second + 0xe6u, 0, 2);
    angle.put(Second + 0x1au, 20, 1);
    angle.put(Second + 0x46u, 0x50, 2);
    angle.put(Second + 0x50u, 0, 2);
    angle.put(Second + 0xa8u, 0, 2);
    angle.angle_output = test.output;
    check(angle.run() == NBA97_TEXT_COMPLETE && angle.calls == test.calls);
    check(((0u - test.output + 0x100u) & 0x3ffu) == test.window);
  }
}

void callback_refusals_and_live_machine_aliases() {
  constexpr std::array<std::uint32_t, 11> callback_pcs{{
      0x8005f598u,
      0x8005f68cu,
      0x8005f6ccu,
      0x8005f6dcu,
      0x8005f6ecu,
      0x8005f6fcu,
      0x8005f7bcu,
      0x8005f7fcu,
      0x8005f80cu,
      0x8005f81cu,
      0x8005f82cu,
  }};
  for (auto pc : callback_pcs) {
    Fixture refused;
    refused.put(First + 0xdau, 0, 1);
    refused.put(First + 0xe6u, 0, 2);
    refused.put(Second + 0xe6u, 0, 2);
    for (auto actor : {First, Second}) {
      refused.put(actor + 0x1au, 20, 1);
      refused.put(actor + 0x46u, 0x50, 2);
      refused.put(actor + 0x50u, 0, 2);
      refused.put(actor + 0xa8u, 0, 2);
    }
    refused.refuse_pc = pc;
    check(refused.run() == NBA97_TEXT_IO_REFUSED &&
          refused.progress.stopped_pc == pc &&
          refused.progress.stopped_entry != 0 &&
          refused.progress.machine.registers.gpr[31].word == pc + 8u);
  }

  Fixture moved;
  moved.relocate_machine = true;
  check(moved.run() == NBA97_TEXT_COMPLETE && moved.progress.completed);
  check(moved.get(AlternateFirst + 0xdcu, 1) == 0xbbu &&
        moved.get(AlternateSecond + 0xdcu, 1) == 0xaau);
  check(moved.progress.machine.registers.gpr[29].word == 0x800fee58u &&
        moved.progress.restored_return_address.word == 0x82345678u &&
        moved.progress.machine.registers.gpr[16].word == 0x70000010u &&
        moved.progress.machine.registers.gpr[17].word == 0x70000011u);
  check(moved.progress.machine.hi.word == 0x10203040u &&
        moved.progress.machine.hi.known_mask == 6 &&
        moved.progress.machine.lo.word == 0xa0b0c0d0u &&
        moved.progress.machine.lo.known_mask == 9);
}

void factor_quirk_and_actor_gates() {
  Fixture both_zero;
  both_zero.zero_factors = true;
  check(both_zero.run() == NBA97_TEXT_COMPLETE && both_zero.calls == 1);
  bool read_first_height = false;
  bool read_second_height = false;
  for (unsigned i = 0; i < both_zero.progress.access_events; ++i) {
    read_first_height |= both_zero.journal[i].pc == 0x8005f5c0u;
    read_second_height |= both_zero.journal[i].pc == 0x8005f5d0u;
  }
  check(read_first_height && read_second_height);

  Fixture first_height;
  first_height.zero_factors = true;
  first_height.put(First + 0x10u, 1, 4);
  check(first_height.run() == NBA97_TEXT_COMPLETE);
  read_first_height = false;
  read_second_height = false;
  for (unsigned i = 0; i < first_height.progress.access_events; ++i) {
    read_first_height |= first_height.journal[i].pc == 0x8005f5c0u;
    read_second_height |= first_height.journal[i].pc == 0x8005f5d0u;
  }
  check(read_first_height && !read_second_height &&
        first_height.get(First + 0xe2u, 2) == 0 &&
        first_height.get(Second + 0xe2u, 2) == 0);

  Fixture state_gate;
  state_gate.put(Second + 0xe6u, 0, 2);
  state_gate.put(Second + 0x1au, 19, 1);
  check(state_gate.run() == NBA97_TEXT_COMPLETE && state_gate.calls == 1 &&
        state_gate.get(Second + 0xe6u, 2) == 14);
  Fixture motion_gate;
  motion_gate.put(Second + 0xe6u, 0, 2);
  motion_gate.put(Second + 0x1au, 20, 1);
  motion_gate.put(Second + 0x46u, 0x4f, 2);
  check(motion_gate.run() == NBA97_TEXT_COMPLETE && motion_gate.calls == 1 &&
        motion_gate.get(Second + 0xe6u, 2) == 0);
  Fixture da_gate;
  da_gate.put(First + 0xe6u, 0, 2);
  da_gate.put(First + 0xdau, 1, 1);
  check(da_gate.run() == NBA97_TEXT_COMPLETE &&
        da_gate.get(First + 0x14u, 2) == 0);
  Fixture da_pass;
  da_pass.put(First + 0xe6u, 0, 2);
  da_pass.put(First + 0xdau, 0, 1);
  check(da_pass.run() == NBA97_TEXT_COMPLETE &&
        da_pass.get(First + 0x14u, 2) != 0);
}

void partial_multiply_and_divide_knownness() {
  Fixture zero_dividend;
  zero_dividend.context.io = Fixture::custom_geometry;
  zero_dividend.context.user = &zero_dividend;
  zero_dividend.geometry_result = 0x100;
  zero_dividend.geometry_mask = 14;
  zero_dividend.put(First + 8u, 0x1000u, 4);
  zero_dividend.put(Second + 8u, 0x1000u, 4);
  zero_dividend.put(First + 0x0cu, 0x2100u, 4);
  zero_dividend.put(Second + 0x0cu, 0x2000u, 4);
  check(zero_dividend.run() == NBA97_TEXT_COMPLETE &&
        zero_dividend.progress.returned_value.word == 0 &&
        zero_dividend.progress.normal_x.word == 0 &&
        zero_dividend.progress.normal_x.known_mask == 15);

  Fixture divide_prefix;
  divide_prefix.context.io = Fixture::custom_geometry;
  divide_prefix.context.user = &divide_prefix;
  divide_prefix.geometry_result = 1;
  divide_prefix.put(First + 8u, 0x1001u, 4, 1);
  divide_prefix.put(Second + 8u, 0, 4);
  check(divide_prefix.run() == NBA97_TEXT_UNKNOWN &&
        divide_prefix.progress.normal_x.word == 0x100100u &&
        divide_prefix.progress.normal_x.known_mask == 3);

  Fixture multiply_prefix;
  multiply_prefix.put(First + 0x0cu, 0x2001u, 4);
  multiply_prefix.put(Second + 0x0cu, 0x2000u, 4);
  multiply_prefix.put(Second + 0xe6u, 0, 2);
  multiply_prefix.partial_multiply_stop = true;
  const int multiply_result = multiply_prefix.run();
  check(multiply_result == NBA97_TEXT_UNKNOWN);
  check(multiply_prefix.progress.stopped_pc == 0x8005f5f8u);
  check(multiply_prefix.progress.machine.lo.word == 0x1234u);
  check((multiply_prefix.progress.machine.lo.known_mask & 1u) == 1u);
}

void budgets_mapping_knownness_and_wrapping_stack() {
  Fixture complete;
  check(complete.run() == NBA97_TEXT_COMPLETE);
  const auto operations = complete.progress.operations;
  for (std::size_t budget = 0; budget < operations; ++budget) {
    Fixture prefix;
    prefix.context.operation_budget = budget;
    check(prefix.run() == NBA97_TEXT_LIMIT &&
          prefix.progress.operations == budget);
  }
  Fixture unknown_sp;
  unknown_sp.context.machine.registers.gpr[29].known_mask = 7;
  check(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stopped_pc == 0x8005f3c0u);
  Fixture unaligned;
  ++unaligned.context.machine.registers.gpr[29].word;
  check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x8005f3c0u);
  Fixture missing;
  missing.region.size = 0x100u;
  check(missing.run() == NBA97_TEXT_RESOURCE);
  Fixture bad_byte;
  bad_byte.known[bad_byte.offset(First + 8u)] = 2;
  check(bad_byte.run() == NBA97_TEXT_ARGUMENT &&
        bad_byte.progress.stopped_pc == 0x8005f3f0u);
  Fixture overlap;
  std::array<Nba97GameTextRegion, 2> regions{{
      overlap.region,
      {Ram + 4u, overlap.bytes.data() + 4u, overlap.known.data() + 4u, 8},
  }};
  overlap.context.memory = {regions.data(), regions.size()};
  check(overlap.run() == NBA97_TEXT_ARGUMENT);
  check(nba97_game_actor_collision_response(nullptr, &overlap.progress) ==
        NBA97_TEXT_ARGUMENT);
  check(nba97_game_actor_collision_response(&overlap.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);

  Fixture no_known;
  no_known.region.known = nullptr;
  no_known.context.machine.registers.gpr[16].known_mask = 7;
  const auto frame_address = EntrySp - 0x58u + 0x30u;
  const auto frame_before = no_known.get(frame_address, 4);
  check(no_known.run() == NBA97_TEXT_ARGUMENT &&
        no_known.get(frame_address, 4) == frame_before &&
        no_known.progress.stores == 0);

  Fixture wrap;
  wrap.context.machine.registers.gpr[29] = {0x40u, 15};
  std::array<std::uint8_t, 24> high{};
  std::array<std::uint8_t, 24> high_known{};
  std::array<std::uint8_t, 64> low{};
  std::array<std::uint8_t, 64> low_known{};
  high_known.fill(1);
  low_known.fill(1);
  std::array<Nba97GameTextRegion, 3> wrap_regions{{
      wrap.region,
      {0xffffffe8u, high.data(), high_known.data(), high.size()},
      {0, low.data(), low_known.data(), low.size()},
  }};
  wrap.context.memory = {wrap_regions.data(), wrap_regions.size()};
  check(wrap.run() == NBA97_TEXT_COMPLETE &&
        wrap.progress.frame_stack_pointer == 0xffffffe8u &&
        wrap.progress.machine.registers.gpr[29].word == 0x40u &&
        wrap.progress.restored_return_address.word == 0x81234568u);

  Fixture unknown_ra;
  unknown_ra.context.machine.registers.gpr[31].known_mask = 7;
  check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.stopped_pc == 0x8005f880u &&
        unknown_ra.progress.instruction_count ==
            complete.progress.instruction_count);
}

void full_machine_and_geometry_adapter_guards() {
  Fixture f;
  Nba97GameActorCollisionResponseEvent event{};
  event.pc = 0x8005f424u;
  event.delay_slot_pc = 0x8005f428u;
  event.entry = 0x8007066cu;
  event.kind = NBA97_GAME_ACTOR_COLLISION_RESPONSE_GEOMETRY_8007066C;
  event.argument_count = 2;
  auto machine = f.context.machine;
  machine.registers.gpr[4] = {0xffffff00u, 15};
  machine.registers.gpr[5] = {0x100u, 15};
  machine.registers.gpr[31] = {0x8005f42cu, 15};
  check(nba97_game_actor_collision_response_geometry_child(
            &f.geometry, &f.context.memory, &event, &machine) == 1 &&
        machine.registers.gpr[4].word == 0x100u &&
        machine.registers.gpr[5].word == 0x100u &&
        machine.registers.gpr[2].word > 0u);
  event.pc ^= 4u;
  const auto before = machine;
  check(!nba97_game_actor_collision_response_geometry_child(
            &f.geometry, &f.context.memory, &event, &machine) &&
        machine.registers.gpr[2].word == before.registers.gpr[2].word);
  event.pc ^= 4u;
  machine.registers.gpr[4].known_mask = 7;
  check(!nba97_game_actor_collision_response_geometry_child(
      &f.geometry, &f.context.memory, &event, &machine));
}
} // namespace

int main() {
  positive_and_rejected_geometry();
  call_prefixes_and_failures();
  all_twelve_calls_and_velocity_routes();
  callback_refusals_and_live_machine_aliases();
  factor_quirk_and_actor_gates();
  partial_multiply_and_divide_knownness();
  budgets_mapping_knownness_and_wrapping_stack();
  full_machine_and_geometry_adapter_guards();
  std::printf("game actor collision response: %u checks\n", checks);
}
