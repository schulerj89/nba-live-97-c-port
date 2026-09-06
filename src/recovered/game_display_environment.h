#ifndef NBA97_GAME_DISPLAY_ENVIRONMENT_H
#define NBA97_GAME_DISPLAY_ENVIRONMENT_H
#include "game_scene_startup.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef Nba97GameSceneStartupWord Nba97GameDisplayEnvironmentWord;
typedef Nba97GameSceneStartupRegisters Nba97GameDisplayEnvironmentRegisters;
typedef struct Nba97GameDisplayEnvironmentMachine {
  Nba97GameDisplayEnvironmentRegisters registers;
  Nba97GameDisplayEnvironmentWord hi, lo;
} Nba97GameDisplayEnvironmentMachine;
enum Nba97GameDisplayEnvironmentCallKind {
  NBA97_GAME_DISPLAY_ENVIRONMENT_DEBUG = 1,
  NBA97_GAME_DISPLAY_ENVIRONMENT_ORIGIN_HELPER,
  NBA97_GAME_DISPLAY_ENVIRONMENT_GPU_COMMAND,
  NBA97_GAME_DISPLAY_ENVIRONMENT_VIDEO_MODE,
  NBA97_GAME_DISPLAY_ENVIRONMENT_COPY,
  NBA97_GAME_DISPLAY_ENVIRONMENT_CALL_KIND_COUNT
};
typedef struct Nba97GameDisplayEnvironmentEvent {
  uint32_t pc, delay_slot_pc, entry;
  size_t operation, invocation;
  uint8_t kind, argument_count;
} Nba97GameDisplayEnvironmentEvent;
typedef int (*Nba97GameDisplayEnvironmentIo)(
    void *, const Nba97GameTextMemory *,
    const Nba97GameDisplayEnvironmentEvent *,
    Nba97GameDisplayEnvironmentMachine *);
enum Nba97GameDisplayEnvironmentAccessKind {
  NBA97_GAME_DISPLAY_ENVIRONMENT_READ = 1,
  NBA97_GAME_DISPLAY_ENVIRONMENT_STORE = 2
};
typedef struct Nba97GameDisplayEnvironmentAccess {
  uint32_t pc, address, value;
  size_t operation;
  uint8_t width, known_mask, kind;
} Nba97GameDisplayEnvironmentAccess;
typedef struct Nba97GameDisplayEnvironmentContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameDisplayEnvironmentMachine machine;
  Nba97GameDisplayEnvironmentIo io;
  void *user;
  Nba97GameDisplayEnvironmentAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameDisplayEnvironmentContext;
typedef struct Nba97GameDisplayEnvironmentProgress {
  size_t operations, accesses, reads, stores, callbacks_completed,
      access_events;
  size_t call_attempts[NBA97_GAME_DISPLAY_ENVIRONMENT_CALL_KIND_COUNT],
      call_count[NBA97_GAME_DISPLAY_ENVIRONMENT_CALL_KIND_COUNT];
  uint32_t stopped_pc, stopped_address, stopped_entry, frame_stack_pointer;
  Nba97GameDisplayEnvironmentWord origin_command, horizontal_command,
      vertical_command, mode_command, return_v0, restored_return_address,
      restored_s3, restored_s2, restored_s1, restored_s0;
  Nba97GameDisplayEnvironmentMachine machine;
  uint8_t screen_rectangle_changed, mode_changed, completed;
} Nba97GameDisplayEnvironmentProgress;
// clang-format off
/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80099CA4
 * Range: 0x80099CA4..0x8009A153 (inclusive)
 * Source size: 1200 bytes / 300 instructions
 * Evidence: fresh Ghidra game_80099ca4.txt; instruction-byte SHA-256
 * 68b6e8d4cf5fd051425a8d1f4b8c9ec1252cc2e0790f713166d6c256853248e0
 *
 * Purpose: Build and submit the PS1 display-origin, screen-range, and display-mode commands, then copy the live display environment into its cache.
 * Inputs: Full live GPR/HI/LO state, display-environment pointer in a0, retained stack/environment/cache/global mappings, and typed debug, origin-helper, GPU-command, video-mode, and copy services.
 * Returns: Live machine with v0 assigned from callback-mutable s0 after copy, five registers restored through live sp, sp advanced by 0x28, and restored ra consumed by JR.
 * Guest memory: Reads display fields and caches 0x800C562C..0x800C563F, debug/hardware globals 0x800C55B8/BC/C0/C2/C3, stores video mode at live s0+0x12, saves/restores five stack words, and delegates the final ordered 20-byte copy.
 * Calls: Dynamic debug at 0x80099CF0; 0x8009A8A8 at 0x80099D14; dynamic GPU command at 0x80099D6C; 0x800985CC at 0x80099DE8; dynamic GPU commands at 0x80099F78 and 0x80099FA4; 0x800985CC at 0x8009A034; dynamic GPU command at 0x8009A114; 0x8009CB0C at 0x8009A128.
 * Original quirks: Source signed clamps and wrapping arithmetic are retained; callbacks may relocate s0/sp and mutate packed bounds; cache equality skips only its corresponding update; every call/branch delay remains live.
 * Native mapping: Guest addresses remain validated uint32_t values with per-byte knownness; all nine unresolved services are typed full-machine callbacks without host pointer casts or fabricated GPU/video behavior.
 */
int nba97_game_display_environment(Nba97GameDisplayEnvironmentContext *,
                                   Nba97GameDisplayEnvironmentProgress *);
// clang-format on
#ifdef __cplusplus
}
#endif
#endif
