#ifndef NBA97_GAME_CLEAR_ORDERING_TABLE_ADAPTER_H
#define NBA97_GAME_CLEAR_ORDERING_TABLE_ADAPTER_H

#include "recovered/game_clear_ordering_table.h"
#include "recovered/game_match_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameClearOrderingTableMatchFrameCallIndex {
    NBA97_GAME_CLEAR_ORDERING_TABLE_CALL_49084 = 0,
    NBA97_GAME_CLEAR_ORDERING_TABLE_CALL_49094,
    NBA97_GAME_CLEAR_ORDERING_TABLE_MATCH_FRAME_CALL_COUNT
};

enum Nba97GameClearOrderingTableAdapterResult {
    NBA97_GAME_CLEAR_ORDERING_TABLE_ENTRY_MACHINE_REQUIRED = -30,
    NBA97_GAME_CLEAR_ORDERING_TABLE_CHILD_INCOMPLETE = -31
};

/* The narrow match-frame event has no complete GPR/SP/HI/LO payload. This
 * provider must independently supply a fresh source-proven machine for every
 * invocation. Return exactly 1 when the complete entry machine was supplied. */
typedef int (*Nba97GameClearOrderingTableEntryMachineProvider)(void*,
    const Nba97MatchFrameCall*, size_t,
    Nba97GameClearOrderingTableMachine*);

typedef struct Nba97GameClearOrderingTableMatchFrameBinding {
    Nba97GameTextMemory memory;
    size_t operation_budget;
    Nba97GameClearOrderingTableEntryMachineProvider entry_machine_provider;
    void* entry_machine_user;
    Nba97GameClearOrderingTableIo io;
    void* user;
    Nba97GameClearOrderingTableAccess* access_journal;
    size_t access_journal_capacity;
    Nba97MatchFrameIo fallback;
    void* fallback_user;
    size_t invocations;
    size_t completions;
    size_t fallback_callbacks_completed;
    size_t call_count[
        NBA97_GAME_CLEAR_ORDERING_TABLE_MATCH_FRAME_CALL_COUNT];
    Nba97MatchFrameCall event[
        NBA97_GAME_CLEAR_ORDERING_TABLE_MATCH_FRAME_CALL_COUNT];
    Nba97GameClearOrderingTableProgress progress[
        NBA97_GAME_CLEAR_ORDERING_TABLE_MATCH_FRAME_CALL_COUNT];
    int result[NBA97_GAME_CLEAR_ORDERING_TABLE_MATCH_FRAME_CALL_COUNT];
} Nba97GameClearOrderingTableMatchFrameBinding;

/* Bind only the exact 0x80049084 and 0x80049094 calls to 0x80099960. The
 * source-proven event supplies a0/a1 and JAL supplies ra=pc+8; every other
 * machine field comes independently from entry_machine_provider. */
int nba97_game_clear_ordering_table_from_match_frame(
    void*, const Nba97MatchFrameCall*, Nba97GamePeriodValue*);

/* Execute the complete existing 0x80049018 frame owner. Both ordering-table
 * calls are intercepted, while all other typed services are forwarded. */
int nba97_game_match_frame_with_clear_ordering_table(
    const Nba97MatchFrameContext*,
    Nba97GameClearOrderingTableMatchFrameBinding*,
    Nba97MatchFrameProgress*);

#ifdef __cplusplus
}
#endif
#endif
