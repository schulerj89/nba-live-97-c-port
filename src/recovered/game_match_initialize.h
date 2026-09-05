#ifndef NBA97_GAME_MATCH_INITIALIZE_H
#define NBA97_GAME_MATCH_INITIALIZE_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameMatchInitializeRegister {
    NBA97_MATCH_INITIALIZE_ZERO = 0,
    NBA97_MATCH_INITIALIZE_AT = 1,
    NBA97_MATCH_INITIALIZE_V0 = 2,
    NBA97_MATCH_INITIALIZE_V1 = 3,
    NBA97_MATCH_INITIALIZE_A0 = 4,
    NBA97_MATCH_INITIALIZE_A1 = 5,
    NBA97_MATCH_INITIALIZE_A2 = 6,
    NBA97_MATCH_INITIALIZE_A3 = 7,
    NBA97_MATCH_INITIALIZE_T0 = 8,
    NBA97_MATCH_INITIALIZE_S0 = 16,
    NBA97_MATCH_INITIALIZE_T8 = 24,
    NBA97_MATCH_INITIALIZE_T9 = 25,
    NBA97_MATCH_INITIALIZE_K0 = 26,
    NBA97_MATCH_INITIALIZE_K1 = 27,
    NBA97_MATCH_INITIALIZE_GP = 28,
    NBA97_MATCH_INITIALIZE_SP = 29,
    NBA97_MATCH_INITIALIZE_FP = 30,
    NBA97_MATCH_INITIALIZE_RA = 31,
    NBA97_MATCH_INITIALIZE_REGISTER_COUNT = 32
};

typedef struct Nba97GameMatchInitializeWord {
    uint32_t word;
    uint8_t known_mask; /* One bit per little-endian source byte. */
} Nba97GameMatchInitializeWord;

typedef struct Nba97GameMatchInitializeRegisters {
    Nba97GameMatchInitializeWord gpr[NBA97_MATCH_INITIALIZE_REGISTER_COUNT];
} Nba97GameMatchInitializeRegisters;

enum Nba97GameMatchInitializeCallKind {
    NBA97_MATCH_INITIALIZE_MEMORY_ZERO = 1,
    NBA97_MATCH_INITIALIZE_CHILD_80063D58,
    NBA97_MATCH_INITIALIZE_CHILD_80029114,
    NBA97_MATCH_INITIALIZE_CHILD_8007FD40,
    NBA97_MATCH_INITIALIZE_CHILD_800294F8,
    NBA97_MATCH_INITIALIZE_CHILD_8002AB30,
    NBA97_MATCH_INITIALIZE_CHILD_800640D8,
    NBA97_MATCH_INITIALIZE_CHILD_800659F0,
    NBA97_MATCH_INITIALIZE_CHILD_80065DB0,
    NBA97_MATCH_INITIALIZE_CHILD_80031E00,
    NBA97_MATCH_INITIALIZE_CHILD_80038A18,
    NBA97_MATCH_INITIALIZE_CHILD_800763F4
};

typedef struct Nba97GameMatchInitializeEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation; /* One-based parent access/call order. */
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameMatchInitializeEvent;

/* The callback observes registers after JAL has assigned ra and after the
 * delay slot has executed. It may synchronously mutate retained memory and
 * every live GPR. Return 1 only when the original child boundary returned. */
typedef int (*Nba97GameMatchInitializeIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameMatchInitializeEvent*, Nba97GameMatchInitializeRegisters*);

enum Nba97GameMatchInitializeAccessKind {
    NBA97_MATCH_INITIALIZE_READ = 1,
    NBA97_MATCH_INITIALIZE_STORE = 2
};

typedef struct Nba97GameMatchInitializeAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameMatchInitializeAccess;

typedef struct Nba97GameMatchInitializeContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameMatchInitializeRegisters registers;
    Nba97GameMatchInitializeIo io;
    void* user;
    Nba97GameMatchInitializeAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameMatchInitializeContext;

typedef struct Nba97GameMatchInitializeProgress {
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
    Nba97GameMatchInitializeWord team_snapshot[2];
    Nba97GameMatchInitializeWord restored_return_address;
    Nba97GameMatchInitializeRegisters registers;
    uint8_t completed;
} Nba97GameMatchInitializeProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8002DB90
 * Range: 0x8002DB90..0x8002DC37 (inclusive)
 * Source size: 168 bytes / 42 instructions
 * Evidence: fresh Ghidra listing recovery_20260905_game_8002db90.txt; routine SHA-256 c1569d2ae6b58be97cd7511f5dd2bee7be70684d9e1fc9ba9abd3ad9f83ce6f3
 *
 * Purpose: Snapshot the selected teams, clear match state, and initialize eleven ordered match subsystems.
 * Inputs: All live MIPS GPRs; team words at 0x80021D74/0x80021D78; retained stack and match-state mappings.
 * Returns: Final child v0 and all other live GPRs, with ra reloaded from the live frame and sp advanced by 0x18.
 * Guest memory: Reads team words before the frame; writes ra at entry sp-8, snapshots at 0x80022084/0x80022ADC, zeroes 0x800FDB4C..0x800FE9C7 through 0x800A3A74, clears 0x80020C18, then reloads ra; exact source order is retained.
 * Calls: 0x800A3A74, 0x80063D58, 0x80029114, 0x8007FD40, 0x800294F8, 0x8002AB30, 0x800640D8, 0x800659F0, 0x80065DB0, 0x80031E00, 0x80038A18, 0x800763F4.
 * Original quirks: Team reads precede stack setup; a0=-1 is assigned in the 0x8002DC10 JAL delay slot; child and stack mutations remain live.
 * Native mapping: 32-bit guest addresses use validated retained regions; the zero-fill child is supplied by a narrow adapter and unresolved children remain typed callbacks.
 */
int nba97_game_match_initialize(Nba97GameMatchInitializeContext*,
    Nba97GameMatchInitializeProgress*);

#ifdef __cplusplus
}
#endif
#endif
