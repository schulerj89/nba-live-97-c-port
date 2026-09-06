#ifndef NBA97_GAME_ACTOR_COLLISION_RESPONSE_H
#define NBA97_GAME_ACTOR_COLLISION_RESPONSE_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameActorCollisionResponseWord;
typedef Nba97GameMatchClocksMachine Nba97GameActorCollisionResponseMachine;
typedef Nba97GameMatchClocksAccess Nba97GameActorCollisionResponseAccess;

enum Nba97GameActorCollisionResponseCallKind {
  NBA97_GAME_ACTOR_COLLISION_RESPONSE_GEOMETRY_8007066C = 1,
  NBA97_GAME_ACTOR_COLLISION_RESPONSE_RESOLVE_8005EA28,
  NBA97_GAME_ACTOR_COLLISION_RESPONSE_ANGLE_800706E4,
  NBA97_GAME_ACTOR_COLLISION_RESPONSE_ANIMATION_80056B78,
  NBA97_GAME_ACTOR_COLLISION_RESPONSE_ANIMATION_80056CE0,
  NBA97_GAME_ACTOR_COLLISION_RESPONSE_ANIMATION_80056C28,
  NBA97_GAME_ACTOR_COLLISION_RESPONSE_ANIMATION_80056C84,
  NBA97_GAME_ACTOR_COLLISION_RESPONSE_CALL_KIND_COUNT
};

typedef struct Nba97GameActorCollisionResponseEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameActorCollisionResponseEvent;

/* The callback observes JAL ra and the completed delay instruction. It may
 * mutate every GPR, HI/LO, retained byte, live SP, saved frame word, or actor
 * alias before execution resumes. */
typedef int (*Nba97GameActorCollisionResponseIo)(
    void *, const Nba97GameTextMemory *,
    const Nba97GameActorCollisionResponseEvent *,
    Nba97GameActorCollisionResponseMachine *);

typedef struct Nba97GameActorCollisionResponseContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameActorCollisionResponseMachine machine;
  Nba97GameActorCollisionResponseIo io;
  void *user;
  Nba97GameActorCollisionResponseAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameActorCollisionResponseContext;

typedef struct Nba97GameActorCollisionResponseProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_count[NBA97_GAME_ACTOR_COLLISION_RESPONSE_CALL_KIND_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  uint32_t arithmetic_trap_code;
  Nba97GameActorCollisionResponseWord normal_x;
  Nba97GameActorCollisionResponseWord normal_y;
  Nba97GameActorCollisionResponseWord normal_velocity;
  Nba97GameActorCollisionResponseWord tangent_velocity;
  Nba97GameActorCollisionResponseWord restored_return_address;
  Nba97GameActorCollisionResponseWord returned_value;
  Nba97GameActorCollisionResponseMachine machine;
  uint8_t completed;
} Nba97GameActorCollisionResponseProgress;

enum { NBA97_GAME_ACTOR_COLLISION_RESPONSE_ARITHMETIC_TRAP = -6 };

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8005F3BC
 * Range: 0x8005F3BC..0x8005F887 (inclusive)
 * Source size: 1228 bytes / 307 instructions
 * Evidence: fresh Ghidra game_8005f3bc.txt; instruction SHA-256
 * d2b867cd620f32dcf2beac1d853b0a6d6883441b361f0b627ef5d509b78cf01e
 *
 * Purpose: Resolve actor contact by projecting relative motion onto the
 * contact normal, clamping response factors, updating velocities and
 * animation state, and publishing the symmetric contact IDs.
 * Inputs: All 32 live GPRs and HI/LO; a0 first actor, a1 second actor; live SP
 * and saved-register frame; actor, descriptor and factor-table memory; and
 * twelve full-machine child call sites.
 * Returns: v0 zero when distance or incoming normal speed rejects contact,
 * otherwise one after contact publication; the live-sp epilogue restores
 * ra/s8..s0 and JR consumes the restored ra.
 * Guest memory: Uses a wrapping 0x58-byte stack frame, actor fields, descriptor
 * byte +0x0A, signed factor table 0x800B8324, angle output at SP+0x20, and
 * halfword global 0x800FDB88 in exact source access order.
 * Calls: 0x8005F424 -> 0x8007066C; 0x8005F598 -> 0x8005EA28;
 * 0x8005F68C -> 0x800706E4; 0x8005F6CC -> 0x80056B78;
 * 0x8005F6DC -> 0x80056CE0; 0x8005F6EC -> 0x80056C28;
 * 0x8005F6FC -> 0x80056C84; 0x8005F7BC -> 0x800706E4;
 * 0x8005F7FC -> 0x80056B78; 0x8005F80C -> 0x80056CE0;
 * 0x8005F81C -> 0x80056C28; 0x8005F82C -> 0x80056C84.
 * Original quirks: DIV's source BREAK guards remain modeled after the positive
 * distance gate; every branch/JAL delay executes before refusal; the first E2
 * reload is unsigned then explicitly sign-extended; callback-mutated SP and
 * saved registers stay live; actor ID publication truncates to bytes.
 * Native mapping: Guest addresses remain validated uint32_t mapped values with
 * per-byte knownness and exact access prefixes. Geometry uses a narrow native
 * bridge; remaining children retain explicit full-machine callback contracts.
 */
int nba97_game_actor_collision_response(
    Nba97GameActorCollisionResponseContext *,
    Nba97GameActorCollisionResponseProgress *);

#ifdef __cplusplus
}
#endif
#endif
