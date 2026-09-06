#include "game_draw_packet_adapter.h"
#include "recovered/game_bios_memory_copy.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

size_t checks;

void check(bool condition, const char *message) {
  ++checks;
  if (!condition) {
    std::fprintf(stderr, "game_draw_packet_integration_tests: %s\n", message);
    std::exit(1);
  }
}

void write16(uint8_t *bytes, size_t offset, uint16_t value) {
  bytes[offset] = static_cast<uint8_t>(value);
  bytes[offset + 1] = static_cast<uint8_t>(value >> 8u);
}

void write32(uint8_t *bytes, size_t offset, uint32_t value) {
  for (unsigned byte = 0u; byte != 4u; ++byte)
    bytes[offset + byte] = static_cast<uint8_t>(value >> (8u * byte));
}

struct NaturalFixture {
  static constexpr uint32_t base = UINT32_C(0x80000000);
  static constexpr size_t size = 0x200000u;
  static constexpr uint32_t environment = UINT32_C(0x80080000);
  static constexpr uint32_t stack = UINT32_C(0x801ff000);
  static constexpr uint32_t table = UINT32_C(0x80070000);
  std::vector<uint8_t> data = std::vector<uint8_t>(size);
  std::vector<uint8_t> known = std::vector<uint8_t>(size, 1u);
  Nba97GameTextRegion region{base, data.data(), known.data(), data.size()};
  Nba97GameDrawPacketEnvironmentBinding packet_binding{};
  Nba97GameDrawEnvironmentContext context{};
  Nba97GameDrawEnvironmentProgress progress{};
  size_t submit_calls{};
  size_t copy_calls{};
  Nba97GameBiosMemoryCopyProgress copy_progress{};

