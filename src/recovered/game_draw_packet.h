#ifndef NBA97_GAME_DRAW_PACKET_H
#define NBA97_GAME_DRAW_PACKET_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameDrawPacketWord;
typedef Nba97GameMatchClocksMachine Nba97GameDrawPacketMachine;
typedef Nba97GameMatchClocksAccess Nba97GameDrawPacketAccess;

enum Nba97GameDrawPacketCallKind {
  NBA97_GAME_DRAW_PACKET_CHILD_8009A644 = 1,
  NBA97_GAME_DRAW_PACKET_CHILD_8009A710,
  NBA97_GAME_DRAW_PACKET_CHILD_8009A7DC,
  NBA97_GAME_DRAW_PACKET_CHILD_8009A5E8,
  NBA97_GAME_DRAW_PACKET_CHILD_8009A824,
  NBA97_GAME_DRAW_PACKET_CALL_KIND_COUNT
};

typedef struct Nba97GameDrawPacketEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameDrawPacketEvent;

/* The callback observes JAL's ra and its completed delay slot. It may mutate
 * mapped memory and every CPU GPR, HI, and LO. Return exactly 1 only after the
 * original child boundary returns. */
typedef int (*Nba97GameDrawPacketIo)(void *, const Nba97GameTextMemory *,
                                     const Nba97GameDrawPacketEvent *,
                                     Nba97GameDrawPacketMachine *);

typedef struct Nba97GameDrawPacketContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameDrawPacketMachine machine;
  Nba97GameDrawPacketIo io;
  void *user;
  Nba97GameDrawPacketAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameDrawPacketContext;

typedef struct Nba97GameDrawPacketProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_count[NBA97_GAME_DRAW_PACKET_CALL_KIND_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  Nba97GameDrawPacketWord restored_return_address;
  Nba97GameDrawPacketWord restored_s1;
  Nba97GameDrawPacketWord restored_s0;
  Nba97GameDrawPacketMachine machine;
  uint8_t completed;
} Nba97GameDrawPacketProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8009A344
 * Range: 0x8009A344..0x8009A5E7 (inclusive)
 * Source size: 676 bytes / 169 instructions
 * Evidence: fresh Ghidra game_8009a344.txt; instruction SHA-256
 * a39d124d77f8bebd819dbe4335dba2449efea31d5eeb659df7d165572b0e0da5
 *
 * Purpose: Build a draw-environment GPU packet, including an optional
 * background-clear primitive.
 * Inputs: Full 32-GPR/HI-LO machine; a0 is the guest packet and a1 is the
 * guest environment; retained stack, display-limit halfwords, and five typed
 * child boundaries are live.
 * Returns: v0 is live t0 minus one; ra/s1/s0 reload through live sp, sp then
 * advances by 0x28, and the restored ra is consumed by JR.
 * Guest memory: Saves three frame words, reads environment fields in source
 * order, stores six draw-state words and a count byte, and conditionally uses
 * stack rectangle temporaries to append three background words.
 * Calls: 0x8009A644 at 0x8009A364; 0x8009A710 at 0x8009A39C;
 * 0x8009A7DC at 0x8009A3B0; 0x8009A5E8 at 0x8009A3C8;
 * 0x8009A824 at 0x8009A3D4.
 * Original quirks: The last call's delay slot stores the preceding v0; t0 is
 * assigned seven in the background-test delay; signed size clamps use global
 * halfwords minus one; offset rectangles are distinguished from 64-aligned
 * fills; low-half arithmetic wraps; callbacks keep all machine mutations.
 * Native mapping: Guest addresses remain validated uint32_t mappings with
 * per-byte knownness; unresolved children are typed full-machine callbacks.
 */
int nba97_game_draw_packet(Nba97GameDrawPacketContext *,
                           Nba97GameDrawPacketProgress *);

#ifdef __cplusplus
}
#endif
#endif
