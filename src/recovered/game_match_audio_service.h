#ifndef NBA97_GAME_MATCH_AUDIO_SERVICE_H
#define NBA97_GAME_MATCH_AUDIO_SERVICE_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameMatchAudioServiceWord;
typedef Nba97GameMatchClocksMachine Nba97GameMatchAudioServiceMachine;

enum Nba97GameMatchAudioServiceCallKind {
    NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_800A5810 = 1,
    NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8008472C,
    NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_80088D0C,
    NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8008847C,
    NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_80084588,
    NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_80083EEC,
    NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8002A46C,
    NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8002A0A8,
    NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_800AD9FC,
    NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8009DC10,
    NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8009F8D8,
    NBA97_GAME_MATCH_AUDIO_SERVICE_CALL_KIND_COUNT
};

typedef struct Nba97GameMatchAudioServiceEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation;
    size_t invocation;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameMatchAudioServiceEvent;

/* The callback observes JAL's ra and the completed delay slot. It may mutate
 * all 32 GPRs, HI/LO, retained memory, and saved stack words. Return exactly
 * 1 only after the original child boundary returns synchronously. */
typedef int (*Nba97GameMatchAudioServiceIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameMatchAudioServiceEvent*,
    Nba97GameMatchAudioServiceMachine*);

enum Nba97GameMatchAudioServiceAccessKind {
    NBA97_GAME_MATCH_AUDIO_SERVICE_READ = 1,
    NBA97_GAME_MATCH_AUDIO_SERVICE_STORE = 2
};

typedef struct Nba97GameMatchAudioServiceAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameMatchAudioServiceAccess;

typedef struct Nba97GameMatchAudioServiceContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameMatchAudioServiceMachine machine;
    Nba97GameMatchAudioServiceIo io;
    void* user;
    Nba97GameMatchAudioServiceAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameMatchAudioServiceContext;

typedef struct Nba97GameMatchAudioServiceProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t access_events;
    size_t call_count[NBA97_GAME_MATCH_AUDIO_SERVICE_CALL_KIND_COUNT];
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    Nba97GameMatchAudioServiceWord clock_delta;
    Nba97GameMatchAudioServiceWord restored_return_address;
    Nba97GameMatchAudioServiceWord restored_s1;
    Nba97GameMatchAudioServiceWord restored_s0;
    Nba97GameMatchAudioServiceMachine machine;
    uint8_t completed;
} Nba97GameMatchAudioServiceProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8002A264
 * Range: 0x8002A264..0x8002A463 (inclusive)
 * Source size: 512 bytes / 128 instructions
 * Evidence: fresh Ghidra game_8002a264.txt; instruction-byte SHA-256 68e4fe53f12e6dbedafb6b16b9afa76973d43663a95d8badc90ad4bfe120f573
 *
 * Purpose: Advance the match audio service clock and dispatch mode-specific stream, cue, and teardown services.
 * Inputs: All 32 live MIPS GPRs, HI/LO, retained stack, clock at 0x800E430C, audio state/timers at 0x800FDA0C..0x800FDA10, phase 0x800170BC, stream handle/status globals, and eleven typed child services.
 * Returns: Live child GPR and HI/LO state with ra/s1/s0 reloaded through mutable sp, sp advanced by 0x20, and restored ra consumed by JR.
 * Guest memory: Saves ra/s1/s0, publishes the new clock before mode dispatch, reads and writes audio state and timer globals in exact source order, stores through callback-live s0 in mode 2, and reloads saved words through live sp.
 * Calls: 0x800A5810 at 0x8002A270; 0x8008472C at 0x8002A2DC; 0x80088D0C at 0x8002A2EC; 0x8008847C at 0x8002A2FC; 0x80084588 at 0x8002A30C; 0x80083EEC at 0x8002A320; 0x8002A46C at 0x8002A388; 0x8002A0A8 at 0x8002A400; 0x800AD9FC at 0x8002A424; 0x8009DC10 at 0x8002A43C; 0x8009F8D8 at 0x8002A444.
 * Original quirks: Clock and timer subtraction wrap at 32 and 16 bits; signed modes outside 1..3 are ignored; mode-1 timer 480+ resets to 120 before subtraction; mode-3 readiness uses nested zero/signed tests; branch and JAL delay-slot register effects survive exits and unknown predicates.
 * Native mapping: Guest addresses remain validated uint32_t retained-memory addresses; per-byte knownness and full mutable machine callbacks preserve unknown and failure prefixes without host-pointer casts or fabricated child results.
 */
int nba97_game_match_audio_service(Nba97GameMatchAudioServiceContext*,
    Nba97GameMatchAudioServiceProgress*);

#ifdef __cplusplus
}
#endif
#endif
