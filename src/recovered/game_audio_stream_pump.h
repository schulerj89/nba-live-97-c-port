#ifndef NBA97_GAME_AUDIO_STREAM_PUMP_H
#define NBA97_GAME_AUDIO_STREAM_PUMP_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GameAudioStreamPumpWord;
typedef Nba97GameMatchInitializeRegisters Nba97GameAudioStreamPumpRegisters;

enum Nba97GameAudioStreamPumpCallKind {
    NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_8008472C = 1,
    NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80086190,
    NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088018,
    NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_800840F0,
    NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088288,
    NBA97_GAME_AUDIO_STREAM_PUMP_CALL_KIND_COUNT
};

typedef struct Nba97GameAudioStreamPumpEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation; /* One-based parent access/call order. */
    size_t invocation; /* One-based invocation of this call kind. */
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameAudioStreamPumpEvent;

/* The callback observes all 32 live GPRs after JAL assigned ra and after its
 * delay-slot NOP. It may mutate retained memory and every live GPR. */
typedef int (*Nba97GameAudioStreamPumpIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameAudioStreamPumpEvent*, Nba97GameAudioStreamPumpRegisters*);

enum Nba97GameAudioStreamPumpAccessKind {
    NBA97_GAME_AUDIO_STREAM_PUMP_READ = 1,
    NBA97_GAME_AUDIO_STREAM_PUMP_STORE = 2
};

typedef struct Nba97GameAudioStreamPumpAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameAudioStreamPumpAccess;

typedef struct Nba97GameAudioStreamPumpContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameAudioStreamPumpRegisters registers;
    Nba97GameAudioStreamPumpIo io;
    void* user;
    Nba97GameAudioStreamPumpAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameAudioStreamPumpContext;

typedef struct Nba97GameAudioStreamPumpProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t access_events;
    size_t call_count[NBA97_GAME_AUDIO_STREAM_PUMP_CALL_KIND_COUNT];
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    Nba97GameAudioStreamPumpWord initial_status;
    Nba97GameAudioStreamPumpWord first_flags;
    Nba97GameAudioStreamPumpWord last_stream_status;
    Nba97GameAudioStreamPumpWord returned_value;
    Nba97GameAudioStreamPumpWord restored_return_address;
    Nba97GameAudioStreamPumpWord restored_s8;
    Nba97GameAudioStreamPumpRegisters registers;
    uint8_t completed;
} Nba97GameAudioStreamPumpProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80083EEC
 * Range: 0x80083EEC..0x800840EF (inclusive)
 * Source size: 516 bytes / 129 instructions
 * Evidence: fresh Ghidra game_80083eec.txt; routine SHA-256 9ba0ebb07d46228b3571c86cbf3bb9de39e23cb0af0e97ab80dabee1b8c2e4ab
 *
 * Purpose: Pump the active audio stream and route mode-specific terminal and handler statuses.
 * Inputs: All 32 live MIPS GPRs, retained stack/global memory, flags byte at 0x800C43B0, handle word at 0x800C438C, and five typed child services.
 * Returns: Zero on an initially negative gate result, otherwise the live frame+0x14 word, with ra/s8 reloaded through live s8 and sp restored from that same frame.
 * Guest memory: Saves ra/s8 at entry sp-4/-8, initializes live s8+0x14, repeatedly stores and separately reloads status at live s8+0x10, reads 0x800C43B0/0x800C438C, and reloads saved ra/s8 in source order.
 * Calls: 0x8008472C; repeated 0x80086190/0x80088018; mode 5 handler 0x800840F0; and the source-retained mode 4 handler 0x80088288.
 * Original quirks: Mode 5 separately reloads status for both handler and loop predicates and normally overwrites a handler return with zero on the terminal pass; mode 4 requires a positive first status reload and a less-than--9 second reload, a contradictory condition under stable memory that is preserved rather than repaired.
 * Native mapping: Guest addresses remain validated uint32_t values with little-endian per-byte knownness; all five original children are typed full-GPR callbacks and no audio algorithm is substituted.
 */
int nba97_game_audio_stream_pump(Nba97GameAudioStreamPumpContext*,
    Nba97GameAudioStreamPumpProgress*);

#ifdef __cplusplus
}
#endif
#endif
