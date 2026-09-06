#ifndef NBA97_GAME_LATE_PERIOD_LIMITS_H
#define NBA97_GAME_LATE_PERIOD_LIMITS_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GameLatePeriodLimitsWord;
typedef Nba97GameMatchInitializeRegisters Nba97GameLatePeriodLimitsRegisters;

enum Nba97GameLatePeriodLimitsAccessKind {
    NBA97_GAME_LATE_PERIOD_LIMITS_READ = 1,
    NBA97_GAME_LATE_PERIOD_LIMITS_STORE = 2
};

typedef struct Nba97GameLatePeriodLimitsAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation; /* One-based mapped-access attempt order. */
    uint8_t width;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameLatePeriodLimitsAccess;

typedef struct Nba97GameLatePeriodLimitsContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped guest accesses. */
    Nba97GameLatePeriodLimitsRegisters registers;
    Nba97GameLatePeriodLimitsAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameLatePeriodLimitsContext;

typedef struct Nba97GameLatePeriodLimitsProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t access_events;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    Nba97GameLatePeriodLimitsWord clock;
    Nba97GameLatePeriodLimitsWord period;
    Nba97GameLatePeriodLimitsWord limit;
    Nba97GameLatePeriodLimitsWord home_before;
    Nba97GameLatePeriodLimitsWord away_before;
    Nba97GameLatePeriodLimitsRegisters registers;
    uint8_t selected_late_period_limit;
    uint8_t home_raised;
    uint8_t away_raised;
    uint8_t completed;
} Nba97GameLatePeriodLimitsProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80067550
 * Range: 0x80067550..0x800675E3 (inclusive)
 * Source size: 148 bytes / 37 instructions
 * Evidence: fresh Ghidra listing game_80067550.txt; routine SHA-256 85107c952ec4e23503c49127f7b40425964341a68a4d9453b3eae1adb6ef98f7
 *
 * Purpose: Clear the live minimum, then enforce the period-three/four late-clock minimum on the home and away retained halfwords.
 * Inputs: All 32 live MIPS GPRs; signed word 0x800FDB58, signed halfword 0x800FDB68, and unsigned halfwords 0x8010606C, 0x8001EE24, and 0x8001EEE8.
 * Returns: Final v0/v1/a0/a1/at source state, every untouched GPR unchanged, and the live ra consumed by JR.
 * Guest memory: Reads 0x800FDB58, always stores zero to 0x8010606C, conditionally reads 0x800FDB68, may store 5 or 4 to 0x8010606C, then reads the live limit/home/away halfwords and conditionally raises home and away in source order.
 * Calls: None observed.
 * Original quirks: The first zero store is the 0x80067564 BEQ delay slot and survives an unknown clock predicate; signed LW/LH decisions differ from unsigned LHU limits, branch-delay arithmetic overwrites v0, and all additions wrap at 32 bits.
 * Native mapping: Guest addresses stay uint32_t values and use validated retained regions with per-byte knownness; no guest integer is cast to a native pointer and no absent caller GPR is fabricated.
 */
int nba97_game_late_period_limits(Nba97GameLatePeriodLimitsContext*,
    Nba97GameLatePeriodLimitsProgress*);

#ifdef __cplusplus
}
#endif
#endif
