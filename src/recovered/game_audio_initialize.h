#ifndef NBA97_GAME_AUDIO_INITIALIZE_H
#define NBA97_GAME_AUDIO_INITIALIZE_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GameAudioInitializeWord;
typedef Nba97GameMatchInitializeRegisters Nba97GameAudioInitializeRegisters;

enum Nba97GameAudioInitializeEventKind {
    NBA97_GAME_AUDIO_INITIALIZE_HEAP_RELEASE = 1,
    NBA97_GAME_AUDIO_INITIALIZE_RESOURCE_LOAD,
    NBA97_GAME_AUDIO_INITIALIZE_F4F0,
    NBA97_GAME_AUDIO_INITIALIZE_ADB48,
    NBA97_GAME_AUDIO_INITIALIZE_CDC0,
    NBA97_GAME_AUDIO_INITIALIZE_CC28,
    NBA97_GAME_AUDIO_INITIALIZE_BANK_UPLOAD,
    NBA97_GAME_AUDIO_INITIALIZE_MASTER_VOLUME,
    NBA97_GAME_AUDIO_INITIALIZE_CHANNEL_VOLUME
};

typedef struct Nba97GameAudioInitializeEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation; /* One-based mapped-access/call order. */
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameAudioInitializeEvent;

/* The callback observes all GPRs after JAL writes ra and after the delay slot.
 * It may synchronously mutate retained memory and every live GPR. Return 1
 * only when the original child boundary returned. */
typedef int (*Nba97GameAudioInitializeIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameAudioInitializeEvent*, Nba97GameAudioInitializeRegisters*);

enum Nba97GameAudioInitializeAccessKind {
    NBA97_GAME_AUDIO_INITIALIZE_READ = 1,
    NBA97_GAME_AUDIO_INITIALIZE_STORE = 2
};

typedef struct Nba97GameAudioInitializeAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask; /* One bit per little-endian source byte. */
    uint8_t kind;
} Nba97GameAudioInitializeAccess;

typedef struct Nba97GameAudioInitializeContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameAudioInitializeRegisters registers;
    Nba97GameAudioInitializeIo io;
    void* user;
    Nba97GameAudioInitializeAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameAudioInitializeContext;

typedef struct Nba97GameAudioInitializeProgress {
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
    Nba97GameAudioInitializeWord old_bank_header;
    Nba97GameAudioInitializeWord new_bank_header;
    Nba97GameAudioInitializeWord bank_body;
    Nba97GameAudioInitializeWord volume_setting;
    Nba97GameAudioInitializeWord scaled_volume;
    Nba97GameAudioInitializeWord raw_volume_return;
    Nba97GameAudioInitializeWord restored_return_address;
    Nba97GameAudioInitializeWord restored_s0;
    Nba97GameAudioInitializeRegisters registers;
    uint8_t completed;
} Nba97GameAudioInitializeProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80029114
 * Range: 0x80029114..0x800291FF (inclusive)
 * Source size: 236 bytes / 59 instructions
 * Evidence: fresh Ghidra listing game_80029114.txt; routine SHA-256 71cd42b84299dd4d1b730301841a8897bf119088269cfcdf9b1929b928cb249f
 *
 * Purpose: Replace the gameplay sound bank, initialize its audio services, and apply the clamped user volume.
 * Inputs: All live MIPS GPRs; old header at 0x8001502C; settings byte at 0x80021D7C; retained stack, bank-header, and result mappings.
 * Returns: Raw 0x80088E84 v0 and all other live GPRs, with ra/s0 reloaded through the live frame and sp advanced by 0x18.
 * Guest memory: Reads 0x8001502C before framing and again before upload, stores the new header there, reads 0x80021D7C, stores raw v0 at 0x80021EE0, and saves/reloads ra/s0 in exact source order.
 * Calls: optional 0x80090698; 0x80029BFC twice; 0x8008F4F0; 0x800ADB48; 0x8008CDC0; 0x8008CC28; 0x800AD360; 0x80090698; 0x800ACA08; 0x80088E84.
 * Original quirks: The branch-delay s0 save is unconditional; the second load result moves to s0 in a later JAL delay slot; volume is unsigned byte times 15 clamped to 127; raw final v0 is stored and returned.
 * Native mapping: 32-bit guest addresses use validated retained regions; complete compatible children may be composed by a narrow adapter and all others remain typed callbacks.
 */
int nba97_game_audio_initialize(Nba97GameAudioInitializeContext*,
    Nba97GameAudioInitializeProgress*);

#ifdef __cplusplus
}
#endif
#endif
