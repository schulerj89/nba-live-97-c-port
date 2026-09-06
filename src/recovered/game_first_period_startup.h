#ifndef NBA97_GAME_FIRST_PERIOD_STARTUP_H
#define NBA97_GAME_FIRST_PERIOD_STARTUP_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GameFirstPeriodStartupWord;
typedef Nba97GameMatchInitializeRegisters Nba97GameFirstPeriodStartupRegisters;

enum Nba97GameFirstPeriodStartupCallKind {
    NBA97_GAME_FIRST_PERIOD_STARTUP_295D0 = 1,
    NBA97_GAME_FIRST_PERIOD_STARTUP_2A244,
    NBA97_GAME_FIRST_PERIOD_STARTUP_FRAME_PUMP,
    NBA97_GAME_FIRST_PERIOD_STARTUP_2DDCC,
    NBA97_GAME_FIRST_PERIOD_STARTUP_2A254,
    NBA97_GAME_FIRST_PERIOD_STARTUP_65DB0,
    NBA97_GAME_FIRST_PERIOD_STARTUP_7EF4C
};

typedef struct Nba97GameFirstPeriodStartupEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation; /* One-based mapped-access/call order. */
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameFirstPeriodStartupEvent;

/* The callback receives the complete GPR file after JAL writes ra and after
 * the delay instruction. It may synchronously mutate retained memory and every
 * live GPR. Return 1 only when the exact child boundary has returned. */
typedef int (*Nba97GameFirstPeriodStartupIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameFirstPeriodStartupEvent*,
    Nba97GameFirstPeriodStartupRegisters*);

enum Nba97GameFirstPeriodStartupAccessKind {
    NBA97_GAME_FIRST_PERIOD_STARTUP_READ = 1,
    NBA97_GAME_FIRST_PERIOD_STARTUP_STORE = 2
};

typedef struct Nba97GameFirstPeriodStartupAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask; /* One bit per little-endian source byte. */
    uint8_t kind;
} Nba97GameFirstPeriodStartupAccess;

typedef struct Nba97GameFirstPeriodStartupContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameFirstPeriodStartupRegisters registers;
    Nba97GameFirstPeriodStartupIo io;
    void* user;
    Nba97GameFirstPeriodStartupAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameFirstPeriodStartupContext;

typedef struct Nba97GameFirstPeriodStartupProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t access_events;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    Nba97GameFirstPeriodStartupWord presentation_flag;
    Nba97GameFirstPeriodStartupWord restored_return_address;
    Nba97GameFirstPeriodStartupRegisters registers;
    uint8_t optional_presentation_executed;
    uint8_t completed;
} Nba97GameFirstPeriodStartupProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x800673F0
 * Range: 0x800673F0..0x80067467 (inclusive)
 * Source size: 120 bytes / 30 instructions
 * Evidence: fresh Ghidra listing game_800673f0.txt; routine SHA-256 bb4e8f79b42f234c61f2e6f1529ca92ded800e35cfdbeb4f2ce96dea0d0e5285
 *
 * Purpose: Run first-period setup, optionally pump and clear presentation state, then dispatch the tip-off startup services.
 * Inputs: All 32 live MIPS GPRs; retained stack; unsigned byte 0x800EB680; and seven typed child boundaries.
 * Returns: Final child/live GPRs with ra reloaded through the child-mutable stack, sp advanced by 0x18, and restored ra consumed by JR.
 * Guest memory: Saves ra at entry sp-8, reads 0x800EB680, optionally stores zero at 0x800FDB4E, always stores 0xFFFF at 0x800FDB94, then reloads ra from live sp+0x10; exact access order is retained.
 * Calls: 0x800295D0, 0x8002A244, optional 0x8002DD84 and 0x8002DDCC, 0x8002A254, 0x80065DB0, and 0x8007EF4C.
 * Original quirks: The presentation byte is unsigned and any known nonzero value takes the optional path; a0=1 is assigned in the 0x80067434 JAL delay; child mutations of sp, saved ra, and every GPR remain live.
 * Native mapping: Guest addresses remain uint32_t and use validated retained regions; incompatible narrow recovered-child APIs remain explicit full-GPR callbacks and no host pointer or absent ABI state is fabricated.
 */
int nba97_game_first_period_startup(Nba97GameFirstPeriodStartupContext*,
    Nba97GameFirstPeriodStartupProgress*);

#ifdef __cplusplus
}
#endif
#endif
