#ifndef NBA97_GAME_CLEAR_ORDERING_TABLE_H
#define NBA97_GAME_CLEAR_ORDERING_TABLE_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GameClearOrderingTableWord;
typedef Nba97GameMatchInitializeRegisters Nba97GameClearOrderingTableRegisters;

enum Nba97GameClearOrderingTableRegister {
    NBA97_GAME_CLEAR_ORDERING_TABLE_S1 = NBA97_MATCH_INITIALIZE_S0 + 1
};

typedef struct Nba97GameClearOrderingTableMachine {
    Nba97GameClearOrderingTableRegisters registers;
    Nba97GameClearOrderingTableWord hi;
    Nba97GameClearOrderingTableWord lo;
} Nba97GameClearOrderingTableMachine;

enum Nba97GameClearOrderingTableCallKind {
    NBA97_GAME_CLEAR_ORDERING_TABLE_DEBUG = 1,
    NBA97_GAME_CLEAR_ORDERING_TABLE_BACKEND,
    NBA97_GAME_CLEAR_ORDERING_TABLE_CALL_KIND_COUNT
};

typedef struct Nba97GameClearOrderingTableEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t target;
    size_t operation;
    size_t invocation;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameClearOrderingTableEvent;

/* The typed boundary observes the complete machine after JALR assigns ra and
 * its delay slot assigns the final argument. It may mutate every live GPR,
 * HI/LO, and retained byte. Return exactly 1 after the original child returns. */
typedef int (*Nba97GameClearOrderingTableIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameClearOrderingTableEvent*,
    Nba97GameClearOrderingTableMachine*);

enum Nba97GameClearOrderingTableAccessKind {
    NBA97_GAME_CLEAR_ORDERING_TABLE_READ = 1,
    NBA97_GAME_CLEAR_ORDERING_TABLE_STORE = 2
};

typedef struct Nba97GameClearOrderingTableAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameClearOrderingTableAccess;

typedef struct Nba97GameClearOrderingTableContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameClearOrderingTableMachine machine;
    Nba97GameClearOrderingTableIo io;
    void* user;
    Nba97GameClearOrderingTableAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameClearOrderingTableContext;

typedef struct Nba97GameClearOrderingTableProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t access_events;
    size_t call_attempts[NBA97_GAME_CLEAR_ORDERING_TABLE_CALL_KIND_COUNT];
    size_t call_count[NBA97_GAME_CLEAR_ORDERING_TABLE_CALL_KIND_COUNT];
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_target;
    uint32_t frame_stack_pointer;
    Nba97GameClearOrderingTableWord debug_level;
    Nba97GameClearOrderingTableWord debug_predicate;
    Nba97GameClearOrderingTableWord debug_target;
    Nba97GameClearOrderingTableWord dispatch_table;
    Nba97GameClearOrderingTableWord backend_target;
    Nba97GameClearOrderingTableWord ordering_table_head;
    Nba97GameClearOrderingTableWord return_v0;
    Nba97GameClearOrderingTableWord restored_return_address;
    Nba97GameClearOrderingTableWord restored_s1;
    Nba97GameClearOrderingTableWord restored_s0;
    Nba97GameClearOrderingTableMachine machine;
    uint8_t completed;
} Nba97GameClearOrderingTableProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80099960
 * Range: 0x80099960..0x800999F7 (inclusive)
 * Source size: 152 bytes / 38 instructions
 * Evidence: fresh Ghidra game_80099960.txt; instruction-byte SHA-256 083408a7237e02d799c70d934420dcff4a7abae0b49927919b183417ffd9750b
 *
 * Purpose: Dispatch the PsyQ ClearOTagR service and publish the CPU ordering-table head word.
 * Inputs: All 32 live MIPS GPRs, HI/LO, object pointer in a0, count in a1, retained stack, debug byte 0x800C55C2, debug target word 0x800C55BC, and backend table pointer 0x800C55B8.
 * Returns: Live GPR and HI/LO state with v0 set from callback-mutable s0, ra/s1/s0 restored through callback-mutable sp, sp advanced by 0x20, and restored ra consumed by JR.
 * Guest memory: Reads the debug gate before stack setup, saves ra/s1/s0, resolves both dynamic targets in source order, stores 0x000C567C through live s0 after the accepted backend, and reloads the three saved words.
 * Calls: Dynamic debug JALR at 0x800999A0 from 0x800C55BC with three arguments; dynamic backend JALR at 0x800999BC from [[0x800C55B8]+0x2C] with two arguments.
 * Original quirks: The branch delay always saves ra; debug callback mutation affects the later table lookup; count zero does not skip the backend or head store; backend v0 is discarded; callback mutations of sp/s0/s1/HI/LO remain live.
 * Native mapping: Guest pointers remain validated uint32_t addresses with per-byte knownness; dynamic services are typed callbacks and their initial retail targets are evidence rather than hardcoded behavior.
 */
int nba97_game_clear_ordering_table(
    Nba97GameClearOrderingTableContext*,
    Nba97GameClearOrderingTableProgress*);

#ifdef __cplusplus
}
#endif
#endif
