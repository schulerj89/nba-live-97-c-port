#ifndef NBA97_GAME_MEMORY_COPY_H
#define NBA97_GAME_MEMORY_COPY_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameMemoryCopyContext {
    Nba97GameTextMemory memory;
    /* Counts attempted source memory accesses. Arithmetic is not counted. */
    size_t operation_budget;
    uint32_t source;
    uint32_t destination;
    uint32_t length;
} Nba97GameMemoryCopyContext;

typedef struct Nba97GameMemoryCopyProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    /* Actual instruction traffic. Aligned LWL/LWR or SWL/SWR pairs count
     * the same four bytes twice, exactly as the source bus accesses do. */
    size_t bytes_read;
    size_t bytes_stored;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t source;
    uint32_t destination;
    uint32_t requested_length;
    uint32_t working_source;
    uint32_t working_destination;
    uint32_t working_count;
    /* This routine returns alignment bits, not the destination pointer. */
    uint32_t return_v0;
    uint8_t return_v0_known;
    uint8_t backward;
    uint8_t unaligned;
    uint8_t completed;
} Nba97GameMemoryCopyProgress;

enum {
    NBA97_GAME_MEMORY_COPY_ARITHMETIC_TRAP = -6
};

/* Complete original GAMEONLY memmove-like helper 0x800AA468..0x800AA787.
 * Main reaches it at call PC 0x80029B94 with the loader-owned FELOAD image,
 * destination 0x801E0000, and the exact heap-request size returned by 90D60.
 * The routine chooses direction with signed address comparisons, snapshots
 * eight or four words before each grouped write, and uses real little-endian
 * LWL/LWR/SWL/SWR access pairs for unaligned spans and backward tail words.
 *
 * Those details are intentionally not replaced by host memmove: overlapping
 * aliases, access refusal prefixes, unknown-byte propagation, redundant
 * aligned partial-word traffic, and the unusual v0 alignment result remain
 * observable. The signed ADD instructions at 0x800AA65C/66C/670 can trap.
 * A signed-negative length can also wrap into a vast loop; operation_budget
 * exposes a bounded prefix instead of repairing that original-game behavior.
 * Unknown bytes require a destination knownness array so their state can be
 * represented; known=NULL continues to mean every byte is known.
 *
 * Fresh read-only Ghidra SHA-256 for all 800 bytes:
 * 2d9ed18f5de6fe3edc1fab9996769b418452b1c32eb3fd2cce7ed1f2b0c2350d. */
int nba97_game_memory_copy(Nba97GameMemoryCopyContext*,
    Nba97GameMemoryCopyProgress*);

#ifdef __cplusplus
}
#endif
#endif