  NaturalFixture(size_t packet_budget = 256u) {
    write16(data.data(), environment - base + 0u, 64u);
    write16(data.data(), environment - base + 2u, 32u);
    write16(data.data(), environment - base + 4u, 64u);
    write16(data.data(), environment - base + 6u, 64u);
    write16(data.data(), environment - base + 8u, 2u);
    write16(data.data(), environment - base + 10u, 3u);
    write16(data.data(), environment - base + 0x14u, UINT16_C(0x1234));
    data[environment - base + 0x16u] = 0x44u;
    data[environment - base + 0x17u] = 0x55u;
    data[environment - base + 0x18u] = 1u;
    data[environment - base + 0x19u] = 0x11u;
    data[environment - base + 0x1au] = 0x22u;
    data[environment - base + 0x1bu] = 0x33u;
    data[UINT32_C(0x800c55c2) - base] = 0u;
    write16(data.data(), UINT32_C(0x800c55c4) - base, 640u);
    write16(data.data(), UINT32_C(0x800c55c6) - base, 480u);
    write32(data.data(), UINT32_C(0x800c55b8) - base, table);
    write32(data.data(), table - base + 4u, UINT32_C(0x11223344));
    write32(data.data(), table - base + 8u, UINT32_C(0x8000abcc));
    write32(data.data(), environment - base + 0x1cu, UINT32_C(0xaa000000));

    nba97_game_draw_packet_environment_binding_init(
        &packet_binding, packet_budget, packet_child, this, nullptr, 0u,
        nullptr, nullptr);
    context.memory = {&region, 1u};
    context.operation_budget = 256u;
    for (unsigned index = 0u; index != 32u; ++index) {
      context.machine.registers.gpr[index] =
          {UINT32_C(0x10000000) + index, 15u};
    }
    context.machine.registers.gpr[0] = {0u, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
        {environment, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {stack, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {UINT32_C(0x80048f54), 15u};
    context.machine.hi = {UINT32_C(0x55667788), 9u};
    context.machine.lo = {UINT32_C(0xaabbccdd), 6u};
    context.io = environment_io;
    context.user = this;
  }

  static int packet_child(void *, const Nba97GameTextMemory *,
                          const Nba97GameDrawPacketEvent *event,
                          Nba97GameDrawPacketMachine *machine) {
    machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
        {UINT32_C(0x90000000) + event->kind, 15u};
    return 1;
  }


  static int bios_copy(void* opaque,const Nba97GameTextMemory*,const Nba97GameBiosMemoryCopyEvent* event,Nba97GameBiosMemoryCopyMachine* machine) {
    auto& self=*static_cast<NaturalFixture*>(opaque);
    check(event->pc==0x8009cb10u&&event->delay_slot_pc==0x8009cb14u&&event->entry==0xa0u&&event->service==0x2a,"BIOS trampoline boundary");
    const auto destination=machine->registers.gpr[4].word-base;
    const auto source=machine->registers.gpr[5].word-base;
    const auto length=machine->registers.gpr[6].word;
    check(length==0x5c,"draw environment copy length");
    std::memmove(self.data.data()+destination,self.data.data()+source,length);
    std::memmove(self.known.data()+destination,self.known.data()+source,length);
    machine->registers.gpr[2]={0xffffffffu,15};return 1;
  }
  static int environment_io(void *opaque, const Nba97GameTextMemory *memory,
                            const Nba97GameDrawEnvironmentEvent *event,
                            Nba97GameDrawEnvironmentMachine *machine) {
    auto &fixture = *static_cast<NaturalFixture *>(opaque);
    if (event->kind == NBA97_GAME_DRAW_ENVIRONMENT_PACKET_8009A344 ||
        event->entry == UINT32_C(0x8009a344))
      return nba97_game_draw_packet_from_draw_environment(
          &fixture.packet_binding, memory, event, machine);
    if (event->kind == NBA97_GAME_DRAW_ENVIRONMENT_SUBMIT_INDIRECT) {
      ++fixture.submit_calls;
      check(event->pc == UINT32_C(0x80099b58), "natural submit pc");
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
          {UINT32_C(0x12345678), 15u};
      return 1;
    }
    if (event->kind == NBA97_GAME_DRAW_ENVIRONMENT_COPY_8009CB0C) {
      ++fixture.copy_calls;
      check(event->pc == UINT32_C(0x80099b68), "natural copy pc");
      Nba97GameBiosMemoryCopyContext child{};child.memory=*memory;child.operation_budget=1;
      child.machine=*machine;child.io=bios_copy;child.user=&fixture;
      const int result=nba97_game_bios_memory_copy(&child,&fixture.copy_progress);
      *machine=fixture.copy_progress.machine;return result==NBA97_TEXT_COMPLETE;

    }
    return 0;
  }

  int run() { return nba97_game_draw_environment(&context, &progress); }
};

int accepting_fallback(void *opaque, const Nba97GameTextMemory *,
                       const Nba97GameDrawEnvironmentEvent *,
                       Nba97GameDrawEnvironmentMachine *) {
  ++*static_cast<size_t *>(opaque);
  return 1;
}

void natural_complete() {
  NaturalFixture fixture;
  int result = fixture.run();
  if (result != NBA97_TEXT_COMPLETE)
    std::fprintf(stderr,
                 "natural diagnostic result=%d parent_pc=%08x nested=%d "
                 "nested_pc=%08x entry=%08x\n",
                 result, fixture.progress.stopped_pc,
                 fixture.packet_binding.result,
                 fixture.packet_binding.progress.stopped_pc,
                 fixture.packet_binding.progress.stopped_entry);
  check(result == NBA97_TEXT_COMPLETE, "natural BD completes");
  check(fixture.packet_binding.result == NBA97_TEXT_COMPLETE,
        "nested packet completes");
  check(fixture.packet_binding.invocations == 1u, "one nested packet");
  check(fixture.submit_calls == 1u, "one submit");
  check(fixture.copy_calls == 1u, "one copy");
  check(fixture.copy_progress.completed&&fixture.copy_progress.machine.registers.gpr[9].word==0x2a&&fixture.copy_progress.machine.registers.gpr[10].word==0xa0,"recovered copy trampoline completed");
  check(std::memcmp(fixture.data.data()+0xc55d0u,fixture.data.data()+NaturalFixture::environment-NaturalFixture::base,0x5c)==0,"copied completed packet and environment");
  const size_t packet = NaturalFixture::environment - NaturalFixture::base +
                        0x1cu;
  check(fixture.data[packet + 3u] == 9u, "natural packet count");
  check(fixture.data[packet] == 0xffu &&
            fixture.data[packet + 1u] == 0xffu &&
            fixture.data[packet + 2u] == 0xffu &&
            fixture.data[packet + 3u] == 9u,
        "BD tags packet after builder");
  check(fixture.progress.machine.hi.word == UINT32_C(0x55667788) &&
            fixture.progress.machine.hi.known_mask == 9u,
        "natural hi preserved");
  check(fixture.progress.machine.lo.word == UINT32_C(0xaabbccdd) &&
            fixture.progress.machine.lo.known_mask == 6u,
        "natural lo preserved");
}

void natural_failure_prefix() {
  NaturalFixture fixture(0u);
  check(fixture.run() == NBA97_TEXT_IO_REFUSED,
        "BD reports nested callback refusal");
  check(fixture.packet_binding.result == NBA97_TEXT_LIMIT,
        "nested limit retained");
  check(fixture.packet_binding.progress.operations == 0u,
        "nested zero budget prefix");
  check(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            NaturalFixture::stack - 0x20u - 0x28u,
        "nested child frame mutation reaches parent");
}

void adapter_guards() {
  NaturalFixture fixture;
  auto machine = fixture.context.machine;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
      {UINT32_C(0x80099b28), 15u};
  Nba97GameDrawEnvironmentEvent event{};
  event.pc = UINT32_C(0x80099b20);
  event.delay_slot_pc = UINT32_C(0x80099b24);
  event.entry = UINT32_C(0x8009a344);
  event.kind = NBA97_GAME_DRAW_ENVIRONMENT_PACKET_8009A344;
  event.argument_count = 2u;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
      {NaturalFixture::environment + 0x1cu, 15u};
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] =
      {NaturalFixture::environment, 15u};
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
      {NaturalFixture::stack - 0x20u, 15u};

  size_t fallback_calls = 0u;
  nba97_game_draw_packet_environment_binding_init(
      &fixture.packet_binding, 256u, NaturalFixture::packet_child, &fixture,
      nullptr, 0u, accepting_fallback, &fallback_calls);
  auto original = machine;
  event.kind = NBA97_GAME_DRAW_ENVIRONMENT_SUBMIT_INDIRECT;
  check(nba97_game_draw_packet_from_draw_environment(
            &fixture.packet_binding, &fixture.context.memory, &event,
            &machine) == 0,
        "same entry wrong kind rejected");
  check(fallback_calls == 0u, "assigned entry never falls back");
  check(std::memcmp(&machine, &original, sizeof(machine)) == 0,
        "malformed assigned event immutable");

  event.kind = NBA97_GAME_DRAW_ENVIRONMENT_PACKET_8009A344;
  event.entry = UINT32_C(0x80001234);
  check(nba97_game_draw_packet_from_draw_environment(
            &fixture.packet_binding, &fixture.context.memory, &event,
            &machine) == 0,
        "right kind wrong entry rejected");
  check(fallback_calls == 0u, "assigned kind never falls back");

  event.entry = UINT32_C(0x8009a344);
  event.delay_slot_pc++;
  check(nba97_game_draw_packet_from_draw_environment(
            &fixture.packet_binding, &fixture.context.memory, &event,
            &machine) == 0,
        "wrong delay rejected");
  event.delay_slot_pc--;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 14u;
  check(nba97_game_draw_packet_from_draw_environment(
            &fixture.packet_binding, &fixture.context.memory, &event,
            &machine) == 0,
        "partial ra rejected");

  machine = original;
  event.kind = NBA97_GAME_DRAW_ENVIRONMENT_SUBMIT_INDIRECT;
  event.entry = UINT32_C(0x80001111);
  check(nba97_game_draw_packet_from_draw_environment(
            &fixture.packet_binding, &fixture.context.memory, &event,
            &machine) == 1,
        "unrelated event falls back");
  check(fallback_calls == 1u, "fallback count");
}

} // namespace

int main() {
  natural_complete();
  natural_failure_prefix();
  adapter_guards();
  std::printf("game_draw_packet_integration_tests: %zu checks passed\n",
              checks);
  return 0;
}
