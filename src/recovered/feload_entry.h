#ifndef NBA97_FELOAD_ENTRY_H
#define NBA97_FELOAD_ENTRY_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97FeloadEntryRegisterIndex {
    NBA97_FELOAD_R_ZERO = 0,
    NBA97_FELOAD_R_AT = 1,
    NBA97_FELOAD_R_V0 = 2,
    NBA97_FELOAD_R_V1 = 3,
    NBA97_FELOAD_R_A0 = 4,
    NBA97_FELOAD_R_A1 = 5,
    NBA97_FELOAD_R_A2 = 6,
    NBA97_FELOAD_R_A3 = 7,
    NBA97_FELOAD_R_T0 = 8,
    NBA97_FELOAD_R_T1 = 9,
    NBA97_FELOAD_R_T2 = 10,
    NBA97_FELOAD_R_T3 = 11,
    NBA97_FELOAD_R_T4 = 12,
    NBA97_FELOAD_R_T5 = 13,
    NBA97_FELOAD_R_T6 = 14,
    NBA97_FELOAD_R_T7 = 15,
    NBA97_FELOAD_R_S0 = 16,
    NBA97_FELOAD_R_S1 = 17,
    NBA97_FELOAD_R_S2 = 18,
    NBA97_FELOAD_R_S3 = 19,
    NBA97_FELOAD_R_S4 = 20,
    NBA97_FELOAD_R_S5 = 21,
    NBA97_FELOAD_R_S6 = 22,
    NBA97_FELOAD_R_S7 = 23,
    NBA97_FELOAD_R_T8 = 24,
    NBA97_FELOAD_R_T9 = 25,
    NBA97_FELOAD_R_K0 = 26,
    NBA97_FELOAD_R_K1 = 27,
    NBA97_FELOAD_R_GP = 28,
    NBA97_FELOAD_R_SP = 29,
    NBA97_FELOAD_R_S8 = 30,
    NBA97_FELOAD_R_RA = 31,
    NBA97_FELOAD_REGISTER_COUNT = 32
};

typedef struct Nba97FeloadEntryRegister {
    uint32_t word;
    uint8_t known;
} Nba97FeloadEntryRegister;

typedef struct Nba97FeloadEntryRegisters {
    Nba97FeloadEntryRegister gpr[NBA97_FELOAD_REGISTER_COUNT];
} Nba97FeloadEntryRegisters;

enum Nba97FeloadEntryAccessKind {
    NBA97_FELOAD_ENTRY_READ = 1,
    NBA97_FELOAD_ENTRY_WRITE = 2
};

typedef struct Nba97FeloadEntryAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    uint8_t width;
    uint8_t known_mask;
    uint8_t kind;
} Nba97FeloadEntryAccess;

typedef void (*Nba97FeloadEntryObserveAccess)(void*,
    const Nba97FeloadEntryAccess*);

enum Nba97FeloadEntryEventKind {
    NBA97_FELOAD_ENTRY_CHILD_801E1590 = 1,
    NBA97_FELOAD_ENTRY_CHILD_801E136C = 2
};

enum Nba97FeloadEntryCalleeOutcome {
    NBA97_FELOAD_ENTRY_CALLEE_RETURNED = 0,
    NBA97_FELOAD_ENTRY_CALLEE_TRANSFERRED = 1,
    NBA97_FELOAD_ENTRY_CALLEE_UNSET = 255
};

typedef struct Nba97FeloadEntryEvent {
    uint32_t pc;
    uint32_t entry;
    Nba97FeloadEntryRegisters registers;
    uint8_t kind;
    uint8_t argument_count;
} Nba97FeloadEntryEvent;

/* A callback executes the exact child named by event.entry. The supplied
 * registers start as the child's entry register file and may be replaced with
 * its live return/transfer state. Return 1 only after performing that boundary
 * and set outcome to RETURNED or TRANSFERRED; malformed register knownness or
 * an unset outcome is rejected. Mapped bytes/knownness may change synchronously. */
typedef int (*Nba97FeloadEntryIo)(void*, const Nba97GameTextMemory*,
    const Nba97FeloadEntryEvent*, Nba97FeloadEntryRegisters*,
    enum Nba97FeloadEntryCalleeOutcome*);

typedef struct Nba97FeloadEntryContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Counts attempted accesses and child calls. */
    Nba97FeloadEntryRegisters registers;
    Nba97FeloadEntryIo io;
    Nba97FeloadEntryObserveAccess observe_access;
    void* user;
} Nba97FeloadEntryContext;

typedef struct Nba97FeloadEntryProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t words_cleared;
    size_t callbacks_completed;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t heap_base;
    uint32_t heap_size;
    Nba97FeloadEntryRegister saved_return_address;
    Nba97FeloadEntryRegister restored_return_address;
    Nba97FeloadEntryRegisters registers;
    uint8_t first_child_entered;
    uint8_t second_child_entered;
    uint8_t delay_slot_completed;
    uint8_t transferred;
    uint8_t trapped;
    uint8_t completed;
} Nba97FeloadEntryProgress;

enum Nba97FeloadEntryResult {
    NBA97_FELOAD_ENTRY_BREAK_TRAP = -6,
    NBA97_FELOAD_ENTRY_ARITHMETIC_TRAP = -7
};

/*
 * PS1 SUBROUTINE
 * Program: FELOAD
 * Address: 0x801E1410
 * Range: 0x801E1410..0x801E14B7 (inclusive)
 * Source size: 168 bytes / 42 instructions
 * Evidence: Ghidra listing feload_entry_801e1410.txt, instruction-slice SHA-256 22bb7caff6b8fd97b13608b31ea7af7515c565dac67a989c418496c1818b0716; raw listing cross-checked with Capstone
 *
 * Purpose: Clear FELOAD BSS, establish its stack/heap/global-pointer state, and transfer through its two startup children.
 * Inputs: Full live MIPS GPR file; ra is saved, a2/a3 and untouched registers are forwarded, and guest words 0x801E8B70/0x801E8B6C configure stack and heap.
 * Returns: No ordinary source return; a transferred child exposes its live GPR state, while a returning second child reaches BREAK 1.
 * Guest memory: Ordered word clears across 0x801E903C..0x801EB087, reads 0x801E8B70 then 0x801E8B6C, stores 0x801E8B50 then 0x801E8B4C then 0x801E903C, and reloads 0x801E903C after the first child.
 * Calls: 0x801E1590 at 0x801E1498, then 0x801E136C at 0x801E14AC.
 * Original quirks: ADDI at 0x801E1440 traps on signed overflow; the 0x801E149C call delay slot uses trapping ADDI; a returning second child executes BREAK 1.
 * Native mapping: 32-bit guest addresses use validated retained-memory regions; unresolved children use the typed callback and no guest integer is cast to a host pointer.
 */
int nba97_feload_entry(Nba97FeloadEntryContext*,
    Nba97FeloadEntryProgress*);

#ifdef __cplusplus
}
#endif

#endif
