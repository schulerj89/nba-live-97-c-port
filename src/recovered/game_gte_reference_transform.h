#ifndef NBA97_GAME_GTE_REFERENCE_TRANSFORM_H
#define NBA97_GAME_GTE_REFERENCE_TRANSFORM_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameGteReferenceTransformWord;
typedef Nba97GameMatchClocksMachine Nba97GameGteReferenceTransformMachine;
typedef Nba97GameMatchClocksAccess Nba97GameGteReferenceTransformAccess;

enum {
  NBA97_GAME_GTE_REFERENCE_TRANSFORM_CONTROL_COUNT = 32,
  NBA97_GAME_GTE_REFERENCE_TRANSFORM_DATA_COUNT = 32
};

typedef struct Nba97GameGteReferenceTransformState {
  Nba97GameGteReferenceTransformWord
      control[NBA97_GAME_GTE_REFERENCE_TRANSFORM_CONTROL_COUNT];
  Nba97GameGteReferenceTransformWord
      data[NBA97_GAME_GTE_REFERENCE_TRANSFORM_DATA_COUNT];
} Nba97GameGteReferenceTransformState;

typedef struct Nba97GameGteReferenceTransformHardwareEvent {
  uint32_t pc;
  uint32_t command;
  size_t operation;
  size_t invocation;
} Nba97GameGteReferenceTransformHardwareEvent;

/* The callback executes only COP2 0x480012 against the retained GTE bank. It
 * cannot mutate the CPU machine or guest memory. Return an exact NBA97_TEXT_*
 * result; COMPLETE publishes the returned bank after canonical validation. */
typedef int (*Nba97GameGteReferenceTransformHardware)(
    void *, const Nba97GameGteReferenceTransformHardwareEvent *,
    Nba97GameGteReferenceTransformState *);

typedef struct Nba97GameGteReferenceTransformContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameGteReferenceTransformMachine machine;
  Nba97GameGteReferenceTransformState state;
  Nba97GameGteReferenceTransformHardware hardware;
  void *hardware_user;
  Nba97GameGteReferenceTransformAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameGteReferenceTransformContext;

typedef struct Nba97GameGteReferenceTransformProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t hardware_calls;
  size_t hardware_completed;
  size_t access_events;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_command;
  Nba97GameGteReferenceTransformMachine machine;
  Nba97GameGteReferenceTransformState state;
  uint8_t completed;
} Nba97GameGteReferenceTransformProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80056650
 * Range: 0x80056650..0x80056677 (inclusive)
 * Source size: 40 bytes / 10 instructions
 * Evidence: fresh Ghidra game_80056650.txt; instruction SHA-256
 * b821ac83a86822b7f7f86d144c71f923b2252469c4e5be70d445dcb90af2a68b
 *
 * Purpose: Load one reference vector, transform it through the retained GTE,
 * and store MAC1..3 plus FLAG.
 * Inputs: Full 32-GPR/HI-LO machine, explicit GTE controls/data, and mapped
 * guest memory addressed by a0/a1/a2.
 * Returns: CPU v0 receives raw FLAG; every other CPU GPR and HI/LO is
 * preserved. The final FLAG store executes in the jr ra delay slot.
 * Guest memory: Reads words at a0+0/a0+4, stores MAC1..3 at a1+0/+4/+8,
 * then stores FLAG at a2+0, preserving aliases and uint32 address wrap.
 * Calls: No PS1 subroutine callees; COP2 0x480012 executes at 0x8005665C.
 * Original quirks: The second full load validates all four bytes before its
 * low half is sign-extended as V0Z; raw MAC endpoints differ from saturated IR.
 * Native mapping: A typed hardware callback composes the retained GTE bank;
 * byte knownness and access prefixes are preserved without host pointer casts.
 */
int nba97_game_gte_reference_transform(
    Nba97GameGteReferenceTransformContext *,
    Nba97GameGteReferenceTransformProgress *);

#ifdef __cplusplus
}
#endif
#endif
