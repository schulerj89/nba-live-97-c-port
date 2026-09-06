#ifndef NBA97_GAME_SPEECH_STARTUP_H
#define NBA97_GAME_SPEECH_STARTUP_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GameSpeechStartupWord;
typedef Nba97GameMatchInitializeRegisters Nba97GameSpeechStartupRegisters;

enum Nba97GameSpeechStartupCallKind {
    NBA97_GAME_SPEECH_STARTUP_CHILD_8002A1B8 = 1,
    NBA97_GAME_SPEECH_STARTUP_CHILD_800853F4,
    NBA97_GAME_SPEECH_STARTUP_CHILD_800859C8,
    NBA97_GAME_SPEECH_STARTUP_CHILD_800889F4,
    NBA97_GAME_SPEECH_STARTUP_CHILD_80029CA0,
    NBA97_GAME_SPEECH_STARTUP_CHILD_80083D38,
    NBA97_GAME_SPEECH_STARTUP_CHILD_800ABFBC,
    NBA97_GAME_SPEECH_STARTUP_CHILD_80083EEC,
    NBA97_GAME_SPEECH_STARTUP_CHILD_800A5810,
    NBA97_GAME_SPEECH_STARTUP_CHILD_8008847C,
    NBA97_GAME_SPEECH_STARTUP_CHILD_8002ABB4,
    NBA97_GAME_SPEECH_STARTUP_CALL_KIND_COUNT
};

typedef struct Nba97GameSpeechStartupEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation; /* One-based parent access/call order. */
    size_t invocation; /* One-based invocation of this call kind. */
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameSpeechStartupEvent;

/* The callback observes all 32 live GPRs after JAL assigned ra and after its
 * delay slot completed. It may mutate retained memory and every live GPR.
 * Return 1 only when the original child boundary returned. */
typedef int (*Nba97GameSpeechStartupIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameSpeechStartupEvent*, Nba97GameSpeechStartupRegisters*);

enum Nba97GameSpeechStartupAccessKind {
    NBA97_GAME_SPEECH_STARTUP_READ = 1,
    NBA97_GAME_SPEECH_STARTUP_STORE = 2
};

typedef struct Nba97GameSpeechStartupAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameSpeechStartupAccess;

typedef struct Nba97GameSpeechStartupContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameSpeechStartupRegisters registers;
    Nba97GameSpeechStartupIo io;
    void* user;
    Nba97GameSpeechStartupAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameSpeechStartupContext;

typedef struct Nba97GameSpeechStartupProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t access_events;
    size_t call_count[NBA97_GAME_SPEECH_STARTUP_CALL_KIND_COUNT];
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    Nba97GameSpeechStartupWord language;
    Nba97GameSpeechStartupWord speech_handle;
    Nba97GameSpeechStartupWord published_voice;
    Nba97GameSpeechStartupWord deadline;
    Nba97GameSpeechStartupWord restored_return_address;
    Nba97GameSpeechStartupWord restored_s0;
    Nba97GameSpeechStartupRegisters registers;
    uint8_t completed;
} Nba97GameSpeechStartupProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x800800F8
 * Range: 0x800800F8..0x80080247 (inclusive)
 * Source size: 336 bytes / 84 instructions
 * Evidence: fresh Ghidra game_800800f8.txt; routine SHA-256 de945f95906f20d9632ddff0b12472b08b7296fc34c510edcb7c169727839628
 *
 * Purpose: Initialize localized speech playback, publish its voice handle, and pump the speech service until ready or the signed clock deadline expires.
 * Inputs: All 32 live MIPS GPRs, retained global/stack memory, language word at 0x80015018, and eleven typed child services.
 * Returns: Final cleanup-child v0 and all other live GPRs, with ra/s0 reloaded through child-mutable live sp and sp advanced by 0x20.
 * Guest memory: Saves ra/s0 at frame+0x1C/+0x18; clears 0x80103FB0 and 0x800C4568; reads language; stores/reloads 0x8002149C; writes the fifth 0x80029CA0 argument at live sp+0x10; publishes 0x800DC7E8; then reloads ra/s0 in source order.
 * Calls: 0x8002A1B8, 0x800853F4, 0x800859C8, 0x800889F4, 0x80029CA0, 0x80083D38, 0x800ABFBC, 0x80083EEC, 0x800A5810, repeated 0x8008847C/0x800A5810/0x80083EEC, and 0x8002ABB4.
 * Original quirks: Non-1/2 languages uniquely reload the live handle; the timeout is signed and strict after a wrapping +240 deadline; both loop exits clear a0 in branch delay slots; every child may replace s0/sp and saved stack words.
 * Native mapping: Guest addresses use validated retained regions with per-byte knownness; unresolved children remain typed callbacks and no audio, clock, or readiness result is fabricated.
 */
int nba97_game_speech_startup(Nba97GameSpeechStartupContext*,
    Nba97GameSpeechStartupProgress*);

#ifdef __cplusplus
}
#endif
#endif
