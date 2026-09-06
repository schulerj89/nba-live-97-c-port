#ifndef NBA97_GAME_ACTOR_INPUT_H
#define NBA97_GAME_ACTOR_INPUT_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameActorInputWord;
typedef Nba97GameMatchClocksMachine Nba97GameActorInputMachine;
typedef Nba97GameMatchClocksAccess Nba97GameActorInputAccess;

enum Nba97GameActorInputCallKind {
  NBA97_GAME_ACTOR_INPUT_CHILD_8008F224 = 1,
  NBA97_GAME_ACTOR_INPUT_CHILD_8002D2DC,
  NBA97_GAME_ACTOR_INPUT_CHILD_800700E4,
  NBA97_GAME_ACTOR_INPUT_CHILD_80061760,
  NBA97_GAME_ACTOR_INPUT_CHILD_80063B74,
  NBA97_GAME_ACTOR_INPUT_CHILD_8006FAC4,
  NBA97_GAME_ACTOR_INPUT_CHILD_800670A8,
  NBA97_GAME_ACTOR_INPUT_CHILD_8006AFB0,
  NBA97_GAME_ACTOR_INPUT_CHILD_8006C518,
  NBA97_GAME_ACTOR_INPUT_CHILD_8006B064,
  NBA97_GAME_ACTOR_INPUT_CHILD_8006C720,
  NBA97_GAME_ACTOR_INPUT_CHILD_8006B168,
  NBA97_GAME_ACTOR_INPUT_CHILD_8006CAE0,
  NBA97_GAME_ACTOR_INPUT_CHILD_800597EC,
  NBA97_GAME_ACTOR_INPUT_CHILD_8005853C,
  NBA97_GAME_ACTOR_INPUT_CHILD_8006CE60,
  NBA97_GAME_ACTOR_INPUT_CHILD_8006AC0C,
  NBA97_GAME_ACTOR_INPUT_CHILD_8006BD88,
  NBA97_GAME_ACTOR_INPUT_CHILD_8006B170,
  NBA97_GAME_ACTOR_INPUT_CHILD_8005C5E0,
  NBA97_GAME_ACTOR_INPUT_CHILD_80059F44,
  NBA97_GAME_ACTOR_INPUT_CHILD_8005B028,
  NBA97_GAME_ACTOR_INPUT_CHILD_8005C438,
  NBA97_GAME_ACTOR_INPUT_CHILD_80059968,
  NBA97_GAME_ACTOR_INPUT_CHILD_8005D070,
  NBA97_GAME_ACTOR_INPUT_CHILD_8005CF5C,
  NBA97_GAME_ACTOR_INPUT_CHILD_8005D9F0,
  NBA97_GAME_ACTOR_INPUT_CALL_KIND_COUNT
};

typedef struct Nba97GameActorInputEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameActorInputEvent;

/* The callback observes JAL ra and its completed delay instruction. It may
 * mutate every GPR, HI/LO, retained byte, saved frame word, and loop register.
 */
typedef int (*Nba97GameActorInputIo)(void *, const Nba97GameTextMemory *,
                                     const Nba97GameActorInputEvent *,
                                     Nba97GameActorInputMachine *);

typedef struct Nba97GameActorInputContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameActorInputMachine machine;
  Nba97GameActorInputIo io;
  void *user;
  Nba97GameActorInputAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameActorInputContext;

typedef struct Nba97GameActorInputProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_count[NBA97_GAME_ACTOR_INPUT_CALL_KIND_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  uint32_t computed_action_target;
  uint32_t instruction_count;
  Nba97GameActorInputWord restored_return_address;
  Nba97GameActorInputMachine machine;
  uint8_t completed;
} Nba97GameActorInputProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x800686B8
 * Range: 0x800686B8..0x80068BF7 (inclusive)
 * Source size: 1344 bytes / 336 instructions
 * Evidence: fresh Ghidra game_800686b8.txt; instruction SHA-256
 * 3e5499e7557606eb376943d7e759430c9193de244fdd3db9165e21404fe35c8d
 *
 * Purpose: Poll and map human controls, update actor input state, and dispatch
 * each of ten actors through its runtime action-table entry.
 * Inputs: All 32 GPRs and HI/LO; live SP; option/countdown, ten actor pointers,
 * team/controller/current pointers, actor fields, phase gates, and the 21-word
 * guest action table at 0x800275C4.
 * Returns: Callback-live machine state with ra and s8..s0 reloaded through live
 * SP, SP advanced by 0x48, and the restored live ra consumed by JR.
 * Guest memory: Reads and writes the 0x48 frame, countdown, actor/controller
 * and team fields, current pointer globals, ten actor slots, and action table
 * in exact source order.
 * Calls: 0x8008F224, 0x8002D2DC, 0x800700E4, 0x80061760,
 * 0x80063B74, 0x8006FAC4, then the computed cases 0x800670A8, 0x8006AFB0,
 * 0x8006C518, 0x8006B064, 0x8006C720, 0x8006B168, 0x8006CAE0, 0x800597EC,
 * 0x8005853C, 0x8006CE60, 0x8006AC0C, 0x8006BD88, 0x8006B170,
 * 0x8005C5E0, 0x80059F44, 0x8005B028, 0x8005C438, 0x80059968,
 * 0x8005D070, 0x8005CF5C, and 0x8005D9F0.
 * Original quirks: The option read precedes frame allocation; the BEQ delay
 * always saves s0; countdown and actor B6 wrap; signed and unsigned team/state
 * comparisons mix; the runtime table target, live loop index, S6 cursor, and
 * current controller pointer remain callback-mutable.
 * Native mapping: Guest addresses remain validated uint32_t mapped values. The
 * runtime table is read from guest memory; it is never replaced by a native
 * state-to-child lookup. Full-machine callbacks preserve aliases and prefixes.
 */
int nba97_game_actor_input(Nba97GameActorInputContext *,
                           Nba97GameActorInputProgress *);

#ifdef __cplusplus
}
#endif
#endif
