#ifndef NBA97_GAME_STREAM_READINESS_H
#define NBA97_GAME_STREAM_READINESS_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameStreamReadinessWord;
typedef Nba97GameMatchClocksMachine Nba97GameStreamReadinessMachine;
typedef Nba97GameMatchClocksAccess Nba97GameStreamReadinessAccess;

enum Nba97GameStreamReadinessCallKind {
    NBA97_GAME_STREAM_READINESS_CHILD_80084448 = 1,
    NBA97_GAME_STREAM_READINESS_CALL_KIND_COUNT
};

typedef struct Nba97GameStreamReadinessEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation;
    size_t invocation;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameStreamReadinessEvent;

/* The callback observes JAL's ra after its NOP delay and the complete live
 * machine. It may mutate every GPR, HI/LO, retained memory, and saved frame.
 * Return exactly 1 only after 0x80084448 has returned synchronously. */
typedef int (*Nba97GameStreamReadinessIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameStreamReadinessEvent*,
    Nba97GameStreamReadinessMachine*);

typedef struct Nba97GameStreamReadinessContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameStreamReadinessMachine machine;
    Nba97GameStreamReadinessIo io;
    void* user;
    Nba97GameStreamReadinessAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameStreamReadinessContext;

typedef struct Nba97GameStreamReadinessProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t access_events;
    size_t call_count[NBA97_GAME_STREAM_READINESS_CALL_KIND_COUNT];
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    Nba97GameStreamReadinessWord loaded_flag;
    Nba97GameStreamReadinessWord saved_return_address;
    Nba97GameStreamReadinessWord saved_s8;
    Nba97GameStreamReadinessWord restored_return_address;
    Nba97GameStreamReadinessWord restored_s8;
    Nba97GameStreamReadinessMachine machine;
    uint8_t completed;
} Nba97GameStreamReadinessProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80088D0C
 * Range: 0x80088D0C..0x80088D7B (inclusive)
 * Source size: 112 bytes / 28 words; Ghidra body 104 bytes / 26 instructions
 * Evidence: fresh Ghidra game_80088d0c.txt body SHA-256 abb0ef9b5d11169191ecf6b120604cfd96fa6b60c37c2dcc69a3c87931a7545c; raw full-span SHA-256 e23900b4a5d3149a9da1e4d35da1cc34ec44377f1fd4fba1a3539c70d8660e76
 *
 * Purpose: Report whether an enabled audio stream has a child readiness value below signed two.
 * Inputs: All 32 live MIPS GPRs, HI/LO, retained stack, signed halfword flag at 0x800F0FDC, and the typed 0x80084448 service.
 * Returns: v0 is zero for a disabled flag or child result >=2 and one for child result <2, including negative results; ra/s8 reload through mutable sp selected from live s8, sp advances by 0x18, and all other child-mutated machine state remains live.
 * Guest memory: Saves ra at entry sp-4 and s8 at entry sp-8, reads 0x800F0FDC, then reloads ra and s8 through callback-mutable s8/sp in that order.
 * Calls: 0x80084448 at 0x80088D30 with no arguments and a NOP delay slot.
 * Original quirks: Signed negative child values mean ready; words at 0x80088D50/0x80088D54 are an unreachable duplicate J/NOP pair inside the full span.
 * Native mapping: Guest addresses stay validated uint32_t retained-memory addresses; full mutable machine callbacks and per-byte knownness preserve aliases, unknown branches, exact prefixes, and HI/LO without host-pointer casts.
 */
int nba97_game_stream_readiness(Nba97GameStreamReadinessContext*,
    Nba97GameStreamReadinessProgress*);

#ifdef __cplusplus
}
#endif
#endif
