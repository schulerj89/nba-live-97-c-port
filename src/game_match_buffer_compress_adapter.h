#ifndef NBA97_GAME_MATCH_BUFFER_COMPRESS_ADAPTER_H
#define NBA97_GAME_MATCH_BUFFER_COMPRESS_ADAPTER_H

#include "recovered/game_match_buffer_compress.h"
#include "recovered/game_match_buffer_record.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameMatchBufferCompressBinding {
  size_t operation_budget;
  Nba97GameMatchBufferCompressAccess *access_journal;
  size_t access_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97GameMatchBufferRecordEvent event;
  Nba97GameMatchBufferCompressProgress progress;
  int result;
} Nba97GameMatchBufferCompressBinding;

/* Intercept only BW's exact 0x80076E58 compression call, including the
 * a3=0x82 delay slot and assigned return address. */
int nba97_game_match_buffer_compress_from_record(
    void *, const Nba97GameTextMemory *,
    const Nba97GameMatchBufferRecordEvent *,
    Nba97GameMatchBufferRecordMachine *);

/* Execute the real BW owner with compression composed and preserve its rewind
 * child through the parent's typed callback. */
int nba97_game_match_buffer_record_with_compress(
    const Nba97GameMatchBufferRecordContext *,
    Nba97GameMatchBufferCompressBinding *,
    Nba97GameMatchBufferRecordProgress *);

#ifdef __cplusplus
}
#endif
#endif
