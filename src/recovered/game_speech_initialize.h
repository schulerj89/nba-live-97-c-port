#ifndef NBA97_GAME_SPEECH_INITIALIZE_H
#define NBA97_GAME_SPEECH_INITIALIZE_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GameSpeechInitializeWord;
typedef Nba97GameMatchInitializeRegisters Nba97GameSpeechInitializeRegisters;

enum Nba97GameSpeechInitializeCallKind {
    NBA97_GAME_SPEECH_INITIALIZE_RESOURCE_LOAD_80029BFC = 1,
    NBA97_GAME_SPEECH_INITIALIZE_INSTALL_800ADBF8,
    NBA97_GAME_SPEECH_INITIALIZE_OPEN_800AEC00,
    NBA97_GAME_SPEECH_INITIALIZE_RESET_8007FA50,
    NBA97_GAME_SPEECH_INITIALIZE_LOOKUP_8007FC08,
    NBA97_GAME_SPEECH_INITIALIZE_ALLOCATE_80090160,
    NBA97_GAME_SPEECH_INITIALIZE_COPY_8009CB0C,
    NBA97_GAME_SPEECH_INITIALIZE_CONVERT_800AE54C,
    NBA97_GAME_SPEECH_INITIALIZE_RELEASE_80090698
};

typedef struct Nba97GameSpeechInitializeEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation;
    size_t invocation;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameSpeechInitializeEvent;

/* The callback observes all 32 GPRs after JAL writes ra and its delay slot
 * executes. It may mutate every GPR and retained memory. Return 1 only after
 * the original child boundary has returned. */
typedef int (*Nba97GameSpeechInitializeIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameSpeechInitializeEvent*,
    Nba97GameSpeechInitializeRegisters*);

enum Nba97GameSpeechInitializeAccessKind {
    NBA97_GAME_SPEECH_INITIALIZE_READ = 1,
    NBA97_GAME_SPEECH_INITIALIZE_STORE = 2
};

typedef struct Nba97GameSpeechInitializeAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameSpeechInitializeAccess;

typedef struct Nba97GameSpeechInitializeContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameSpeechInitializeRegisters registers;
    Nba97GameSpeechInitializeIo io;
    void* user;
    Nba97GameSpeechInitializeAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameSpeechInitializeContext;

typedef struct Nba97GameSpeechInitializeProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t access_events;
    size_t call_count[10]; /* Indexed by Nba97GameSpeechInitializeCallKind. */
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    Nba97GameSpeechInitializeWord first_language;
    Nba97GameSpeechInitializeWord second_language;
    Nba97GameSpeechInitializeWord index_payload;
    Nba97GameSpeechInitializeWord allocation_size;
    Nba97GameSpeechInitializeWord allocated_buffer;
    Nba97GameSpeechInitializeWord restored_register[10];
    Nba97GameSpeechInitializeRegisters registers;
    uint8_t completed;
} Nba97GameSpeechInitializeProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8007FD40
 * Range: 0x8007FD40..0x800800F7 (inclusive)
 * Source size: 952 bytes / 238 instructions
 * Evidence: fresh Ghidra game_8007fd40.txt; routine SHA-256 d1ffbb2ca744f160f0a0b7f37955b936b64cbb7adb0117a5f70a6a70843d9a94
 *
 * Purpose: Load language-specific speech indexes, build 110 speech records, pack and convert the first 100 payloads, and release the index.
 * Inputs: All 32 live MIPS GPRs; language at 0x80015018; team IDs, roster-pointer tables, roster IDs/bytes, retained speech records, stack memory, and typed child services.
 * Returns: Final release-child v0 and all live GPRs, with ra/s8..s0 reloaded in source order through live sp and sp advanced by 0x38.
 * Guest memory: Reads language twice; publishes auxiliary/index/allocation pointers; reads team/roster data; writes lookup records at 0x80102FE0, ten sentinels, packed pointers and converted handles; saves and reloads ten stack words in exact order.
 * Calls: 0x80029BFC three times per execution across five static call sites; 0x800ADBF8; 0x800AEC00 twice; 0x8007FA50; 0x8007FC08 one hundred times; 0x80090160; conditional 0x8009CB0C/0x800AE54C pairs; and 0x80090698.
 * Original quirks: The first language read precedes frame setup; the second is live; roster IDs and category bytes are signed while team IDs are unsigned; loop registers and saved stack are child-mutable; sums and addresses wrap; null allocation is unchecked.
 * Native mapping: Guest addresses remain uint32_t and use validated retained regions; 0x80029BFC may be composed through its recovered owner and all other children remain full-GPR typed callbacks.
 */
int nba97_game_speech_initialize(Nba97GameSpeechInitializeContext*,
    Nba97GameSpeechInitializeProgress*);

#ifdef __cplusplus
}
#endif
#endif
