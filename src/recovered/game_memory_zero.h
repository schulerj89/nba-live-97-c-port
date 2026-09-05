#ifndef NBA97_GAME_MEMORY_ZERO_H
#define NBA97_GAME_MEMORY_ZERO_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameMemoryZeroContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Counts completed or attempted source stores. */
    uint32_t destination;
    uint32_t length;
    /* The source never assigns v0; retain the live incoming register exactly. */
    uint32_t incoming_v0;
    uint8_t incoming_v0_known;
} Nba97GameMemoryZeroContext;

typedef struct Nba97GameMemoryZeroProgress {
    size_t operations;
    size_t accesses;
    size_t stores;
    size_t bytes_stored; /* Store traffic, including overlapping SWR/SWL bytes. */
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t destination;
    uint32_t requested_length;
    uint32_t working_destination; /* Source a0 at return or bounded stop. */
    uint32_t working_count;       /* Source a1 at return or bounded stop. */
    uint32_t return_v0;
    uint8_t return_v0_known;
    uint8_t used_small_path;
    uint8_t completed;
} Nba97GameMemoryZeroProgress;

/* Original GAMEONLY zero-fill entry 0x800A3A74. Its sole instruction forces
 * a2 to zero, then execution falls through the complete optimized memory-fill
 * core at 0x800A3A78..0x800A3BB7 (80 more instructions). Main calls this
 * entry at 0x80029B84 with (0x800D6DEC, 0x20), immediately after controller
 * suspension, to clear eight retained shutdown-callback words before FELOAD.
 *
 * Preserve the source store schedule, not merely the final memset result:
 * SWR aligns the head, 128/16/4-byte tiers issue word stores, and a trailing
 * SWL may repeat bytes already cleared. The signed length<4 branch has an SB
 * in its delay slot, so length zero and ordinary negative lengths still clear
 * one byte. INT_MIN is worse: subtracting one wraps to INT_MAX and attempts
 * 0x80000000 byte stores. operation_budget safely exposes a prefix of that
 * original behavior; it is not a semantic repair. The routine performs no
 * reads and leaves incoming v0 live.
 *
 * Entry SHA-256 (0x800A3A74..77):
 * 3eec77d0e95c14d4c06c9e1d4548029c2bcc34fa7770a485652dbb193a79036c
 * Shared-core SHA-256 (0x800A3A78..0x800A3BB7):
 * 5cf83e6e51d1bf5e8b4accba1415bedee7aa4d9a5c63c188b29f34b1678825f8
 * Effective-path SHA-256 (all 324 bytes):
 * 968a1ee3cee7769e2adb6c49db48dfe8836a0c76d91f05581076bf809690f772. */
int nba97_game_memory_zero(Nba97GameMemoryZeroContext*,
    Nba97GameMemoryZeroProgress*);

#ifdef __cplusplus
}
#endif
#endif
