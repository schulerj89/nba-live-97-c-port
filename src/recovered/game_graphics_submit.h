#ifndef NBA97_GAME_GRAPHICS_SUBMIT_H
#define NBA97_GAME_GRAPHICS_SUBMIT_H
#include "game_draw_environment.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef Nba97GameDrawEnvironmentWord Nba97GameGraphicsSubmitWord;
typedef Nba97GameDrawEnvironmentMachine Nba97GameGraphicsSubmitMachine;
typedef Nba97GameDrawEnvironmentAccess Nba97GameGraphicsSubmitAccess;

enum Nba97GameGraphicsSubmitCallKind {
  NBA97_GAME_GRAPHICS_SUBMIT_START = 1,
  NBA97_GAME_GRAPHICS_SUBMIT_WAIT,
  NBA97_GAME_GRAPHICS_SUBMIT_DRAIN,
  NBA97_GAME_GRAPHICS_SUBMIT_CRITICAL,
  NBA97_GAME_GRAPHICS_SUBMIT_INSTALL,
  NBA97_GAME_GRAPHICS_SUBMIT_INDIRECT,
  NBA97_GAME_GRAPHICS_SUBMIT_CALL_KIND_COUNT
};

typedef struct Nba97GameGraphicsSubmitEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameGraphicsSubmitEvent;

typedef int (*Nba97GameGraphicsSubmitIo)(void *, const Nba97GameTextMemory *,
                                         const Nba97GameGraphicsSubmitEvent *,
                                         Nba97GameGraphicsSubmitMachine *);

typedef struct Nba97GameGraphicsSubmitContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameGraphicsSubmitMachine machine;
  Nba97GameGraphicsSubmitIo io;
  void *user;
  Nba97GameGraphicsSubmitAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameGraphicsSubmitContext;

typedef struct Nba97GameGraphicsSubmitProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_attempts[NBA97_GAME_GRAPHICS_SUBMIT_CALL_KIND_COUNT];
  size_t call_count[NBA97_GAME_GRAPHICS_SUBMIT_CALL_KIND_COUNT];
  size_t full_queue_iterations;
  size_t gpu_poll_iterations;
  size_t copy_iterations;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  Nba97GameGraphicsSubmitWord saved_critical;
  Nba97GameGraphicsSubmitWord return_v0;
  Nba97GameGraphicsSubmitWord restored_return_address;
  Nba97GameGraphicsSubmitWord restored_s3;
  Nba97GameGraphicsSubmitWord restored_s2;
  Nba97GameGraphicsSubmitWord restored_s1;
  Nba97GameGraphicsSubmitWord restored_s0;
  Nba97GameGraphicsSubmitMachine machine;
  uint8_t queued;
  uint8_t completed;
} Nba97GameGraphicsSubmitProgress;

// clang-format off
/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8009B298
 * Range: 0x8009B298..0x8009B57B (inclusive)
 * Source size: 740 bytes / 185 instructions
 * Evidence: fresh Ghidra game_8009b298.txt; instruction-byte SHA-256 e611ef5c70173153a773cadfb8933306a299c75ba03f1d723a5f824f7f29e9bf
 *
 * Purpose: Submit graphics work immediately when safe or enqueue it in the retained 64-entry graphics scheduling ring.
 * Inputs: Full live GPR/HI/LO state with function in a0, data in a1, signed byte count in a2, callback argument in a3, retained scheduler/MMIO memory, and typed child services.
 * Returns: v0 is zero after direct submission, 0xFFFFFFFF after a failed full-queue wait, or queued depth modulo 64; saved registers restore through live sp.
 * Guest memory: Saves/restores five stack words, reads scheduler flags/head/tail/MMIO status, writes critical state and last-call globals, or copies words and publishes one queue entry at 0x80104748..0x80105F47.
 * Calls: 0x8009BAFC at 0x8009B2BC; 0x8009BB30 at 0x8009B2CC; 0x8009B57C at 0x8009B2DC; 0x800986F8 at 0x8009B304; live s3 at 0x8009B3A8; 0x800986F8 at 0x8009B3D4; 0x8009863C at 0x8009B3EC; 0x800986F8 at 0x8009B530; 0x8009B57C at 0x8009B538.
 * Original quirks: Queue copy truncates signed byte count toward zero by four, reloads head for every destination, and negative counts still publish the embedded pointer; direct GPU polling is unbounded except by the native operation budget.
 * Native mapping: Guest addresses remain validated uint32_t mapped values with per-byte knownness; unresolved routines are full-machine typed callbacks and no GPU body is emulated.
 */
int nba97_game_graphics_submit(Nba97GameGraphicsSubmitContext *,
                               Nba97GameGraphicsSubmitProgress *);
// clang-format on
#ifdef __cplusplus
}
#endif
#endif
