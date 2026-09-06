#include "recovered/frontend_resource_load.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using U = std::uint32_t;
unsigned checks;
void check(bool value, int line) {
  ++checks;
  if (!value)
    throw std::runtime_error("frontend-resource-load line " +
                             std::to_string(line));
}
#define CHECK(x) check((x), __LINE__)
constexpr U Base = 0x80000000u, Size = 0x200000u, Sp = 0x801f0000u;
constexpr U Filename = 0x80024854u, Allocation = 0x80170000u;
struct Seen {
  Nba97FrontendResourceLoadEvent event{};
  Nba97FrontendResourceLoadMachine machine{};
};
struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendResourceLoadContext context{};
  Nba97FrontendResourceLoadProgress progress{};
  std::array<Nba97FrontendResourceLoadAccess, 32> access{};
  std::array<U, 80> pcs{};
  std::vector<Seen> calls;
  U cached = 0x80160000u, size = 0x1000u, allocation = Allocation,
    hook = 0x80061000u;
  std::uint8_t cached_mask = 15, size_mask = 15, descriptor_mask = 9,
               hook_mask = 15;
  U relocate_frame = 0, late_size = 0, late_hook = 0;
  unsigned refuse = 0;
  bool malformed = false;
  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {0x31000000u + i * 0x101u,
                                          std::uint8_t((i % 15) + 1)};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[4] = {Filename, 5};
    context.machine.registers.gpr[5] = {0x13579bdfu, 10};
    context.machine.registers.gpr[6] = {7, 3};
    context.machine.registers.gpr[29] = {Sp, 15};
    context.machine.registers.gpr[31] = {0x8007b16cu, 15};
    context.machine.hi = {0x12345678u, 5};
    context.machine.lo = {0x9abcdef0u, 10};
    put(0x800d9b50u, hook, hook_mask);
    context.memory = {&region, 1};
    context.operation_budget = 25;
    context.io = callback;
    context.user = this;
    context.access_journal = access.data();
    context.access_journal_capacity = access.size();
    context.instruction_journal = pcs.data();
    context.instruction_journal_capacity = pcs.size();
  }
  void put(U a, U v, std::uint8_t mask = 15) {
    if (a < Base || a - Base > Size - 4)
      throw std::runtime_error("fixture write outside RAM");
    for (unsigned i = 0; i < 4; ++i) {
      bytes[a - Base + i] = std::uint8_t(v >> (i * 8));
      if (region.known)
        known[a - Base + i] = std::uint8_t((mask >> i) & 1);
    }
  }
  U get(U a) const {
    if (a < Base || a - Base > Size - 4)
      throw std::runtime_error("fixture read outside RAM");
    U v = 0;
    for (unsigned i = 0; i < 4; ++i)
      v |= U(bytes[a - Base + i]) << (i * 8);
    return v;
  }
  int run() { return nba97_frontend_resource_load(&context, &progress); }
  static int callback(void *o, const Nba97GameTextMemory *,
                      const Nba97FrontendResourceLoadEvent *e,
                      Nba97FrontendResourceLoadMachine *m) {
    auto &f = *static_cast<Fixture *>(o);
    if (!e || !m)
      return 0;
    f.calls.push_back({*e, *m});
    if (e->site == 1)
      m->registers.gpr[2] = {f.cached, f.cached_mask};
    if (e->site == 2) {
      U sp = m->registers.gpr[29].word;
      if (f.relocate_frame) {
        if (sp < Base || sp - Base > Size - 64 || f.relocate_frame < Base ||
            f.relocate_frame - Base > Size - 64)
          return 0;
        for (unsigned i = 0; i < 64; ++i) {
          f.bytes[f.relocate_frame - Base + i] = f.bytes[sp - Base + i];
          if (f.region.known)
            f.known[f.relocate_frame - Base + i] = f.known[sp - Base + i];
        }
        m->registers.gpr[29] = {f.relocate_frame, 15};
        sp = f.relocate_frame;
      }
      f.put(sp + 24, 0x44);
      f.put(sp + 32, f.size, f.size_mask);
    }
    if (e->site == 3) {
      m->registers.gpr[2] = {f.allocation, 15};
      if (f.allocation)
        f.put(f.allocation, 0x55667788u, f.descriptor_mask);
    }
    if (e->site == 4 && f.late_size)
      f.put(m->registers.gpr[29].word + 32, f.late_size);
    if (e->site == 5 && f.late_hook)
      f.put(0x800d9b50, f.late_hook);
    if (e->site == 6)
      m->registers.gpr[2] = {0x89abcdefu, 6};
    if (f.malformed)
      m->hi.known_mask = 16;
    return e->site != f.refuse;
  }
};
void full() {
  Fixture f;
  CHECK(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
  CHECK(f.progress.instruction_count == 59 && f.progress.operations == 25 &&
        f.progress.accesses == 19 && f.progress.reads == 12 &&
        f.progress.stores == 7 && f.calls.size() == 6);
  for (unsigned i = 0; i < 59; ++i)
    CHECK(f.pcs[i] == 0x8007b1d0u + i * 4);
  const U cp[] = {0x8007b1f0, 0x8007b214, 0x8007b230,
                  0x8007b250, 0x8007b268, 0x8007b28c};
  const U tg[] = {0x8008a2c8, 0x8008a594, 0x80077160,
                  0x8008a810, 0x8008a7b0, 0x80061000};
  const unsigned ac[] = {1, 5, 4, 3, 1, 4};
  for (unsigned i = 0; i < 6; ++i)
    CHECK(f.calls[i].event.pc == cp[i] &&
          f.calls[i].event.delay_slot_pc == cp[i] + 4 &&
          f.calls[i].event.entry == tg[i] &&
          f.calls[i].event.argument_count == ac[i]);
  CHECK(f.progress.cached_result_discarded &&
        f.progress.file_size.word == 0x1000 &&
        f.progress.allocation_result.word == Allocation &&
        f.progress.descriptor_word.word == 0x55667788 &&
        f.progress.callback_pointer.word == 0x80061000 &&
        f.progress.dynamic_return.word == 0x89abcdef &&
        f.progress.machine.registers.gpr[2].word == 0x89abcdef &&
        f.get(0x800d9ae8) == 0x1000);
  CHECK(f.calls[1].machine.registers.gpr[4].word == Filename &&
        f.calls[1].machine.registers.gpr[5].word == Sp - 40 &&
        f.calls[1].machine.registers.gpr[6].word == Sp - 36 &&
        f.calls[1].machine.registers.gpr[7].word == Sp - 32 &&
        f.get(Sp - 48) == 7);
  const U access_pcs[] = {0x8007b1d4, 0x8007b1dc, 0x8007b1e4, 0x8007b1ec,
                          0x8007b1f4, 0x8007b218, 0x8007b21c, 0x8007b244,
                          0x8007b248, 0x8007b24c, 0x8007b258, 0x8007b260,
                          0x8007b264, 0x8007b274, 0x8007b29c, 0x8007b2a0,
                          0x8007b2a4, 0x8007b2a8, 0x8007b2ac};
  const U addresses[] = {Sp - 20, Sp - 12,    Sp - 16, Sp - 8,     Sp - 24,
                         Sp - 48, Sp - 32,    Sp - 40, Allocation, Sp - 32,
                         Sp - 32, 0x800d9ae8, Sp - 40, 0x800d9b50, Sp - 8,
                         Sp - 12, Sp - 16,    Sp - 20, Sp - 24};
  const unsigned operations[] = {1,  2,  3,  4,  5,  7,  9,  11, 12, 13,
                                 15, 16, 17, 19, 21, 22, 23, 24, 25};
  for (unsigned i = 0; i < 19; ++i)
    CHECK(f.access[i].pc == access_pcs[i] &&
          f.access[i].address == addresses[i] &&
          f.access[i].operation == operations[i]);
  CHECK(f.calls[2].machine.registers.gpr[4].word == Filename &&
        f.calls[2].machine.registers.gpr[5].word == 0x1000 &&
        f.calls[2].machine.registers.gpr[6].word == 0x13579bdf &&
        f.calls[2].machine.registers.gpr[7].word == 7);
  CHECK(f.calls[3].machine.registers.gpr[4].word == 0x44 &&
        f.calls[3].machine.registers.gpr[5].word == 0x55667788 &&
        f.calls[3].machine.registers.gpr[6].word == 0x1000);
  CHECK(f.calls[4].machine.registers.gpr[4].word == 0x44 &&
        f.calls[5].machine.registers.gpr[4].word == Allocation &&
        f.calls[5].machine.registers.gpr[5].word == Filename &&
        f.calls[5].machine.registers.gpr[6].word == 0x13579bdf &&
        f.calls[5].machine.registers.gpr[7].word == 7);
}
void paths() {
  Fixture zero;
  zero.cached = 0;
  zero.size = 0;
  zero.hook = 0;
  zero.put(0x800d9b50, 0);
  CHECK(zero.run() == NBA97_TEXT_COMPLETE && zero.calls.size() == 2 &&
        zero.progress.operations == 15 &&
        zero.progress.instruction_count == 36 &&
        zero.progress.machine.registers.gpr[2].word == 0);
  Fixture null_alloc;
  null_alloc.allocation = 0;
  CHECK(null_alloc.run() == NBA97_TEXT_COMPLETE &&
        null_alloc.calls.size() == 5 && null_alloc.progress.operations == 19 &&
        null_alloc.progress.instruction_count == 51 &&
        null_alloc.progress.machine.registers.gpr[2].word == 0x89abcdef);
  Fixture nohook;
  nohook.hook = 0;
  nohook.put(0x800d9b50, 0);
  CHECK(nohook.run() == NBA97_TEXT_COMPLETE && nohook.calls.size() == 5 &&
        nohook.progress.machine.registers.gpr[2].word == Allocation);
}
void budgets() {
  for (std::size_t b = 0; b < 25; ++b) {
    Fixture f;
    f.context.operation_budget = b;
    CHECK(f.run() == NBA97_TEXT_LIMIT && f.progress.operations == b &&
          !f.progress.completed);
  }
  for (unsigned s = 1; s <= 6; ++s) {
    Fixture f;
    f.refuse = s;
    CHECK(f.run() == NBA97_TEXT_IO_REFUSED &&
          f.progress.call_attempts[s] == 1 && f.progress.call_count[s] == 0);
  }
  Fixture noio;
  noio.context.io = nullptr;
  CHECK(noio.run() == NBA97_TEXT_IO_REFUSED && noio.progress.operations == 6);
  Fixture bad;
  bad.malformed = true;
  CHECK(bad.run() == NBA97_TEXT_ARGUMENT && bad.progress.operations == 6);
}
void knownnessDecisions() {
  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture cached_zero;
    cached_zero.cached = 0;
    cached_zero.cached_mask = std::uint8_t(mask);
    CHECK(cached_zero.run() ==
          (mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN));
    if (mask != 15)
      CHECK(cached_zero.progress.stopped_pc == 0x8007b1fcu);

    Fixture size_zero;
    size_zero.size = 0;
    size_zero.size_mask = std::uint8_t(mask);
    CHECK(size_zero.run() ==
          (mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN));
    if (mask != 15)
      CHECK(size_zero.progress.stopped_pc == 0x8007b224u);

    Fixture hook_zero;
    hook_zero.put(0x800d9b50u, 0, std::uint8_t(mask));
    CHECK(hook_zero.run() ==
          (mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN));
    if (mask != 15)
      CHECK(hook_zero.progress.stopped_pc == 0x8007b27cu);
  }

  Fixture partial_descriptor;
  partial_descriptor.descriptor_mask = 5;
  CHECK(partial_descriptor.run() == NBA97_TEXT_COMPLETE &&
        partial_descriptor.progress.descriptor_word.known_mask == 5 &&
        partial_descriptor.calls[3].machine.registers.gpr[5].known_mask == 5);

  Fixture unknown_nonzero_hook;
  unknown_nonzero_hook.put(0x800d9b50u, 0x80061001u, 1);
  CHECK(unknown_nonzero_hook.run() == NBA97_TEXT_UNKNOWN &&
        unknown_nonzero_hook.progress.stopped_pc == 0x8007b28cu &&
        unknown_nonzero_hook.progress.instruction_count == 49);
}

