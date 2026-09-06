#ifndef NBA97_GAME_AUDIO_STREAM_STATUS_H
#define NBA97_GAME_AUDIO_STREAM_STATUS_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GameAudioStreamStatusWord;
typedef Nba97GameMatchInitializeRegisters Nba97GameAudioStreamStatusRegisters;

enum Nba97GameAudioStreamStatusBoundary {
    NBA97_GAME_AUDIO_STREAM_STATUS_BODY_BYTES = 196,
    NBA97_GAME_AUDIO_STREAM_STATUS_BODY_INSTRUCTIONS = 49,
    NBA97_GAME_AUDIO_STREAM_STATUS_SPAN_BYTES = 228,
    NBA97_GAME_AUDIO_STREAM_STATUS_SPAN_WORDS = 57,
    NBA97_GAME_AUDIO_STREAM_STATUS_EXCLUDED_PAIRS = 4
};

enum Nba97GameAudioStreamStatusAccessKind {
    NBA97_GAME_AUDIO_STREAM_STATUS_READ = 1,
    NBA97_GAME_AUDIO_STREAM_STATUS_STORE = 2
};

typedef struct Nba97GameAudioStreamStatusAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask; /* One bit per little-endian source byte. */
    uint8_t kind;
} Nba97GameAudioStreamStatusAccess;

typedef struct Nba97GameAudioStreamStatusContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses. */
    Nba97GameAudioStreamStatusRegisters registers;
    Nba97GameAudioStreamStatusAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameAudioStreamStatusContext;

typedef struct Nba97GameAudioStreamStatusProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t access_events;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t frame_stack_pointer;
    Nba97GameAudioStreamStatusWord first_flags;
    Nba97GameAudioStreamStatusWord busy;
    Nba97GameAudioStreamStatusWord second_flags;
    Nba97GameAudioStreamStatusWord third_flags;
    Nba97GameAudioStreamStatusWord returned_value;
    Nba97GameAudioStreamStatusWord restored_s8;
    Nba97GameAudioStreamStatusRegisters registers;
    uint8_t completed;
} Nba97GameAudioStreamStatusProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8008472C
 * Range: 0x8008472C..0x8008480F (inclusive)
 * Source size: Ghidra body 196 bytes / 49 instructions; full inclusive span 228 bytes / 57 instruction words
 * Evidence: fresh Ghidra game_8008472c.txt body SHA-256 6bb2afd423750ae33eb791180f4e0e1c9cb43c80163f7516db94a6b753ae11fd; full-span SHA-256 8a972b73dca8932a878dccf456b7d65969bb2ab00675a5cd56d41fe27ad4a96c
 *
 * Purpose: Report the source audio-stream gate status from live flags and busy bytes.
 * Inputs: All 32 live MIPS GPRs, retained stack memory, repeated flags byte reads at 0x800C43B0, and busy byte 0x800C43B1.
 * Returns: v0=-14 when flags bit 1 is clear, 4 when busy is nonzero, 3 when flags bits 1, 0, and 2 are set, otherwise 1; s8 is reloaded, sp is restored, and ra is consumed unchanged by JR.
 * Guest memory: Stores entry s8 at wrapped entry-sp-8, reads the live flags byte up to three separate times and busy once in source order, then reloads s8 through live sp.
 * Calls: None observed.
 * Original quirks: Repeated flags loads are not hoisted; return -14 is signed; the inclusive span contains unreachable redundant J 0x800847FC/NOP pairs at 0x80084760/64, 0x80084788/8C, 0x800847B8/BC, and 0x800847E8/EC outside the 49-instruction Ghidra body.
 * Native mapping: Guest addresses remain validated uint32_t values with little-endian per-byte knownness; excluded span words are retained as static boundary annotations and never claimed as executed PCs.
 */
int nba97_game_audio_stream_status(Nba97GameAudioStreamStatusContext*,
    Nba97GameAudioStreamStatusProgress*);

#ifdef __cplusplus
}
#endif
#endif
