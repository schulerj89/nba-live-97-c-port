#ifndef NBA97_GAME_MATCH_BUFFER_RECORD_ADAPTER_H
#define NBA97_GAME_MATCH_BUFFER_RECORD_ADAPTER_H

#include "game_match_buffer_rewind_adapter.h"
#include "recovered/game_match_buffer_record.h"
#include "recovered/game_period_startup.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameMatchBufferRecordBinding {
  size_t operation_budget;
  size_t rewind_operation_budget;
  size_t zero_operation_budget;
  Nba97GameMatchBufferRecordIo io;
  void *user;
  Nba97GameMatchBufferRecordAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameMatchBufferRewindAccess *rewind_journal;
  size_t rewind_journal_capacity;
  size_t invocations;
  size_t completions;
  size_t rewind_invocations;
  size_t compression_invocations;
  Nba97GamePeriodStartupEvent event[2];
  Nba97GameMatchBufferRecordProgress progress;
  Nba97GameMatchBufferRewindBinding rewind;
  int result;
  int nested_result;
} Nba97GameMatchBufferRecordBinding;

void nba97_game_match_buffer_record_binding_init(
    Nba97GameMatchBufferRecordBinding *, size_t, size_t, size_t);

int nba97_game_match_buffer_record_from_period_startup(
    void *, const Nba97GameTextMemory *, const Nba97GamePeriodStartupEvent *,
    Nba97GamePeriodStartupRegisters *);

int nba97_game_period_startup_with_match_buffer_record(
    const Nba97GamePeriodStartupContext *, Nba97GameMatchBufferRecordBinding *,
    Nba97GamePeriodStartupProgress *);

#ifdef __cplusplus
}
#endif
#endif
