#ifndef NBA97_GAME_AUDIO_STREAM_SERVICE_H
#define NBA97_GAME_AUDIO_STREAM_SERVICE_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GameAudioStreamServiceWord;
typedef Nba97GameMatchInitializeRegisters Nba97GameAudioStreamServiceRegisters;

enum Nba97GameAudioStreamServiceCallKind {
    NBA97_GAME_AUDIO_STREAM_SERVICE_CHILD_800861E4 = 1,
    NBA97_GAME_AUDIO_STREAM_SERVICE_CALL_KIND_COUNT
};

typedef struct Nba97GameAudioStreamServiceEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation; /* One-based parent access/call order. */
    size_t invocation;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameAudioStreamServiceEvent;

/* The callback observes all 32 live GPRs after JAL assigned ra and after its
 * delay-slot NOP. It may mutate retained memory and every live GPR. */
typedef int (*Nba97GameAudioStreamServiceIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameAudioStreamServiceEvent*,
    Nba97GameAudioStreamServiceRegisters*);

enum Nba97GameAudioStreamServiceAccessKind {
    NBA97_GAME_AUDIO_STREAM_SERVICE_READ = 1,
    NBA97_GAME_AUDIO_STREAM_SERVICE_STORE = 2
};

typedef struct Nba97GameAudioStreamServiceAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameAudioStreamServiceAccess;

typedef struct Nba97GameAudioStreamServiceContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameAudioStreamServiceRegisters registers;
    Nba97GameAudioStreamServiceIo io;
    void* user;
    Nba97GameAudioStreamServiceAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameAudioStreamServiceContext;

typedef struct Nba97GameAudioStreamServiceProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t access_events;
    size_t call_count[NBA97_GAME_AUDIO_STREAM_SERVICE_CALL_KIND_COUNT];
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    Nba97GameAudioStreamServiceWord global_pointer;
    Nba97GameAudioStreamServiceWord header_state;
    Nba97GameAudioStreamServiceWord returned_value;
    Nba97GameAudioStreamServiceWord restored_return_address;
    Nba97GameAudioStreamServiceWord restored_s8;
    Nba97GameAudioStreamServiceRegisters registers;
    uint8_t completed;
} Nba97GameAudioStreamServiceProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80086190
 * Range: 0x80086190..0x800861E3 (inclusive)
 * Source size: 84 bytes / 21 instructions
 * Evidence: fresh Ghidra game_80086190.txt; routine SHA-256 b83c6d9aff01ad310de9d79ab81294ebfb885942103169424a50f7fa76da2b80
 *
 * Purpose: Service the live audio-stream header unless its state word is exactly one.
 * Inputs: All 32 live MIPS GPRs, retained stack memory, the live header pointer at 0x8010473C and its state word at pointer+0x24, and typed child 0x800861E4.
 * Returns: v0=1 when the live state is exactly one, otherwise the raw child v0; ra/s8 are reloaded through callback-live s8 and sp is restored from that frame.
 * Guest memory: Saves ra/s8 at entry sp-4/-8, reads 0x8010473C then the unchecked wrapping pointer+0x24, and reloads saved ra/s8 in source order.
 * Calls: 0x800861E4 at 0x800861C4 with NOP delay, only when the live state is not exactly one.
 * Original quirks: ORI publishes v0=1 before the state branch; child-mutated s8 selects the epilogue frame even when child sp differs; null, wrapping, and aliased header pointers are unchecked.
 * Native mapping: Guest addresses remain uint32_t values resolved only through validated retained-memory regions with little-endian per-byte knownness; adjacent 0x800861E4 remains a typed dependency.
 */
int nba97_game_audio_stream_service(Nba97GameAudioStreamServiceContext*,
    Nba97GameAudioStreamServiceProgress*);

#ifdef __cplusplus
}
#endif
#endif
