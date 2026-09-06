#ifndef NBA97_GAME_ACTOR_RESUME_H
#define NBA97_GAME_ACTOR_RESUME_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameActorResumeWord;
typedef Nba97GameMatchClocksMachine Nba97GameActorResumeMachine;
typedef Nba97GameMatchClocksAccess Nba97GameActorResumeAccess;

enum Nba97GameActorResumeCallKind {
  NBA97_GAME_ACTOR_RESUME_CHILD_80056FFC = 1,
  NBA97_GAME_ACTOR_RESUME_CHILD_8005703C,
  NBA97_GAME_ACTOR_RESUME_CHILD_800582CC,
  NBA97_GAME_ACTOR_RESUME_CALL_KIND_COUNT
};

typedef struct Nba97GameActorResumeEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameActorResumeEvent;

/* The callback observes JAL ra and its completed delay slot. It may mutate
 * every GPR, HI/LO, retained memory, s0/sp, and saved frame words. */
typedef int (*Nba97GameActorResumeIo)(void *, const Nba97GameTextMemory *,
                                      const Nba97GameActorResumeEvent *,
                                      Nba97GameActorResumeMachine *);

typedef struct Nba97GameActorResumeContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameActorResumeMachine machine;
  Nba97GameActorResumeIo io;
  void *user;
  Nba97GameActorResumeAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameActorResumeContext;

typedef struct Nba97GameActorResumeProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_count[NBA97_GAME_ACTOR_RESUME_CALL_KIND_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  Nba97GameActorResumeWord restored_return_address;
  Nba97GameActorResumeWord restored_s0;
  Nba97GameActorResumeMachine machine;
  uint8_t completed;
} Nba97GameActorResumeProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x800582DC
 * Range: 0x800582DC..0x800583FB (inclusive)
 * Source size: 288 bytes / 72 instructions
 * Evidence: fresh Ghidra game_800582dc.txt; instruction SHA-256
 * a9ecb213007e80cf4fa52701ed0b7d61a6c462630092492659f1d5eb90612c8d
 *
 * Purpose: Resume or reset one actor's team state, animation flags, and copied
 * motion fields before invoking three actor services. Inputs: All 32 live MIPS
 * GPRs and HI/LO; actor pointer in a0, caller mode in a1, retained stack,
 * phase/team globals, and actor/nested-object fields. Returns: Raw final-child
 * register state with ra/s0 reloaded through live sp, sp advanced by 0x18, and
 * callback-mutated GPR/HI/LO state otherwise retained. Guest memory: Reads
 * phase 0x800FDB90 before the frame; saves s0/ra; reads and writes actor
 * offsets 0x1A,0x20,0x46,0x4A,0x4E,0x60,0x64,0x9A,0xA2,0xA6,0xB8,0xD9 and
 * nested pointer+0x0D in source order; reloads ra/s0. Calls: 0x80056FFC at
 * 0x80058374, 0x8005703C at 0x8005837C, and 0x800582CC at 0x800583E0. Original
 * quirks: Unsigned actor team bytes compare against signed halfwords; either
 * animation value below 37 forces a1=1; low flag bits alone gate the nested
 * pointer; the final 0xA6 store is the third JAL delay slot. Native mapping:
 * Guest addresses remain validated uint32_t retained-memory values; full
 * mutable machine callbacks and per-byte knownness preserve aliases and failure
 * prefixes without host-pointer casts.
 */
int nba97_game_actor_resume(Nba97GameActorResumeContext *,
                            Nba97GameActorResumeProgress *);

#ifdef __cplusplus
}
#endif
#endif
