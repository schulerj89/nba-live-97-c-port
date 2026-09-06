#ifndef NBA97_GAME_MATCH_BUFFER_PENDING_ADAPTER_H
#define NBA97_GAME_MATCH_BUFFER_PENDING_ADAPTER_H

#include "recovered/game_match_buffer_pending.h"
#include "recovered/game_period_startup.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameMatchBufferPendingPeriodBinding {
  size_t operation_budget;
  Nba97GameMatchBufferPendingAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GamePeriodStartupIo fallback;
  void *fallback_user;
  Nba97GameMatchBufferPendingProgress progress;
  Nba97GamePeriodStartupEvent first_event;
  Nba97GamePeriodStartupEvent second_event;
  int result;
  size_t invocations;
  size_t completions;
  size_t first_invocations;
  size_t second_invocations;
} Nba97GameMatchBufferPendingPeriodBinding;

/* Intercept both exact 0x80076B28 events from the recovered period-startup
 * owner. Unrelated children pass to fallback; malformed assigned boundaries
 * are rejected without fallback. HI/LO enter explicitly unknown because the
 * parent exposes only GPRs and this leaf neither reads nor writes HI/LO. */
int nba97_game_match_buffer_pending_from_period_startup(
    void *, const Nba97GameTextMemory *, const Nba97GamePeriodStartupEvent *,
    Nba97GamePeriodStartupRegisters *);

#ifdef __cplusplus
}
#endif
#endif
