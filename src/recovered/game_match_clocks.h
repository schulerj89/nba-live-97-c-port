#ifndef NBA97_GAME_MATCH_CLOCKS_H
#define NBA97_GAME_MATCH_CLOCKS_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GameMatchClocksWord;
typedef Nba97GameMatchInitializeRegisters Nba97GameMatchClocksRegisters;

enum Nba97GameMatchClocksRegister {
    NBA97_GAME_MATCH_CLOCKS_S1 = NBA97_MATCH_INITIALIZE_S0 + 1,
    NBA97_GAME_MATCH_CLOCKS_S2 = NBA97_MATCH_INITIALIZE_S0 + 2
};

typedef struct Nba97GameMatchClocksMachine {
    Nba97GameMatchClocksRegisters registers;
    Nba97GameMatchClocksWord hi;
    Nba97GameMatchClocksWord lo;
} Nba97GameMatchClocksMachine;

enum Nba97GameMatchClocksCallKind {
    NBA97_GAME_MATCH_CLOCKS_CHILD_80029258 = 1,
    NBA97_GAME_MATCH_CLOCKS_CHILD_8007F9C4,
    NBA97_GAME_MATCH_CLOCKS_CALL_KIND_COUNT
};

typedef struct Nba97GameMatchClocksEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation;
    size_t invocation;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameMatchClocksEvent;

/* The callback observes JAL's ra and the completed delay slot, including the
 * entire 32-GPR file and HI/LO. It may mutate all machine state and retained
 * memory. Return exactly 1 only after the original child boundary returns. */
typedef int (*Nba97GameMatchClocksIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameMatchClocksEvent*, Nba97GameMatchClocksMachine*);

enum Nba97GameMatchClocksAccessKind {
    NBA97_GAME_MATCH_CLOCKS_READ = 1,
    NBA97_GAME_MATCH_CLOCKS_STORE = 2
};

typedef struct Nba97GameMatchClocksAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameMatchClocksAccess;

typedef struct Nba97GameMatchClocksMultiplyTrace {
    uint32_t pc;
    Nba97GameMatchClocksWord multiplicand;
    Nba97GameMatchClocksWord multiplier;
    Nba97GameMatchClocksWord hi;
    Nba97GameMatchClocksWord lo;
    Nba97GameMatchClocksWord mfhi;
    Nba97GameMatchClocksWord adjusted;
    Nba97GameMatchClocksWord shifted;
    Nba97GameMatchClocksWord sign;
    Nba97GameMatchClocksWord seconds;
} Nba97GameMatchClocksMultiplyTrace;

typedef struct Nba97GameMatchClocksContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameMatchClocksMachine machine;
    Nba97GameMatchClocksIo io;
    void* user;
    Nba97GameMatchClocksAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameMatchClocksContext;

typedef struct Nba97GameMatchClocksProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t access_events;
    size_t call_count[NBA97_GAME_MATCH_CLOCKS_CALL_KIND_COUNT];
    size_t multiply_count;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    Nba97GameMatchClocksWord restored_return_address;
    Nba97GameMatchClocksWord restored_s2;
    Nba97GameMatchClocksWord restored_s1;
    Nba97GameMatchClocksWord restored_s0;
    Nba97GameMatchClocksMultiplyTrace multiply[4];
    Nba97GameMatchClocksMachine machine;
    uint8_t main_clock_eligible;
    uint8_t shot_clock_eligible;
    uint8_t completed;
} Nba97GameMatchClocksProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80067A60
 * Range: 0x80067A60..0x80067D37 (inclusive)
 * Source size: 728 bytes / 182 instructions
 * Evidence: fresh Ghidra game_80067a60.txt; instruction-byte SHA-256 c98700c14432e8a6f74f4b1abb90b9ad82cd02cff4618117a1748a29341a354f
 *
 * Purpose: Gate and decrement the match, shot, and team clocks while dispatching exact threshold sound services.
 * Inputs: All 32 live MIPS GPRs, HI/LO, signed delta in a0, retained stack, clock/phase globals 0x800FDB58..0x800FDBA4, flags 0x80021D90/0x80021D92, and team timer/state halfwords 0x8001EEB4..0x8001EF7A.
 * Returns: Live GPR and HI/LO state with ra/s2/s1/s0 reloaded through mutable sp, sp advanced by 0x30, and restored ra consumed by JR.
 * Guest memory: Saves s2 before capturing a0, reads and writes all clock gates/timers in source order, stores the main and shot clock decrements in branch delay slots, clears 0x800FDB86, updates both team timers/states, and reloads the four saved words through live sp.
 * Calls: 0x80029258 at 0x80067B78, 0x80067BC8, and 0x80067CA8; 0x8007F9C4 at 0x80067B94 and 0x80067BB8.
 * Original quirks: Signed MULT by 0x88888889 and explicit HI/LO/MFHI/SRA/ADDU/SUBU implement division by 60; phase 0x81 pauses; phase 0x82 has extra live gates; shot seconds are compared after signed low-half truncation; positive team timers can underflow until the next invocation clamps them.
 * Native mapping: Guest addresses remain validated uint32_t retained-memory addresses; per-byte knownness and full mutable machine callbacks preserve unknown and failure prefixes without host-pointer casts or fabricated child results.
 */
int nba97_game_match_clocks(
    Nba97GameMatchClocksContext*, Nba97GameMatchClocksProgress*);

#ifdef __cplusplus
}
#endif
#endif
