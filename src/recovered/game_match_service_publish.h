#ifndef NBA97_GAME_MATCH_SERVICE_PUBLISH_H
#define NBA97_GAME_MATCH_SERVICE_PUBLISH_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameMatchServicePublishWord;
typedef Nba97GameMatchClocksMachine Nba97GameMatchServicePublishMachine;

enum Nba97GameMatchServicePublishCallKind {
    NBA97_GAME_MATCH_SERVICE_PUBLISH_CHILD_8002A264 = 1,
    NBA97_GAME_MATCH_SERVICE_PUBLISH_CALL_KIND_COUNT
};

typedef struct Nba97GameMatchServicePublishEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation;
    size_t invocation;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameMatchServicePublishEvent;

/* The callback observes JAL's ra after the NOP delay slot and the complete
 * 32-GPR/HI/LO machine. It may mutate that machine and retained memory.
 * Return exactly 1 only after the original child boundary has returned. */
typedef int (*Nba97GameMatchServicePublishIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameMatchServicePublishEvent*,
    Nba97GameMatchServicePublishMachine*);

enum Nba97GameMatchServicePublishAccessKind {
    NBA97_GAME_MATCH_SERVICE_PUBLISH_READ = 1,
    NBA97_GAME_MATCH_SERVICE_PUBLISH_STORE = 2
};

typedef struct Nba97GameMatchServicePublishAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameMatchServicePublishAccess;

typedef struct Nba97GameMatchServicePublishContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameMatchServicePublishMachine machine;
    Nba97GameMatchServicePublishIo io;
    void* user;
    Nba97GameMatchServicePublishAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameMatchServicePublishContext;

typedef struct Nba97GameMatchServicePublishProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t access_events;
    size_t call_count[NBA97_GAME_MATCH_SERVICE_PUBLISH_CALL_KIND_COUNT];
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    Nba97GameMatchServicePublishWord loaded_status;
    Nba97GameMatchServicePublishWord loaded_phase;
    Nba97GameMatchServicePublishWord saved_return_address;
    Nba97GameMatchServicePublishWord child_return_v0;
    Nba97GameMatchServicePublishWord child_return_v1;
    Nba97GameMatchServicePublishWord restored_return_address;
    Nba97GameMatchServicePublishMachine machine;
    uint8_t completed;
} Nba97GameMatchServicePublishProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8002DE34
 * Range: 0x8002DE34..0x8002DE73 (inclusive)
 * Source size: 64 bytes / 16 instructions
 * Evidence: fresh Ghidra game_8002de34.txt; instruction-byte SHA-256 a1df6b4753d88d815358610c1599c22296c972ddd6874312d286c8d1f14a3bc8
 *
 * Purpose: Publish the live match status and signed phase to service globals, invoke the match service child, and return through the mutable stack frame.
 * Inputs: All 32 live MIPS GPRs, HI/LO, retained halfwords at 0x800F9FFE and 0x800FDB90, and retained stack memory addressed by live sp.
 * Returns: The child's raw v0/v1 and all other live machine state, with ra reloaded through live sp, sp advanced by 0x18, and the restored ra consumed by JR.
 * Guest memory: Reads 0x800F9FFE and 0x800FDB90 before allocating the frame; saves ra at entry sp-8; stores status at 0x80015028 and sign-extended phase at 0x800170BC; reloads ra through child-mutable sp.
 * Calls: 0x8002A264 at 0x8002DE5C with no arguments and a NOP delay slot.
 * Original quirks: Source reads precede the stack save; native aliases may let publication overwrite saved ra; unknown loaded bytes propagate through stores; child mutations of sp, ra, GPRs, HI, LO, memory, and saved-ra bytes remain live.
 * Native mapping: Guest addresses remain uint32_t values resolved through validated retained regions with per-byte knownness; the unresolved child is a full-machine typed callback and no guest integer is cast to a host pointer.
 */
int nba97_game_match_service_publish(
    Nba97GameMatchServicePublishContext*,
    Nba97GameMatchServicePublishProgress*);

#ifdef __cplusplus
}
#endif
#endif
