#include "game_display_environment_adapter.h"
#include <cstdint>
#include <cstring>
namespace {
struct Run {
  Nba97GameSceneStartupIo fallback;
  void *user;
  Nba97GameDisplayEnvironmentSceneBinding *b;
};
int indexOf(const Nba97GameSceneStartupEvent *e) {
  if (!e)
    return -1;
  if (e->pc == 0x80048f20u)
    return 0;
  if (e->pc == 0x80048f78u)
    return 1;
  return -1;
}
bool target(const Nba97GameSceneStartupEvent *e) {
  return e && (indexOf(e) >= 0 || e->entry == 0x80099ca4u ||
               e->kind == NBA97_GAME_SCENE_STARTUP_DISPLAY_80099CA4);
}
bool regsValid(const Nba97GameSceneStartupRegisters &r) {
  if (r.gpr[0].word || r.gpr[0].known_mask != 15)
    return false;
  for (unsigned i = 0; i < 32; i++)
    if (r.gpr[i].known_mask > 15)
      return false;
  return true;
}
bool machineValid(const Nba97GameDisplayEnvironmentMachine &m) {
  return regsValid(m.registers) && m.hi.known_mask <= 15 &&
         m.lo.known_mask <= 15;
}
bool memoryValid(const Nba97GameTextMemory &memory) {
  if (!memory.region && memory.count)
    return false;
  for (std::size_t i = 0; i < memory.count; ++i) {
    const auto &a = memory.region[i];
    if (!a.data || !a.size || a.size > UINT64_C(0x100000000) ||
        std::uint64_t(a.base) + a.size > UINT64_C(0x100000000))
      return false;
    for (std::size_t j = 0; j < i; ++j) {
      const auto &b = memory.region[j];
      if (std::uint64_t(a.base) < std::uint64_t(b.base) + b.size &&
          std::uint64_t(b.base) < std::uint64_t(a.base) + a.size)
        return false;
    }
  }
  return true;
}
int dispatch(void *o, const Nba97GameTextMemory *m,
             const Nba97GameSceneStartupEvent *e,
             Nba97GameSceneStartupRegisters *r) {
  auto &x = *static_cast<Run *>(o);
  if (target(e))
    return nba97_game_display_environment_from_scene_startup(x.b, m, e, r);
  if (!x.fallback)
    return 0;
  int ok = x.fallback(x.user, m, e, r);
  if (ok == 1)
    x.b->fallback_callbacks_completed++;
  return ok;
}
} // namespace
int nba97_game_display_environment_from_scene_startup(
    void *o, const Nba97GameTextMemory *mem,
    const Nba97GameSceneStartupEvent *e, Nba97GameSceneStartupRegisters *regs) {
  auto *b = static_cast<Nba97GameDisplayEnvironmentSceneBinding *>(o);
  int i = indexOf(e);
  if (!b || !mem || !regs || i < 0 || e->entry != 0x80099ca4u ||
      e->delay_slot_pc != e->pc + 4u ||
      e->kind != NBA97_GAME_SCENE_STARTUP_DISPLAY_80099CA4 ||
      e->argument_count != 1 || !regsValid(*regs) ||
      regs->gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15 ||
      regs->gpr[NBA97_MATCH_INITIALIZE_RA].word != e->pc + 8u ||
      !memoryValid(*mem) ||
      (!b->access_journal && b->access_journal_capacity)) {
    if (b && i >= 0)
      b->result[i] = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  Nba97GameDisplayEnvironmentMachine m{};
  m.registers = *regs;
  if (b->hi_lo_provider) {
    if (b->hi_lo_provider(b->hi_lo_user, e, b->invocations + 1, &m.hi, &m.lo) !=
        1) {
      b->result[i] = NBA97_TEXT_ARGUMENT;
      return 0;
    }
  } else {
    m.hi = {0, 0};
    m.lo = {0, 0};
  }
  if (!machineValid(m)) {
    b->result[i] = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  b->invocations++;
  b->call_count[i]++;
  b->event[i] = *e;
  Nba97GameDisplayEnvironmentContext c{};
  c.memory = *mem;
  c.operation_budget = b->operation_budget;
  c.machine = m;
  c.io = b->io;
  c.user = b->user;
  c.access_journal = b->access_journal;
  c.access_journal_capacity = b->access_journal_capacity;
  b->result[i] = nba97_game_display_environment(&c, &b->progress[i]);
  Nba97GameDisplayEnvironmentMachine returned = b->progress[i].machine;
  bool malformed =
      b->result[i] == NBA97_TEXT_ARGUMENT && !regsValid(returned.registers) &&
      (b->progress[i].callbacks_completed <
       b->progress[i].call_attempts[NBA97_GAME_DISPLAY_ENVIRONMENT_DEBUG] +
           b->progress[i]
               .call_attempts[NBA97_GAME_DISPLAY_ENVIRONMENT_ORIGIN_HELPER] +
           b->progress[i]
               .call_attempts[NBA97_GAME_DISPLAY_ENVIRONMENT_GPU_COMMAND] +
           b->progress[i]
               .call_attempts[NBA97_GAME_DISPLAY_ENVIRONMENT_VIDEO_MODE] +
           b->progress[i].call_attempts[NBA97_GAME_DISPLAY_ENVIRONMENT_COPY]);
  m = returned;
  *regs = m.registers;
  b->final_hi[i] = m.hi;
  b->final_lo[i] = m.lo;
  if (malformed)
    return 1;
  if (b->result[i] != NBA97_TEXT_COMPLETE)
    return 0;
  b->completions++;
  return 1;
}
int nba97_game_scene_startup_with_display_environment(
    const Nba97GameSceneStartupContext *p,
    Nba97GameDisplayEnvironmentSceneBinding *b,
    Nba97GameSceneStartupProgress *out) {
  if (!p || !b || !out)
    return NBA97_TEXT_ARGUMENT;
  b->invocations = b->completions = b->fallback_callbacks_completed = 0;
  std::memset(b->call_count, 0, sizeof b->call_count);
  std::memset(b->event, 0, sizeof b->event);
  std::memset(b->progress, 0, sizeof b->progress);
  std::memset(b->result, 0, sizeof b->result);
  std::memset(b->final_hi, 0, sizeof b->final_hi);
  std::memset(b->final_lo, 0, sizeof b->final_lo);
  Run r{p->io, p->user, b};
  Nba97GameSceneStartupContext c = *p;
  c.io = dispatch;
  c.user = &r;
  int result = nba97_game_scene_startup(&c, out);
  if (result == NBA97_TEXT_IO_REFUSED) {
    int i = out->stopped_pc == 0x80048f20u
                ? NBA97_GAME_DISPLAY_ENVIRONMENT_SCENE_48F20
            : out->stopped_pc == 0x80048f78u
                ? NBA97_GAME_DISPLAY_ENVIRONMENT_SCENE_48F78
                : -1;
    if (i >= 0 && b->result[i] == NBA97_TEXT_ARGUMENT)
      return NBA97_TEXT_ARGUMENT;
  }
  return result;
}
