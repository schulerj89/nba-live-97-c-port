#ifndef NBA97_GAME_PERIOD_STARTUP_H
#define NBA97_GAME_PERIOD_STARTUP_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GamePeriodStartupWord;
typedef Nba97GameMatchInitializeRegisters Nba97GamePeriodStartupRegisters;

enum Nba97GamePeriodStartupCallKind {
    NBA97_GAME_PERIOD_STARTUP_PERIOD_INITIALIZE = 1,
    NBA97_GAME_PERIOD_STARTUP_PLAYER_ATTRIBUTES,
    NBA97_GAME_PERIOD_STARTUP_ZERO_PERIOD_SERVICE,
    NBA97_GAME_PERIOD_STARTUP_NONZERO_PERIOD_SERVICE,
    NBA97_GAME_PERIOD_STARTUP_A25C,
    NBA97_GAME_PERIOD_STARTUP_35318,
    NBA97_GAME_PERIOD_STARTUP_29590,
    NBA97_GAME_PERIOD_STARTUP_FRAME_PUMP,
    NBA97_GAME_PERIOD_STARTUP_76B28,
    NBA97_GAME_PERIOD_STARTUP_76B3C,
    NBA97_GAME_PERIOD_STARTUP_A584C,
    NBA97_GAME_PERIOD_STARTUP_35678
};

typedef struct Nba97GamePeriodStartupEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation; /* One-based mapped-access/call order. */
    uint8_t kind;
    uint8_t argument_count;
} Nba97GamePeriodStartupEvent;

/* The callback receives the complete GPR file after JAL writes ra and after
 * the delay instruction. It may synchronously mutate retained memory and every
 * live GPR. Return 1 only when the exact child boundary has returned. */
typedef int (*Nba97GamePeriodStartupIo)(void*, const Nba97GameTextMemory*,
    const Nba97GamePeriodStartupEvent*, Nba97GamePeriodStartupRegisters*);

enum Nba97GamePeriodStartupAccessKind {
    NBA97_GAME_PERIOD_STARTUP_READ = 1,
    NBA97_GAME_PERIOD_STARTUP_STORE = 2
};

typedef struct Nba97GamePeriodStartupAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask; /* One bit per little-endian source byte. */
    uint8_t kind;
} Nba97GamePeriodStartupAccess;

typedef struct Nba97GamePeriodStartupContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GamePeriodStartupRegisters registers;
    Nba97GamePeriodStartupIo io;
    void* user;
    Nba97GamePeriodStartupAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GamePeriodStartupContext;

typedef struct Nba97GamePeriodStartupProgress {
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
    Nba97GamePeriodStartupWord period_selector;
    Nba97GamePeriodStartupWord published_pointer;
    Nba97GamePeriodStartupWord optional_flag;
    Nba97GamePeriodStartupWord restored_return_address;
    Nba97GamePeriodStartupWord restored_s0;
    Nba97GamePeriodStartupRegisters registers;
    uint8_t used_nonzero_period_path;
    uint8_t optional_service_called;
    uint8_t completed;
} Nba97GamePeriodStartupProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80067468
 * Range: 0x80067468..0x8006754F (inclusive)
 * Source size: 232 bytes / 58 instructions
 * Evidence: fresh Ghidra listing game_80067468.txt; routine SHA-256 fc4dfb230d9560bd6c0b0fbd88158f007b77333f0f4ab43f651a75ea3bb1d426
 *
 * Purpose: Start a match period, publish its live ball/frame state, pump one frame, and dispatch repeated startup services.
 * Inputs: All 32 live MIPS GPRs; retained stack; signed halfword 0x800FDB68; pointer word 0x80020C14; unsigned halfword 0x8001EDEC; and fourteen typed child boundaries.
 * Returns: Final child/live GPRs with ra and s0 reloaded through the child-mutable stack, sp advanced by 0x18, and the restored ra used by JR.
 * Guest memory: Saves ra/s0 in entry sp-4/sp-8, reads 0x800FDB68 and 0x80020C14, stores live s0 at 0x800FDB92 before publishing the pointer at 0x800FDC48, stores live post-pump s0 at 0x800FDB6C, reads 0x8001EDEC, then reloads ra/s0 in source order.
 * Calls: 0x80065DB0, 0x80063EDC, one of 0x800673F0/0x80067194, 0x8002A25C, 0x80035318, 0x80029590, 0x8002DD84, 0x80076B28/0x80076B3C twice each, 0x800A584C twice, and optional 0x80035678.
 * Original quirks: The first JAL delay saves s0; later JAL delays assign a0/a1/s0; LH selects the branch as signed data while LHU accepts every nonzero bit pattern; child mutations of s0, sp, saved stack words, and all other GPRs remain live.
 * Native mapping: Guest addresses remain uint32_t and use validated retained regions; incompatible recovered children remain explicit full-GPR callbacks and no host pointer or absent ABI state is fabricated.
 */
int nba97_game_period_startup(Nba97GamePeriodStartupContext*,
    Nba97GamePeriodStartupProgress*);

#ifdef __cplusplus
}
#endif
#endif