void relocationAndReloads() {
  Fixture moved;
  moved.relocate_frame = 0x801ed000u;
  CHECK(moved.run() == NBA97_TEXT_COMPLETE && moved.progress.completed &&
        moved.progress.machine.registers.gpr[29].word == 0x801ed040u &&
        moved.progress.restored_return_address.word == 0x8007b16cu &&
        moved.progress.restored_s0.word == 0x31001010u &&
        moved.progress.restored_s3.word == 0x31001313u);

  Fixture reloaded_size;
  reloaded_size.late_size = 0x2345u;
  CHECK(reloaded_size.run() == NBA97_TEXT_COMPLETE &&
        reloaded_size.get(0x800d9ae8u) == 0x2345u &&
        reloaded_size.access[10].pc == 0x8007b258u &&
        reloaded_size.access[11].pc == 0x8007b260u);

  Fixture reloaded_hook;
  reloaded_hook.put(0x800d9b50u, 0);
  reloaded_hook.late_hook = 0x80062000u;
  CHECK(reloaded_hook.run() == NBA97_TEXT_COMPLETE &&
        reloaded_hook.calls.back().event.entry == 0x80062000u &&
        reloaded_hook.progress.callback_pointer.word == 0x80062000u);
}

void edges() {
  Fixture unknown_hook;
  unknown_hook.put(0x800d9b50, 0x80061000, 7);
  CHECK(unknown_hook.run() == NBA97_TEXT_UNKNOWN &&
        unknown_hook.progress.stopped_pc == 0x8007b28c &&
        unknown_hook.progress.instruction_count == 49);
  Fixture bad_hook;
  bad_hook.put(0x800d9b50, 0x80061002);
  CHECK(bad_hook.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        bad_hook.progress.stopped_target == 0x80061002);
  Fixture partial_ra;
  partial_ra.context.machine.registers.gpr[31].known_mask = 7;
  CHECK(partial_ra.run() == NBA97_TEXT_UNKNOWN &&
        partial_ra.progress.stopped_pc == 0x8007b2b4);
  Fixture absent;
  for (unsigned i = 0; i < 32; ++i)
    absent.context.machine.registers.gpr[i].known_mask = 15;
  absent.region.known = nullptr;
  CHECK(absent.run() == NBA97_TEXT_COMPLETE && absent.progress.completed);
  Fixture malformed;
  malformed.known[Sp - 20u - Base] = 2;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.stopped_pc == 0x8007b1d4u);
  Fixture overlap;
  Nba97GameTextRegion rs[2] = {overlap.region, overlap.region};
  overlap.context.memory = {rs, 2};
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT);
}
} // namespace
int main() {
  try {
    full();
    paths();
    budgets();
    knownnessDecisions();
    relocationAndReloads();
    edges();
    std::printf("frontend_resource_load_tests passed %u checks\n", checks);
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
