#ifndef NBA97_GAME_STREAM_QUEUE_COUNT_H
#define NBA97_GAME_STREAM_QUEUE_COUNT_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameStreamQueueCountWord;
typedef Nba97GameMatchClocksMachine Nba97GameStreamQueueCountMachine;
typedef Nba97GameMatchClocksAccess Nba97GameStreamQueueCountAccess;

enum Nba97GameStreamQueueCountCallKind {
    NBA97_GAME_STREAM_QUEUE_COUNT_CHILD_80093D94 = 1,
    NBA97_GAME_STREAM_QUEUE_COUNT_CHILD_80093DD4,
    NBA97_GAME_STREAM_QUEUE_COUNT_CALL_KIND_COUNT
};

typedef struct Nba97GameStreamQueueCountEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation;
    size_t invocation;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameStreamQueueCountEvent;

/* Each callback observes JAL's ra after its NOP delay and the complete live
 * machine. It may mutate every GPR, HI/LO, retained memory, and stack words.
 * Return exactly 1 only after the selected original child has returned. */
typedef int (*Nba97GameStreamQueueCountIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameStreamQueueCountEvent*,
    Nba97GameStreamQueueCountMachine*);

typedef struct Nba97GameStreamQueueCountContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameStreamQueueCountMachine machine;
    Nba97GameStreamQueueCountIo io;
    void* user;
    Nba97GameStreamQueueCountAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameStreamQueueCountContext;

typedef struct Nba97GameStreamQueueCountProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t access_events;
    size_t call_count[NBA97_GAME_STREAM_QUEUE_COUNT_CALL_KIND_COUNT];
    size_t loop_iterations;
    size_t links_counted;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    Nba97GameStreamQueueCountWord initial_head;
    Nba97GameStreamQueueCountWord active_pointer;
    Nba97GameStreamQueueCountWord counter_after_increment;
    Nba97GameStreamQueueCountWord counter_after_decrement;
    Nba97GameStreamQueueCountWord saved_return_address;
    Nba97GameStreamQueueCountWord saved_s8;
    Nba97GameStreamQueueCountWord restored_return_address;
    Nba97GameStreamQueueCountWord restored_s8;
    Nba97GameStreamQueueCountWord returned_count;
    Nba97GameStreamQueueCountMachine machine;
    uint8_t completed;
} Nba97GameStreamQueueCountProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80084448
 * Range: 0x80084448..0x80084587 (inclusive)
 * Source size: 320 bytes / 80 instructions
 * Evidence: fresh Ghidra game_80084448.txt; instruction SHA-256 9c34b212baa379cc2205a2b9858b6cfe771c32d183acb141da987b39859a59c1
 *
 * Purpose: Lock the audio stream queue, count its nonterminal forward links, and unlock it while maintaining the live lock counter.
 * Inputs: All 32 live MIPS GPRs, HI/LO, retained stack, queue head at 0x800C43A0, lock counter at 0x800C4410, linked node words, and two typed no-argument services.
 * Returns: v0 is -1 when the initial head is null, otherwise the wrapped local link count loaded after unlock; ra/s8 reload through live s8-selected stack and all child-mutated machine state remains live.
 * Guest memory: Saves ra/s8, clears frame+0x14, reads and repeatedly traverses frame+0x10/node words in source order, increments then decrements 0x800C4410 with wrap, loads the result after unlock, and restores the frame.
 * Calls: 0x80093D94 at 0x8008447C, then 0x80093DD4 at 0x8008455C, both with no arguments and NOP delay slots.
 * Original quirks: Sentinels 0xFFFFFFFE/0xFFFFFFFF terminate without dereference; head is reread after locking; pointer and node words are deliberately reread; cycles run without cleanup until a native operation budget stops the exact prefix.
 * Native mapping: Guest addresses remain validated uint32_t retained-memory addresses; full mutable machine callbacks and per-byte knownness preserve aliases, wrapping, unknown decisions, and HI/LO without host-pointer casts.
 */
int nba97_game_stream_queue_count(Nba97GameStreamQueueCountContext*,
    Nba97GameStreamQueueCountProgress*);

#ifdef __cplusplus
}
#endif
#endif
