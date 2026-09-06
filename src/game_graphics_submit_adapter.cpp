#include "game_graphics_submit_adapter.h"
#include <cstdint>
#include <cstring>
namespace {
struct Run {
  Nba97GameDrawEnvironmentIo fallback;
  void *user;
  Nba97GameGraphicsSubmitBinding *b;
};
bool target(const Nba97GameDrawEnvironmentEvent *e) {
  return e && e->pc == 0x80099b58u && e->entry == 0x8009b298u;
}
bool machineValid(const Nba97GameDrawEnvironmentMachine &m) {
  if (m.registers.gpr[0].word || m.registers.gpr[0].known_mask != 15 ||
      m.hi.known_mask > 15 || m.lo.known_mask > 15)
    return false;
  for (unsigned i = 0; i < 32; i++)
    if (m.registers.gpr[i].known_mask > 15)
      return false;
  return true;
}
bool memoryValid(const Nba97GameTextMemory &m) {
  if (!m.region && m.count)
    return false;
  for (std::size_t i = 0; i < m.count; i++) {
    const auto &a = m.region[i];
    if (!a.data || !a.size || a.size > UINT64_C(0x100000000) ||
        std::uint64_t(a.base) + a.size > UINT64_C(0x100000000))
      return false;
    for (std::size_t j = 0; j < i; j++) {
      const auto &b = m.region[j];
      if (std::uint64_t(a.base) < std::uint64_t(b.base) + b.size &&
          std::uint64_t(b.base) < std::uint64_t(a.base) + a.size)
        return false;
    }
  }
  return true;
}
int dispatch(void *o, const Nba97GameTextMemory *m,
             const Nba97GameDrawEnvironmentEvent *e,
             Nba97GameDrawEnvironmentMachine *x) {
  auto &r = *static_cast<Run *>(o);
  if (target(e))
    return nba97_game_graphics_submit_from_draw_environment(r.b, m, e, x);
  if (!r.fallback)
    return 0;
  int ok = r.fallback(r.user, m, e, x);
  if (ok == 1)
    r.b->fallback_callbacks_completed++;
  return ok;
}
} // namespace
int nba97_game_graphics_submit_from_draw_environment(
    void *o, const Nba97GameTextMemory *m,
    const Nba97GameDrawEnvironmentEvent *e,
    Nba97GameDrawEnvironmentMachine *x) {
  auto *b = static_cast<Nba97GameGraphicsSubmitBinding *>(o);
  if (!b || !m || !e || !x || e->pc != 0x80099b58u ||
      e->delay_slot_pc != 0x80099b5cu || e->entry != 0x8009b298u ||
      e->kind != NBA97_GAME_DRAW_ENVIRONMENT_SUBMIT_INDIRECT ||
      e->argument_count != 4 || !machineValid(*x) ||
      x->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15 ||
      x->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word != 0x80099b60u ||
      !memoryValid(*m) || (!b->access_journal && b->access_journal_capacity)) {
    if (b)
      b->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  b->invocations++;
  b->event = *e;
  Nba97GameGraphicsSubmitContext c{};
  c.memory = *m;
  c.operation_budget = b->operation_budget;
  c.machine = *x;
  c.io = b->io;
  c.user = b->user;
  c.access_journal = b->access_journal;
  c.access_journal_capacity = b->access_journal_capacity;
  b->result = nba97_game_graphics_submit(&c, &b->progress);
  *x = b->progress.machine;
  if (b->result != NBA97_TEXT_COMPLETE)
    return 0;
  b->completions++;
  return 1;
}
int nba97_game_draw_environment_with_graphics_submit(
    const Nba97GameDrawEnvironmentContext *p, Nba97GameGraphicsSubmitBinding *b,
    Nba97GameDrawEnvironmentProgress *out) {
  if (!p || !b || !out)
    return NBA97_TEXT_ARGUMENT;
  b->invocations = b->completions = b->fallback_callbacks_completed = 0;
  std::memset(&b->event, 0, sizeof b->event);
  std::memset(&b->progress, 0, sizeof b->progress);
  b->result = NBA97_TEXT_ARGUMENT;
  Run r{p->io, p->user, b};
  Nba97GameDrawEnvironmentContext c = *p;
  c.io = dispatch;
  c.user = &r;
  int result = nba97_game_draw_environment(&c, out);
  if (result == NBA97_TEXT_IO_REFUSED && out->stopped_pc == 0x80099b58u &&
      b->invocations)
    return b->result;
  return result;
}
