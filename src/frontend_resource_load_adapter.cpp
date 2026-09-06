#include "frontend_resource_load_adapter.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace {
constexpr std::uint8_t FE = NBA97_FRONTEND_MAIN_PROGRAM_FEONLY;
constexpr Nba97FrontendResourceLoadSiteContract Contracts[] = {
    {0, 0, 0, 0, 0},
    {0x8007b1f0u, 0x8007b1f4u, 0x8008a2c8u, 1, FE},
    {0x8007b214u, 0x8007b218u, 0x8008a594u, 5, FE},
    {0x8007b230u, 0x8007b234u, 0x80077160u, 4, FE},
    {0x8007b250u, 0x8007b254u, 0x8008a810u, 3, FE},
    {0x8007b268u, 0x8007b26cu, 0x8008a7b0u, 1, FE},
    {0x8007b28cu, 0x8007b290u, 0, 4, FE}};
static_assert(sizeof Contracts / sizeof Contracts[0] ==
              NBA97_FRONTEND_RESOURCE_LOAD_SITE_COUNT);
bool machineValid(const Nba97FrontendResourceLoadMachine &m) {
  if (m.registers.gpr[0].word || m.registers.gpr[0].known_mask != 15 ||
      m.hi.known_mask > 15 || m.lo.known_mask > 15)
    return false;
  for (unsigned i = 0; i < 32; ++i)
    if (m.registers.gpr[i].known_mask > 15)
      return false;
  return true;
}
bool memoryValid(const Nba97GameTextMemory &m) {
  if (!m.region && m.count)
    return false;
  for (std::size_t i = 0; i < m.count; ++i) {
    const auto &a = m.region[i];
    if (!a.data || !a.size || a.size > UINT64_C(0x100000000) ||
        std::uint64_t(a.base) + a.size > UINT64_C(0x100000000))
      return false;
    for (std::size_t j = 0; j < i; ++j) {
      const auto &b = m.region[j];
      if (std::uint64_t(a.base) < std::uint64_t(b.base) + b.size &&
          std::uint64_t(b.base) < std::uint64_t(a.base) + a.size)
        return false;
    }
  }
  return true;
}
struct Composition {
  Nba97FrontendLoadPayloadIo fallback;
  void *fallback_user;
  Nba97FrontendResourceLoadBinding *binding;
  Nba97FrontendResourceLoadAdapterProgress *out;
};
int composed(void *o, const Nba97GameTextMemory *m,
             const Nba97FrontendLoadPayloadEvent *e,
             Nba97FrontendLoadPayloadMachine *cpu) {
  auto &c = *static_cast<Composition *>(o);
  if (e && e->site == NBA97_FRONTEND_LOAD_PAYLOAD_SITE_8007B164) {
    std::size_t before = c.binding->invocations;
    int ok = nba97_frontend_resource_load_from_frontend_load_payload(c.binding,
                                                                     m, e, cpu);
    if (c.binding->invocations != before) {
      ++c.out->invocations;
      if (c.binding->result == NBA97_TEXT_COMPLETE)
        ++c.out->completions;
      c.out->parent_event = c.binding->parent_event;
      c.out->parent_machine = c.binding->parent_machine;
      c.out->progress = c.binding->progress;
      c.out->result = c.binding->result;
    }
    return ok;
  }
  return c.fallback ? c.fallback(c.fallback_user, m, e, cpu) : 0;
}
} // namespace
int nba97_frontend_resource_load_site_contract(
    uint8_t site, Nba97FrontendResourceLoadSiteContract *c) {
  if (!c || !site || site >= NBA97_FRONTEND_RESOURCE_LOAD_SITE_COUNT)
    return 0;
  *c = Contracts[site];
  return 1;
}
int nba97_frontend_resource_load_from_frontend_load_payload(
    void *o, const Nba97GameTextMemory *m,
    const Nba97FrontendLoadPayloadEvent *e,
    Nba97FrontendLoadPayloadMachine *cpu) {
  auto *b = static_cast<Nba97FrontendResourceLoadBinding *>(o);
  if (!b || !m || !e || !cpu || !memoryValid(*m) || !machineValid(*cpu) ||
      e->site != NBA97_FRONTEND_LOAD_PAYLOAD_SITE_8007B164 ||
      e->pc != 0x8007b164u || e->delay_slot_pc != 0x8007b168u ||
      e->entry != 0x8007b1d0u || e->invocation != 1 || e->argument_count != 3 ||
      e->target_program != FE ||
      cpu->registers.gpr[NBA97_FRONTEND_RESOURCE_LOAD_RA].word != 0x8007b16cu ||
      cpu->registers.gpr[NBA97_FRONTEND_RESOURCE_LOAD_RA].known_mask != 15 ||
      (!b->access_journal && b->access_journal_capacity) ||
      (!b->instruction_journal && b->instruction_journal_capacity)) {
    if (b)
      b->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  ++b->invocations;
  b->parent_event = *e;
  b->parent_machine = *cpu;
  Nba97FrontendResourceLoadContext c{};
  c.memory = *m;
  c.operation_budget = b->operation_budget;
  c.machine = *cpu;
  c.io = b->io;
  c.user = b->user;
  c.access_journal = b->access_journal;
  c.access_journal_capacity = b->access_journal_capacity;
  c.instruction_journal = b->instruction_journal;
  c.instruction_journal_capacity = b->instruction_journal_capacity;
  b->result = nba97_frontend_resource_load(&c, &b->progress);
  *cpu = b->progress.machine;
  if (b->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++b->completions;
  return 1;
}
int nba97_frontend_load_payload_with_recovered_resource(
    Nba97FrontendLoadPayloadContext *c, Nba97FrontendResourceLoadBinding *b,
    Nba97FrontendLoadPayloadProgress *p,
    Nba97FrontendResourceLoadAdapterProgress *out) {
  if (!out)
    return NBA97_TEXT_ARGUMENT;
  std::memset(out, 0, sizeof *out);
  if (!c || !b)
    return NBA97_TEXT_ARGUMENT;
  Composition x{c->io, c->user, b, out};
  auto composed_context = *c;
  composed_context.io = composed;
  composed_context.user = &x;
  return nba97_frontend_load_payload(&composed_context, p);
}
