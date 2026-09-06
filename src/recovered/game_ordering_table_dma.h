#ifndef NBA97_GAME_ORDERING_TABLE_DMA_H
#define NBA97_GAME_ORDERING_TABLE_DMA_H

#include "game_clear_ordering_table.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameClearOrderingTableWord Nba97GameOrderingTableDmaWord;
typedef Nba97GameClearOrderingTableRegisters
    Nba97GameOrderingTableDmaRegisters;
typedef Nba97GameClearOrderingTableMachine Nba97GameOrderingTableDmaMachine;

enum Nba97GameOrderingTableDmaCallKind {
    NBA97_GAME_ORDERING_TABLE_DMA_START = 1,
    NBA97_GAME_ORDERING_TABLE_DMA_WAIT,
    NBA97_GAME_ORDERING_TABLE_DMA_CALL_KIND_COUNT
};

typedef struct Nba97GameOrderingTableDmaEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation;
    size_t invocation;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameOrderingTableDmaEvent;

/* Each callback observes the complete machine after JAL writes ra and after
 * its NOP delay slot. It may mutate all GPRs, HI/LO, device registers, stack,
 * and retained RAM. Return exactly 1 after the original child returns. */
typedef int (*Nba97GameOrderingTableDmaIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameOrderingTableDmaEvent*,
    Nba97GameOrderingTableDmaMachine*);

enum Nba97GameOrderingTableDmaAccessKind {
    NBA97_GAME_ORDERING_TABLE_DMA_READ = 1,
    NBA97_GAME_ORDERING_TABLE_DMA_STORE = 2
};

typedef struct Nba97GameOrderingTableDmaAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameOrderingTableDmaAccess;

typedef struct Nba97GameOrderingTableDmaContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameOrderingTableDmaMachine machine;
    Nba97GameOrderingTableDmaIo io;
    void* user;
    Nba97GameOrderingTableDmaAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameOrderingTableDmaContext;

typedef struct Nba97GameOrderingTableDmaProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t access_events;
    size_t call_attempts[NBA97_GAME_ORDERING_TABLE_DMA_CALL_KIND_COUNT];
    size_t call_count[NBA97_GAME_ORDERING_TABLE_DMA_CALL_KIND_COUNT];
    size_t wait_iterations;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    Nba97GameOrderingTableDmaWord channel_control_address;
    Nba97GameOrderingTableDmaWord initial_channel_control;
    Nba97GameOrderingTableDmaWord transfer_start;
    Nba97GameOrderingTableDmaWord transfer_count;
    Nba97GameOrderingTableDmaWord started_channel_control;
    Nba97GameOrderingTableDmaWord initial_busy_mask;
    Nba97GameOrderingTableDmaWord last_wait_result;
    Nba97GameOrderingTableDmaWord last_busy_mask;
    Nba97GameOrderingTableDmaWord return_v0;
    Nba97GameOrderingTableDmaWord restored_return_address;
    Nba97GameOrderingTableDmaWord restored_s1;
    Nba97GameOrderingTableDmaWord restored_s0;
    Nba97GameOrderingTableDmaMachine machine;
    uint8_t completed;
} Nba97GameOrderingTableDmaProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8009A97C
 * Range: 0x8009A97C..0x8009AA63 (inclusive)
 * Source size: 232 bytes / 58 instructions
 * Evidence: fresh Ghidra game_8009a97c.txt; instruction-byte SHA-256 3c4579b04a6bc93731d4328d630c48510c4cb64bdb40694267946013017bfaaa
 *
 * Purpose: Program reverse ordering-table DMA and poll its hardware-busy state through the two SDK service boundaries.
 * Inputs: All 32 live MIPS GPRs, HI/LO, ordering-table address in a0, raw count in a1, retained stack, DMA pointer globals 0x800C56A4/0x800C56A8/0x800C56AC/0x800C56B0, their mapped targets, and typed children 0x8009BAFC/0x8009BB30.
 * Returns: Live GPR and HI/LO state with v0 equal to callback-mutable s0 on success or 0xFFFFFFFF after a nonzero wait result; ra/s1/s0 reload through live sp, sp advances by 0x20, and restored ra is consumed by JR.
 * Guest memory: Saves s0/ra/s1, sets bit 27 through 0x800C56B0, clears and starts the live channel-control register, writes a0+(a1<<2)-4 and raw a1 through the live DMA register pointers, repeatedly reloads channel control, then restores the frame.
 * Calls: 0x8009BAFC at 0x8009A9F0 followed, while busy, by repeated 0x8009BB30 calls at 0x8009AA1C; both have NOP delay slots and no explicit arguments.
 * Original quirks: Count zero programs a0-4; signed and wrapping counts are not validated; every branch delay overwrites v0; child mutations of sp/s0/s1 and pointer globals remain live; a perpetually busy device runs until the host operation budget stops its exact prefix.
 * Native mapping: RAM and device addresses remain validated uint32_t guest addresses with per-byte knownness; hardware services stay typed full-machine callbacks and no DMA side effect is fabricated.
 */
int nba97_game_ordering_table_dma(Nba97GameOrderingTableDmaContext*,
    Nba97GameOrderingTableDmaProgress*);

#ifdef __cplusplus
}
#endif
#endif
